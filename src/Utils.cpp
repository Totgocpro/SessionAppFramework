#include <SessionAppFramework/Utils.hpp>
#include <SessionAppFramework/Exceptions.hpp>

#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/sha.h>

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace Saf::Utils {

// ─────────────────────────────────────────────────────────
// Hex
// ─────────────────────────────────────────────────────────

std::string BytesToHex(const Bytes& data) {
    std::ostringstream oss;
    oss << std::hex << std::setfill('0');
    for (auto b : data)
        oss << std::setw(2) << static_cast<int>(b);
    return oss.str();
}

Bytes HexToBytes(const std::string& hex) {
    if (hex.size() % 2 != 0)
        throw std::invalid_argument("HexToBytes: odd-length hex string");
    Bytes out;
    out.reserve(hex.size() / 2);
    for (size_t i = 0; i < hex.size(); i += 2) {
        auto byte = static_cast<uint8_t>(std::stoi(hex.substr(i, 2), nullptr, 16));
        out.push_back(byte);
    }
    return out;
}

// ─────────────────────────────────────────────────────────
// Base64
// ─────────────────────────────────────────────────────────

static const char* kB64Chars =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static const char* kB64UrlChars =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

static std::string Base64EncodeInternal(const Bytes& data, const char* alphabet, bool padding) {
    std::string out;
    out.reserve(((data.size() + 2) / 3) * 4);
    size_t i = 0;
    while (i + 2 < data.size()) {
        uint32_t triple = (data[i] << 16) | (data[i + 1] << 8) | data[i + 2];
        out += alphabet[(triple >> 18) & 0x3F];
        out += alphabet[(triple >> 12) & 0x3F];
        out += alphabet[(triple >>  6) & 0x3F];
        out += alphabet[(triple      ) & 0x3F];
        i += 3;
    }
    if (i < data.size()) {
        uint32_t partial = data[i] << 16;
        if (i + 1 < data.size()) partial |= data[i + 1] << 8;
        out += alphabet[(partial >> 18) & 0x3F];
        out += alphabet[(partial >> 12) & 0x3F];
        if (i + 1 < data.size()) out += alphabet[(partial >> 6) & 0x3F];
        else if (padding)        out += '=';
        if (padding) out += '=';
    }
    return out;
}

std::string Base64Encode(const Bytes& data) {
    return Base64EncodeInternal(data, kB64Chars, true);
}

std::string Base64UrlEncode(const Bytes& data) {
    return Base64EncodeInternal(data, kB64UrlChars, false);
}

static int B64Decode6(char c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+' || c == '-') return 62;
    if (c == '/' || c == '_') return 63;
    if (c == '=')             return 0;   // padding
    return -1;
}

static Bytes Base64DecodeInternal(const std::string& b64) {
    Bytes out;
    out.reserve((b64.size() * 3) / 4);
    size_t i = 0;
    while (i < b64.size()) {
        if (i + 3 >= b64.size() && b64[i] == '=') break;
        int a = B64Decode6(b64[i]);
        int b = (i + 1 < b64.size()) ? B64Decode6(b64[i + 1]) : 0;
        int c = (i + 2 < b64.size()) ? B64Decode6(b64[i + 2]) : 0;
        int d = (i + 3 < b64.size()) ? B64Decode6(b64[i + 3]) : 0;
        if (a < 0) break;
        out.push_back(static_cast<uint8_t>((a << 2) | (b >> 4)));
        if (i + 2 < b64.size() && b64[i + 2] != '=')
            out.push_back(static_cast<uint8_t>((b << 4) | (c >> 2)));
        if (i + 3 < b64.size() && b64[i + 3] != '=')
            out.push_back(static_cast<uint8_t>((c << 6) | d));
        i += 4;
    }
    return out;
}

Bytes Base64Decode(const std::string& b64)    { return Base64DecodeInternal(b64); }
Bytes Base64UrlDecode(const std::string& b64) { return Base64DecodeInternal(b64); }

// ─────────────────────────────────────────────────────────
// Time
// ─────────────────────────────────────────────────────────

int64_t NowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

int64_t NowSeconds() {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

// ─────────────────────────────────────────────────────────
// String helpers
// ─────────────────────────────────────────────────────────

std::string ToLower(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(), ::tolower);
    return out;
}

std::string ToUpper(const std::string& s) {
    std::string out = s;
    std::transform(out.begin(), out.end(), out.begin(), ::toupper);
    return out;
}

bool StartsWith(const std::string& s, const std::string& prefix) {
    return s.size() >= prefix.size() && s.substr(0, prefix.size()) == prefix;
}

Bytes StringToBytes(const std::string& s) {
    return Bytes(s.begin(), s.end());
}

std::string BytesToString(const Bytes& b) {
    return std::string(b.begin(), b.end());
}

// ─────────────────────────────────────────────────────────
// Random
// ─────────────────────────────────────────────────────────

Bytes RandomBytes(size_t n) {
    Bytes out(n);
    if (RAND_bytes(out.data(), static_cast<int>(n)) != 1)
        throw CryptoException("RAND_bytes failed");
    return out;
}

// ─────────────────────────────────────────────────────────
// Hash
// ─────────────────────────────────────────────────────────

Bytes Sha512(const Bytes& data) {
    Bytes out(SHA512_DIGEST_LENGTH);
    SHA512(data.data(), data.size(), out.data());
    return out;
}

Bytes Sha256(const Bytes& data) {
    Bytes out(SHA256_DIGEST_LENGTH);
    SHA256(data.data(), data.size(), out.data());
    return out;
}

} // namespace Saf::Utils
