#pragma once

#include "SessionAppFramework.hpp"
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <optional>

namespace Session {

// Forward declarations
class Client;
class Conversation;
class Group;
class Message;

// ─────────────────────────────────────────────────────────
// User
// ─────────────────────────────────────────────────────────
class User {
public:
    User(Client* client, const std::string& id);

    std::string GetId() const;
    std::string GetDisplayName() const; // Best effort (local contact or profile)

    // Actions
    void SendMessage(const std::string& text);
    void SendFile(const std::string& filePath);
    void SendVoice(const std::string& filePath);
    void SendTypingStarted();
    void SendTypingStopped();
    void SendReadReceipt(const std::vector<int64_t>& timestamps = {});

private:
    Client* m_Client;
    std::string m_Id;
};

// ─────────────────────────────────────────────────────────
// Group
// ─────────────────────────────────────────────────────────
class Group {
public:
    Group(Client* client, const std::string& id);

    std::string GetId() const;
    std::string GetName() const;
    std::string GetDescription() const;
    std::vector<User> GetMembers() const;
    bool IsAdmin() const;

    // Actions
    void SendMessage(const std::string& text);
    void SendFile(const std::string& filePath);
    void SendVoice(const std::string& filePath);
    void SendTypingStarted();
    void SendTypingStopped();
    void SendReadReceipt(const std::vector<int64_t>& timestamps = {});
    void Leave();
    
    Conversation GetConversation() const;

    // Management (Admin only)
    void SetName(const std::string& name);
    void SetDescription(const std::string& description);
    void AddMember(const std::string& userId);
    void RemoveMember(const std::string& userId);
    void PromoteMember(const std::string& userId);
    void DemoteMember(const std::string& userId);
    void Delete(); // Destroy the group

private:
    Client* m_Client;
    std::string m_Id;
};

// ─────────────────────────────────────────────────────────
// Conversation (DM or Group)
// ─────────────────────────────────────────────────────────
class Conversation {
public:
    Conversation(Client* client, const std::string& id, bool isGroup);

    std::string GetId() const;
    std::string GetName() const;   // DM: display name of the other user, Group: group name
    bool        IsGroup() const;

    User  GetUser() const;   // throws if IsGroup()
    Group GetGroup() const;  // throws if !IsGroup()

    // Send messages
    void SendMessage(const std::string& text);
    void SendFile(const std::string& filePath);
    void SendVoice(const std::string& filePath);
    void SendTypingStarted();
    void SendTypingStopped();
    void SendReadReceipt(const std::vector<int64_t>& timestamps = {});

    // Retrieve recent messages from the network (does not fire OnMessage callbacks)
    std::vector<Message> GetMessages(int limit = 50);

private:
    Client* m_Client;
    std::string m_Id;
    bool m_IsGroup;
};

// ─────────────────────────────────────────────────────────
// Message
// ─────────────────────────────────────────────────────────
class Message {
public:
    Message(Client* client, const Saf::Message& raw);

    std::string GetId() const;
    std::string GetContent() const;
    User        GetAuthor() const;
    bool        IsGroup() const;
    Group       GetGroup() const; // Throws if not a group message
    Conversation GetConversation() const;

    // Attachments
    bool        HasFile() const;
    std::string GetFileName() const;
    long        GetFileSize() const;
    void        SaveFile(const std::string& destPath) const;

    // Interactions
    void Reply(const std::string& text);
    void React(const std::string& emoji);
    void MarkAsRead();
    void Delete(); // Unsend
    void SendReadReceipt(); // Send read receipt for this message
    bool IsExpirationUpdate() const;
    int GetExpirationTimer() const; // Returns expiration timer in seconds
    int GetExpirationType() const;  // 0=none, 1=delete_after_read, 2=delete_after_send

private:
    friend class Client;
    friend class Conversation;
    Client* m_Client;
    Saf::Message m_Raw;
};

// ─────────────────────────────────────────────────────────
// Client
// ─────────────────────────────────────────────────────────
class Client {
public:
    // Generate new account if seed is empty, otherwise load
    Client(const std::string& seedOrMnemonic = "");
    ~Client();

    // Lifecycle
    void Start(); // Blocking or Non-Blocking? Let's make it start threads and return.
    void Stop();

    // Identity
    User GetMe();
    std::string GetMnemonic() const;
    void SetDisplayName(const std::string& name);
    void SetProfilePicture(const std::string& filePath);

    // Configuration
    void SetMessageDbPath(const std::string& path);
    std::string GetMessageDbPath() const;

    // Events
    using MessageCallback = std::function<void(Message)>;
    using ReactionCallback = std::function<void(User reactor, Message target, std::string emoji, bool added)>;
    using TypingCallback = std::function<void(User sender, bool started)>;
    using ReadReceiptCallback = std::function<void(User sender, std::vector<int64_t> timestamps)>;
    using UnsendCallback = std::function<void(User sender, int64_t timestamp)>;
    using DataExtractionCallback = std::function<void(User sender, int extractionType)>;
    using CallCallback = std::function<void(User sender, int callType, const std::string& uuid)>;
    using ExpirationUpdateCallback = std::function<void(User sender, uint32_t timer, int type)>;
    using GroupInviteCallback = std::function<void(Group group, const std::string& name)>;
    using GroupPromotedToAdminCallback = std::function<void(Group group)>;
    using GroupInfoChangedCallback = std::function<void(Group group, int changeType, const std::string& newValue)>;
    using GroupMemberLeftCallback = std::function<void(Group group, User member, bool isNotification)>;
    using MessageRequestResponseCallback = std::function<void(User sender)>;

    void OnMessage(MessageCallback callback);
    void OnReaction(ReactionCallback callback);
    void OnTyping(TypingCallback callback);
    void OnReadReceipt(ReadReceiptCallback callback);
    void OnUnsend(UnsendCallback callback);
    void OnDataExtraction(DataExtractionCallback callback);
    void OnCall(CallCallback callback);
    void OnExpirationUpdate(ExpirationUpdateCallback callback);
    void OnGroupInvite(GroupInviteCallback callback);
    void OnGroupPromotedToAdmin(GroupPromotedToAdminCallback callback);
    void OnGroupInfoChanged(GroupInfoChangedCallback callback);
    void OnGroupMemberLeft(GroupMemberLeftCallback callback);
    void OnMessageRequestResponse(MessageRequestResponseCallback callback);

    // Factory / Accessors
    Group CreateGroup(const std::string& name);
    Group GetGroup(const std::string& groupId);
    User  GetUser(const std::string& userId);
    Conversation GetConversation(const std::string& conversationId);

    // Communities / Open Groups
    void JoinCommunity(const std::string& fullUrl);
    void LeaveCommunity(const std::string& fullUrl);
    std::vector<Saf::CommunityRoom> GetCommunities() const;
    void SendCommunityMessage(const std::string& fullUrl, const std::string& text);
    void SendCommunityFile(const std::string& fullUrl, const std::string& filePath);

    // ONS Resolution
    Saf::OnsResult ResolveOns(const std::string& onsName);

    // Internal Access (for advanced usage if needed)
    Saf::Account&        GetAccount();
    Saf::MessageService& GetMessageService();
    Saf::GroupManager&   GetGroupManager();
    Saf::FileTransfer&   GetFileTransfer();
    Saf::SwarmManager&   GetSwarmManager();

private:
    friend class User;
    friend class Group;
    friend class Message;

    // Pimpl-style or direct members? 
    // Since we are wrapping Saf, we'll hold them directly.
    Saf::Account       m_Account;
    Saf::NetworkClient m_Net;
    std::unique_ptr<Saf::SwarmManager>  m_Swarm;
    std::unique_ptr<Saf::ConfigManager> m_Config;
    std::unique_ptr<Saf::GroupManager>  m_Gm;
    std::unique_ptr<Saf::MessageService> m_Ms;
    std::unique_ptr<Saf::FileTransfer>   m_Ft;

    MessageCallback m_OnMessage;
    ReactionCallback m_OnReaction;
    TypingCallback m_OnTyping;
    ReadReceiptCallback m_OnReadReceipt;
    UnsendCallback m_OnUnsend;
    DataExtractionCallback m_OnDataExtraction;
    CallCallback m_OnCall;
    ExpirationUpdateCallback m_OnExpirationUpdate;
    GroupInviteCallback m_OnGroupInvite;
    GroupPromotedToAdminCallback m_OnGroupPromotedToAdmin;
    GroupInfoChangedCallback m_OnGroupInfoChanged;
    GroupMemberLeftCallback m_OnGroupMemberLeft;
    MessageRequestResponseCallback m_OnMessageRequestResponse;
    Saf::SelfProfile m_Profile;
    std::string      m_MessageDbPath = "messages.db";
    std::atomic<bool> m_Running{false};
};

} // namespace Session
