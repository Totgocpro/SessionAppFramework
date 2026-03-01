#pragma once

#include "Types.hpp"
#include "NetworkClient.hpp"
#include <vector>
#include <string>
#include <memory>

namespace Saf {

/**
 * @brief Builds and dispatches 3-hop onion requests.
 *
 * An onion request wraps the payload in 3 layers of encryption
 * (one per hop node).  Each node can only see its upstream neighbour
 * and its downstream neighbour, preserving sender anonymity.
 *
 * Encryption layers (outermost → innermost):
 *  - Guard node layer  : encrypted to guard node X25519 pubkey
 *  - Middle node layer : encrypted to middle node X25519 pubkey
 *  - Exit node layer   : encrypted to exit node X25519 pubkey
 *  - Payload           : the actual HTTPS request to the final destination
 *
 * Note: This requires libsession-util built with ENABLE_ONIONERQ.
 */
class OnionRouter {
public:
    explicit OnionRouter(NetworkClient& networkClient);
    ~OnionRouter();

    OnionRouter(const OnionRouter&)            = delete;
    OnionRouter& operator=(const OnionRouter&) = delete;

    /**
     * @brief Wraps a plain HTTP request into a 3-hop onion request and sends it.
     *
     * @param innerRequest  The actual request destined for the final server.
     * @param hops          Exactly 3 SessionNodes: guard, middle, exit.
     * @return The decrypted response from the final server.
     * @throws NetworkException on onion routing failure.
     */
    NetworkClient::Response Send(const NetworkClient::Request&      innerRequest,
                                 const std::vector<SessionNode>&     hops);

    /**
     * @brief Encrypts the payload for a single hop.
     *
     * Used internally; exposed for testing.
     *
     * @param payload     Plaintext JSON blob for this hop.
     * @param nodeX25519  32-byte X25519 public key of the hop node.
     * @param ephemeralKey Output: the ephemeral X25519 pubkey to include in the header.
     * @return Ciphertext blob.
     */
    static Bytes EncryptHop(const Bytes&  payload,
                            const Bytes&  nodeX25519,
                            Bytes&        ephemeralKey);

    /**
     * @brief Decrypts the exit node's response.
     *
     * @param ciphertext   Raw ciphertext from the exit node.
     * @param sharedSecret 32-byte shared secret (derived from ECDH).
     * @return Decrypted response bytes.
     */
    static Bytes DecryptResponse(const Bytes& ciphertext,
                                 const Bytes& sharedSecret);

private:
    struct Impl;
    std::unique_ptr<Impl> m_Impl;
};

} // namespace Saf
