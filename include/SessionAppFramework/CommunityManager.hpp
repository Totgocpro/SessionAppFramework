#pragma once

#include "Types.hpp"
#include "Account.hpp"
#include "NetworkClient.hpp"
#include "ConfigManager.hpp"
#include "SwarmManager.hpp"
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <map>
#include <set>

namespace Saf {

/**
 * @brief Manages Session Communities (Open Groups) via SOGS v3 API.
 *
 * Communities are public chat rooms hosted on SOGS (Session Open Group Server).
 * Messages are NOT end-to-end encrypted but are sent over HTTPS to the server.
 * Users can join, send messages, react, upload files, and moderate.
 */
class CommunityManager {
public:
    CommunityManager(const Account& account,
                     NetworkClient& networkClient,
                     ConfigManager& configManager,
                     SwarmManager& swarmManager);
    ~CommunityManager();

    CommunityManager(const CommunityManager&) = delete;
    CommunityManager& operator=(const CommunityManager&) = delete;

    // ─────────────────────────────────────────────────────────
    // Room Management
    // ─────────────────────────────────────────────────────────

    /**
     * @brief Joins a community room by its full URL.
     * @param fullUrl  e.g. "https://example.com/r/RoomName?public_key=..."
     */
    void Join(const std::string& fullUrl);

    /**
     * @brief Leaves a community room.
     */
    void Leave(const std::string& fullUrl);

    /**
     * @brief Returns all joined community rooms.
     */
    std::vector<CommunityRoom> GetRooms() const;

    /**
     * @brief Returns a specific room by full URL.
     */
    CommunityRoom GetRoom(const std::string& fullUrl) const;

    // ─────────────────────────────────────────────────────────
    // Messaging
    // ─────────────────────────────────────────────────────────

    /**
     * @brief Sends a text message to a community room.
     * @param fullUrl  The community room full URL.
     * @param text     The message text.
     * @return         Server-assigned message ID.
     */
    std::string SendMessage(const std::string& fullUrl, const std::string& text);

    /**
     * @brief Sends a file to a community room.
     */
    std::string SendFile(const std::string& fullUrl, const std::string& filePath);

    /**
     * @brief Sends a reaction to a community message.
     */
    std::string SendReaction(const std::string& fullUrl, const std::string& messageId, const std::string& emoji);

    /**
     * @brief Deletes a reaction from a community message.
     */
    std::string DeleteReaction(const std::string& fullUrl, const std::string& messageId, const std::string& emoji);

    /**
     * @brief Deletes a community message (moderator only).
     */
    void DeleteMessage(const std::string& fullUrl, const std::string& messageId);

    /**
     * @brief Deletes all messages from a user (moderator only).
     */
    void DeleteUserMessages(const std::string& fullUrl, const std::string& sessionId);

    // ─────────────────────────────────────────────────────────
    // Polling
    // ─────────────────────────────────────────────────────────

    /**
     * @brief Polls all community rooms for new messages.
     * @return List of new community messages.
     */
    std::vector<CommunityMessage> PollAll();

    /**
     * @brief Polls a single community room for new messages.
     */
    std::vector<CommunityMessage> PollRoom(const std::string& fullUrl);

    // ─────────────────────────────────────────────────────────
    // Moderation
    // ─────────────────────────────────────────────────────────

    /**
     * @brief Bans a user from a community (admin only).
     */
    void BanUser(const std::string& fullUrl, const std::string& sessionId);

    /**
     * @brief Unbans a user from a community (admin only).
     */
    void UnbanUser(const std::string& fullUrl, const std::string& sessionId);

    /**
     * @brief Adds a moderator to a community (admin only).
     */
    void AddModerator(const std::string& fullUrl, const std::string& sessionId);

    /**
     * @brief Removes a moderator from a community (admin only).
     */
    void RemoveModerator(const std::string& fullUrl, const std::string& sessionId);

    /**
     * @brief Gets the list of moderators for a community.
     */
    std::vector<std::string> GetModerators(const std::string& fullUrl) const;

    // ─────────────────────────────────────────────────────────
    // Callbacks
    // ─────────────────────────────────────────────────────────

    using CommunityMessageCallback = std::function<void(const CommunityMessage&)>;
    void OnMessage(CommunityMessageCallback callback);

private:
    struct Impl;
    std::unique_ptr<Impl> m_Impl;
};

} // namespace Saf
