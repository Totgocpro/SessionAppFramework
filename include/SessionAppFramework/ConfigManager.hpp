#pragma once

#include "Types.hpp"
#include "Account.hpp"
#include "SwarmManager.hpp"
#include <string>
#include <vector>
#include <memory>
#include <optional>

namespace Saf {

/**
 * @brief Manages all libsession-util config objects for the local account.
 *
 * Config types handled:
 *  - UserProfile   : display name, profile picture
 *  - Contacts      : contact list (approved, blocked, name overrides)
 *  - ConvoInfoVolatile : unread counts, last-read timestamps
 *  - UserGroups    : list of groups / communities the user is in
 *  - GroupInfo     : per-group metadata  (name, description, pic)
 *  - GroupMembers  : per-group member list
 *  - GroupKeys     : per-group encryption key rotation
 *
 * Configs are stored in the swarm as encrypted blobs and are
 * merged with libsession-util's CRDT-style merge logic.
 *
 * Usage:
 * @code
 *   ConfigManager cfg(account, swarmManager);
 *   cfg.SetDisplayName("MyBot");
 *   cfg.Push();       // push changes to swarm
 *   cfg.Pull();       // pull + merge from swarm
 *
 *   std::string name = cfg.GetDisplayName();
 * @endcode
 */
class ConfigManager {
public:
    ConfigManager(const Account& account, SwarmManager& swarmManager);
    ~ConfigManager();

    ConfigManager(const ConfigManager&)            = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;

    // ─────────────────────────────────────────────────────────
    // Sync (push / pull all config types)
    // ─────────────────────────────────────────────────────────

    /**
     * @brief Pushes all dirty (locally changed) config objects to the swarm.
     * @throws SwarmException / NetworkException on failure.
     */
    void Push();

    /**
     * @brief Pulls all config types from the swarm and merges them locally.
     * @throws SwarmException / NetworkException on failure.
     */
    void Pull();

    // ─────────────────────────────────────────────────────────
    // UserProfile
    // ─────────────────────────────────────────────────────────

    /**
     * @brief Returns the current display name stored in UserProfile config.
     */
    std::string GetDisplayName() const;

    /**
     * @brief Sets the display name.  Call Push() to persist to swarm.
     * @param name  New display name (max 100 chars).
     */
    void SetDisplayName(const std::string& name);

    /**
     * @brief Returns the profile picture URL (empty if not set).
     */
    std::string GetProfilePictureUrl() const;

    /**
     * @brief Sets the profile picture URL and encryption key.
     * @param url  Public URL of the uploaded profile picture.
     * @param key  32-byte encryption key (from FileTransfer::Upload).
     */
    void SetProfilePicture(const std::string& url, const Bytes& key);

    // ─────────────────────────────────────────────────────────
    // Contacts
    // ─────────────────────────────────────────────────────────

    /**
     * @brief Returns all contacts stored in Contacts config.
     */
    std::vector<Contact> GetContacts() const;

    /**
     * @brief Adds or updates a contact.  Call Push() to persist.
     */
    void UpsertContact(const Contact& contact);

    /**
     * @brief Removes a contact.  Call Push() to persist.
     */
    void RemoveContact(const AccountId& contactId);

    /**
     * @brief Marks a contact as approved (two-way approval model).
     */
    void ApproveContact(const AccountId& contactId);

    /**
     * @brief Blocks a contact.
     */
    void BlockContact(const AccountId& contactId);

    /**
     * @brief Unblocks a contact.
     */
    void UnblockContact(const AccountId& contactId);

    /**
     * @brief Returns a single contact by Account ID.
     * @return std::nullopt if not found.
     */
    std::optional<Contact> FindContact(const AccountId& contactId) const;

    // ─────────────────────────────────────────────────────────
    // UserGroups (the list of groups this account belongs to)
    // ─────────────────────────────────────────────────────────

    /**
     * @brief Returns the list of Group IDs the local account is a member of.
     */
    std::vector<std::string> GetGroupIds() const;

    /**
     * @brief Returns the auth data (for non-admins) of a V2 (03) group.
     */
    Bytes GetGroupAuthData(const std::string& groupId) const;

    /**
     * @brief Returns the admin key (if any) of a V2 (03) group.
     */
    Bytes GetGroupAdminKey(const std::string& groupId) const;

    /**
     * @brief Returns the name of a legacy (05) group.
     */
    std::string GetLegacyGroupName(const std::string& groupId) const;

    /**
     * @brief Returns members for a legacy (05) group.
     */
    std::vector<GroupMember> GetLegacyGroupMembers(const std::string& groupId) const;

    void AddLegacyGroupMember(const std::string& groupId, const AccountId& memberId, bool admin = false);
    void RemoveLegacyGroupMember(const std::string& groupId, const AccountId& memberId);
    void PromoteLegacyGroupMember(const std::string& groupId, const AccountId& memberId, bool admin);

    /**
     * @brief Adds a group entry to UserGroups config.  Call Push() to persist.
     */
    void AddGroupEntry(const std::string& groupId,
                       const std::string& name,
                       const Bytes&       adminKey = {},
                       const Bytes&       authData = {});

    /**
     * @brief Removes a group entry from UserGroups config.
     */
    void RemoveGroupEntry(const std::string& groupId);

    // ─────────────────────────────────────────────────────────
    // ConvoInfoVolatile (per-conversation state)
    // ─────────────────────────────────────────────────────────

    /**
     * @brief Marks a DM conversation as read up to the given timestamp.
     */
    void MarkConvoRead(const AccountId& contactId, int64_t timestampMs);

    /**
     * @brief Returns the last-read timestamp for a conversation.
     */
    int64_t GetLastReadTimestamp(const AccountId& contactId) const;

    // ─────────────────────────────────────────────────────────
    // Internal helpers (used by GroupManager)
    // ─────────────────────────────────────────────────────────

    /// Push/pull a specific config namespace
    void PushNamespace(int ns);
    void PullNamespace(int ns);

private:
    struct Impl;
    std::unique_ptr<Impl> m_Impl;
};

} // namespace Saf
