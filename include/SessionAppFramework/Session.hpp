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
    void Leave();
    
    // Management (Admin only)
    void SetName(const std::string& name);
    void AddMember(const std::string& userId);
    void RemoveMember(const std::string& userId);
    void PromoteMember(const std::string& userId);
    void DemoteMember(const std::string& userId);

private:
    Client* m_Client;
    std::string m_Id;
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
    
    // Attachments
    bool        HasFile() const;
    std::string GetFileName() const;
    long        GetFileSize() const;
    void        SaveFile(const std::string& destPath) const;

    // Interactions
    void Reply(const std::string& text);
    void React(const std::string& emoji);
    void MarkAsRead();

private:
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

    void OnMessage(MessageCallback callback);
    void OnReaction(ReactionCallback callback);

    // Factory / Accessors
    Group CreateGroup(const std::string& name);
    Group GetGroup(const std::string& groupId);
    User  GetUser(const std::string& userId);

    // Internal Access (for advanced usage if needed)
    Saf::Account&        GetAccount();
    Saf::MessageService& GetMessageService();
    Saf::GroupManager&   GetGroupManager();
    Saf::FileTransfer&   GetFileTransfer();

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
    Saf::SelfProfile m_Profile;
    std::string      m_MessageDbPath = "messages.db";
    std::atomic<bool> m_Running{false};
};

} // namespace Session
