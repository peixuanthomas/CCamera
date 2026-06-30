#pragma once

#include <string>
#include <string_view>

namespace ccamera::database_crypto {

inline constexpr const char* kEnvelopeFormat = "ccamera.face_database.encrypted";
inline constexpr int kEnvelopeVersion = 1;
inline constexpr const char* kKdfName = "PBKDF2-HMAC-SHA256";

std::string encrypt_json(std::string_view plaintext_json, std::string_view passphrase);
std::string decrypt_json(std::string_view envelope_json, std::string_view passphrase);

}  // namespace ccamera::database_crypto

