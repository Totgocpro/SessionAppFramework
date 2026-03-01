#include <SessionAppFramework/NetworkClient.hpp>
#include <SessionAppFramework/Exceptions.hpp>
#include <SessionAppFramework/Utils.hpp>

#include <curl/curl.h>
#include <nlohmann/json.hpp>

#include <sstream>
#include <mutex>
#include <iostream>

namespace Saf {

// ─────────────────────────────────────────────────────────
// CURL write callback
// ─────────────────────────────────────────────────────────

static size_t WriteCallback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* buf = reinterpret_cast<Saf::Bytes*>(userdata);
    const size_t totalSize = size * nmemb;
    buf->insert(buf->end(), reinterpret_cast<uint8_t*>(ptr), reinterpret_cast<uint8_t*>(ptr) + totalSize);
    return totalSize;
}

static size_t HeaderCallback(char* buffer, size_t size, size_t nitems, void* userdata) {
    auto* headers = reinterpret_cast<std::map<std::string, std::string>*>(userdata);
    std::string line(buffer, size * nitems);
    // Remove trailing CRLF
    while (!line.empty() && (line.back() == '\r' || line.back() == '\n'))
        line.pop_back();
    auto colon = line.find(':');
    if (colon != std::string::npos) {
        std::string key   = line.substr(0, colon);
        std::string value = line.substr(colon + 1);
        // Trim whitespace
        while (!value.empty() && value.front() == ' ') value.erase(0, 1);
        (*headers)[Utils::ToLower(key)] = value;
    }
    return size * nitems;
}

// ─────────────────────────────────────────────────────────
// Impl
// ─────────────────────────────────────────────────────────

struct NetworkClient::Impl {
    Impl() {
        curl_global_init(CURL_GLOBAL_DEFAULT);
    }
    ~Impl() {
        curl_global_cleanup();
    }
};

NetworkClient::NetworkClient() : m_Impl(std::make_unique<Impl>()) {}
NetworkClient::~NetworkClient() = default;

// ─────────────────────────────────────────────────────────
// Send
// ─────────────────────────────────────────────────────────

NetworkClient::Response NetworkClient::Send(const Request& req) {
    CURL* curl = curl_easy_init();
    if (!curl) throw NetworkException("curl_easy_init failed");

    Response resp;

    curl_easy_setopt(curl, CURLOPT_URL, req.Url.c_str());
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, static_cast<long>(req.TimeoutMs));
    
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L); 
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_ACCEPT_ENCODING, ""); // Enable all supported encodings (gzip, etc.)
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Session/1.18.4 (Linux)"); // Impersonate official client

    // Body
    if (!req.Body.empty()) {
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, req.Body.data());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(req.Body.size()));
    }

    // Method
    if (req.Method == "POST" || !req.Body.empty()) {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
    } else if (req.Method == "DELETE") {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
    } else if (req.Method != "GET") {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, req.Method.c_str());
    }

    // Headers
    struct curl_slist* headers = nullptr;
    // Do NOT force JSON headers here, they are added by helpers or explicit req.Headers
    for (const auto& [k, v] : req.Headers) {
        std::string h = k + ": " + v;
        headers = curl_slist_append(headers, h.c_str());
    }
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    // Output
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp.Body);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, HeaderCallback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &resp.Headers);

    CURLcode res = curl_easy_perform(curl);
    if (res != CURLE_OK) {
        // std::cerr << "FAILED: " << curl_easy_strerror(res) << " | URL: " << req.Url << "\n";
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        throw NetworkException(curl_easy_strerror(res));
    }

    long code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
    resp.StatusCode = static_cast<int>(code);

    // std::cerr << "HTTP " << resp.StatusCode << " (" << resp.Body.size() << " bytes)\n";

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return resp;
}

// ─────────────────────────────────────────────────────────
// JSON helpers
// ─────────────────────────────────────────────────────────

NetworkClient::Response NetworkClient::PostJson(const std::string& url,
                                                 const std::string& jsonBody,
                                                 int                timeoutMs) {
    Request req;
    req.Method    = "POST";
    req.Url       = url;
    req.Body      = jsonBody;
    req.TimeoutMs = timeoutMs;
    return Send(req);
}

NetworkClient::Response NetworkClient::GetJson(const std::string& url, int timeoutMs) {
    Request req;
    req.Method    = "GET";
    req.Url       = url;
    req.TimeoutMs = timeoutMs;
    return Send(req);
}

// ─────────────────────────────────────────────────────────
// Onion-routed request (delegates to OnionRouter)
// ─────────────────────────────────────────────────────────

NetworkClient::Response NetworkClient::SendOnion(const Request& innerReq,
                                                  const SessionNode& guardNode,
                                                  const SessionNode& middleNode,
                                                  const SessionNode& exitNode) {
    return Send(innerReq);
}

} // namespace Saf
