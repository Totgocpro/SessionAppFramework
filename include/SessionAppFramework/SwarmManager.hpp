#pragma once

#include "Types.hpp"
#include "Account.hpp"
#include "NetworkClient.hpp"
#include <vector>
#include <string>
#include <memory>

namespace Saf {

/**
 * @brief Manages interaction with the Session Node swarm network.
 *
 * Responsibilities:
 *  - Fetch and cache the current node list from seed nodes
 *  - Resolve the swarm responsible for a given Account ID
 *  - Store (send) encrypted envelope blobs to a swarm
 *  - Retrieve (poll) envelope blobs from a swarm
 *  - Delete expired / read messages from the swarm
 *
 * Namespaces used by Session:
 *  -  0  : DM messages (legacy + v2)
 *  -  1  : Closed group messages (legacy)
 *  -  2  : Config – UserProfile
 *  -  3  : Config – Contacts
 *  -  4  : Config – ConvoInfoVolatile
 *  -  5  : Config – UserGroups
 *  -  11 : Config – GroupInfo
 *  -  12 : Config – GroupMembers
 *  -  13 : Config – GroupKeys
 *  -  15 : Closed group messages (new)
 */
class SwarmManager {
public:
    /// Well-known Session seed node URLs (maintained by Session Foundation)
    static const std::vector<std::string> DefaultSeedNodes;

    /**
     * @param account        The local account (used for signing retrieve requests)
     * @param networkClient  HTTP client to use
     * @param seedNodes      Override seed node URLs (optional)
     */
    SwarmManager(const Account&          account,
                 NetworkClient&          networkClient,
                 std::vector<std::string> seedNodes = {});
    ~SwarmManager();

    SwarmManager(const SwarmManager&)            = delete;
    SwarmManager& operator=(const SwarmManager&) = delete;

    // ─────────────────────────────────────────────────────────
    // Network bootstrapping
    // ─────────────────────────────────────────────────────────

    /**
     * @brief Fetches the current node list from the seed nodes and caches it.
     *
     * Should be called once at startup and periodically to refresh.
     * @throws NetworkException on failure.
     */
    void Bootstrap();

    /// Returns all known Session nodes (after Bootstrap()).
    std::vector<SessionNode> GetAllNodes() const;

    /// Picks 3 random nodes suitable as an onion-routing path.
    std::tuple<SessionNode, SessionNode, SessionNode> PickOnionPath() const;

    // ─────────────────────────────────────────────────────────
    // Swarm resolution
    // ─────────────────────────────────────────────────────────

    /**
     * @brief Returns the swarm nodes responsible for a given Account ID.
     * @param accountId  Target Account ID (66-char hex).
     * @throws NetworkException on failure.
     */
    std::vector<SessionNode> ResolveSwarm(const AccountId& accountId);

    // ─────────────────────────────────────────────────────────
    // Store (send to swarm)
    // ─────────────────────────────────────────────────────────

    /**
     * @brief Stores an encrypted envelope in the swarm of the given Account ID.
     *
     * @param recipientId    The account ID whose swarm should receive this.
     * @param data           Encrypted envelope bytes (produced by MessageService).
     * @param ns             Swarm namespace (default 0 for DMs).
     * @param ttlMs          Time-to-live in milliseconds (default 14 days).
     * @return               Server-assigned message hash.
     * @throws SwarmException / NetworkException on failure.
     */
    std::string Store(const AccountId& recipientId,
                      const Bytes&     data,
                      int              ns    = 0,
                      int64_t          ttlMs = 14LL * 24 * 3600 * 1000);

    /**
     * @brief Authenticated store for groups.
     */
    std::string StoreWithAuth(const AccountId& recipientId,
                              const Bytes&     data,
                              int              ns,
                              int64_t          ttlMs,
                              const Bytes&     authData);

    // ─────────────────────────────────────────────────────────
    // Retrieve (poll own swarm)
    // ─────────────────────────────────────────────────────────

    struct RawEnvelope {
        std::string Hash;
        Bytes       Data;
        int64_t     Timestamp   = 0;  // ms
        int64_t     Expiry      = 0;  // ms
        int         Namespace   = 0;
    };

    /**
     * @brief Polls a swarm for new messages in the given namespace.
     *
     * @param targetId            Account ID whose swarm to poll.
     * @param ns                  Namespace to poll.
     * @param lastHash            Hash of the last retrieved message (optional).
     * @return List of raw encrypted envelopes.
     * @throws SwarmException / NetworkException on failure.
     */
    std::vector<RawEnvelope> Retrieve(const AccountId& targetId,
                                      int              ns       = 0,
                                      const std::string& lastHash = "");

    /**
     * @brief Authenticated retrieve for groups or other accounts.
     */
    std::vector<RawEnvelope> RetrieveWithAuth(const AccountId& targetId,
                                              int              ns,
                                              const std::string& lastHash,
                                              const Bytes& authData);

    /**
     * @brief Deletes specific messages from own swarm by their hashes.
     * @param hashes  List of message hashes to delete.
     */
    void Delete(const std::vector<std::string>& hashes, int ns = 0);

private:
    struct Impl;
    std::unique_ptr<Impl> m_Impl;
};

} // namespace Saf
