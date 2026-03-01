#pragma once

#include "Types.hpp"
#include <string>
#include <vector>
#include <cstdint>
#include <chrono>

namespace Saf::Utils {

// ─────────────────────────────────────────────────────────
// Hex encoding / decoding
// ─────────────────────────────────────────────────────────
std::string   BytesToHex(const Bytes& data);
Bytes         HexToBytes(const std::string& hex);

// ─────────────────────────────────────────────────────────
// Base64 (standard + URL-safe)
// ─────────────────────────────────────────────────────────
std::string   Base64Encode(const Bytes& data);
Bytes         Base64Decode(const std::string& b64);
std::string   Base64UrlEncode(const Bytes& data);
Bytes         Base64UrlDecode(const std::string& b64);

// ─────────────────────────────────────────────────────────
// Time helpers
// ─────────────────────────────────────────────────────────
int64_t       NowMs();    ///< current time in milliseconds since epoch
int64_t       NowSeconds();

// ─────────────────────────────────────────────────────────
// String helpers
// ─────────────────────────────────────────────────────────
std::string   ToLower(const std::string& s);
std::string   ToUpper(const std::string& s);
bool          StartsWith(const std::string& s, const std::string& prefix);
Bytes         StringToBytes(const std::string& s);
std::string   BytesToString(const Bytes& b);

// ─────────────────────────────────────────────────────────
// Random bytes
// ─────────────────────────────────────────────────────────
Bytes         RandomBytes(size_t n);

// ─────────────────────────────────────────────────────────
// SHA-512 (used for key derivation)
// ─────────────────────────────────────────────────────────
Bytes         Sha512(const Bytes& data);
Bytes         Sha256(const Bytes& data);

} // namespace Saf::Utils
