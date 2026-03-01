#pragma once

#include "Types.hpp"
#include "Account.hpp"
#include "NetworkClient.hpp"
#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace Saf {

/**
 * @brief Handles end-to-end encrypted file upload and download.
 */
class FileTransfer {
public:
    /// Default Session file server URL
    static constexpr const char* DefaultFileServerUrl =
        "http://filev2.getsession.org";

    /**
     * @param account        Session account (for authenticated downloads).
     * @param networkClient  HTTP client to use.
     * @param fileServerUrl  Override file server URL (optional).
     */
    explicit FileTransfer(const Account& account,
                          NetworkClient& networkClient,
                          std::string    fileServerUrl = DefaultFileServerUrl);
    ~FileTransfer();

    FileTransfer(const FileTransfer&)            = delete;
    FileTransfer& operator=(const FileTransfer&) = delete;

    // ─────────────────────────────────────────────────────────
    // Upload
    // ─────────────────────────────────────────────────────────

    /**
     * @brief Encrypts and uploads raw bytes.
     *
     * @param fileName      Original file name (used only in metadata).
     * @param data          Raw (unencrypted) file bytes.
     * @param mimeType      MIME type string, e.g. "image/jpeg".
     * @param onProgress    Optional progress callback (bytes sent, total).
     * @return              FileInfo to embed in the outgoing message.
     * @throws FileTransferException on network or crypto failure.
     */
    FileInfo Upload(const std::string&  fileName,
                    const Bytes&        data,
                    const std::string&  mimeType    = "application/octet-stream",
                    ProgressCallback    onProgress  = nullptr);

    /**
     * @brief Convenience overload: reads from a file path.
     * @param filePath    Path to the file on disk.
     * @param mimeType    MIME type (auto-detected from extension if empty).
     * @param onProgress  Optional progress callback.
     */
    FileInfo UploadFile(const std::string& filePath,
                        const std::string& mimeType   = "",
                        ProgressCallback   onProgress = nullptr);

    // ─────────────────────────────────────────────────────────
    // Download
    // ─────────────────────────────────────────────────────────

    /**
     * @brief Downloads and decrypts a file described in an incoming Message.
     *
     * @param message    The received file message (Type == MessageType::File
     *                   or MessageType::Image).
     * @param onProgress Optional progress callback (bytes received, total).
     * @return           Decrypted raw bytes.
     * @throws FileTransferException on network, crypto, or integrity failure.
     */
    Bytes Download(const Message&   message,
                   ProgressCallback onProgress = nullptr);

    /**
     * @brief Downloads and decrypts using explicit FileInfo.
     * @param info       FileInfo containing URL, encryption key, digest.
     * @param onProgress Optional progress callback.
     */
    Bytes Download(const FileInfo&  info,
                   ProgressCallback onProgress = nullptr);

    /**
     * @brief Saves a downloaded file to disk.
     * @param message   Received file message.
     * @param destPath  Destination file path.
     */
    void DownloadToFile(const Message&     message,
                        const std::string& destPath,
                        ProgressCallback   onProgress = nullptr);

private:
    struct Impl;
    std::unique_ptr<Impl> m_Impl;
};

} // namespace Saf
