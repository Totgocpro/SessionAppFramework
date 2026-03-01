#include <SessionAppFramework/ConfigManager.hpp>
#include <SessionAppFramework/Exceptions.hpp>
#include <SessionAppFramework/Utils.hpp>

// libsession-util config headers
#include <session/config/user_profile.hpp>
#include <session/config/contacts.hpp>
#include <session/config/convo_info_volatile.hpp>
#include <session/config/user_groups.hpp>

#include <nlohmann/json.hpp>
#include <stdexcept>
#include <optional>
#include <span>
#include <unordered_set>
#include <iostream>

using json = nlohmann::json;

namespace Saf {

// ─────────────────────────────────────────────────────────
// Impl
// ─────────────────────────────────────────────────────────

struct ConfigManager::Impl {
    const Account& Account_;
    SwarmManager&  Swarm;

    // libsession-util config objects
    std::unique_ptr<session::config::UserProfile>       UserProfile;
    std::unique_ptr<session::config::Contacts>          Contacts;
    std::unique_ptr<session::config::ConvoInfoVolatile> ConvoInfo;
    std::unique_ptr<session::config::UserGroups>        UserGroups;

    // Namespace constants (from libsession-util)
    static constexpr int NS_USER_PROFILE  = 2;
    static constexpr int NS_CONTACTS      = 3;
    static constexpr int NS_CONVO_VOLATILE = 4;
    static constexpr int NS_USER_GROUPS   = 5;

    Impl(const Account& acc, SwarmManager& sw)
        : Account_(acc), Swarm(sw) {

        auto seed = acc.GetPrivateSeed();
        std::span<const unsigned char> seed_span{ seed.data(), seed.size() };

        UserProfile = std::make_unique<session::config::UserProfile>(seed_span, std::nullopt);
        Contacts = std::make_unique<session::config::Contacts>(seed_span, std::nullopt);
        ConvoInfo = std::make_unique<session::config::ConvoInfoVolatile>(seed_span, std::nullopt);
        UserGroups = std::make_unique<session::config::UserGroups>(seed_span, std::nullopt);
    }

    // Push a single config namespace to the swarm
    void PushNs(session::config::ConfigBase& cfg, int ns) {
        if (!cfg.needs_push()) return;
        auto [seqno, payloads, obs] = cfg.push();
        std::unordered_set<std::string> hashes;
        for (const auto& payload : payloads) {
            auto h = Swarm.Store(Account_.GetAccountId(), payload, ns,
                                 /* ttlMs = 30 days */ 30LL * 24 * 3600 * 1000);
            if (!h.empty()) hashes.insert(h);
        }
        cfg.confirm_pushed(seqno, hashes);

        // Delete obsolete messages from the swarm
        if (!obs.empty()) {
            Swarm.Delete(obs, ns);
        }
    }

    // Pull and merge a config namespace from the swarm
    void PullNs(session::config::ConfigBase& cfg, int ns) {
        try {
            auto envelopes = Swarm.Retrieve(Account_.GetAccountId(), ns, "");
            // if (!envelopes.empty()) {
            //     std::cout << "[ConfigManager] Pulled " << envelopes.size() << " messages for NS " << ns << std::endl;
            // }
            std::vector<std::pair<std::string, std::vector<unsigned char>>> configs;
            for (const auto& env : envelopes) {
                configs.push_back({ env.Hash, env.Data });
            }
            if (!configs.empty())
                cfg.merge(configs);
        } catch (...) {}
    }
};

// ─────────────────────────────────────────────────────────
// ConfigManager
// ─────────────────────────────────────────────────────────

ConfigManager::ConfigManager(const Account& account, SwarmManager& swarmManager)
    : m_Impl(std::make_unique<Impl>(account, swarmManager)) {}

ConfigManager::~ConfigManager() = default;

void ConfigManager::Push() {
    m_Impl->PushNs(*m_Impl->UserProfile, Impl::NS_USER_PROFILE);
    m_Impl->PushNs(*m_Impl->Contacts,    Impl::NS_CONTACTS);
    m_Impl->PushNs(*m_Impl->ConvoInfo,   Impl::NS_CONVO_VOLATILE);
    m_Impl->PushNs(*m_Impl->UserGroups,  Impl::NS_USER_GROUPS);
}

void ConfigManager::Pull() {
    m_Impl->PullNs(*m_Impl->UserProfile, Impl::NS_USER_PROFILE);
    m_Impl->PullNs(*m_Impl->Contacts,    Impl::NS_CONTACTS);
    m_Impl->PullNs(*m_Impl->ConvoInfo,   Impl::NS_CONVO_VOLATILE);
    m_Impl->PullNs(*m_Impl->UserGroups,  Impl::NS_USER_GROUPS);
}

void ConfigManager::PushNamespace(int ns) {
    if (ns == Impl::NS_USER_PROFILE) m_Impl->PushNs(*m_Impl->UserProfile, ns);
    else if (ns == Impl::NS_CONTACTS) m_Impl->PushNs(*m_Impl->Contacts, ns);
    else if (ns == Impl::NS_CONVO_VOLATILE) m_Impl->PushNs(*m_Impl->ConvoInfo, ns);
    else if (ns == Impl::NS_USER_GROUPS) m_Impl->PushNs(*m_Impl->UserGroups, ns);
    else throw SafException("Unknown config namespace " + std::to_string(ns));
}

void ConfigManager::PullNamespace(int ns) {
    if (ns == Impl::NS_USER_PROFILE) m_Impl->PullNs(*m_Impl->UserProfile, ns);
    else if (ns == Impl::NS_CONTACTS) m_Impl->PullNs(*m_Impl->Contacts, ns);
    else if (ns == Impl::NS_CONVO_VOLATILE) m_Impl->PullNs(*m_Impl->ConvoInfo, ns);
    else if (ns == Impl::NS_USER_GROUPS) m_Impl->PullNs(*m_Impl->UserGroups, ns);
    else throw SafException("Unknown config namespace " + std::to_string(ns));
}

// ─────────────────────────────────────────────────────────
// UserProfile
// ─────────────────────────────────────────────────────────

std::string ConfigManager::GetDisplayName() const {
    auto name = m_Impl->UserProfile->get_name();
    return name ? std::string(*name) : "";
}

void ConfigManager::SetDisplayName(const std::string& name) {
    // std::cout << "[ConfigManager] Setting DisplayName to: " << name << std::endl;
    m_Impl->UserProfile->set_name(name);
    // auto check = m_Impl->UserProfile->get_name();
    // if (check) {
    //     std::cout << "[ConfigManager] Internal check SUCCESS: name is now '" << std::string(*check) << "'" << std::endl;
    // } else {
    //     std::cout << "[ConfigManager] Internal check FAILED: name is still NULL!" << std::endl;
    // }
}

std::string ConfigManager::GetProfilePictureUrl() const {
    auto pic = m_Impl->UserProfile->get_profile_pic();
    return pic.url;
}

void ConfigManager::SetProfilePicture(const std::string& url, const Bytes& key) {
    session::config::profile_pic pic;
    pic.url = url;
    pic.key.assign(key.begin(), key.end());
    m_Impl->UserProfile->set_profile_pic(pic);
}

// ─────────────────────────────────────────────────────────
// Contacts
// ─────────────────────────────────────────────────────────

std::vector<Contact> ConfigManager::GetContacts() const {
    std::vector<Contact> out;
    for (auto it = m_Impl->Contacts->begin(); it != m_Impl->Contacts->end(); ++it) {
        Contact c;
        c.Id         = std::string(it->session_id);
        c.Name       = it->name;
        c.IsApproved = it->approved;
        c.IsBlocked  = it->blocked;
        out.push_back(std::move(c));
    }
    return out;
}

void ConfigManager::UpsertContact(const Contact& contact) {
    auto c = m_Impl->Contacts->get_or_construct(contact.Id);
    if (!contact.Name.empty()) c.name = contact.Name;
    c.approved = contact.IsApproved;
    c.blocked  = contact.IsBlocked;
    m_Impl->Contacts->set(c);
}

void ConfigManager::RemoveContact(const AccountId& contactId) {
    m_Impl->Contacts->erase(contactId);
}

void ConfigManager::ApproveContact(const AccountId& contactId) {
    auto c = m_Impl->Contacts->get_or_construct(contactId);
    c.approved = true;
    m_Impl->Contacts->set(c);
}

void ConfigManager::BlockContact(const AccountId& contactId) {
    auto c = m_Impl->Contacts->get_or_construct(contactId);
    c.blocked = true;
    m_Impl->Contacts->set(c);
}

void ConfigManager::UnblockContact(const AccountId& contactId) {
    auto c = m_Impl->Contacts->get_or_construct(contactId);
    c.blocked = false;
    m_Impl->Contacts->set(c);
}

std::optional<Contact> ConfigManager::FindContact(const AccountId& contactId) const {
    auto c = m_Impl->Contacts->get(contactId);
    if (!c) return std::nullopt;
    Contact out;
    out.Id         = std::string(c->session_id);
    out.Name       = c->name;
    out.IsApproved = c->approved;
    out.IsBlocked  = c->blocked;
    return out;
}

// ─────────────────────────────────────────────────────────
// UserGroups
// ─────────────────────────────────────────────────────────

std::vector<std::string> ConfigManager::GetGroupIds() const {
    std::vector<std::string> ids;
    // New style groups (03)
    for (auto it = m_Impl->UserGroups->begin_groups();
         it != m_Impl->UserGroups->end(); ++it) {
        ids.push_back(std::string(it->id));
    }
    // Legacy groups (05)
    for (auto it = m_Impl->UserGroups->begin_legacy_groups();
         it != m_Impl->UserGroups->end(); ++it) {
        ids.push_back(std::string(it->session_id));
    }
    return ids;
}

std::string ConfigManager::GetLegacyGroupName(const std::string& groupId) const {
    auto g = m_Impl->UserGroups->get_legacy_group(groupId);
    return g ? g->name : "";
}

Bytes ConfigManager::GetGroupAuthData(const std::string& groupId) const {
    auto g = m_Impl->UserGroups->get_group(groupId);
    if (!g) return {};
    return Bytes(g->auth_data.begin(), g->auth_data.end());
}

Bytes ConfigManager::GetGroupAdminKey(const std::string& groupId) const {
    auto g = m_Impl->UserGroups->get_group(groupId);
    if (!g) return {};
    return Bytes(g->secretkey.begin(), g->secretkey.end());
}

std::vector<GroupMember> ConfigManager::GetLegacyGroupMembers(const std::string& groupId) const {
    std::vector<GroupMember> out;
    auto g = m_Impl->UserGroups->get_legacy_group(groupId);
    if (!g) return out;
    for (const auto& [mid, admin] : g->members()) {
        GroupMember m;
        m.Id   = mid;
        m.Role = admin ? GroupMemberRole::Admin : GroupMemberRole::Standard;
        out.push_back(std::move(m));
    }
    return out;
}

void ConfigManager::AddLegacyGroupMember(const std::string& groupId, const AccountId& memberId, bool admin) {
    auto g = m_Impl->UserGroups->get_or_construct_legacy_group(groupId);
    g.insert(memberId, admin);
    m_Impl->UserGroups->set(g);
}

void ConfigManager::RemoveLegacyGroupMember(const std::string& groupId, const AccountId& memberId) {
    auto g = m_Impl->UserGroups->get_legacy_group(groupId);
    if (g) {
        g->erase(memberId);
        m_Impl->UserGroups->set(*g);
    }
}

void ConfigManager::PromoteLegacyGroupMember(const std::string& groupId, const AccountId& memberId, bool admin) {
    auto g = m_Impl->UserGroups->get_legacy_group(groupId);
    if (g) {
        g->insert(memberId, admin);
        m_Impl->UserGroups->set(*g);
    }
}

void ConfigManager::AddGroupEntry(const std::string& groupId,
                                    const std::string& name,
                                    const Bytes& adminKey,
                                    const Bytes& authData) {
    if (groupId.substr(0, 2) == "03") {
        auto g = m_Impl->UserGroups->get_or_construct_group(groupId);
        if (!name.empty()) g.name = name;
        if (!adminKey.empty()) {
            g.secretkey.assign(adminKey.begin(), adminKey.end());
        }
        if (!authData.empty()) {
            g.auth_data.assign(authData.begin(), authData.end());
        }
        m_Impl->UserGroups->set(g);
    } else {
        auto g = m_Impl->UserGroups->get_or_construct_legacy_group(groupId);
        if (!name.empty()) g.name = name;
        if (!adminKey.empty()) {
            g.enc_seckey.assign(adminKey.begin(), adminKey.end());
        }
        m_Impl->UserGroups->set(g);
    }
}

void ConfigManager::RemoveGroupEntry(const std::string& groupId) {
    if (groupId.substr(0, 2) == "03") {
        m_Impl->UserGroups->erase_group(groupId);
    } else {
        m_Impl->UserGroups->erase_legacy_group(groupId);
    }
}

// ─────────────────────────────────────────────────────────
// ConvoInfoVolatile
// ─────────────────────────────────────────────────────────

void ConfigManager::MarkConvoRead(const AccountId& contactId, int64_t timestampMs) {
    auto c = m_Impl->ConvoInfo->get_or_construct_1to1(contactId);
    c.last_read = timestampMs;
    m_Impl->ConvoInfo->set(c);
}

int64_t ConfigManager::GetLastReadTimestamp(const AccountId& contactId) const {
    auto c = m_Impl->ConvoInfo->get_1to1(contactId);
    return c ? c->last_read : 0;
}

} // namespace Saf
