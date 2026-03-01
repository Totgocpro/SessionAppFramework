#include <SessionAppFramework/MessageService.hpp>
#include <SessionAppFramework/Exceptions.hpp>
#include <SessionAppFramework/Utils.hpp>

// libsession-util headers
#include <session/session_encrypt.hpp>
#include <session/session_protocol.hpp>
#include <session/config/user_profile.hpp>
#include <SessionProtos.pb.h>
#include <oxenc/bt_serialize.h>
#include <oxenc/hex.h>

#include <nlohmann/json.hpp>
#include <atomic>
#include <chrono>
#include <thread>
#include <mutex>
#include <vector>
#include <functional>
#include <stdexcept>
#include <iostream>
#include <set>
#include <map>
#include <fstream>
#include <random>

using json = nlohmann::json;

namespace Saf {

// ─────────────────────────────────────────────────────────
// Impl
// ─────────────────────────────────────────────────────────

struct MessageService::Impl {
    const Account&              Account_;
    SwarmManager&               Swarm;
    GroupManager&               GroupMgr;
    std::vector<MessageCallback> MessageCallbacks;
    std::vector<ErrorCallback>  ErrorCallbacks;
    std::atomic<bool>           Running{false};
    std::thread                 PollThread;
    PollConfig                  Config;
    int64_t                     LastSeenTs = 0;
    std::mutex                  Mutex;
    std::map<std::string, int64_t> m_ProcessedMessages; // hash -> timestamp (ms)
    std::map<std::string, std::string> m_LastHashes;    // conversationId -> last message hash
    SelfProfile                 m_SelfProfile;

    Impl(const Account& acc, SwarmManager& sw, GroupManager& gm)
        : Account_(acc), Swarm(sw), GroupMgr(gm) {}

    bool PruneMessageDb() {
        int64_t fifteenDaysMs = 15LL * 24 * 3600 * 1000;
        int64_t now = Utils::NowMs();
        
        std::lock_guard<std::mutex> lock(Mutex);
        size_t initialSize = m_ProcessedMessages.size();
        auto it = m_ProcessedMessages.begin();
        while (it != m_ProcessedMessages.end()) {
            if (now - it->second > fifteenDaysMs) {
                it = m_ProcessedMessages.erase(it);
            } else {
                ++it;
            }
        }
        return m_ProcessedMessages.size() != initialSize;
    }

    void LoadMessageDb() {
        if (Config.MessageDbPath.empty()) return;
        std::ifstream ifs(Config.MessageDbPath, std::ios::binary);
        if (!ifs) return;

        std::lock_guard<std::mutex> lock(Mutex);
        m_ProcessedMessages.clear();
        m_LastHashes.clear();

        uint32_t processedCount = 0;
        if (!ifs.read(reinterpret_cast<char*>(&processedCount), sizeof(processedCount))) return;
        for (uint32_t i = 0; i < processedCount; ++i) {
            uint16_t len = 0;
            if (!ifs.read(reinterpret_cast<char*>(&len), sizeof(len))) break;
            std::string hash(len, '\0');
            if (!ifs.read(&hash[0], len)) break;
            int64_t ts = 0;
            if (!ifs.read(reinterpret_cast<char*>(&ts), sizeof(ts))) break;
            m_ProcessedMessages[hash] = ts;
        }

        uint32_t hashCount = 0;
        if (!ifs.read(reinterpret_cast<char*>(&hashCount), sizeof(hashCount))) return;
        for (uint32_t i = 0; i < hashCount; ++i) {
            uint16_t idLen = 0;
            if (!ifs.read(reinterpret_cast<char*>(&idLen), sizeof(idLen))) break;
            std::string id(idLen, '\0');
            if (!ifs.read(&id[0], idLen)) break;

            uint16_t hashLen = 0;
            if (!ifs.read(reinterpret_cast<char*>(&hashLen), sizeof(hashLen))) break;
            std::string hash(hashLen, '\0');
            if (!ifs.read(&hash[0], hashLen)) break;
            m_LastHashes[id] = hash;
        }
    }

    void SaveMessageDb() {
        if (Config.MessageDbPath.empty()) return;
        
        std::lock_guard<std::mutex> lock(Mutex);
        std::ofstream ofs(Config.MessageDbPath, std::ios::binary | std::ios::trunc);
        if (!ofs) return;

        uint32_t processedCount = static_cast<uint32_t>(m_ProcessedMessages.size());
        ofs.write(reinterpret_cast<const char*>(&processedCount), sizeof(processedCount));
        for (const auto& [hash, ts] : m_ProcessedMessages) {
            uint16_t len = static_cast<uint16_t>(hash.size());
            ofs.write(reinterpret_cast<const char*>(&len), sizeof(len));
            ofs.write(hash.data(), len);
            ofs.write(reinterpret_cast<const char*>(&ts), sizeof(ts));
        }

        uint32_t hashCount = static_cast<uint32_t>(m_LastHashes.size());
        ofs.write(reinterpret_cast<const char*>(&hashCount), sizeof(hashCount));
        for (const auto& [id, hash] : m_LastHashes) {
            uint16_t idLen = static_cast<uint16_t>(id.size());
            ofs.write(reinterpret_cast<const char*>(&idLen), sizeof(idLen));
            ofs.write(id.data(), idLen);

            uint16_t hashLen = static_cast<uint16_t>(hash.size());
            ofs.write(reinterpret_cast<const char*>(&hashLen), sizeof(hashLen));
            ofs.write(hash.data(), hashLen);
        }
        ofs.flush();
    }

    void FireMessage(const Message& m) {
        std::vector<MessageCallback> callbacks;
        {
            std::lock_guard<std::mutex> lock(Mutex);
            callbacks = MessageCallbacks;
        }
        for (auto& cb : callbacks) cb(m);
    }
    void FireError(const std::string& e) {
        std::vector<ErrorCallback> callbacks;
        {
            std::lock_guard<std::mutex> lock(Mutex);
            callbacks = ErrorCallbacks;
        }
        for (auto& cb : callbacks) cb(e);
    }

    bool TryProcessPlaintext(const std::vector<uint8_t>& plaintext, const std::string& senderId, const std::string& conversationId, const std::string& hash, int64_t storageTs, int64_t envelopeTs) {
        {
            std::lock_guard<std::mutex> lock(Mutex);
            if (m_ProcessedMessages.count(hash)) return true;
        }

        try {
            SessionProtos::Content content;
            if (!content.ParseFromArray(plaintext.data(), plaintext.size())) return false;

            Message msg;
            msg.Sender    = senderId;
            msg.Recipient = Account_.GetAccountId();
            if (conversationId.size() == 66 && conversationId.substr(0, 2) == "03") {
                msg.GroupId = conversationId;
            }
            
            msg.Timestamp = envelopeTs;
            msg.Id        = std::to_string(envelopeTs);

            if (content.has_datamessage()) {
                const auto& dm = content.datamessage();
                msg.Body = dm.body();

                if (dm.attachments_size() > 0) {
                    const auto& att = dm.attachments(0);
                    msg.Type     = MessageType::File;
                    msg.FileName = att.filename();
                    msg.MimeType = att.contenttype();
                    msg.FileSize = att.size();
                    json fi;
                    fi["url"]    = att.url();
                    fi["key"]    = Utils::Base64Encode(Bytes(att.key().begin(), att.key().end()));
                    fi["digest"] = Utils::Base64Encode(Bytes(att.digest().begin(), att.digest().end()));
                    msg.Data     = Utils::StringToBytes(fi.dump());
                    if (msg.MimeType.find("image/") == 0) msg.Type = MessageType::Image;
                }

                if (dm.has_reaction()) {
                    msg.Type          = MessageType::Reaction;
                    msg.ReactionEmoji = dm.reaction().emoji();
                    msg.ReactionToId  = std::to_string(dm.reaction().id()); 
                    msg.IsReactionRemoval = (dm.reaction().action() == SessionProtos::DataMessage_Reaction_Action_REMOVE);
                }

                if (dm.has_quote()) {
                    msg.Type      = MessageType::Reply;
                    msg.ReplyToId = std::to_string(dm.quote().id());
                }

                if (dm.has_groupupdatemessage()) {
                    const auto& gum = dm.groupupdatemessage();
                    if (gum.has_invitemessage()) {
                        const auto& invite = gum.invitemessage();
                        msg.Type = MessageType::GroupInvite;
                        msg.GroupId = invite.groupsessionid();
                        msg.GroupName = invite.name();
                        msg.MemberAuthData = Bytes(invite.memberauthdata().begin(), invite.memberauthdata().end());
                        msg.AdminSignature = Bytes(invite.adminsignature().begin(), invite.adminsignature().end());
                        msg.Body = "Invited to group: " + msg.GroupName;
                    }
                }
            } else {
                if (content.has_messagerequestresponse()) {
                    msg.Body = "[Message Request Response]";
                } else {
                    return true; 
                }
            }

            FireMessage(msg);
            {
                std::lock_guard<std::mutex> lock(Mutex);
                m_ProcessedMessages[hash] = storageTs;
            }
            SaveMessageDb();
            return true;
        } catch (...) {
            return false;
        }
    }

    void ProcessEnvelopes(const std::vector<SwarmManager::RawEnvelope>& envelopes, const std::string& conversationId) {
        if (envelopes.empty()) return;
        
        std::vector<std::string> hashesToDelete;

        for (const auto& env : envelopes) {
            if (!Running.load()) break;

            if (env.Timestamp > LastSeenTs) LastSeenTs = env.Timestamp;

            bool processed = false;
            auto sk = Account_.GetEd25519PrivateKey(); // 64 bytes
            std::vector<uint8_t> sk_vec(sk.begin(), sk.end());
            std::span<const uint8_t> sk_span(sk_vec.data(), sk_vec.size());
            
            session::DecodeEnvelopeKey keys = {};
            session::array_uc32 pro_backend_pk = {};

            if (conversationId.size() == 66 && conversationId.substr(0, 2) == "03") {
                try {
                    auto encKeyBytes = GroupMgr.GetEncryptionKey(conversationId);
                    if (encKeyBytes.empty()) {
                        // std::cout << "[DEBUG] No encryption key for group: " << conversationId << "\n"; When you just join a group, or receve a message when the bot was offline
                    } else {
                        session::cleared_uc32 groupEncKey;
                        std::copy_n(encKeyBytes.begin(), 32, groupEncKey.begin());
                        
                        std::vector<std::span<const uint8_t>> group_keys_list = { groupEncKey };
                        keys.decrypt_keys = group_keys_list;
                        
                        session::array_uc32 groupPk;
                        oxenc::from_hex(conversationId.begin() + 2, conversationId.end(), groupPk.begin());
                        keys.group_ed25519_pubkey = groupPk;

                        auto decoded = session::decode_envelope(keys, std::span<const uint8_t>(env.Data.data(), env.Data.size()), pro_backend_pk);
                        std::string senderId = "05" + oxenc::to_hex(decoded.sender_x25519_pubkey);
                        int64_t envTs = decoded.envelope.timestamp.count();
                        if (TryProcessPlaintext(Bytes(decoded.content_plaintext.begin(), decoded.content_plaintext.end()), senderId, conversationId, env.Hash, env.Timestamp, envTs)) {
                            processed = true;
                        }
                    }
                } catch (const std::exception& e) {
                    // std::cout << "[DEBUG] Group decryption failed for " << conversationId << ": " << e.what() << "\n";
                }
            }

            if (processed) {
                hashesToDelete.push_back(env.Hash);
                continue;
            }

            std::vector<std::span<const uint8_t>> keys_list = {sk_span};
            keys.decrypt_keys = keys_list;
            keys.group_ed25519_pubkey = std::nullopt;

            try {
                oxenc::bt_dict_consumer dict(std::string_view(reinterpret_cast<const char*>(env.Data.data()), env.Data.size()));
                if (dict.skip_until("e")) {
                    auto list = dict.consume_list_consumer();
                    while (!list.is_finished()) {
                        auto blob_view = list.consume_string_view();
                        try {
                            auto decoded = session::decode_envelope(keys, std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(blob_view.data()), blob_view.size()), pro_backend_pk);
                            std::string senderId = "05" + oxenc::to_hex(decoded.sender_x25519_pubkey);
                            int64_t envTs = decoded.envelope.timestamp.count();
                            if (TryProcessPlaintext(Bytes(decoded.content_plaintext.begin(), decoded.content_plaintext.end()), senderId, conversationId, env.Hash, env.Timestamp, envTs)) {
                                processed = true;
                                break;
                            }
                        } catch (...) {}
                    }
                }
            } catch (...) {}

            if (processed) {
                hashesToDelete.push_back(env.Hash);
                continue;
            }

            try {
                auto decoded = session::decode_envelope(keys, std::span<const uint8_t>(env.Data.data(), env.Data.size()), pro_backend_pk);
                std::string senderId = "05" + oxenc::to_hex(decoded.sender_x25519_pubkey);
                int64_t envTs = decoded.envelope.timestamp.count();
                if (TryProcessPlaintext(Bytes(decoded.content_plaintext.begin(), decoded.content_plaintext.end()), senderId, conversationId, env.Hash, env.Timestamp, envTs)) processed = true;
            } catch (...) {}

            if (processed) {
                hashesToDelete.push_back(env.Hash);
                continue;
            }

            try {
                auto [plaintext_vec, senderId] = session::decrypt_incoming_session_id(
                    std::span<const unsigned char>(sk.data(), sk.size()),
                    std::span<const unsigned char>(env.Data.data(), env.Data.size())
                );
                if (TryProcessPlaintext(plaintext_vec, senderId, conversationId, env.Hash, env.Timestamp, env.Timestamp)) processed = true;
            } catch (...) {}

            if (processed) {
                hashesToDelete.push_back(env.Hash);
            }
        }

        if (!hashesToDelete.empty()) {
            if (conversationId == Account_.GetAccountId()) {
                Swarm.Delete(hashesToDelete, Config.Namespace);
            } else if (conversationId.size() == 66) {
                if (conversationId.substr(0, 2) == "03") Swarm.Delete(hashesToDelete, 11);
                else if (conversationId.substr(0, 2) == "05") Swarm.Delete(hashesToDelete, 1);
            }
        }

        {
            std::lock_guard<std::mutex> lock(Mutex);
            m_LastHashes[conversationId] = envelopes.back().Hash;
        }
        SaveMessageDb();
    }

    void DoPoll() {
        try {
            if (PruneMessageDb()) SaveMessageDb();

            if (!Running.load()) return;

            GroupMgr.GetAll();

            std::string userLastHash;
            {
                std::lock_guard<std::mutex> lock(Mutex);
                userLastHash = m_LastHashes[Account_.GetAccountId()];
            }
            auto userEnvelopes = Swarm.Retrieve(Account_.GetAccountId(), Config.Namespace, userLastHash);
            ProcessEnvelopes(userEnvelopes, Account_.GetAccountId());

            if (Config.PollGroups) {
                auto groups = GroupMgr.GetAll();
                for (const auto& gid : groups) {
                     if (!Running.load()) break;
                     std::string gLastHash;
                     {
                         std::lock_guard<std::mutex> lock(Mutex);
                         gLastHash = m_LastHashes[gid.Id];
                     }
                     
                     Bytes authData;
                     if (gid.Id.substr(0, 2) == "03" && !gid.IsAdmin) {
                         authData = GroupMgr.GetAuthData(gid.Id);
                     }

                     int groupNs = 0;
                     if (gid.Id.size() == 66) {
                         if (gid.Id.substr(0, 2) == "03") groupNs = 11;
                         else if (gid.Id.substr(0, 2) == "05") groupNs = 1;
                     }

                     auto groupEnvelopes = Swarm.RetrieveWithAuth(gid.Id, groupNs, gLastHash, authData);
                     ProcessEnvelopes(groupEnvelopes, gid.Id);
                }
            }
        } catch (const std::exception& ex) {
            FireError(std::string("Poll error: ") + ex.what());
        }
    }

    std::string EncodeAndSend(const std::string& destinationId, SessionProtos::Content& content) {
        int64_t now = Utils::NowMs();
        content.set_sigtimestamp(now);
        
        if (content.has_datamessage()) {
            auto* dm = content.mutable_datamessage();
            if (dm->timestamp() == 0) dm->set_timestamp(now);

            if (dm->has_reaction()) {
                auto* r = dm->mutable_reaction();
                std::string auth = r->author();
                // In Session, the reaction author MUST be the sender of the original message
                if (auth.size() == 64) auth = "05" + auth;
                r->set_author(Utils::ToLower(auth));
            }
            if (dm->has_quote()) {
                auto* q = dm->mutable_quote();
                std::string auth = q->author();
                if (auth.size() == 64) auth = "05" + auth;
                q->set_author(Utils::ToLower(auth));
            }

            SelfProfile profileCopy;
            {
                std::lock_guard<std::mutex> lock(Mutex);
                profileCopy = m_SelfProfile;
            }

            if (!profileCopy.DisplayName.empty()) {
                auto* prof = dm->mutable_profile();
                prof->set_displayname(profileCopy.DisplayName);
                prof->set_lastupdateseconds(now / 1000);
                if (!profileCopy.ProfilePictureUrl.empty()) {
                    prof->set_profilepicture(profileCopy.ProfilePictureUrl);
                }
            }
        }

        std::string serialized = content.SerializeAsString();
        auto sk = Account_.GetEd25519PrivateKey();
        std::vector<uint8_t> encoded;

        if (destinationId.size() == 66 && destinationId.substr(0, 2) == "03") {
            session::array_uc33 groupPk;
            oxenc::from_hex(destinationId.begin() + 2, destinationId.end(), groupPk.begin() + 1);
            groupPk[0] = 0x03;
            auto encKeyBytes = GroupMgr.GetEncryptionKey(destinationId);
            session::cleared_uc32 groupEncKey;
            std::copy_n(encKeyBytes.begin(), 32, groupEncKey.begin());
            encoded = session::encode_for_group(
                std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(serialized.data()), serialized.size()),
                std::span<const uint8_t>(sk.data(), sk.size()),
                std::chrono::milliseconds(now),
                groupPk,
                groupEncKey,
                std::nullopt
            );

            Bytes authData;
            // Get auth data if we are not admin
            auto g = GroupMgr.Get(destinationId);
            if (!g.IsAdmin) {
                authData = GroupMgr.GetAuthData(destinationId);
            }
            // If the group has no members yet, it might not be a valid group entry
            return Swarm.StoreWithAuth(destinationId, encoded, 11, 14LL * 24 * 3600 * 1000, authData);
        } else if (destinationId.size() == 66 && destinationId.substr(0, 2) == "05") {
             // Distinguish between MP and Legacy Group
             auto members = GroupMgr.GetMembers(destinationId);
             if (!members.empty()) {
                 // Legacy group: use Namespace 1
                 session::array_uc33 recipientPk;
                 oxenc::from_hex(destinationId.begin() + 2, destinationId.end(), recipientPk.begin() + 1);
                 recipientPk[0] = 0x05;
                 encoded = session::encode_for_1o1(
                    std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(serialized.data()), serialized.size()),
                    std::span<const uint8_t>(sk.data(), sk.size()),
                    std::chrono::milliseconds(now),
                    recipientPk,
                    std::nullopt
                 );
                 return Swarm.Store(destinationId, encoded, 1);
             }
        }
        
        // Default to MP (Namespace 0)
        session::array_uc33 recipientPk;
        std::string hexPk = destinationId;
        if (hexPk.size() == 66 && hexPk.substr(0, 2) == "05") hexPk = hexPk.substr(2);
        oxenc::from_hex(hexPk.begin(), hexPk.end(), recipientPk.begin() + 1);
        recipientPk[0] = 0x05;
        encoded = session::encode_for_1o1(
            std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(serialized.data()), serialized.size()),
            std::span<const uint8_t>(sk.data(), sk.size()),
            std::chrono::milliseconds(now),
            recipientPk,
            std::nullopt
        );
        return Swarm.Store(destinationId, encoded, 0);
    }
};

MessageService::MessageService(const Account& account, SwarmManager& swarmManager, GroupManager& groupManager)
    : m_Impl(std::make_unique<Impl>(account, swarmManager, groupManager)) {}

MessageService::~MessageService() { StopPolling(); }

void MessageService::SetSelfProfile(const SelfProfile& profile) {
    std::lock_guard<std::mutex> lock(m_Impl->Mutex);
    m_Impl->m_SelfProfile = profile;
}

std::string MessageService::SendText(const AccountId& recipientId, const std::string& text) {
    SessionProtos::Content content;
    auto* dm = content.mutable_datamessage();
    dm->set_body(text);
    return m_Impl->EncodeAndSend(recipientId, content);
}

std::string MessageService::SendGroupText(const std::string& groupId, const std::string& text) { return SendText(groupId, text); }

std::string MessageService::SendFile(const AccountId& recipientId, const FileInfo& fileInfo) {
    SessionProtos::Content content;
    auto* dm = content.mutable_datamessage();
    auto* att = dm->add_attachments();
    
    // V1 uses numeric IDs. We'll use a random 64-bit ID or derive from URL segment if possible.
    // For simplicity, generate a random ID since the client uses the URL for download.
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<uint64_t> dis;
    att->set_id(dis(gen));
    
    att->set_url(fileInfo.Url);
    att->set_filename(fileInfo.FileName);
    att->set_contenttype(fileInfo.MimeType);
    att->set_size(static_cast<uint32_t>(fileInfo.Size));
    att->set_key(fileInfo.Key.data(), fileInfo.Key.size());
    
    if (!fileInfo.Digest.empty()) {
        att->set_digest(fileInfo.Digest.data(), fileInfo.Digest.size());
    }
    
    return m_Impl->EncodeAndSend(recipientId, content);
}

std::string MessageService::SendGroupFile(const std::string& groupId, const FileInfo& fileInfo) { return SendFile(groupId, fileInfo); }

std::string MessageService::SendReaction(const AccountId& conversationId, const std::string& messageId, const AccountId& authorId, const std::string& emoji) {
    SessionProtos::Content content;
    auto* dm = content.mutable_datamessage();
    auto* react = dm->mutable_reaction();
    try { react->set_id(std::stoull(messageId)); } catch (...) { react->set_id(0); }
    react->set_author(authorId); 
    react->set_emoji(emoji);
    react->set_action(SessionProtos::DataMessage_Reaction_Action_REACT);
    return m_Impl->EncodeAndSend(conversationId, content);
}

std::string MessageService::SendGroupReaction(const std::string& groupId, const std::string& messageId, const AccountId& authorId, const std::string& emoji) { return SendReaction(groupId, messageId, authorId, emoji); }

std::string MessageService::SendReply(const AccountId& recipientId, const std::string& messageId, const std::string& text, const std::string& originalText) {
    SessionProtos::Content content;
    auto* dm = content.mutable_datamessage();
    dm->set_body(text);
    auto* quote = dm->mutable_quote();
    try { quote->set_id(std::stoull(messageId)); } catch (...) { quote->set_id(0); }
    quote->set_author(recipientId); 
    quote->set_text(originalText); 
    return m_Impl->EncodeAndSend(recipientId, content);
}

std::string MessageService::SendGroupReply(const std::string& groupId, const std::string& messageId, const std::string& text, const std::string& originalText) { return SendReply(groupId, messageId, text, originalText); }

std::string MessageService::SendApproval(const AccountId& recipientId) {
    SessionProtos::Content content;
    auto* resp = content.mutable_messagerequestresponse();
    resp->set_isapproved(true);
    return m_Impl->EncodeAndSend(recipientId, content);
}

std::string MessageService::SendGroupInviteResponse(const AccountId& groupId, bool approved) {
    SessionProtos::Content content;
    auto* gum = content.mutable_datamessage()->mutable_groupupdatemessage();
    gum->mutable_inviteresponse()->set_isapproved(approved);
    return m_Impl->EncodeAndSend(groupId, content);
}

std::string MessageService::SendGroupInvite(const AccountId& recipientId, const std::string& groupId, const std::string& groupName) {
    SessionProtos::Content content;
    auto* gum = content.mutable_datamessage()->mutable_groupupdatemessage();
    if (groupId.substr(0, 2) == "03") {
        auto* invite = gum->mutable_invitemessage();
        invite->set_groupsessionid(groupId);
        invite->set_name(groupName);
        // For V2, we'd need auth data/admin sig, which we don't have here easily.
        // But for V1 (focus), we use MemberChange.
    } else {
        auto* mc = gum->mutable_memberchangemessage();
        mc->set_type(SessionProtos::GroupUpdateMemberChangeMessage_Type_ADDED);
        mc->add_membersessionids(recipientId);
        // In V1, the group ID is often passed in the body or via another field.
        // Actually, for legacy groups, it was often the group ID itself.
    }
    return m_Impl->EncodeAndSend(recipientId, content);
}

std::string MessageService::SendDeleteMember(const std::string& groupId, const std::vector<AccountId>& memberIds) {
    SessionProtos::Content content;
    auto* gum = content.mutable_datamessage()->mutable_groupupdatemessage();
    auto* dm = gum->mutable_deletemembercontent();
    for (const auto& mid : memberIds) {
        dm->add_membersessionids(mid);
    }
    return m_Impl->EncodeAndSend(groupId, content);
}

void MessageService::OnMessage(MessageCallback callback) {
    std::lock_guard<std::mutex> lock(m_Impl->Mutex);
    m_Impl->MessageCallbacks.push_back(std::move(callback));
}

void MessageService::OnError(ErrorCallback callback) {
    std::lock_guard<std::mutex> lock(m_Impl->Mutex);
    m_Impl->ErrorCallbacks.push_back(std::move(callback));
}

void MessageService::StartPolling(PollConfig config) {
    if (m_Impl->Running.load()) return;
    m_Impl->Config  = config;
    m_Impl->LoadMessageDb();
    m_Impl->Running = true;
    m_Impl->PollThread = std::thread([this]() {
        while (m_Impl->Running.load()) {
            m_Impl->DoPoll();
            if (!m_Impl->Running.load()) break;
            std::this_thread::sleep_for(m_Impl->Config.Interval);
        }
    });
}

void MessageService::StopPolling() {
    m_Impl->Running = false;
    if (m_Impl->PollThread.joinable()) m_Impl->PollThread.join();
}

bool MessageService::IsPolling() const { return m_Impl->Running.load(); }

std::vector<Message> MessageService::PollOnce() {
    m_Impl->DoPoll();
    return {}; 
}

int64_t MessageService::GetLastSeenTimestamp() const { return m_Impl->LastSeenTs; }

void MessageService::ResetLastSeen() { m_Impl->LastSeenTs = 0; }

} // namespace Saf
