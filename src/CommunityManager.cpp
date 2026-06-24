#include <SessionAppFramework/CommunityManager.hpp>
#include <SessionAppFramework/Exceptions.hpp>
#include <SessionAppFramework/Utils.hpp>

#include <nlohmann/json.hpp>
#include <sodium.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <mutex>
#include <random>
#include <regex>

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace Saf {

// ─────────────────────────────────────────────────────────
// Impl
// ─────────────────────────────────────────────────────────

struct CommunityManager::Impl {
    const Account&  Account_;
    NetworkClient&  Net;
    ConfigManager&  Config;
    SwarmManager&   Swarm;

    std::map<std::string, CommunityRoom> Rooms;          // fullUrl -> room
    std::map<std::string, std::string>   LastMessageIds; // fullUrl -> last message ID

    std::vector<CommunityMessageCallback> MessageCallbacks;
    std::mutex Mutex;

    Impl(const Account& acc, NetworkClient& net, ConfigManager& cfg, SwarmManager& sw)
        : Account_(acc), Net(net), Config(cfg), Swarm(sw) {}

    static bool ParseFullUrl(const std::string& fullUrl, std::string& baseUrl, std::string& room, std::string& pubkeyHex) {
        // Format: https://server.com/r/RoomName?public_key=hex64
        // or:      https://server.com/RoomName?public_key=hex64
        std::regex urlRe(R"(^(https?://[^/]+)/r/([^?]+)\?public_key=([0-9a-fA-F]{64}))");
        std::smatch m;
        if (!std::regex_match(fullUrl, m, urlRe)) {
            std::regex urlRe2(R"(^(https?://[^/]+)/([^?]+)\?public_key=([0-9a-fA-F]{64}))");
            if (!std::regex_match(fullUrl, m, urlRe2)) {
                return false;
            }
        }
        baseUrl = m[1].str();
        room = m[2].str();
        pubkeyHex = m[3].str();
        return true;
    }

    std::string RoomApiUrl(const CommunityRoom& room) {
        return room.BaseUrl + "/r/" + room.Room;
    }

    CommunityMessage ParseMessage(const json& msg, const std::string& baseUrl, const std::string& roomName) {
        CommunityMessage cm;
        cm.Id = std::to_string(msg.value("id", 0LL));
        cm.Sender = msg.value("user", "");
        cm.Body = msg.value("text", "");
        cm.Timestamp = msg.value("posted", 0LL);
        cm.Room = roomName;
        cm.BaseUrl = baseUrl;
        return cm;
    }

    json AuthHeaders() {
        return {
            {"X-SOGS-Pubkey", Utils::BytesToHex(Account_.GetEd25519PrivateKey())}
        };
    }
};

// ─────────────────────────────────────────────────────────
// CommunityManager
// ─────────────────────────────────────────────────────────

CommunityManager::CommunityManager(const Account& account,
                                    NetworkClient& networkClient,
                                    ConfigManager& configManager,
                                    SwarmManager& swarmManager)
    : m_Impl(std::make_unique<Impl>(account, networkClient, configManager, swarmManager)) {}

CommunityManager::~CommunityManager() = default;

// ─────────────────────────────────────────────────────────
// Room Management
// ─────────────────────────────────────────────────────────

void CommunityManager::Join(const std::string& fullUrl) {
    std::string baseUrl, room, pubkeyHex;
    if (!Impl::ParseFullUrl(fullUrl, baseUrl, room, pubkeyHex)) {
        throw SafException("Invalid community URL: " + fullUrl);
    }

    CommunityRoom cr;
    cr.BaseUrl = baseUrl;
    cr.Room = room;
    cr.PubKey = Utils::HexToBytes(pubkeyHex);
    cr.FullUrl = fullUrl;

    // Fetch room info from SOGS
    try {
        auto resp = m_Impl->Net.GetJson(cr.BaseUrl + "/rooms/" + cr.Room + "/info", 10000);
        if (resp.StatusCode == 200) {
            auto body = json::parse(Utils::BytesToString(resp.Body));
            cr.Name = body.value("name", room);
            cr.Description = body.value("description", "");
        }
    } catch (...) {}

    std::lock_guard<std::mutex> lock(m_Impl->Mutex);
    m_Impl->Rooms[fullUrl] = cr;
}

void CommunityManager::Leave(const std::string& fullUrl) {
    std::lock_guard<std::mutex> lock(m_Impl->Mutex);
    m_Impl->Rooms.erase(fullUrl);
    m_Impl->LastMessageIds.erase(fullUrl);
}

std::vector<CommunityRoom> CommunityManager::GetRooms() const {
    std::lock_guard<std::mutex> lock(m_Impl->Mutex);
    std::vector<CommunityRoom> rooms;
    for (const auto& [url, room] : m_Impl->Rooms) {
        rooms.push_back(room);
    }
    return rooms;
}

CommunityRoom CommunityManager::GetRoom(const std::string& fullUrl) const {
    std::lock_guard<std::mutex> lock(m_Impl->Mutex);
    auto it = m_Impl->Rooms.find(fullUrl);
    if (it == m_Impl->Rooms.end()) throw SafException("Not joined to community: " + fullUrl);
    return it->second;
}

// ─────────────────────────────────────────────────────────
// Messaging
// ─────────────────────────────────────────────────────────

std::string CommunityManager::SendMessage(const std::string& fullUrl, const std::string& text) {
    auto room = GetRoom(fullUrl);

    json body = {
        {"text", text}
    };

    try {
        std::string url = m_Impl->RoomApiUrl(room) + "/messages";
        auto req = m_Impl->Net.PostJson(url, body.dump(), 10000);
        if (req.StatusCode != 200) throw SafException("Send failed: HTTP " + std::to_string(req.StatusCode));
        auto resp = json::parse(Utils::BytesToString(req.Body));
        return std::to_string(resp.value("id", 0LL));
    } catch (const std::exception& e) {
        throw SafException(std::string("Community send failed: ") + e.what());
    }
}

std::string CommunityManager::SendFile(const std::string& fullUrl, const std::string& filePath) {
    auto room = GetRoom(fullUrl);

    std::ifstream f(filePath, std::ios::binary);
    if (!f.is_open()) throw SafException("Cannot open file: " + filePath);
    Bytes data((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());

    std::string fileName = fs::path(filePath).filename().string();
    std::string url = m_Impl->RoomApiUrl(room) + "/files";

    NetworkClient::Request req;
    req.Method = "POST";
    req.Url = url;
    req.Body = std::string(reinterpret_cast<const char*>(data.data()), data.size());
    req.Headers["Content-Type"] = "application/octet-stream";
    req.TimeoutMs = 60000;

    auto resp = m_Impl->Net.Send(req);
    if (resp.StatusCode != 200) throw SafException("File upload failed: HTTP " + std::to_string(resp.StatusCode));

    json jresp = json::parse(Utils::BytesToString(resp.Body));
    return std::to_string(jresp.value("id", 0LL));
}

std::string CommunityManager::SendReaction(const std::string& fullUrl, const std::string& messageId, const std::string& emoji) {
    auto room = GetRoom(fullUrl);

    json body = {
        {"reaction", emoji}
    };

    try {
        std::string url = m_Impl->RoomApiUrl(room) + "/messages/" + messageId + "/reactions";
        auto resp = m_Impl->Net.PostJson(url, body.dump(), 5000);
        if (resp.StatusCode != 200) throw SafException("Reaction failed: HTTP " + std::to_string(resp.StatusCode));
        return messageId;
    } catch (const std::exception& e) {
        throw SafException(std::string("Community reaction failed: ") + e.what());
    }
}

std::string CommunityManager::DeleteReaction(const std::string& fullUrl, const std::string& messageId, const std::string& emoji) {
    auto room = GetRoom(fullUrl);

    try {
        std::string url = m_Impl->RoomApiUrl(room) + "/messages/" + messageId + "/reactions?reaction=" + emoji;
        NetworkClient::Request req;
        req.Method = "DELETE";
        req.Url = url;
        req.TimeoutMs = 5000;
        auto resp = m_Impl->Net.Send(req);
        if (resp.StatusCode != 200) throw SafException("Delete reaction failed: HTTP " + std::to_string(resp.StatusCode));
        return messageId;
    } catch (const std::exception& e) {
        throw SafException(std::string("Community delete reaction failed: ") + e.what());
    }
}

void CommunityManager::DeleteMessage(const std::string& fullUrl, const std::string& messageId) {
    auto room = GetRoom(fullUrl);

    try {
        std::string url = m_Impl->RoomApiUrl(room) + "/messages/" + messageId;
        NetworkClient::Request req;
        req.Method = "DELETE";
        req.Url = url;
        req.TimeoutMs = 5000;
        m_Impl->Net.Send(req);
    } catch (const std::exception& e) {
        throw SafException(std::string("Community delete message failed: ") + e.what());
    }
}

void CommunityManager::DeleteUserMessages(const std::string& fullUrl, const std::string& sessionId) {
    auto room = GetRoom(fullUrl);

    try {
        json body = {
            {"session_id", sessionId}
        };
        std::string url = m_Impl->RoomApiUrl(room) + "/messages";
        NetworkClient::Request req;
        req.Method = "DELETE";
        req.Url = url;
        req.Body = body.dump();
        req.TimeoutMs = 10000;
        m_Impl->Net.Send(req);
    } catch (const std::exception& e) {
        throw SafException(std::string("Community delete user messages failed: ") + e.what());
    }
}

// ─────────────────────────────────────────────────────────
// Polling
// ─────────────────────────────────────────────────────────

std::vector<CommunityMessage> CommunityManager::PollAll() {
    std::vector<CommunityMessage> allMessages;
    std::lock_guard<std::mutex> lock(m_Impl->Mutex);

    for (auto& [url, room] : m_Impl->Rooms) {
        try {
            auto msgs = PollRoom(url);
            allMessages.insert(allMessages.end(), msgs.begin(), msgs.end());
        } catch (...) {}
    }
    return allMessages;
}

std::vector<CommunityMessage> CommunityManager::PollRoom(const std::string& fullUrl) {
    std::lock_guard<std::mutex> lock(m_Impl->Mutex);

    auto it = m_Impl->Rooms.find(fullUrl);
    if (it == m_Impl->Rooms.end()) return {};

    const auto& room = it->second;
    std::string lastId = m_Impl->LastMessageIds[fullUrl];

    try {
        std::string url = m_Impl->RoomApiUrl(room) + "/messages";
        if (!lastId.empty()) {
            url += "?after=" + lastId;
        }

        auto resp = m_Impl->Net.GetJson(url, 10000);
        if (resp.StatusCode != 200) return {};

        auto body = json::parse(Utils::BytesToString(resp.Body));

        std::vector<CommunityMessage> messages;
        if (body.is_array()) {
            for (const auto& m : body) {
                auto cm = m_Impl->ParseMessage(m, room.BaseUrl, room.Room);
                messages.push_back(cm);
            }
        }

        if (!messages.empty()) {
            m_Impl->LastMessageIds[fullUrl] = messages.back().Id;

            for (auto& cb : m_Impl->MessageCallbacks) {
                for (const auto& msg : messages) {
                    cb(msg);
                }
            }
        }

        return messages;
    } catch (const std::exception& e) {
        return {};
    }
}

// ─────────────────────────────────────────────────────────
// Moderation
// ─────────────────────────────────────────────────────────

void CommunityManager::BanUser(const std::string& fullUrl, const std::string& sessionId) {
    auto room = GetRoom(fullUrl);
    json body = {{"session_id", sessionId}};

    try {
        std::string url = m_Impl->RoomApiUrl(room) + "/moderate/ban";
        m_Impl->Net.PostJson(url, body.dump(), 10000);
    } catch (const std::exception& e) {
        throw SafException(std::string("Ban failed: ") + e.what());
    }
}

void CommunityManager::UnbanUser(const std::string& fullUrl, const std::string& sessionId) {
    auto room = GetRoom(fullUrl);
    json body = {{"session_id", sessionId}};

    try {
        std::string url = m_Impl->RoomApiUrl(room) + "/moderate/unban";
        m_Impl->Net.PostJson(url, body.dump(), 10000);
    } catch (const std::exception& e) {
        throw SafException(std::string("Unban failed: ") + e.what());
    }
}

void CommunityManager::AddModerator(const std::string& fullUrl, const std::string& sessionId) {
    auto room = GetRoom(fullUrl);
    json body = {{"session_id", sessionId}};

    try {
        std::string url = m_Impl->RoomApiUrl(room) + "/moderate/add_moderator";
        m_Impl->Net.PostJson(url, body.dump(), 10000);
    } catch (const std::exception& e) {
        throw SafException(std::string("Add moderator failed: ") + e.what());
    }
}

void CommunityManager::RemoveModerator(const std::string& fullUrl, const std::string& sessionId) {
    auto room = GetRoom(fullUrl);
    json body = {{"session_id", sessionId}};

    try {
        std::string url = m_Impl->RoomApiUrl(room) + "/moderate/remove_moderator";
        m_Impl->Net.PostJson(url, body.dump(), 10000);
    } catch (const std::exception& e) {
        throw SafException(std::string("Remove moderator failed: ") + e.what());
    }
}

std::vector<std::string> CommunityManager::GetModerators(const std::string& fullUrl) const {
    auto room = GetRoom(fullUrl);

    try {
        std::string url = m_Impl->RoomApiUrl(room) + "/moderators";
        auto resp = m_Impl->Net.GetJson(url, 10000);
        if (resp.StatusCode == 200) {
            auto body = json::parse(Utils::BytesToString(resp.Body));
            return body.get<std::vector<std::string>>();
        }
    } catch (...) {}
    return {};
}

void CommunityManager::OnMessage(CommunityMessageCallback callback) {
    std::lock_guard<std::mutex> lock(m_Impl->Mutex);
    m_Impl->MessageCallbacks.push_back(std::move(callback));
}

} // namespace Saf
