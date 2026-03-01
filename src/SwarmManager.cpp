#include <SessionAppFramework/SwarmManager.hpp>
#include <SessionAppFramework/Exceptions.hpp>
#include <SessionAppFramework/Utils.hpp>

#include <nlohmann/json.hpp>
#include <oxenc/bt_producer.h>

#include <algorithm>
#include <random>
#include <sstream>
#include <stdexcept>
#include <iostream>
#include <map>
#include <atomic>

// libsession-util headers for crypto
#include <session/session_encrypt.hpp>
#include <session/config/groups/keys.hpp>
#include <session/config/groups/info.hpp>
#include <session/config/groups/members.hpp>
#include <sodium.h>

using json = nlohmann::json;

namespace Saf {

// ─────────────────────────────────────────────────────────
// Network Time
// ─────────────────────────────────────────────────────────

static std::atomic<int64_t> g_NetworkOffset{0};

static int64_t GetNetworkTimeMs() {
    return Utils::NowMs() + g_NetworkOffset.load();
}

// ─────────────────────────────────────────────────────────
// Default Session seed nodes
// ─────────────────────────────────────────────────────────

const std::vector<std::string> SwarmManager::DefaultSeedNodes = {
    "https://seed1.getsession.org",
    "https://seed2.getsession.org",
    "https://seed3.getsession.org",
};

// ─────────────────────────────────────────────────────────
// Impl
// ─────────────────────────────────────────────────────────

struct SwarmManager::Impl {
    const Account&           Account_;
    NetworkClient&           Net;
    std::vector<std::string> SeedNodes;
    std::vector<SessionNode> NodeList;
    std::mt19937             Rng{std::random_device{}()};

    Impl(const Account& acc, NetworkClient& net, std::vector<std::string> seeds)
        : Account_(acc), Net(net), SeedNodes(std::move(seeds)) {}

    static std::string JsonToString(const json& v) {
        if (v.is_string()) return v.get<std::string>();
        std::string s = v.dump();
        if (s.size() >= 2 && s.front() == '"' && s.back() == '"')
            s = s.substr(1, s.size() - 2);
        return s;
    }

    static SessionNode ParseNode(const json& obj) {
        SessionNode n;
        try {
            if (obj.contains("pubkey_ed25519")) n.PublicKey = JsonToString(obj["pubkey_ed25519"]);
            if (obj.contains("public_ip"))      n.Ip        = JsonToString(obj["public_ip"]);
            else if (obj.contains("ip"))        n.Ip        = JsonToString(obj["ip"]);

            if (obj.contains("storage_port")) {
                n.Port = obj["storage_port"].is_number() ? obj["storage_port"].get<int>() : std::stoi(JsonToString(obj["storage_port"]));
            } else if (obj.contains("port")) {
                n.Port = obj["port"].is_number() ? obj["port"].get<int>() : std::stoi(JsonToString(obj["port"]));
            } else if (obj.contains("port_https")) {
                n.Port = obj["port_https"].is_number() ? obj["port_https"].get<int>() : std::stoi(JsonToString(obj["port_https"]));
            }
            
            if (obj.contains("swarm_id"))       n.SwarmId   = JsonToString(obj["swarm_id"]);
            else if (obj.contains("swarm"))     n.SwarmId   = JsonToString(obj["swarm"]);
        } catch (...) {}
        return n;
    }

    static std::string NodeUrl(const std::string& ip, int port, const std::string& path) {
        std::string host = ip;
        if (host.find(':') != std::string::npos) host = "[" + host + "]";
        return "https://" + host + ":" + std::to_string(port) + path;
    }

    static uint64_t PubkeyToSwarmSpace(const Bytes& xPk) {
        if (xPk.size() != 32) return 0;
        uint64_t res = 0;
        for (size_t i = 0; i < 4; i++) {
            uint64_t buf;
            std::memcpy(&buf, xPk.data() + i * 8, 8);
            res ^= buf;
        }
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
        return __builtin_bswap64(res);
#else
        return res;
#endif
    }
};

SwarmManager::SwarmManager(const Account& account, NetworkClient& networkClient, std::vector<std::string> seedNodes)
    : m_Impl(std::make_unique<Impl>(account, networkClient, seedNodes.empty() ? DefaultSeedNodes : std::move(seedNodes))) {}

SwarmManager::~SwarmManager() = default;

void SwarmManager::Bootstrap() {
    std::string lastError = "No seed nodes configured";
    for (const auto& seed : m_Impl->SeedNodes) {
        try {
            json req = {
                {"jsonrpc", "2.0"},
                {"id",      "0"},
                {"method",  "get_n_service_nodes"},
                {"params",  {
                    {"fields", {
                        {"pubkey_ed25519", true},
                        {"public_ip",      true},
                        {"storage_port",   true},
                        {"swarm_id",       true}
                    }}
                }}
            };

            auto resp = m_Impl->Net.PostJson(seed + "/json_rpc", req.dump(), 5000);
            if (resp.StatusCode != 200) {
                lastError = "Seed " + seed + " returned HTTP " + std::to_string(resp.StatusCode);
                continue;
            }

            auto body = json::parse(Utils::BytesToString(resp.Body));
            if (body.contains("result")) body = body["result"];
            
            auto& result = body["service_node_states"];
            m_Impl->NodeList.clear();
            for (const auto& n : result) {
                auto node = Impl::ParseNode(n);
                if (!node.Ip.empty() && !node.PublicKey.empty())
                    m_Impl->NodeList.push_back(std::move(node));
            }
            if (!m_Impl->NodeList.empty()) {
                // Fetch network time
                try {
                    auto timeResp = m_Impl->Net.PostJson(Impl::NodeUrl(m_Impl->NodeList[0].Ip, m_Impl->NodeList[0].Port, "/storage_rpc/v1"), "{\"method\":\"info\"}", 2000);
                    if (timeResp.StatusCode == 200) {
                        auto timeBody = json::parse(Utils::BytesToString(timeResp.Body));
                        if (timeBody.contains("result")) timeBody = timeBody["result"];
                        int64_t netTime = timeBody.value("timestamp", 0LL);
                        if (netTime > 0) {
                            g_NetworkOffset.store(netTime - Utils::NowMs());
                        }
                    }
                } catch (...) {}
                
                return;
            }
        } catch (const std::exception& e) {
            lastError = "Seed " + seed + " failed: " + std::string(e.what());
        }
    }
    throw SwarmException(500, "Bootstrap failed: " + lastError);
}

std::vector<SessionNode> SwarmManager::GetAllNodes() const {
    return m_Impl->NodeList;
}

std::tuple<SessionNode, SessionNode, SessionNode> SwarmManager::PickOnionPath() const {
    if (m_Impl->NodeList.size() < 3)
        throw SwarmException(500, "Not enough nodes for onion path");

    auto nodes = m_Impl->NodeList;
    std::shuffle(nodes.begin(), nodes.end(), m_Impl->Rng);
    return { nodes[0], nodes[1], nodes[2] };
}

std::vector<SessionNode> SwarmManager::ResolveSwarm(const AccountId& accountId) {
    if (m_Impl->NodeList.empty()) Bootstrap();

    std::string myId = accountId;
    if (myId.size() == 66 && (myId.substr(0, 2) == "05" || myId.substr(0, 2) == "03")) myId = myId.substr(2);
    
    Bytes xPk = Utils::HexToBytes(myId);
    uint64_t targetSwarmId = 0;
    if (xPk.size() == 32) targetSwarmId = Impl::PubkeyToSwarmSpace(xPk);

    std::map<uint64_t, std::vector<SessionNode>> swarms;
    for (const auto& n : m_Impl->NodeList) {
        if (n.SwarmId.empty()) continue;
        try {
            uint64_t sid;
            if (n.SwarmId.size() > 2 && n.SwarmId.substr(0, 2) == "0x") sid = std::stoull(n.SwarmId, nullptr, 16);
            else sid = std::stoull(n.SwarmId);
            swarms[sid].push_back(n);
        } catch (...) {}
    }

    std::vector<SessionNode> candidates;
    if (!swarms.empty() && targetSwarmId != 0) {
        auto right_it = swarms.lower_bound(targetSwarmId);
        if (right_it == swarms.end()) right_it = swarms.begin();
        auto left_it = std::prev(right_it == swarms.begin() ? swarms.end() : right_it);
        
        candidates = right_it->second;
        candidates.insert(candidates.end(), left_it->second.begin(), left_it->second.end());
    } else {
        candidates = m_Impl->NodeList;
    }

    if (candidates.empty()) candidates = m_Impl->NodeList;
    
    std::shuffle(candidates.begin(), candidates.end(), m_Impl->Rng);
    int tried = 0;
    for (const auto& node : candidates) {
        if (++tried > 10) break; // Reduced from 20 to 10 for faster recovery
        try {
            json req = {
                {"method", "get_swarm"},
                {"params", { {"pubkey", accountId} }}
            };
            std::string url = Impl::NodeUrl(node.Ip, node.Port, "/storage_rpc/v1");
            auto resp = m_Impl->Net.PostJson(url, req.dump(), 4000); // Reduced to 4s
            if (resp.StatusCode == 200 || resp.StatusCode == 421) {
                auto body = json::parse(Utils::BytesToString(resp.Body));
                if (body.contains("result")) body = body["result"];
                
                std::vector<SessionNode> swarm;
                if (body.contains("snodes")) {
                    for (const auto& sn_obj : body["snodes"]) swarm.push_back(Impl::ParseNode(sn_obj));
                }
                if (!swarm.empty()) return swarm;
            }
        } catch (...) {}
    }
    throw SwarmException(500, "Could not resolve swarm for " + accountId);
}

std::string SwarmManager::Store(const AccountId& recipientId, const Bytes& data, int ns, int64_t ttlMs) {
    return StoreWithAuth(recipientId, data, ns, ttlMs, {});
}

std::string SwarmManager::StoreWithAuth(const AccountId& recipientId, const Bytes& data, int ns, int64_t ttlMs, const Bytes& authData) {
    auto swarm = ResolveSwarm(recipientId);
    int64_t now = GetNetworkTimeMs();
    std::string b64Data = Utils::Base64Encode(data);
    
    json params = {
        {"pubkey",    recipientId},
        {"namespace", ns},
        {"ttl",       ttlMs},
        {"data",      b64Data},
        {"timestamp", now}
    };

    if (!authData.empty() && recipientId.substr(0, 2) == "03") {
        auto sk = m_Impl->Account_.GetEd25519PrivateKey();
        Bytes groupPkHex = Utils::HexToBytes(recipientId.substr(2));
        std::span<const unsigned char> sk_span(sk.data(), sk.size());
        std::span<const unsigned char> gpk_span(groupPkHex.data(), groupPkHex.size());

        session::config::groups::Info dummyInfo(gpk_span, std::nullopt, std::nullopt);
        session::config::groups::Members dummyMembers(gpk_span, std::nullopt, std::nullopt);
        session::config::groups::Keys tempKeys(sk_span, gpk_span, std::nullopt, std::nullopt, dummyInfo, dummyMembers);

        std::string nsStr = (ns == 0) ? "" : std::to_string(ns);
        std::string signMsg = std::string("store") + nsStr + std::to_string(now);
        
        auto auth = tempKeys.swarm_subaccount_sign(
            std::span<const unsigned char>(reinterpret_cast<const uint8_t*>(signMsg.data()), signMsg.size()),
            std::span<const unsigned char>(authData.data(), authData.size()),
            false
        );

        params["subaccount"] = auth.subaccount;
        params["subaccount_sig"] = auth.subaccount_sig;
        params["signature"] = auth.signature;
    } else {
        std::string nsStr = (ns == 0) ? "" : std::to_string(ns);
        std::string sig = m_Impl->Account_.MakeSwarmAuthToken("store", nsStr, std::to_string(now));
        params["signature"] = sig;
        params["sig_timestamp"] = now;
        params["pubkey_ed25519"] = Utils::ToUpper(Utils::BytesToHex(m_Impl->Account_.GetPublicKey()));
    }

    json req = {{"method", "store"}, {"params", params}};
    std::string lastHash = "";

    int successCount = 0;
    for (const auto& node : swarm) {
        try {
            std::string url = Impl::NodeUrl(node.Ip, node.Port, "/storage_rpc/v1");
            auto resp = m_Impl->Net.PostJson(url, req.dump(), 15000);
            if (resp.StatusCode == 200) {
                auto body = json::parse(Utils::BytesToString(resp.Body));
                if (body.contains("result")) body = body["result"];
                std::string hash = body.value("hash", "");
                if (!hash.empty()) {
                    lastHash = hash;
                    successCount++;
                }
            }
        } catch (...) {}
    }

    if (successCount == 0) throw SwarmException(500, "Failed to store message on swarm");
    return lastHash;
}

std::vector<SwarmManager::RawEnvelope> SwarmManager::Retrieve(const AccountId& targetId, int ns, const std::string& lastHash) {
    return RetrieveWithAuth(targetId, ns, lastHash, {});
}

std::vector<SwarmManager::RawEnvelope> SwarmManager::RetrieveWithAuth(const AccountId& targetId, int ns, const std::string& lastHash, const Bytes& authData) {
    auto swarm = ResolveSwarm(targetId);
    int64_t now = GetNetworkTimeMs();

    json params = {
        {"pubkey", targetId},
        {"namespace", ns},
        {"timestamp", now}
    };
    if (!lastHash.empty()) params["last_hash"] = lastHash;

    if (!authData.empty() && targetId.substr(0, 2) == "03") {
        auto sk = m_Impl->Account_.GetEd25519PrivateKey();
        Bytes groupPkHex = Utils::HexToBytes(targetId.substr(2));
        std::span<const unsigned char> sk_span(sk.data(), sk.size());
        std::span<const unsigned char> gpk_span(groupPkHex.data(), groupPkHex.size());

        session::config::groups::Info dummyInfo(gpk_span, std::nullopt, std::nullopt);
        session::config::groups::Members dummyMembers(gpk_span, std::nullopt, std::nullopt);
        session::config::groups::Keys tempKeys(sk_span, gpk_span, std::nullopt, std::nullopt, dummyInfo, dummyMembers);

        std::string nsStr = (ns == 0) ? "" : std::to_string(ns);
        std::string signMsg = std::string("retrieve") + nsStr + std::to_string(now);
        
        auto auth = tempKeys.swarm_subaccount_sign(
            std::span<const unsigned char>(reinterpret_cast<const uint8_t*>(signMsg.data()), signMsg.size()),
            std::span<const unsigned char>(authData.data(), authData.size()),
            false
        );

        params["subaccount"] = auth.subaccount;
        params["subaccount_sig"] = auth.subaccount_sig;
        params["signature"] = auth.signature;
    } else {
        std::string nsStr = (ns == 0) ? "" : std::to_string(ns);
        std::string sig = m_Impl->Account_.MakeSwarmAuthToken("retrieve", nsStr, std::to_string(now));
        params["signature"] = sig;
        params["pubkey_ed25519"] = Utils::ToUpper(Utils::BytesToHex(m_Impl->Account_.GetPublicKey()));
    }

    json req = { {"method", "retrieve"}, {"params", params} };

    for (const auto& node : swarm) {
        try {
            std::string url = Impl::NodeUrl(node.Ip, node.Port, "/storage_rpc/v1");
            auto resp = m_Impl->Net.PostJson(url, req.dump(), 5000); // Fast 5s timeout
            if (resp.StatusCode == 200) {
                auto body = json::parse(Utils::BytesToString(resp.Body));
                if (body.contains("result")) body = body["result"];
                
                std::vector<RawEnvelope> envelopes;
                if (body.contains("messages")) {
                    for (const auto& m : body["messages"]) {
                        RawEnvelope env;
                        env.Data = Utils::Base64Decode(m.value("data", ""));
                        env.Hash = m.value("hash", "");
                        env.Timestamp = m.value("timestamp", 0LL);
                        env.Expiry = m.value("expiry", 0LL);
                        env.Namespace = ns;
                        envelopes.push_back(env);
                    }
                }
                return envelopes;
            }
        } catch (...) {}
    }
    return {};
}

void SwarmManager::Delete(const std::vector<std::string>& hashes, int ns) {
    if (hashes.empty()) return;
    std::string myId = m_Impl->Account_.GetAccountId();
    auto swarm = ResolveSwarm(myId);

    for (const auto& node : swarm) {
        try {
            int64_t ts = GetNetworkTimeMs();
            std::string nsStr = (ns == 0) ? "" : std::to_string(ns);
            std::string sig = m_Impl->Account_.MakeSwarmAuthToken("delete", nsStr, std::to_string(ts));
            
            json req = {
                {"method", "delete"},
                {"params", {
                    {"pubkey",         myId},
                    {"pubkey_ed25519", Utils::ToUpper(Utils::BytesToHex(m_Impl->Account_.GetPublicKey()))},
                    {"messages",       hashes},
                    {"namespace",      ns},
                    {"timestamp",      ts},
                    {"signature",      sig}
                }}
            };
            std::string url = Impl::NodeUrl(node.Ip, node.Port, "/storage_rpc/v1");
            auto resp = m_Impl->Net.PostJson(url, req.dump(), 5000);
            if (resp.StatusCode == 200) return;
        } catch (...) {}
    }
}

} // namespace Saf
