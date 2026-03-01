#include <SessionAppFramework/FileTransfer.hpp>
#include <SessionAppFramework/Exceptions.hpp>
#include <SessionAppFramework/Utils.hpp>

#include <session/attachments.hpp>
#include <nlohmann/json.hpp>
#include <sodium.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/sha.h>
#include <openssl/rand.h>

#include <fstream>
#include <iostream>
#include <sstream>
#include <filesystem>
#include <iomanip>

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace Saf {

struct FileTransfer::Impl {
    const Account& Account_;
    NetworkClient& Net;
    std::string    FileServerUrl;

    Impl(const Account& acc, NetworkClient& net, std::string url)
        : Account_(acc), Net(net), FileServerUrl(std::move(url)) {}
};

FileTransfer::FileTransfer(const Account& account, NetworkClient& networkClient, std::string fileServerUrl)
    : m_Impl(std::make_unique<Impl>(account, networkClient, std::move(fileServerUrl))) {
}

FileTransfer::~FileTransfer() = default;

// --- LEGACY V1 ENCRYPTION (AES-256-CBC + HMAC-SHA256) ---
static std::pair<Bytes, Bytes> EncryptV1(const Bytes& plaintext, Bytes& outKeys) {
    // 1. Generate keys: 32 bytes AES, 32 bytes MAC
    outKeys.resize(64);
    RAND_bytes(outKeys.data(), 64);
    const uint8_t* aesKey = outKeys.data();
    const uint8_t* macKey = outKeys.data() + 32;

    // 2. Generate 16 bytes IV
    uint8_t iv[16];
    RAND_bytes(iv, 16);

    // 3. Encrypt AES-256-CBC
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    EVP_EncryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, aesKey, iv);
    
    Bytes ciphertext(plaintext.size() + 16); 
    int outLen = 0;
    int finalOutLen = 0;
    EVP_EncryptUpdate(ctx, ciphertext.data(), &outLen, plaintext.data(), (int)plaintext.size());
    EVP_EncryptFinal_ex(ctx, ciphertext.data() + outLen, &finalOutLen);
    EVP_CIPHER_CTX_free(ctx);
    ciphertext.resize(outLen + finalOutLen);

    // 4. Construct IV || Ciphertext
    Bytes ivAndCipher;
    ivAndCipher.reserve(16 + ciphertext.size());
    ivAndCipher.insert(ivAndCipher.end(), iv, iv + 16);
    ivAndCipher.insert(ivAndCipher.end(), ciphertext.begin(), ciphertext.end());

    // 5. Calculate HMAC-SHA256(macKey, IV || Ciphertext)
    uint8_t mac[32];
    unsigned int macLen = 32;
    HMAC(EVP_sha256(), macKey, 32, ivAndCipher.data(), ivAndCipher.size(), mac, &macLen);

    // 6. Final blob: IV || Ciphertext || MAC
    Bytes finalBlob = ivAndCipher;
    finalBlob.insert(finalBlob.end(), mac, mac + 32);

    // 7. Calculate SHA-256 Digest
    Bytes digest(32);
    SHA256(finalBlob.data(), finalBlob.size(), digest.data());

    return { finalBlob, digest };
}

static Bytes DecryptV1(const Bytes& data, const Bytes& keys) {
    if (keys.size() != 64) throw FileTransferException("V1 requires 64-byte keys");
    if (data.size() < 16 + 32) throw FileTransferException("V1 data too short");

    const uint8_t* aesKey = keys.data();
    const uint8_t* macKey = keys.data() + 32;

    const uint8_t* iv = data.data();
    const uint8_t* ciphertext = data.data() + 16;
    size_t cipherLen = data.size() - 16 - 32;
    const uint8_t* remoteMac = data.data() + data.size() - 32;

    uint8_t localMac[32];
    unsigned int macLen = 32;
    HMAC(EVP_sha256(), macKey, 32, data.data(), data.size() - 32, localMac, &macLen);

    if (std::memcmp(localMac, remoteMac, 32) != 0) {
        throw FileTransferException("V1 MAC mismatch");
    }

    Bytes plaintext(cipherLen);
    int outLen = 0;
    int finalOutLen = 0;

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    EVP_DecryptInit_ex(ctx, EVP_aes_256_cbc(), NULL, aesKey, iv);
    EVP_DecryptUpdate(ctx, plaintext.data(), &outLen, ciphertext, (int)cipherLen);
    EVP_DecryptFinal_ex(ctx, plaintext.data() + outLen, &finalOutLen);
    EVP_CIPHER_CTX_free(ctx);

    plaintext.resize(outLen + finalOutLen);
    return plaintext;
}

FileInfo FileTransfer::Upload(const std::string& fileName,
                               const Bytes&       data,
                               const std::string& mimeType,
                               ProgressCallback   /*onProgress*/) {
    
    // V1 Encryption by default
    Bytes keys;
    auto [encrypted, digest] = EncryptV1(data, keys);

    std::string url = "https://filev2.getsession.org/file";
    int64_t nowSec = Utils::NowMs() / 1000;
    std::string sig = m_Impl->Account_.MakeSwarmAuthToken("store", "", std::to_string(nowSec));
    std::string edPk = Utils::BytesToHex(m_Impl->Account_.GetPublicKey());

    NetworkClient::Request req;
    req.Method  = "POST";
    req.Url     = url;
    req.Body    = std::string(reinterpret_cast<const char*>(encrypted.data()), encrypted.size());
    req.Headers["Content-Type"] = "application/octet-stream";
    req.Headers["X-FS-Timestamp"] = std::to_string(nowSec);
    req.Headers["X-FS-Signature"] = sig;
    req.Headers["X-FS-Pubkey"]    = edPk;
    req.TimeoutMs = 60000; 

    auto resp = m_Impl->Net.Send(req);
    if (resp.StatusCode != 200) throw FileTransferException("Upload failed: HTTP " + std::to_string(resp.StatusCode));

    json body = json::parse(Utils::BytesToString(resp.Body));
    FileInfo info;
    info.Id       = body.value("id", "");
    info.Url      = "https://filev2.getsession.org/file/" + info.Id;
    info.FileName = fileName;
    info.MimeType = mimeType.empty() ? "application/octet-stream" : mimeType;
    info.Size     = data.size();
    info.Key      = keys;
    info.Digest   = digest;
    return info;
}

FileInfo FileTransfer::UploadFile(const std::string& filePath, const std::string& mimeType, ProgressCallback onProgress) {
    std::ifstream f(filePath, std::ios::binary);
    if (!f.is_open()) throw FileTransferException("Cannot open file: " + filePath);
    Bytes data((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    return Upload(fs::path(filePath).filename().string(), data, mimeType, onProgress);
}

Bytes FileTransfer::Download(const Message& message, ProgressCallback onProgress) {
    FileInfo info;
    if (!message.Data.empty()) {
        try {
            json j = json::parse(Utils::BytesToString(message.Data));
            info.Url    = j.value("url",    "");
            std::string k = j.value("key", "");
            info.Key = Utils::Base64Decode(k);
            if (info.Key.size() != 32 && info.Key.size() != 64) {
                try { info.Key = Utils::HexToBytes(k); } catch(...) {}
            }
            std::string d = j.value("digest", "");
            if (!d.empty()) info.Digest = Utils::Base64Decode(d);
        } catch (...) {}
    }
    if (info.Url.empty()) info.Url = message.Body;
    return Download(info, onProgress);
}

Bytes FileTransfer::Download(const FileInfo& info, ProgressCallback /*onProgress*/) {
    if (info.Key.empty()) throw FileTransferException("No decryption key");

    std::string fileId = info.Id;
    if (fileId.empty()) {
        size_t lastSlash = info.Url.find_last_of("/");
        if (lastSlash != std::string::npos) fileId = info.Url.substr(lastSlash + 1);
        else fileId = info.Url;
    }

    std::string url = "https://filev2.getsession.org/file/" + fileId;

    try {
        int64_t nowSec = Utils::NowMs() / 1000;
        std::string sig = m_Impl->Account_.MakeSwarmAuthToken("retrieve", "", std::to_string(nowSec));
        std::string edPk = Utils::BytesToHex(m_Impl->Account_.GetPublicKey());

        NetworkClient::Request req;
        req.Method = "GET";
        req.Url    = url;
        req.Headers["User-Agent"] = "Session/1.18.4 (Linux)";
        req.Headers["X-FS-Timestamp"] = std::to_string(nowSec);
        req.Headers["X-FS-Signature"] = sig;
        req.Headers["X-FS-Pubkey"]    = edPk;
        req.TimeoutMs = 60000;

        auto resp = m_Impl->Net.Send(req);
        if (resp.StatusCode != 200) {
            req.Headers.erase("X-FS-Timestamp");
            req.Headers.erase("X-FS-Signature");
            req.Headers.erase("X-FS-Pubkey");
            resp = m_Impl->Net.Send(req);
            if (resp.StatusCode != 200) throw FileTransferException("HTTP " + std::to_string(resp.StatusCode));
        }

        Bytes rawData = resp.Body;

        if (info.Key.size() == 64 && (rawData.empty() || rawData[0] != 0x53)) {
            return DecryptV1(rawData, info.Key);
        }

        size_t offset = 0;
        bool foundS = false;
        if (!rawData.empty() && rawData[0] == 0x53) {
            foundS = true;
            offset = 0;
        } else {
            for (size_t i = 0; i < std::min<size_t>(rawData.size(), 64); ++i) {
                if (rawData[i] == 0x53) {
                    offset = i;
                    foundS = true;
                    break;
                }
            }
        }

        if (!foundS) {
            if (info.Key.size() == 64) return DecryptV1(rawData, info.Key);
            throw FileTransferException("Unknown file format");
        }

        Bytes key32 = info.Key;
        if (key32.size() > 32) key32.resize(32);

        auto decrypted = session::attachment::decrypt(
            std::span<const std::byte>{reinterpret_cast<const std::byte*>(rawData.data() + offset), rawData.size() - offset},
            std::span<const std::byte, 32>{reinterpret_cast<const std::byte*>(key32.data()), 32}
        );
        Bytes out(decrypted.size());
        std::memcpy(out.data(), decrypted.data(), decrypted.size());
        return out;

    } catch (const std::exception& e) {
        throw FileTransferException(std::string("Download failed: ") + e.what());
    }
}

void FileTransfer::DownloadToFile(const Message& message, const std::string& destPath, ProgressCallback onProgress) {
    auto data = Download(message, onProgress);
    std::ofstream f(destPath, std::ios::binary);
    if (!f.is_open()) throw FileTransferException("Cannot write to: " + destPath);
    f.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
}

} // namespace Saf
