#include "face/DatabaseCrypto.hpp"

#include <nlohmann/json.hpp>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <bcrypt.h>
#endif

#include <array>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace ccamera::database_crypto {
namespace {

using Byte = unsigned char;

constexpr std::uint64_t kPbkdf2Iterations = 210000;
constexpr std::size_t kSaltSize = 16;
constexpr std::size_t kNonceSize = 12;
constexpr std::size_t kKeySize = 32;
constexpr std::size_t kTagSize = 16;

ULONG checked_ulong(std::size_t value, const char* name) {
    if (value > std::numeric_limits<ULONG>::max()) {
        throw std::runtime_error(std::string(name) + " is too large for BCrypt");
    }
    return static_cast<ULONG>(value);
}

#ifdef _WIN32

std::string status_message(const char* operation, NTSTATUS status) {
    std::ostringstream out;
    out << operation << " failed with NTSTATUS 0x" << std::hex << std::uppercase
        << static_cast<std::uint32_t>(status);
    return out.str();
}

void check_status(NTSTATUS status, const char* operation) {
    if (status < 0) {
        throw std::runtime_error(status_message(operation, status));
    }
}

class AlgorithmHandle {
public:
    AlgorithmHandle() = default;
    explicit AlgorithmHandle(BCRYPT_ALG_HANDLE handle) : handle_(handle) {}

    AlgorithmHandle(const AlgorithmHandle&) = delete;
    AlgorithmHandle& operator=(const AlgorithmHandle&) = delete;

    AlgorithmHandle(AlgorithmHandle&& other) noexcept : handle_(std::exchange(other.handle_, nullptr)) {}

    AlgorithmHandle& operator=(AlgorithmHandle&& other) noexcept {
        if (this != &other) {
            reset();
            handle_ = std::exchange(other.handle_, nullptr);
        }
        return *this;
    }

    ~AlgorithmHandle() {
        reset();
    }

    BCRYPT_ALG_HANDLE get() const {
        return handle_;
    }

private:
    void reset() noexcept {
        if (handle_ != nullptr) {
            BCryptCloseAlgorithmProvider(handle_, 0);
            handle_ = nullptr;
        }
    }

    BCRYPT_ALG_HANDLE handle_ = nullptr;
};

class KeyHandle {
public:
    KeyHandle() = default;
    explicit KeyHandle(BCRYPT_KEY_HANDLE handle) : handle_(handle) {}

    KeyHandle(const KeyHandle&) = delete;
    KeyHandle& operator=(const KeyHandle&) = delete;

    KeyHandle(KeyHandle&& other) noexcept : handle_(std::exchange(other.handle_, nullptr)) {}

    KeyHandle& operator=(KeyHandle&& other) noexcept {
        if (this != &other) {
            reset();
            handle_ = std::exchange(other.handle_, nullptr);
        }
        return *this;
    }

    ~KeyHandle() {
        reset();
    }

    BCRYPT_KEY_HANDLE get() const {
        return handle_;
    }

private:
    void reset() noexcept {
        if (handle_ != nullptr) {
            BCryptDestroyKey(handle_);
            handle_ = nullptr;
        }
    }

    BCRYPT_KEY_HANDLE handle_ = nullptr;
};

AlgorithmHandle open_algorithm(LPCWSTR algorithm, ULONG flags, const char* operation) {
    BCRYPT_ALG_HANDLE handle = nullptr;
    check_status(BCryptOpenAlgorithmProvider(&handle, algorithm, nullptr, flags), operation);
    return AlgorithmHandle(handle);
}

DWORD get_dword_property(BCRYPT_ALG_HANDLE handle, LPCWSTR property, const char* operation) {
    DWORD value = 0;
    DWORD result_size = 0;
    check_status(
        BCryptGetProperty(handle,
                          property,
                          reinterpret_cast<PUCHAR>(&value),
                          sizeof(value),
                          &result_size,
                          0),
        operation);
    return value;
}

PUCHAR mutable_bytes(std::vector<Byte>& bytes) {
    return bytes.empty() ? nullptr : bytes.data();
}

PUCHAR readonly_bytes(const std::vector<Byte>& bytes) {
    return bytes.empty() ? nullptr : const_cast<PUCHAR>(bytes.data());
}

std::vector<Byte> random_bytes(std::size_t size) {
    std::vector<Byte> bytes(size);
    check_status(BCryptGenRandom(nullptr,
                                 mutable_bytes(bytes),
                                 checked_ulong(bytes.size(), "random byte count"),
                                 BCRYPT_USE_SYSTEM_PREFERRED_RNG),
                 "BCryptGenRandom");
    return bytes;
}

std::vector<Byte> derive_key(std::string_view passphrase, const std::vector<Byte>& salt, std::uint64_t iterations) {
    if (passphrase.empty()) {
        throw std::invalid_argument("face database passphrase must not be empty");
    }

    auto sha256 = open_algorithm(BCRYPT_SHA256_ALGORITHM, BCRYPT_ALG_HANDLE_HMAC_FLAG, "BCryptOpenAlgorithmProvider(SHA256)");
    std::vector<Byte> key(kKeySize);
    check_status(BCryptDeriveKeyPBKDF2(
                     sha256.get(),
                     reinterpret_cast<PUCHAR>(const_cast<char*>(passphrase.data())),
                     checked_ulong(passphrase.size(), "passphrase"),
                     readonly_bytes(salt),
                     checked_ulong(salt.size(), "salt"),
                     iterations,
                     mutable_bytes(key),
                     checked_ulong(key.size(), "derived key"),
                     0),
                 "BCryptDeriveKeyPBKDF2");
    return key;
}

struct AesKey {
    AlgorithmHandle algorithm;
    std::vector<Byte> key_object;
    KeyHandle key;
};

AesKey make_aes_gcm_key(const std::vector<Byte>& key_bytes) {
    auto aes = open_algorithm(BCRYPT_AES_ALGORITHM, 0, "BCryptOpenAlgorithmProvider(AES)");
    check_status(BCryptSetProperty(aes.get(),
                                   BCRYPT_CHAINING_MODE,
                                   reinterpret_cast<PUCHAR>(const_cast<wchar_t*>(BCRYPT_CHAIN_MODE_GCM)),
                                   static_cast<ULONG>(sizeof(BCRYPT_CHAIN_MODE_GCM)),
                                   0),
                 "BCryptSetProperty(AES-GCM)");

    const DWORD object_length =
        get_dword_property(aes.get(), BCRYPT_OBJECT_LENGTH, "BCryptGetProperty(AES object length)");
    std::vector<Byte> key_object(object_length);

    BCRYPT_KEY_HANDLE key_handle = nullptr;
    check_status(BCryptGenerateSymmetricKey(aes.get(),
                                            &key_handle,
                                            mutable_bytes(key_object),
                                            checked_ulong(key_object.size(), "AES key object"),
                                            readonly_bytes(key_bytes),
                                            checked_ulong(key_bytes.size(), "AES key"),
                                            0),
                 "BCryptGenerateSymmetricKey(AES)");

    return {std::move(aes), std::move(key_object), KeyHandle(key_handle)};
}

BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO auth_info_for(std::vector<Byte>& tag, const std::vector<Byte>& nonce) {
    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO auth_info;
    BCRYPT_INIT_AUTH_MODE_INFO(auth_info);
    auth_info.pbNonce = readonly_bytes(nonce);
    auth_info.cbNonce = checked_ulong(nonce.size(), "nonce");
    auth_info.pbTag = mutable_bytes(tag);
    auth_info.cbTag = checked_ulong(tag.size(), "tag");
    return auth_info;
}

std::pair<std::vector<Byte>, std::vector<Byte>> aes_gcm_encrypt(const std::vector<Byte>& key,
                                                                const std::vector<Byte>& nonce,
                                                                const std::vector<Byte>& plaintext) {
    auto aes_key = make_aes_gcm_key(key);
    std::vector<Byte> ciphertext(plaintext.size());
    std::vector<Byte> tag(kTagSize);
    auto auth_info = auth_info_for(tag, nonce);

    DWORD result_size = 0;
    check_status(BCryptEncrypt(aes_key.key.get(),
                               readonly_bytes(plaintext),
                               checked_ulong(plaintext.size(), "plaintext"),
                               &auth_info,
                               nullptr,
                               0,
                               mutable_bytes(ciphertext),
                               checked_ulong(ciphertext.size(), "ciphertext"),
                               &result_size,
                               0),
                 "BCryptEncrypt(AES-GCM)");
    if (result_size != ciphertext.size()) {
        throw std::runtime_error("BCryptEncrypt(AES-GCM) produced an unexpected ciphertext size");
    }
    return {std::move(ciphertext), std::move(tag)};
}

std::vector<Byte> aes_gcm_decrypt(const std::vector<Byte>& key,
                                  const std::vector<Byte>& nonce,
                                  const std::vector<Byte>& ciphertext,
                                  std::vector<Byte> tag) {
    auto aes_key = make_aes_gcm_key(key);
    std::vector<Byte> plaintext(ciphertext.size());
    auto auth_info = auth_info_for(tag, nonce);

    DWORD result_size = 0;
    const NTSTATUS status = BCryptDecrypt(aes_key.key.get(),
                                          readonly_bytes(ciphertext),
                                          checked_ulong(ciphertext.size(), "ciphertext"),
                                          &auth_info,
                                          nullptr,
                                          0,
                                          mutable_bytes(plaintext),
                                          checked_ulong(plaintext.size(), "plaintext"),
                                          &result_size,
                                          0);
    if (status < 0) {
        throw std::runtime_error("AES-GCM authentication failed for encrypted face database");
    }
    if (result_size != plaintext.size()) {
        throw std::runtime_error("BCryptDecrypt(AES-GCM) produced an unexpected plaintext size");
    }
    return plaintext;
}

#else

std::vector<Byte> random_bytes(std::size_t) {
    throw std::runtime_error("encrypted face database is only supported on Windows BCrypt");
}

std::vector<Byte> derive_key(std::string_view, const std::vector<Byte>&, std::uint64_t) {
    throw std::runtime_error("encrypted face database is only supported on Windows BCrypt");
}

std::pair<std::vector<Byte>, std::vector<Byte>> aes_gcm_encrypt(const std::vector<Byte>&,
                                                                const std::vector<Byte>&,
                                                                const std::vector<Byte>&) {
    throw std::runtime_error("encrypted face database is only supported on Windows BCrypt");
}

std::vector<Byte> aes_gcm_decrypt(const std::vector<Byte>&,
                                  const std::vector<Byte>&,
                                  const std::vector<Byte>&,
                                  std::vector<Byte>) {
    throw std::runtime_error("encrypted face database is only supported on Windows BCrypt");
}

#endif

std::string base64_encode(const std::vector<Byte>& bytes) {
    static constexpr char kAlphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::string encoded;
    encoded.reserve(((bytes.size() + 2) / 3) * 4);

    for (std::size_t index = 0; index < bytes.size(); index += 3) {
        const std::uint32_t b0 = bytes[index];
        const std::uint32_t b1 = index + 1 < bytes.size() ? bytes[index + 1] : 0;
        const std::uint32_t b2 = index + 2 < bytes.size() ? bytes[index + 2] : 0;
        const std::uint32_t triple = (b0 << 16) | (b1 << 8) | b2;

        encoded.push_back(kAlphabet[(triple >> 18) & 0x3F]);
        encoded.push_back(kAlphabet[(triple >> 12) & 0x3F]);
        encoded.push_back(index + 1 < bytes.size() ? kAlphabet[(triple >> 6) & 0x3F] : '=');
        encoded.push_back(index + 2 < bytes.size() ? kAlphabet[triple & 0x3F] : '=');
    }

    return encoded;
}

int base64_value(char ch) {
    if (ch >= 'A' && ch <= 'Z') {
        return ch - 'A';
    }
    if (ch >= 'a' && ch <= 'z') {
        return ch - 'a' + 26;
    }
    if (ch >= '0' && ch <= '9') {
        return ch - '0' + 52;
    }
    if (ch == '+') {
        return 62;
    }
    if (ch == '/') {
        return 63;
    }
    return -1;
}

std::vector<Byte> base64_decode(std::string_view encoded, const char* field_name) {
    if (encoded.size() % 4 != 0) {
        throw std::runtime_error(std::string(field_name) + " is not valid base64");
    }

    std::vector<Byte> decoded;
    decoded.reserve((encoded.size() / 4) * 3);

    for (std::size_t index = 0; index < encoded.size(); index += 4) {
        const bool last_group = index + 4 == encoded.size();
        const char c0 = encoded[index];
        const char c1 = encoded[index + 1];
        const char c2 = encoded[index + 2];
        const char c3 = encoded[index + 3];
        if (c0 == '=' || c1 == '=' || (c2 == '=' && c3 != '=') || (!last_group && (c2 == '=' || c3 == '='))) {
            throw std::runtime_error(std::string(field_name) + " is not valid base64");
        }

        const int v0 = base64_value(c0);
        const int v1 = base64_value(c1);
        const int v2 = c2 == '=' ? 0 : base64_value(c2);
        const int v3 = c3 == '=' ? 0 : base64_value(c3);
        if (v0 < 0 || v1 < 0 || v2 < 0 || v3 < 0) {
            throw std::runtime_error(std::string(field_name) + " is not valid base64");
        }

        const std::uint32_t triple =
            (static_cast<std::uint32_t>(v0) << 18) |
            (static_cast<std::uint32_t>(v1) << 12) |
            (static_cast<std::uint32_t>(v2) << 6) |
            static_cast<std::uint32_t>(v3);

        decoded.push_back(static_cast<Byte>((triple >> 16) & 0xFF));
        if (c2 != '=') {
            decoded.push_back(static_cast<Byte>((triple >> 8) & 0xFF));
        }
        if (c3 != '=') {
            decoded.push_back(static_cast<Byte>(triple & 0xFF));
        }
    }

    return decoded;
}

std::string required_string_field(const nlohmann::json& json, const char* name) {
    if (!json.contains(name) || !json.at(name).is_string()) {
        throw std::runtime_error(std::string("encrypted face database envelope field must be a string: ") + name);
    }
    return json.at(name).get<std::string>();
}

int required_version(const nlohmann::json& json) {
    if (!json.contains("version") || !json.at("version").is_number_integer()) {
        throw std::runtime_error("encrypted face database envelope version must be an integer");
    }
    return json.at("version").get<int>();
}

std::uint64_t required_iterations(const nlohmann::json& json) {
    if (!json.contains("iterations") || !json.at("iterations").is_number_unsigned()) {
        throw std::runtime_error("encrypted face database envelope iterations must be a positive integer");
    }
    const auto iterations = json.at("iterations").get<std::uint64_t>();
    if (iterations == 0) {
        throw std::runtime_error("encrypted face database envelope iterations must be positive");
    }
    return iterations;
}

std::vector<Byte> decode_fixed_size_field(const nlohmann::json& json, const char* name, std::size_t expected_size) {
    const std::string encoded = required_string_field(json, name);
    std::vector<Byte> decoded = base64_decode(encoded, name);
    if (decoded.size() != expected_size) {
        throw std::runtime_error(std::string("encrypted face database envelope field has invalid size: ") + name);
    }
    return decoded;
}

std::vector<Byte> decode_ciphertext_field(const nlohmann::json& json) {
    const std::string encoded = required_string_field(json, "ciphertext");
    return base64_decode(encoded, "ciphertext");
}

std::vector<Byte> bytes_from_string(std::string_view text) {
    return {text.begin(), text.end()};
}

std::string string_from_bytes(const std::vector<Byte>& bytes) {
    return std::string(bytes.begin(), bytes.end());
}

}  // namespace

std::string encrypt_json(std::string_view plaintext_json, std::string_view passphrase) {
    if (passphrase.empty()) {
        throw std::invalid_argument("face database passphrase must not be empty");
    }

    const std::vector<Byte> salt = random_bytes(kSaltSize);
    const std::vector<Byte> nonce = random_bytes(kNonceSize);
    const std::vector<Byte> key = derive_key(passphrase, salt, kPbkdf2Iterations);
    const auto [ciphertext, tag] = aes_gcm_encrypt(key, nonce, bytes_from_string(plaintext_json));

    nlohmann::json envelope;
    envelope["format"] = kEnvelopeFormat;
    envelope["version"] = kEnvelopeVersion;
    envelope["kdf"] = kKdfName;
    envelope["iterations"] = kPbkdf2Iterations;
    envelope["salt"] = base64_encode(salt);
    envelope["nonce"] = base64_encode(nonce);
    envelope["ciphertext"] = base64_encode(ciphertext);
    envelope["tag"] = base64_encode(tag);
    return envelope.dump(2) + '\n';
}

std::string decrypt_json(std::string_view envelope_json, std::string_view passphrase) {
    if (passphrase.empty()) {
        throw std::invalid_argument("face database passphrase must not be empty");
    }

    nlohmann::json envelope;
    try {
        envelope = nlohmann::json::parse(envelope_json);
    } catch (const nlohmann::json::exception& ex) {
        throw std::runtime_error(std::string("invalid encrypted face database envelope: ") + ex.what());
    }

    if (!envelope.is_object()) {
        throw std::runtime_error("invalid encrypted face database envelope: root must be an object");
    }
    if (required_string_field(envelope, "format") != kEnvelopeFormat) {
        throw std::runtime_error("invalid encrypted face database envelope format");
    }
    if (required_version(envelope) != kEnvelopeVersion) {
        throw std::runtime_error("unsupported encrypted face database envelope version");
    }
    if (required_string_field(envelope, "kdf") != kKdfName) {
        throw std::runtime_error("unsupported encrypted face database KDF");
    }

    const std::uint64_t iterations = required_iterations(envelope);
    const std::vector<Byte> salt = decode_fixed_size_field(envelope, "salt", kSaltSize);
    const std::vector<Byte> nonce = decode_fixed_size_field(envelope, "nonce", kNonceSize);
    const std::vector<Byte> ciphertext = decode_ciphertext_field(envelope);
    std::vector<Byte> tag = decode_fixed_size_field(envelope, "tag", kTagSize);

    const std::vector<Byte> key = derive_key(passphrase, salt, iterations);
    return string_from_bytes(aes_gcm_decrypt(key, nonce, ciphertext, std::move(tag)));
}

}  // namespace ccamera::database_crypto

