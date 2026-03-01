#include <SessionAppFramework/Account.hpp>
#include <SessionAppFramework/Utils.hpp>
#include <SessionAppFramework/Exceptions.hpp>

// libsession-util headers
#include <session/ed25519.hpp>
#include <session/curve25519.hpp>

#include <openssl/evp.h>
#include <openssl/sha.h>

#include <array>
#include <cstring>
#include <stdexcept>
#include <sstream>
#include <vector>
#include <string>
#include <iostream>

namespace Saf {

// ─────────────────────────────────────────────────────────
// Impl
// ─────────────────────────────────────────────────────────

struct Account::Impl {
    bool      Initialized   = false;
    Bytes     PrivateSeed;      // 32 bytes Ed25519 seed
    Bytes     PublicKey;        // 32 bytes Ed25519 pubkey
    Bytes     X25519Public;     // 32 bytes X25519 pubkey
    Bytes     X25519Private;    // 32 bytes X25519 privkey
    Bytes     Ed25519SK;        // 64 bytes Ed25519 SK (seed + PK)
    std::string Mnemonic;
    AccountId   Id;             // "05" + hex(X25519Public)

    void DeriveAll() {
        if (PrivateSeed.size() != 32) throw CryptoException("Invalid seed size");

        // 1. Derive Ed25519 Keypair using libsession-util/sodium
        // session::ed25519::ed25519_key_pair(seed) -> {pk, sk}
        
        auto [pk, sk] = session::ed25519::ed25519_key_pair(std::span<const unsigned char>(PrivateSeed.data(), 32));
        
        PublicKey.assign(pk.begin(), pk.end());
        Ed25519SK.assign(sk.begin(), sk.end());

        // 2. Convert to X25519
        auto xpk = session::curve25519::to_curve25519_pubkey(pk);
        auto xsk = session::curve25519::to_curve25519_seckey(sk);

        X25519Public.assign(xpk.begin(), xpk.end());
        X25519Private.assign(xsk.begin(), xsk.end());

        // 3. Build Session ID: "05" + hex(X25519Public)
        Bytes idBytes = { 0x05 };
        idBytes.insert(idBytes.end(), X25519Public.begin(), X25519Public.end());
        Id = Utils::BytesToHex(idBytes);

        Initialized = true;
    }
};

// ─────────────────────────────────────────────────────────
// Account
// ─────────────────────────────────────────────────────────

Account::Account() : m_Impl(std::make_unique<Impl>()) {}
Account::~Account() = default;

void Account::Create() {
    m_Impl->PrivateSeed = Utils::RandomBytes(32);
    m_Impl->Mnemonic = Utils::BytesToHex(m_Impl->PrivateSeed);
    m_Impl->DeriveAll();
}

void Account::LoadFromMnemonic(const std::string& mnemonic) {
    Bytes seed;
    if (mnemonic.size() == 64) {
        try { seed = Utils::HexToBytes(mnemonic); }
        catch (...) { throw InvalidMnemonicException(); }
    } else {
        throw InvalidMnemonicException("pass the 64-char hex seed for now");
    }
    m_Impl->PrivateSeed = seed;
    m_Impl->Mnemonic    = mnemonic;
    m_Impl->DeriveAll();
}

void Account::LoadFromSeed(const Bytes& seed) {
    if (seed.size() != 32) throw CryptoException("Seed must be 32 bytes");
    m_Impl->PrivateSeed = seed;
    m_Impl->Mnemonic    = Utils::BytesToHex(seed);
    m_Impl->DeriveAll();
}

AccountId Account::GetAccountId()       const { return m_Impl->Id; }
std::string Account::GetMnemonic()      const { return m_Impl->Mnemonic; }
Bytes Account::GetPrivateSeed()         const { return m_Impl->PrivateSeed; }
Bytes Account::GetPublicKey()           const { return m_Impl->PublicKey; }
Bytes Account::GetEd25519PrivateKey()   const { return m_Impl->Ed25519SK; }
Bytes Account::GetX25519PublicKey()     const { return m_Impl->X25519Public; }
Bytes Account::GetX25519PrivateKey()    const { return m_Impl->X25519Private; }
bool Account::IsInitialized()           const { return m_Impl->Initialized; }

Bytes Account::Sign(const Bytes& message) const {
    if (!m_Impl->Initialized) throw AccountNotInitializedException();

    // Session Swarm Auth uses standard Ed25519 detached signature
    // verified against the Ed25519 public key.
    
    auto sig = session::ed25519::sign(
        std::span<const unsigned char>(m_Impl->Ed25519SK.data(), 64),
        std::span<const unsigned char>(message.data(), message.size())
    );
    
    return Bytes(sig.begin(), sig.end());
}

bool Account::Verify(const Bytes& message,
                     const Bytes& signature,
                     const Bytes& publicKey) {
    if (signature.size() != 64 || publicKey.size() != 32) return false;

    return session::ed25519::verify(
        std::span<const unsigned char>(signature.data(), 64),
        std::span<const unsigned char>(publicKey.data(), 32),
        std::span<const unsigned char>(message.data(), message.size())
    );
}

std::string Account::MakeSwarmAuthToken(const std::string& method,
                                         const std::string& ns,
                                         const std::string& timestamp) const {
    if (!m_Impl->Initialized) throw AccountNotInitializedException();
    
    // Payload: method + (ns == 0 ? "" : ns) + timestamp
    // Matches session-desktop: `${params.method}${params.namespace === 0 ? '' : params.namespace}${signatureTimestamp}`
    std::string nsPart = (ns == "0" || ns == "") ? "" : ns;
    std::string payload = method + nsPart + timestamp;
    
    // Sign the UTF-8 string
    Bytes sig = Sign(Utils::StringToBytes(payload));
    return Utils::Base64Encode(sig);
}

} // namespace Saf
