#pragma once

#include "Types.hpp"
#include "Exceptions.hpp"
#include <string>
#include <array>
#include <memory>

namespace Saf {

/**
 * @brief Manages a Session account (identity).
 *
 * A Session account is identified by an Ed25519 keypair.
 *  - The public key (32 bytes, hex-prefixed with "05") is the Account ID.
 *  - The private key is stored securely in memory only.
 *  - The mnemonic (25-word seed phrase) is the human-readable backup.
 *
 * Usage:
 * @code
 *   Account acc;
 *   acc.Create();                            // generate new account
 *   std::cout << acc.GetAccountId();         // print account ID
 *   std::cout << acc.GetMnemonic();          // 25-word backup phrase
 *
 *   Account acc2;
 *   acc2.LoadFromMnemonic("word1 word2 ..."); // restore existing account
 * @endcode
 */
class Account {
public:
    Account();
    ~Account();

    // Non-copyable (keys must not be duplicated accidentally)
    Account(const Account&)             = delete;
    Account& operator=(const Account&)  = delete;
    Account(Account&&)                  = default;
    Account& operator=(Account&&)       = default;

    // ─────────────────────────────────────────────────────────
    // Account creation / restoration
    // ─────────────────────────────────────────────────────────

    /**
     * @brief Generates a brand new Session account with a fresh keypair.
     * @throws CryptoException on RNG failure.
     */
    void Create();

    /**
     * @brief Restores an account from its 25-word mnemonic seed phrase.
     * @param mnemonic  Space-separated word list (as produced by GetMnemonic()).
     * @throws InvalidMnemonicException if the phrase is malformed.
     */
    void LoadFromMnemonic(const std::string& mnemonic);

    /**
     * @brief Restores an account from a raw 32-byte Ed25519 private seed.
     * @param seed  32 bytes.
     * @throws CryptoException on invalid seed length.
     */
    void LoadFromSeed(const Bytes& seed);

    // ─────────────────────────────────────────────────────────
    // Identity accessors
    // ─────────────────────────────────────────────────────────

    /// Returns the 66-character hex Account ID ("05" + 32-byte pubkey)
    AccountId   GetAccountId()  const;

    /// Returns the 25-word mnemonic recovery phrase
    std::string GetMnemonic()   const;

    /// Returns the raw 32-byte Ed25519 private seed
    Bytes       GetPrivateSeed() const;

    /// Returns the raw 32-byte Ed25519 public key
    Bytes       GetPublicKey()  const;

    /// Returns the raw 64-byte Ed25519 private key (libsodium-style)
    Bytes       GetEd25519PrivateKey() const;

    /// Returns the X25519 public key derived from the Ed25519 pubkey (used for encryption)
    Bytes       GetX25519PublicKey()  const;

    /// Returns the X25519 private key (used for ECDH key exchange)
    Bytes       GetX25519PrivateKey() const;

    /// Returns true if a keypair is loaded
    bool        IsInitialized() const;

    // ─────────────────────────────────────────────────────────
    // Signing
    // ─────────────────────────────────────────────────────────

    /**
     * @brief Sign a message with the Ed25519 private key.
     * @param message  Arbitrary data to sign.
     * @return 64-byte signature.
     * @throws AccountNotInitializedException
     */
    Bytes Sign(const Bytes& message) const;

    /**
     * @brief Verify a signature against a public key.
     * @param message    Data that was signed.
     * @param signature  64-byte Ed25519 signature.
     * @param publicKey  32-byte Ed25519 public key of the alleged signer.
     * @return true if the signature is valid.
     */
    static bool Verify(const Bytes& message,
                       const Bytes& signature,
                       const Bytes& publicKey);

    // ─────────────────────────────────────────────────────────
    // Swarm authentication (used by SwarmManager)
    // ─────────────────────────────────────────────────────────

    /**
     * @brief Produces the authentication token for a swarm request.
     *
     * The token is an Ed25519 signature over:
     *   METHOD + NAMESPACE + TIMESTAMP (as strings, no separator)
     *
     * @param method     e.g. "retrieve", "store"
     * @param ns         namespace integer as string, e.g. "0"
     * @param timestamp  Unix seconds as string
     * @return base64url-encoded 64-byte signature
     */
    std::string MakeSwarmAuthToken(const std::string& method,
                                   const std::string& ns,
                                   const std::string& timestamp) const;

private:
    struct Impl;
    std::unique_ptr<Impl> m_Impl;
};

} // namespace Saf
