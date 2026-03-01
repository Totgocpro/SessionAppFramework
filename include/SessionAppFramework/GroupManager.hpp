#pragma once

#include "Types.hpp"
#include "Account.hpp"
#include "SwarmManager.hpp"
#include "ConfigManager.hpp"
#include <string>
#include <vector>
#include <memory>

namespace Saf {

class MessageService;

/**
 * @brief Manages Session closed groups (new-style, libsession-util based).
 *
 * Group state (members, name, keys) is stored in the swarm via
 * libsession-util config messages (namespaces 11–13).
 *
 * Admins can:
 *  - Create groups
 *  - Change the group name / description / picture
 *  - Add / remove members
 *  - Promote members to admin
 *  - Delete the group
 *
 * All members can:
 *  - Send messages
 *  - Leave the group
 *  - View member list
 *
 * Usage:
 * @code
 *   GroupManager gm(account, swarmManager, configManager);
 *
 *   auto group = gm.Create("My Bot Group");
 *   gm.AddMember(group.Id, "05xyz...");
 *
 *   gm.SendGroupText(group.Id, "Hello everyone!");
 * @endcode
 */
class GroupManager {
public:
    GroupManager(const Account&  account,
                 SwarmManager&   swarmManager,
                 ConfigManager&  configManager);
    ~GroupManager();

    GroupManager(const GroupManager&)            = delete;
    GroupManager& operator=(const GroupManager&) = delete;

    /**
     * @brief Links the message service for sending group update notifications.
     */
    void SetMessageService(MessageService* messageService);

    // ─────────────────────────────────────────────────────────
    // Group lifecycle
    // ─────────────────────────────────────────────────────────

    /**
     * @brief Creates a new closed group.
     * @param name         Human-readable group name.
     * @param description  Optional description.
     * @param memberIds    Initial member Account IDs (local account is added automatically).
     * @return             The new Group object.
     * @throws GroupException / SwarmException
     */
    Group Create(const std::string&              name,
                 const std::string&              description = "",
                 const std::vector<AccountId>&   memberIds   = {});

    /**
     * @brief Deletes (destroys) a group.  Local account must be an admin.
     * @param groupId  Group ID to destroy.
     * @throws GroupException if not admin.
     */
    void Destroy(const std::string& groupId);

    /**
     * @brief Loads all groups the local account is a member of.
     */
    std::vector<Group> GetAll();

    /**
     * @brief Returns details for a single group.
     * @throws GroupException if the group is not found.
     */
    Group Get(const std::string& groupId);

    // ─────────────────────────────────────────────────────────
    // Group metadata
    // ─────────────────────────────────────────────────────────

    /**
     * @brief Updates the group name.  Requires admin.
     */
    void SetName(const std::string& groupId, const std::string& name);

    /**
     * @brief Updates the group description.  Requires admin.
     */
    void SetDescription(const std::string& groupId, const std::string& description);

    // ─────────────────────────────────────────────────────────
    // Member management
    // ─────────────────────────────────────────────────────────

    /**
     * @brief Returns all members of a group.
     */
    std::vector<GroupMember> GetMembers(const std::string& groupId);

    /**
     * @brief Invites a user to join a group.
     *
     * Adds the member to the group config and sends them an invitation
     * message containing the group keys.
     *
     * @param groupId   Group to add the member to.
     * @param memberId  Account ID of the new member.
     * @throws GroupException if not admin.
     */
    void AddMember(const std::string& groupId, const AccountId& memberId);

    /**
     * @brief Removes a member from a group (kick).  Requires admin.
     * @param groupId   Group to remove from.
     * @param memberId  Account ID to remove.
     * @throws GroupException if not admin.
     */
    void RemoveMember(const std::string& groupId, const AccountId& memberId);

    /**
     * @brief Promotes a member to admin.  Requires admin.
     */
    void PromoteToAdmin(const std::string& groupId, const AccountId& memberId);

    /**
     * @brief Demotes an admin to regular member.  Requires admin.
     */
    void DemoteAdmin(const std::string& groupId, const AccountId& memberId);

    /**
     * @brief Joins a group (adds to local config).
     * @param groupId   Group ID to join.
     * @param name      Group name (from invitation).
     */
    void Join(const std::string& groupId, const std::string& name);

    /**
     * @brief Joins a group with authentication data (for non-admins).
     */
    void Join(const std::string& groupId, 
              const std::string& name,
              const Bytes&       memberAuthData,
              const Bytes&       adminSignature);

    /**
     * @brief Leaves a group (sends a leave message and removes from local config).
     * @param groupId  Group to leave.
     */
    void Leave(const std::string& groupId);

    // ─────────────────────────────────────────────────────────
    // Config sync
    // ─────────────────────────────────────────────────────────

    /**
     * @brief Pushes the current local group config to the swarm.
     * @param groupId  Group whose config should be synced.
     */
    void PushConfig(const std::string& groupId);

    /**
     * @brief Pulls config if needed and returns the current group encryption key.
     */
    Bytes GetEncryptionKey(const std::string& groupId);

    /**
     * @brief Returns the auth data (for non-admins) of a V2 (03) group.
     */
    Bytes GetAuthData(const std::string& groupId);

    /**
     * @brief Fetches and merges the group config from the swarm.
     * @param groupId  Group to pull config for.
     */
    void PullConfig(const std::string& groupId);

private:
    struct Impl;
    std::unique_ptr<Impl> m_Impl;
};

} // namespace Saf
