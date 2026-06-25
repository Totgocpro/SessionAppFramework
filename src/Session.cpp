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

void User::SendVoice(const std::string& filePath) {
    auto info = m_Client->GetFileTransfer().UploadFile(filePath, "audio/ogg");
    m_Client->GetMessageService().SendFile(m_Id, info);
}

void User::SendTypingStarted() {
    m_Client->GetMessageService().SendTypingStarted(m_Id);
}

void User::SendTypingStopped() {
    m_Client->GetMessageService().SendTypingStopped(m_Id);
}

void User::SendReadReceipt(const std::vector<int64_t>& timestamps) {
    m_Client->GetMessageService().SendReadReceipt(m_Id, timestamps);
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

void Group::SendVoice(const std::string& filePath) {
    auto info = m_Client->GetFileTransfer().UploadFile(filePath, "audio/ogg");
    m_Client->GetMessageService().SendGroupFile(m_Id, info);
}

void Group::SendTypingStarted() {
    m_Client->GetMessageService().SendTypingStarted(m_Id);
}

void Group::SendTypingStopped() {
    m_Client->GetMessageService().SendTypingStopped(m_Id);
}

void Group::SendReadReceipt(const std::vector<int64_t>& timestamps) {
    m_Client->GetMessageService().SendReadReceipt(m_Id, timestamps);
}

Conversation Group::GetConversation() const {
    return Conversation(m_Client, m_Id, true);
}

void Group::Leave() {
    m_Client->GetGroupManager().Leave(m_Id);
}

void Group::SetName(const std::string& name) {
    m_Client->GetGroupManager().SetName(m_Id, name);
}

void Group::SetDescription(const std::string& description) {
    m_Client->GetGroupManager().SetDescription(m_Id, description);
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

void Group::Delete() {
    m_Client->GetGroupManager().Destroy(m_Id);
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

Conversation Message::GetConversation() const {
    if (IsGroup()) {
        return Conversation(m_Client, m_Raw.GroupId, true);
    }
    // For DMs, the conversation ID is the sender's ID (or recipient's for outgoing)
    std::string convId = m_Raw.Sender;
    if (convId == m_Client->GetMe().GetId()) convId = m_Raw.Recipient;
    return Conversation(m_Client, convId, false);
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
        m_Client->GetMessageService().SendGroupReaction(m_Raw.GroupId, m_Raw.Id, m_Raw.Sender, emoji);
    } else {
        m_Client->GetMessageService().SendReaction(m_Raw.Sender, m_Raw.Id, m_Raw.Sender, emoji);
    }
}

void Message::MarkAsRead() {
    std::vector<int64_t> timestamps;
    try { timestamps.push_back(std::stoll(m_Raw.Id)); } catch (...) {}
    if (timestamps.empty()) timestamps.push_back(m_Raw.Timestamp);

    if (IsGroup()) {
        m_Client->GetMessageService().SendReadReceipt(m_Raw.GroupId, timestamps);
    } else {
        m_Client->GetMessageService().SendReadReceipt(m_Raw.Sender, timestamps);
    }
}

void Message::Delete() {
    int64_t ts = m_Raw.Timestamp;
    try { ts = std::stoll(m_Raw.Id); } catch (...) {}

    if (IsGroup()) {
        m_Client->GetMessageService().SendUnsend(m_Raw.GroupId, ts, m_Raw.Sender);
    } else {
        m_Client->GetMessageService().SendUnsend(m_Raw.Sender, ts, m_Raw.Sender);
    }
}

void Message::SendReadReceipt() {
    MarkAsRead();
}

bool Message::IsExpirationUpdate() const {
    return m_Raw.IsExpirationUpdate;
}

int Message::GetExpirationTimer() const {
    return m_Raw.ExpirationTimer;
}

int Message::GetExpirationType() const {
    return m_Raw.ExpirationType;
}

// ─────────────────────────────────────────────────────────
// Conversation Implementation
// ─────────────────────────────────────────────────────────

Conversation::Conversation(Client* client, const std::string& id, bool isGroup)
    : m_Client(client), m_Id(id), m_IsGroup(isGroup) {}

std::string Conversation::GetId() const { return m_Id; }

std::string Conversation::GetName() const {
    if (m_IsGroup) {
        return m_Client->GetGroupManager().Get(m_Id).Name;
    }
    return m_Client->GetUser(m_Id).GetDisplayName();
}

bool Conversation::IsGroup() const { return m_IsGroup; }

User Conversation::GetUser() const {
    if (m_IsGroup) throw std::runtime_error("Conversation is a group");
    return User(m_Client, m_Id);
}

Group Conversation::GetGroup() const {
    if (!m_IsGroup) throw std::runtime_error("Conversation is not a group");
    return Group(m_Client, m_Id);
}

void Conversation::SendMessage(const std::string& text) {
    if (m_IsGroup) m_Client->GetMessageService().SendGroupText(m_Id, text);
    else m_Client->GetMessageService().SendText(m_Id, text);
}

void Conversation::SendFile(const std::string& filePath) {
    auto info = m_Client->GetFileTransfer().UploadFile(filePath);
    if (m_IsGroup) m_Client->GetMessageService().SendGroupFile(m_Id, info);
    else m_Client->GetMessageService().SendFile(m_Id, info);
}

void Conversation::SendVoice(const std::string& filePath) {
    auto info = m_Client->GetFileTransfer().UploadFile(filePath, "audio/ogg");
    if (m_IsGroup) m_Client->GetMessageService().SendGroupFile(m_Id, info);
    else m_Client->GetMessageService().SendFile(m_Id, info);
}

void Conversation::SendTypingStarted() {
    m_Client->GetMessageService().SendTypingStarted(m_Id);
}

void Conversation::SendTypingStopped() {
    m_Client->GetMessageService().SendTypingStopped(m_Id);
}

void Conversation::SendReadReceipt(const std::vector<int64_t>& timestamps) {
    m_Client->GetMessageService().SendReadReceipt(m_Id, timestamps);
}

std::vector<Message> Conversation::GetMessages(int limit) {
    auto rawMessages = m_Client->GetMessageService().RetrieveConversation(m_Id, limit);
    std::vector<Message> messages;
    for (auto& raw : rawMessages) {
        messages.emplace_back(m_Client, raw);
    }
    return messages;
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
        // Group invite
        if (raw.Type == Saf::MessageType::GroupInvite) {
            if (m_OnGroupInvite) {
                Group group(this, raw.GroupId);
                m_OnGroupInvite(group, raw.GroupName);
            } else {
                // Default: auto-join and accept
                try {
                    m_Gm->Join(raw.GroupId, raw.GroupName, raw.MemberAuthData, raw.AdminSignature);
                    std::this_thread::sleep_for(std::chrono::seconds(1));
                    m_Ms->SendGroupInviteResponse(raw.GroupId, true);
                } catch (...) {}
            }
            return;
        }

        // Promoted to admin
        if (raw.Type == Saf::MessageType::GroupPromotedToAdmin) {
            if (m_OnGroupPromotedToAdmin) {
                Group group(this, raw.GroupId);
                m_OnGroupPromotedToAdmin(group);
            }
            return;
        }

        // Group info changed
        if (raw.Type == Saf::MessageType::GroupInfoChanged) {
            if (m_OnGroupInfoChanged) {
                Group group(this, raw.GroupId);
                m_OnGroupInfoChanged(group, raw.GroupInfoChangeType, raw.GroupInfoNewValue);
            }
            return;
        }

        // Group member left
        if (raw.Type == Saf::MessageType::GroupMemberLeft) {
            if (m_OnGroupMemberLeft) {
                Group group(this, raw.GroupId);
                User member(this, raw.Sender);
                m_OnGroupMemberLeft(group, member, raw.IsMemberLeftNotification);
            }
            return;
        }

        // Message request response
        if (raw.Type == Saf::MessageType::MessageRequestResponse) {
            if (m_OnMessageRequestResponse) {
                User sender(this, raw.Sender);
                m_OnMessageRequestResponse(sender);
            }
            return;
        }

        if (raw.Type == Saf::MessageType::Reaction) {
            if (m_OnReaction) {
                User reactor(this, raw.Sender);
                Saf::Message targetRaw;
                targetRaw.Id = raw.ReactionToId;
                targetRaw.GroupId = raw.GroupId;
                if (raw.GroupId.empty()) {
                    targetRaw.Sender = raw.Sender;
                }
                Message target(this, targetRaw);
                m_OnReaction(reactor, target, raw.ReactionEmoji, !raw.IsReactionRemoval);
            }
            return;
        }

        // Typing indicators
        if (raw.IsTypingStarted || raw.IsTypingStopped) {
            if (m_OnTyping) {
                User sender(this, raw.Sender);
                m_OnTyping(sender, raw.IsTypingStarted);
            }
            return;
        }

        // Read receipts
        if (raw.IsReadReceipt) {
            if (m_OnReadReceipt) {
                User sender(this, raw.Sender);
                m_OnReadReceipt(sender, raw.ReceiptTimestamps);
            }
            return;
        }

        // Unsend requests
        if (raw.IsUnsend) {
            if (m_OnUnsend) {
                User sender(this, raw.Sender);
                int64_t ts = 0;
                try { ts = std::stoll(raw.Id); } catch (...) {}
                m_OnUnsend(sender, ts);
            }
            return;
        }

        // Data extraction notifications
        if (raw.IsDataExtraction) {
            if (m_OnDataExtraction) {
                User sender(this, raw.Sender);
                m_OnDataExtraction(sender, raw.ExtractionType);
            }
            return;
        }

        // Call messages
        if (raw.IsCallMessage) {
            if (m_OnCall) {
                User sender(this, raw.Sender);
                m_OnCall(sender, raw.CallType, raw.CallUuid);
            }
            return;
        }

        // Expiration updates
        if (raw.IsExpirationUpdate) {
            if (m_OnExpirationUpdate) {
                User sender(this, raw.Sender);
                m_OnExpirationUpdate(sender, raw.ExpirationTimer, raw.ExpirationType);
            }
            return;
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

void Client::OnTyping(TypingCallback callback) {
    m_OnTyping = callback;
}

void Client::OnReadReceipt(ReadReceiptCallback callback) {
    m_OnReadReceipt = callback;
}

void Client::OnUnsend(UnsendCallback callback) {
    m_OnUnsend = callback;
}

void Client::OnDataExtraction(DataExtractionCallback callback) {
    m_OnDataExtraction = callback;
}

void Client::OnCall(CallCallback callback) {
    m_OnCall = callback;
}

void Client::OnExpirationUpdate(ExpirationUpdateCallback callback) {
    m_OnExpirationUpdate = callback;
}

void Client::OnGroupInvite(GroupInviteCallback callback) {
    m_OnGroupInvite = callback;
}

void Client::OnGroupPromotedToAdmin(GroupPromotedToAdminCallback callback) {
    m_OnGroupPromotedToAdmin = callback;
}

void Client::OnGroupInfoChanged(GroupInfoChangedCallback callback) {
    m_OnGroupInfoChanged = callback;
}

void Client::OnGroupMemberLeft(GroupMemberLeftCallback callback) {
    m_OnGroupMemberLeft = callback;
}

void Client::OnMessageRequestResponse(MessageRequestResponseCallback callback) {
    m_OnMessageRequestResponse = callback;
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

Conversation Client::GetConversation(const std::string& conversationId) {
    bool isGroup = (conversationId.size() == 66 && conversationId.substr(0, 2) == "03");
    return Conversation(this, conversationId, isGroup);
}

Saf::Account&        Client::GetAccount()        { return m_Account; }
Saf::MessageService& Client::GetMessageService() { return *m_Ms; }
Saf::GroupManager&   Client::GetGroupManager()   { return *m_Gm; }
Saf::FileTransfer&   Client::GetFileTransfer()   { return *m_Ft; }
Saf::SwarmManager&   Client::GetSwarmManager()   { return *m_Swarm; }

// Communities (stubs - full implementation needs CommunityManager)
void Client::JoinCommunity(const std::string& fullUrl) {
    // TODO: Full community manager using session::config::Community from libsession-util
    std::cout << "[Community] Joining: " << fullUrl << std::endl;
}

void Client::LeaveCommunity(const std::string& fullUrl) {
    std::cout << "[Community] Would leave: " << fullUrl << std::endl;
}

std::vector<Saf::CommunityRoom> Client::GetCommunities() const {
    return {}; // TODO: Full community manager
}

void Client::SendCommunityMessage(const std::string& fullUrl, const std::string& text) {
    std::cout << "[Community] Would send message to " << fullUrl << ": " << text << std::endl;
}

void Client::SendCommunityFile(const std::string& fullUrl, const std::string& filePath) {
    std::cout << "[Community] Would send file to " << fullUrl << std::endl;
}

Saf::OnsResult Client::ResolveOns(const std::string& onsName) {
    return m_Swarm->ResolveOns(onsName);
}

} // namespace Session
