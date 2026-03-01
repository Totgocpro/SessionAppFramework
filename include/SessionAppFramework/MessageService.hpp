#pragma once

#include "Types.hpp"
#include "Account.hpp"
#include "SwarmManager.hpp"
#include "GroupManager.hpp"
#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <thread>

namespace Saf {

/**
 * @brief High-level service for sending and receiving Session messages.
 *
 * Encrypts outbound messages and decrypts inbound envelopes using the
 * Session encryption scheme (X25519 ECDH + XSalsa20-Poly1305).
 *
 * Message polling runs on a background thread.
 *
 * Usage:
 * @code
 *   MessageService ms(account, swarmManager, groupManager);
 *   ms.OnMessage([](const Message& m) {
 *       std::cout << m.Sender << ": " << m.Body << "\n";
 *   });
 *   ms.StartPolling({ .Interval = std::chrono::seconds(5) });
 *
 *   ms.SendText("05abc...", "Hello, Session!");
 * @endcode
 */
class MessageService {
public:
    MessageService(const Account& account, SwarmManager& swarmManager, GroupManager& groupManager);
    ~MessageService();

    MessageService(const MessageService&)            = delete;
    MessageService& operator=(const MessageService&) = delete;

    // ─────────────────────────────────────────────────────────
    // Sending
    // ─────────────────────────────────────────────────────────

    /**
     * @brief Sets the profile to be included in sent messages.
     */
    void SetSelfProfile(const SelfProfile& profile);

    /**
     * @brief Sends a plain-text DM to a Session account.
     * @param recipientId  66-char Account ID of the recipient.
     * @param text         UTF-8 text body.
     * @return             Server-assigned message hash.
     * @throws MessageException / SwarmException / NetworkException
     */
    std::string SendText(const AccountId& recipientId,
                         const std::string& text);

    /**
     * @brief Sends a text message to a closed group.
     * @param groupId  Group ID (66-char hex group pubkey).
     * @param text     UTF-8 text body.
     */
    std::string SendGroupText(const std::string& groupId,
                              const std::string& text);

    /**
     * @brief Sends a file message (after uploading via FileTransfer).
     * @param recipientId  Recipient account ID.
     * @param fileInfo     FileInfo returned by FileTransfer::Upload().
     */
    std::string SendFile(const AccountId& recipientId,
                         const FileInfo&  fileInfo);

    /**
     * @brief Sends a file message to a group.
     */
    std::string SendGroupFile(const std::string& groupId,
                              const FileInfo&    fileInfo);

    /**
     * @brief Sends an approval response to a message request.
     */
    std::string SendApproval(const AccountId& recipientId);

    /**
     * @brief Sends a response to a group invitation.
     */
    std::string SendGroupInviteResponse(const AccountId& recipientId, bool approved);

    /**
     * @brief Sends an invitation to a group.
     */
    std::string SendGroupInvite(const AccountId& recipientId, const std::string& groupId, const std::string& groupName);

    /**
     * @brief Kicks members and deletes their content (Admin only).
     */
    std::string SendDeleteMember(const std::string& groupId, const std::vector<AccountId>& memberIds);

    /**
     * @brief Sends a reaction to a message.
     * @param conversationId  Recipient account ID or Group ID.
     * @param messageId       The Id (timestamp) of the message to react to.
     * @param authorId        The Session ID (05...) of the person who sent the original message.
     * @param emoji           The emoji character (e.g. "👍").
     */
    std::string SendReaction(const AccountId& conversationId,
                             const std::string& messageId,
                             const std::string& authorId,
                             const std::string& emoji);

    /**
     * @brief Sends a reaction to a group message.
     */
    std::string SendGroupReaction(const std::string& groupId,
                                  const std::string& messageId,
                                  const std::string& authorId,
                                  const std::string& emoji);

    /**
     * @brief Sends a reply to a message.
     * @param recipientId  Recipient account ID.
     * @param messageId    ID of the message to reply to.
     * @param text         Reply text.
     * @param originalText The text of the original message (optional).
     */
    std::string SendReply(const AccountId& recipientId,
                          const std::string& messageId,
                          const std::string& text,
                          const std::string& originalText = "");

    /**
     * @brief Sends a reply to a group message.
     */
    std::string SendGroupReply(const std::string& groupId,
                               const std::string& messageId,
                               const std::string& text,
                               const std::string& originalText = "");

    // ─────────────────────────────────────────────────────────
    // Receiving (polling)
    // ─────────────────────────────────────────────────────────

    /**
     * @brief Registers a callback invoked for every incoming message.
     *
     * Multiple callbacks can be registered; they are called in order.
     */
    void OnMessage(MessageCallback callback);

    /**
     * @brief Registers a callback for decryption / parsing errors.
     */
    void OnError(ErrorCallback callback);

    /**
     * @brief Starts the background polling thread.
     * @param config  Polling interval and namespace configuration.
     */
    void StartPolling(PollConfig config = {});

    /**
     * @brief Stops the background polling thread (blocks until stopped).
     */
    void StopPolling();

    bool IsPolling() const;

    /**
     * @brief Manually triggers one poll cycle (synchronous).
     * Useful for testing or manual event-loop integration.
     */
    std::vector<Message> PollOnce();

    // ─────────────────────────────────────────────────────────
    // History
    // ─────────────────────────────────────────────────────────

    /// Returns the timestamp of the last successfully polled message.
    int64_t GetLastSeenTimestamp() const;

    /// Resets the "last seen" cursor so all messages are re-delivered.
    void ResetLastSeen();

private:
    struct Impl;
    std::unique_ptr<Impl> m_Impl;
};

} // namespace Saf
