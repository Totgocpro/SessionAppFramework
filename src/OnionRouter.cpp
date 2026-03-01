#include <SessionAppFramework/OnionRouter.hpp>
#include <SessionAppFramework/Exceptions.hpp>
#include <SessionAppFramework/Utils.hpp>

#include <openssl/evp.h>

// NOTE: Full onion-routing requires libsession-util compiled with
// -D ENABLE_ONIONERQ.  When available, the onion-request builder can be
// included as:
//   #include <session/onionreq/builder.hpp>
//
// This file provides the scaffolding + documentation for the 3-hop
// encryption scheme.  The NetworkClient::SendOnion() fall-back is used
// in environments where the full onion-routing stack is not available.

namespace Saf {

struct OnionRouter::Impl {
    NetworkClient& Net;
    explicit Impl(NetworkClient& net) : Net(net) {}
};

OnionRouter::OnionRouter(NetworkClient& networkClient)
    : m_Impl(std::make_unique<Impl>(networkClient)) {}

OnionRouter::~OnionRouter() = default;

// ─────────────────────────────────────────────────────────
// Send
//
// Onion request structure (Session protocol):
//
//   Layer 3 (guard → middle):
//     encrypt(
//       json{
//         "host":    middleNode.ip,
//         "port":    middleNode.port,
//         "target":  "/onion_req/v2",
//         "payload": Layer2
//       },
//       guardX25519Pubkey,
//       ephemeralKey1
//     )
//
//   Layer 2 (middle → exit):
//     encrypt(
//       json{
//         "host":    exitNode.ip,
//         "port":    exitNode.port,
//         "target":  "/onion_req/v2",
//         "payload": Layer1
//       },
//       middleX25519Pubkey,
//       ephemeralKey2
//     )
//
//   Layer 1 (exit → destination):
//     encrypt(
//       json{
//         "host":    dest.host,
//         "target":  dest.path,
//         "method":  innerReq.Method,
//         "headers": innerReq.Headers,
//         "body":    innerReq.Body
//       },
//       exitX25519Pubkey,
//       ephemeralKey3
//     )
//
// Each layer uses:
//   - ephemeral X25519 keypair
//   - ECDH(ephemeralPriv, nodeX25519Pub) → 32-byte secret
//   - HKDF-SHA256(secret) → 32-byte AES key + 16-byte MAC key
//   - AES-256-CTR encrypt + HMAC-SHA256 authenticate
//
// The final HTTP request goes to:
//   POST https://guardNode.ip:guardNode.port/onion_req/v2
//   Body: { "ephemeral_key": base64(ephKey1), "enc_payload": base64(Layer3) }
// ─────────────────────────────────────────────────────────

NetworkClient::Response OnionRouter::Send(
    const NetworkClient::Request&  innerRequest,
    const std::vector<SessionNode>& hops)
{
    if (hops.size() != 3)
        throw NetworkException("OnionRouter::Send requires exactly 3 hops");

#ifdef SAF_ENABLE_ONION
    // TODO: Integrate session::onionreq::Builder from libsession-util
    // Example (pseudocode for the actual API once headers are available):
    //
    //   session::onionreq::Builder builder;
    //   builder.set_destination(innerRequest.Url, innerRequest.Body);
    //   builder.add_hop(hops[0].PublicKey);
    //   builder.add_hop(hops[1].PublicKey);
    //   builder.add_hop(hops[2].PublicKey);
    //   auto [payload, shared_secrets] = builder.build();
    //
    //   NetworkClient::Request outerReq;
    //   outerReq.Url  = "https://" + hops[0].Ip + ":" +
    //                   std::to_string(hops[0].Port) + "/onion_req/v2";
    //   outerReq.Body = payload;
    //   auto raw = m_Impl->Net.Send(outerReq);
    //   return builder.decrypt_response(raw.Body, shared_secrets);

    throw NetworkException("OnionRouter: ENABLE_ONIONERQ not compiled in");
#else
    // Fall-through to direct (non-onioned) request
    return m_Impl->Net.Send(innerRequest);
#endif
}

// ─────────────────────────────────────────────────────────
// EncryptHop – single hop encryption
// ─────────────────────────────────────────────────────────

Bytes OnionRouter::EncryptHop(const Bytes& payload,
                               const Bytes& nodeX25519,
                               Bytes&       ephemeralKey) {
    // 1. Generate ephemeral X25519 keypair
    EVP_PKEY* ephPkey = nullptr;
    {
        EVP_PKEY_CTX* kctx = EVP_PKEY_CTX_new_id(EVP_PKEY_X25519, nullptr);
        EVP_PKEY_keygen_init(kctx);
        EVP_PKEY_keygen(kctx, &ephPkey);
        EVP_PKEY_CTX_free(kctx);
    }

    // 2. Extract ephemeral public key
    ephemeralKey.resize(32);
    size_t epkLen = 32;
    EVP_PKEY_get_raw_public_key(ephPkey, ephemeralKey.data(), &epkLen);

    // 3. ECDH
    EVP_PKEY* nodeKey = EVP_PKEY_new_raw_public_key(EVP_PKEY_X25519, nullptr,
                                                      nodeX25519.data(), 32);
    EVP_PKEY_CTX* dctx = EVP_PKEY_CTX_new(ephPkey, nullptr);
    EVP_PKEY_derive_init(dctx);
    EVP_PKEY_derive_set_peer(dctx, nodeKey);
    size_t secLen = 32;
    Bytes secret(32);
    EVP_PKEY_derive(dctx, secret.data(), &secLen);
    EVP_PKEY_CTX_free(dctx);
    EVP_PKEY_free(nodeKey);
    EVP_PKEY_free(ephPkey);

    // 4. Derive AES key via SHA-256
    Bytes aesKey = Utils::Sha256(secret);

    // 5. AES-256-CTR encrypt
    Bytes iv = Utils::RandomBytes(16);
    Bytes ciphertext(payload.size());
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    EVP_EncryptInit_ex(ctx, EVP_aes_256_ctr(), nullptr,
                       aesKey.data(), iv.data());
    int len = 0;
    EVP_EncryptUpdate(ctx, ciphertext.data(), &len,
                      payload.data(), static_cast<int>(payload.size()));
    EVP_EncryptFinal_ex(ctx, ciphertext.data() + len, &len);
    EVP_CIPHER_CTX_free(ctx);

    // Output: iv (16) + ciphertext
    Bytes out;
    out.reserve(16 + ciphertext.size());
    out.insert(out.end(), iv.begin(),         iv.end());
    out.insert(out.end(), ciphertext.begin(), ciphertext.end());
    return out;
}

Bytes OnionRouter::DecryptResponse(const Bytes& ciphertext,
                                    const Bytes& sharedSecret) {
    if (ciphertext.size() < 16)
        throw NetworkException("DecryptResponse: too short");
    Bytes aesKey = Utils::Sha256(sharedSecret);
    Bytes iv(ciphertext.begin(), ciphertext.begin() + 16);
    Bytes ct(ciphertext.begin() + 16, ciphertext.end());
    Bytes plain(ct.size());
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    EVP_DecryptInit_ex(ctx, EVP_aes_256_ctr(), nullptr,
                       aesKey.data(), iv.data());
    int len = 0;
    EVP_DecryptUpdate(ctx, plain.data(), &len,
                      ct.data(), static_cast<int>(ct.size()));
    EVP_DecryptFinal_ex(ctx, plain.data() + len, &len);
    EVP_CIPHER_CTX_free(ctx);
    return plain;
}

} // namespace Saf
