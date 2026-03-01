#pragma once

#include <stdexcept>
#include <string>

namespace Saf {

/// Base exception for all SessionAppFramework errors
class SafException : public std::runtime_error {
public:
    explicit SafException(const std::string& message)
        : std::runtime_error(message) {}
};

/// Thrown when a cryptographic operation fails
class CryptoException : public SafException {
public:
    explicit CryptoException(const std::string& msg)
        : SafException("Crypto error: " + msg) {}
};

/// Thrown when an account mnemonic / seed is invalid
class InvalidMnemonicException : public SafException {
public:
    explicit InvalidMnemonicException(const std::string& msg = "Invalid mnemonic")
        : SafException(msg) {}
};

/// Thrown for any network-level failure (timeout, unreachable node, etc.)
class NetworkException : public SafException {
public:
    explicit NetworkException(const std::string& msg)
        : SafException("Network error: " + msg) {}
};

/// Thrown when the remote swarm returns a non-200 status code
class SwarmException : public SafException {
public:
    int StatusCode = 0;
    explicit SwarmException(int code, const std::string& msg)
        : SafException("Swarm error (" + std::to_string(code) + "): " + msg)
        , StatusCode(code) {}
};

/// Thrown when message encryption / decryption fails
class MessageException : public SafException {
public:
    explicit MessageException(const std::string& msg)
        : SafException("Message error: " + msg) {}
};

/// Thrown when a group operation fails
class GroupException : public SafException {
public:
    explicit GroupException(const std::string& msg)
        : SafException("Group error: " + msg) {}
};

/// Thrown when a file transfer fails
class FileTransferException : public SafException {
public:
    explicit FileTransferException(const std::string& msg)
        : SafException("File transfer error: " + msg) {}
};

/// Thrown when the account is not loaded / initialised
class AccountNotInitializedException : public SafException {
public:
    AccountNotInitializedException()
        : SafException("Account not initialized – call LoadFromMnemonic() or Create() first") {}
};

} // namespace Saf
