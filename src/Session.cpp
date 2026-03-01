#include <SessionAppFramework/Session.hpp>
#include <iostream>
#include <thread>

namespace Session {

// ─────────────────────────────────────────────────────────
// User Implementation
// ─────────────────────────────────────────────────────────

User::User(Client* client, const std::string& id)
    : m_Client(client), m_Id(id) {}

std::string User::GetId() const { return m_Id; }

std::string User::GetDisplayName() const {
    // Basic implementation: check if it's "me" or fallback to ID
    if (m_Id == m_Client->GetAccount().GetAccountId()) {
        return "Me";
    }
    // TODO: Contact list lookup in future versions
    return m_Id.substr(0, 8) + "...";
}

void User::SendMessage(const std::string& text) {
    m_Client->GetMessageService().SendText(m_Id, text);
}

void User::SendFile(const std::string& filePath) {
    auto info = m_Client->GetFileTransfer().UploadFile(filePath);
    m_Client->GetMessageService().SendFile(m_Id, info);
}

// ─────────────────────────────────────────────────────────
// Group Implementation
// ─────────────────────────────────────────────────────────

Group::Group(Client* client, const std::string& id)
    : m_Client(client), m_Id(id) {}

std::string Group::GetId() const { return m_Id; }

std::string Group::GetName() const {
    return m_Client->GetGroupManager().Get(m_Id).Name;
}

std::string Group::GetDescription() const {
    return m_Client->GetGroupManager().Get(m_Id).Description;
}

std::vector<User> Group::GetMembers() const {
    auto rawMembers = m_Client->GetGroupManager().GetMembers(m_Id);
    std::vector<User> users;
    for (const auto& m : rawMembers) {
        users.emplace_back(m_Client, m.Id);
    }
    return users;
}

bool Group::IsAdmin() const {
    return m_Client->GetGroupManager().Get(m_Id).IsAdmin;
}

void Group::SendMessage(const std::string& text) {
    m_Client->GetMessageService().SendGroupText(m_Id, text);
}

void Group::SendFile(const std::string& filePath) {
    auto info = m_Client->GetFileTransfer().UploadFile(filePath);
    m_Client->GetMessageService().SendGroupFile(m_Id, info);
}

void Group::Leave() {
    m_Client->GetGroupManager().Leave(m_Id);
}

void Group::SetName(const std::string& name) {
    m_Client->GetGroupManager().SetName(m_Id, name);
}

void Group::AddMember(const std::string& userId) {
    m_Client->GetGroupManager().AddMember(m_Id, userId);
}

void Group::RemoveMember(const std::string& userId) {
    m_Client->GetGroupManager().RemoveMember(m_Id, userId);
}

void Group::PromoteMember(const std::string& userId) {
    m_Client->GetGroupManager().PromoteToAdmin(m_Id, userId);
}

void Group::DemoteMember(const std::string& userId) {
    m_Client->GetGroupManager().DemoteAdmin(m_Id, userId);
}

// ─────────────────────────────────────────────────────────
// Message Implementation
// ─────────────────────────────────────────────────────────

Message::Message(Client* client, const Saf::Message& raw)
    : m_Client(client), m_Raw(raw) {}

std::string Message::GetId() const { return m_Raw.Id; }
std::string Message::GetContent() const { return m_Raw.Body; }

User Message::GetAuthor() const {
    return User(m_Client, m_Raw.Sender);
}

bool Message::IsGroup() const {
    return !m_Raw.GroupId.empty();
}

Group Message::GetGroup() const {
    if (!IsGroup()) throw std::runtime_error("Message is not from a group");
    return Group(m_Client, m_Raw.GroupId);
}

bool Message::HasFile() const {
    return m_Raw.Type == Saf::MessageType::File || m_Raw.Type == Saf::MessageType::Image;
}

std::string Message::GetFileName() const { return m_Raw.FileName; }
long Message::GetFileSize() const { return m_Raw.FileSize; }

void Message::SaveFile(const std::string& destPath) const {
    if (!HasFile()) return;
    // FileTransfer::DownloadToFile handles the JSON parsing from m_Raw.Data
    m_Client->GetFileTransfer().DownloadToFile(m_Raw, destPath);
}

void Message::Reply(const std::string& text) {
    if (IsGroup()) {
        m_Client->GetMessageService().SendGroupReply(m_Raw.GroupId, m_Raw.Id, text, m_Raw.Body);
    } else {
        m_Client->GetMessageService().SendReply(m_Raw.Sender, m_Raw.Id, text, m_Raw.Body);
    }
}

void Message::React(const std::string& emoji) {
    if (IsGroup()) {
        // For groups, we must pass the ORIGINAL sender ID as the 'author' of the reaction target
        // This was the fix found earlier.
        m_Client->GetMessageService().SendGroupReaction(m_Raw.GroupId, m_Raw.Id, m_Raw.Sender, emoji);
    } else {
        m_Client->GetMessageService().SendReaction(m_Raw.Sender, m_Raw.Id, m_Raw.Sender, emoji);
    }
}

void Message::MarkAsRead() {
    // TODO: Implement Read Receipts in MessageService first
}

// ─────────────────────────────────────────────────────────
// Client Implementation
// ─────────────────────────────────────────────────────────

Client::Client(const std::string& seedOrMnemonic) {
    if (seedOrMnemonic.empty()) {
        m_Account.Create();
    } else {
        m_Account.LoadFromMnemonic(seedOrMnemonic);
    }

    m_Swarm  = std::make_unique<Saf::SwarmManager>(m_Account, m_Net);
    m_Config = std::make_unique<Saf::ConfigManager>(m_Account, *m_Swarm);
    m_Gm     = std::make_unique<Saf::GroupManager>(m_Account, *m_Swarm, *m_Config);
    m_Ms     = std::make_unique<Saf::MessageService>(m_Account, *m_Swarm, *m_Gm);
    m_Ft     = std::make_unique<Saf::FileTransfer>(m_Account, m_Net);

    // Link managers
    m_Gm->SetMessageService(m_Ms.get());
}

Client::~Client() {
    Stop();
}

void Client::Start() {
    if (m_Running) return;
    m_Running = true;

    // 1. Bootstrap
    m_Swarm->Bootstrap();

    // 2. Sync Config
    m_Config->Pull();
    
    // Load current profile into cache
    m_Profile.DisplayName = m_Config->GetDisplayName();
    m_Profile.ProfilePictureUrl = m_Config->GetProfilePictureUrl();
    m_Ms->SetSelfProfile(m_Profile);

    // 3. Setup Polling
    Saf::PollConfig pollCfg;
    pollCfg.Interval = std::chrono::seconds(2);
    pollCfg.PollGroups = true;
    pollCfg.MessageDbPath = m_MessageDbPath;

    m_Ms->OnMessage([this](const Saf::Message& raw) {
        // Auto-join invites
        if (raw.Type == Saf::MessageType::GroupInvite) {
            try {
                m_Gm->Join(raw.GroupId, raw.GroupName, raw.MemberAuthData, raw.AdminSignature);
                std::this_thread::sleep_for(std::chrono::seconds(1));
                m_Ms->SendGroupInviteResponse(raw.GroupId, true);
            } catch (...) {}
        }

        if (raw.Type == Saf::MessageType::Reaction) {
            if (m_OnReaction) {
                User reactor(this, raw.Sender);
                // Create a "virtual" target message from the ID we have
                Saf::Message targetRaw;
                targetRaw.Id = raw.ReactionToId;
                targetRaw.GroupId = raw.GroupId;
                // If it's a DM, the reply should go back to the person in the conversation
                if (raw.GroupId.empty()) {
                    targetRaw.Sender = raw.Sender;
                }
                
                Message target(this, targetRaw);
                m_OnReaction(reactor, target, raw.ReactionEmoji, !raw.IsReactionRemoval);
            }
            return; // Don't trigger OnMessage for reactions
        }

        if (m_OnMessage) {
            Message msg(this, raw);
            m_OnMessage(msg);
        }
    });

    m_Ms->StartPolling(pollCfg);
}

void Client::Stop() {
    if (!m_Running) return;
    m_Running = false;
    m_Ms->StopPolling();
}

User Client::GetMe() {
    return User(this, m_Account.GetAccountId());
}

std::string Client::GetMnemonic() const {
    return m_Account.GetMnemonic();
}

void Client::SetDisplayName(const std::string& name) {
    // 1. Update Config (Namespace 2) - persistent on swarm
    m_Config->SetDisplayName(name);
    m_Config->Push();

    // 2. Update MessageService profile - sent with every message metadata
    m_Profile.DisplayName = name;
    m_Ms->SetSelfProfile(m_Profile);
}

void Client::SetProfilePicture(const std::string& filePath) {
    // 1. Upload to file server
    auto info = m_Ft->UploadFile(filePath, "image/jpeg");

    // 2. Update Config (Namespace 2)
    m_Config->SetProfilePicture(info.Url, info.Key);
    m_Config->Push();

    // 3. Update MessageService profile
    m_Profile.ProfilePictureUrl = info.Url;
    m_Ms->SetSelfProfile(m_Profile);
}

void Client::SetMessageDbPath(const std::string& path) {
    m_MessageDbPath = path;
}

std::string Client::GetMessageDbPath() const {
    return m_MessageDbPath;
}

void Client::OnMessage(MessageCallback callback) {
    m_OnMessage = callback;
}

void Client::OnReaction(ReactionCallback callback) {
    m_OnReaction = callback;
}

Group Client::CreateGroup(const std::string& name) {
    auto g = m_Gm->Create(name);
    return Group(this, g.Id);
}

Group Client::GetGroup(const std::string& groupId) {
    return Group(this, groupId);
}

User Client::GetUser(const std::string& userId) {
    return User(this, userId);
}

Saf::Account&        Client::GetAccount()        { return m_Account; }
Saf::MessageService& Client::GetMessageService() { return *m_Ms; }
Saf::GroupManager&   Client::GetGroupManager()   { return *m_Gm; }
Saf::FileTransfer&   Client::GetFileTransfer()   { return *m_Ft; }

} // namespace Session
