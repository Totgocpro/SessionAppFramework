#pragma once

#include "Types.hpp"
#include "SessionNode.hpp"
#include <string>
#include <map>
#include <memory>
#include <future>

namespace Saf {

/**
 * @brief Low-level HTTP client used to communicate with Session Nodes.
 *
 * Wraps libcurl.  Supports:
 *  - Plain HTTPS requests (for seed node queries)
 *  - Onion-routed requests through 3 random Session nodes
 *
 * You usually do not use this class directly; use SwarmManager,
 * MessageService, or FileTransfer instead.
 */
class NetworkClient {
public:
    struct Response {
        int         StatusCode = 0;
        Bytes       Body;
        std::map<std::string, std::string> Headers;
    };

    struct Request {
        std::string Method      = "GET";
        std::string Url;
        std::string Body;
        std::map<std::string, std::string> Headers;
        int TimeoutMs           = 10000;
    };

    NetworkClient();
    ~NetworkClient();

    NetworkClient(const NetworkClient&)             = delete;
    NetworkClient& operator=(const NetworkClient&)  = delete;

    // ─────────────────────────────────────────────────────────
    // Plain HTTPS (no onion routing)
    // ─────────────────────────────────────────────────────────

    Response Send(const Request& req);

    // ─────────────────────────────────────────────────────────
    // Onion-routed request  (3-hop)
    //
    // guardNode / middleNode / exitNode: the three hop nodes.
    // The actual JSON payload is sent to exitNode's swarm endpoint.
    // ─────────────────────────────────────────────────────────

    Response SendOnion(const Request& innerReq,
                       const SessionNode& guardNode,
                       const SessionNode& middleNode,
                       const SessionNode& exitNode);

    // ─────────────────────────────────────────────────────────
    // Helper: POST JSON body
    // ─────────────────────────────────────────────────────────
    Response PostJson(const std::string& url,
                      const std::string& jsonBody,
                      int timeoutMs = 10000);

    Response GetJson(const std::string& url, int timeoutMs = 10000);

private:
    struct Impl;
    std::unique_ptr<Impl> m_Impl;
};

} // namespace Saf
