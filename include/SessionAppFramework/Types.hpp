#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <functional>
#include <chrono>
#include <optional>
#include <memory>

namespace Saf {

// ─────────────────────────────────────────────────────────
// Raw byte buffer
// ─────────────────────────────────────────────────────────
using Bytes = std::vector<uint8_t>;

// ─────────────────────────────────────────────────────────
// Account ID  (66-char hex pubkey, Session format)
// ─────────────────────────────────────────────────────────
using AccountId = std::string;

// ─────────────────────────────────────────────────────────
// Message types
// ─────────────────────────────────────────────────────────
enum class MessageType {
    Text,
    Image,
    File,
    Voice,
    GroupInvite,
    Control,   // internal: read receipts, typing indicators, etc.
    Reaction,
    Reply,
};

struct Message {
    std::string     Id;          // Server-assigned hash / message hash
    AccountId       Sender;      // Account ID of sender
    AccountId       Recipient;   // Account ID of recipient (or group)
    std::string     Body;        // Decrypted text body (empty for binary)
    Bytes           Data;        // Binary payload (files / images)
    MessageType     Type        = MessageType::Text;
    int64_t         Timestamp   = 0;   // Unix ms
    int64_t         ExpiresAt   = 0;   // Unix ms, 0 = no expiry
    bool            IsRead      = false;
    std::string     GroupId;     // Non-empty for group messages
    std::string     FileName;    // For file messages
    std::string     MimeType;
    uint64_t        FileSize    = 0;

    // Reactions / Replies
    std::string     ReactionEmoji;
    std::string     ReactionToId; // Message hash being reacted to
    bool            IsReactionRemoval = false;
    std::string     ReplyToId;    // Message hash being replied to

    // Group Invitations (V2)
    std::string     GroupName;
    Bytes           MemberAuthData;
    Bytes           AdminSignature;
};

// ─────────────────────────────────────────────────────────
// Contact
// ─────────────────────────────────────────────────────────
struct Contact {
    AccountId       Id;
    std::string     Name;
    std::string     ProfilePictureUrl;
    bool            IsApproved      = false;
    bool            IsBlocked       = false;
    int64_t         LastUpdated     = 0;
};

// ─────────────────────────────────────────────────────────
// Group
// ─────────────────────────────────────────────────────────
enum class GroupMemberRole {
    Standard,
    Admin,
};

struct GroupMember {
    AccountId       Id;
    std::string     Name;
    GroupMemberRole Role    = GroupMemberRole::Standard;
    bool            IsRemoved = false;
};

struct Group {
    std::string             Id;          // 66-char hex group pubkey
    std::string             Name;
    std::string             Description;
    std::string             ProfilePictureUrl;
    std::vector<GroupMember> Members;
    int64_t                 CreatedAt   = 0;
    bool                    IsAdmin     = false;    // true if the local account is an admin
};

// ─────────────────────────────────────────────────────────
// Session Node
// ─────────────────────────────────────────────────────────
struct SessionNode {
    std::string     PublicKey;      // Ed25519 hex pubkey
    std::string     Ip;
    uint16_t        Port            = 22023;
    uint16_t        HttpsPort       = 0;
    std::string     SwarmId;
};

// ─────────────────────────────────────────────────────────
// File upload / download result
// ─────────────────────────────────────────────────────────
struct FileInfo {
    std::string     Id;             // Server-assigned ID
    std::string     Url;
    std::string     FileName;
    std::string     MimeType;
    uint64_t        Size        = 0;
    Bytes           Key;            // Encryption key (32 bytes)
    Bytes           Digest;         // SHA-256 of ciphertext
};

// ─────────────────────────────────────────────────────────
// Profile
// ─────────────────────────────────────────────────────────
struct SelfProfile {
    std::string DisplayName;
    std::string ProfilePictureUrl;
    // Note: profileKey is usually random and handled internally
};

// ─────────────────────────────────────────────────────────
// Callbacks
// ─────────────────────────────────────────────────────────
using MessageCallback    = std::function<void(const Message&)>;
using ErrorCallback      = std::function<void(const std::string& error)>;
using ProgressCallback   = std::function<void(uint64_t sent, uint64_t total)>;

// ─────────────────────────────────────────────────────────
// Poll configuration
// ─────────────────────────────────────────────────────────
struct PollConfig {
    std::chrono::milliseconds Interval      = std::chrono::seconds(3);
    int                       Namespace     = 0;    // default DM namespace
    bool                      PollGroups    = true;
    bool                      PollConfig    = true;
    std::string               MessageDbPath = "";   // Binary file for processed message IDs
};

} // namespace Saf
