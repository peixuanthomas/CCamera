#include "face/FaceDatabase.hpp"
#include "face/DatabaseCrypto.hpp"

#include <nlohmann/json.hpp>

#include <cassert>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr const char* kPassphrase = "correct horse battery staple";
constexpr const char* kWrongPassphrase = "wrong horse battery staple";

std::filesystem::path unique_test_path() {
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    return std::filesystem::temp_directory_path() /
           ("ccamera_face_db_encrypted_test_" + std::to_string(stamp) + ".json");
}

std::string read_file(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    assert(in);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

void write_file(const std::filesystem::path& path, const std::string& contents) {
    std::ofstream out(path, std::ios::binary);
    assert(out);
    out << contents;
}

template <typename Callable>
void expect_runtime_error(Callable&& callable) {
    bool rejected = false;
    try {
        callable();
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    assert(rejected);
}

void add_alice(ccamera::FaceDatabase& db) {
    db.upsert_person("Alice", {{1.0f, 0.0f, 0.0f}});
    db.upsert_person("Alice", {{0.9f, 0.1f, 0.0f}});
}

void assert_alice_snapshot(const ccamera::FaceDatabaseSnapshot& snapshot) {
    assert(snapshot.people.size() == 1);
    assert(snapshot.people[0].name == "Alice");
    assert(snapshot.people[0].templates.size() == 2);
    assert(snapshot.embedding_dimension == 3);
}

void assert_cached_snapshot(const ccamera::FaceDatabaseSnapshot& snapshot) {
    assert(snapshot.people.size() == 1);
    assert(snapshot.people[0].name == "Cached");
    assert(snapshot.people[0].templates.size() == 1);
    assert(snapshot.embedding_dimension == 3);
}

std::string tamper_base64_field(const std::string& envelope_text, const std::string& field) {
    nlohmann::json envelope = nlohmann::json::parse(envelope_text);
    std::string value = envelope.at(field).get<std::string>();
    bool changed = false;
    for (char& ch : value) {
        if (ch != '=') {
            ch = ch == 'A' ? 'B' : 'A';
            changed = true;
            break;
        }
    }
    assert(changed);
    envelope[field] = value;
    return envelope.dump(2) + '\n';
}

void test_loading_missing_database_returns_empty_snapshot(const std::filesystem::path& path) {
    std::filesystem::remove(path);

    ccamera::FaceDatabase db(path.string(), "");
    db.load_or_create();

    const auto snapshot = db.snapshot();
    assert(snapshot.people.empty());
    assert(snapshot.embedding_dimension == 0);
}

std::string test_save_encrypts_file_and_roundtrips(const std::filesystem::path& path) {
    std::filesystem::remove(path);

    ccamera::FaceDatabase db(path.string(), kPassphrase);
    db.load_or_create();
    add_alice(db);
    db.save();

    const std::string file_contents = read_file(path);
    assert(file_contents.find("Alice") == std::string::npos);
    assert(file_contents.find("\"templates\"") == std::string::npos);
    assert(file_contents.find("embedding_dimension") == std::string::npos);
    assert(file_contents.find("[1.0") == std::string::npos);

    const nlohmann::json envelope = nlohmann::json::parse(file_contents);
    assert(envelope.at("format").get<std::string>() == ccamera::database_crypto::kEnvelopeFormat);
    assert(envelope.at("version").get<int>() == ccamera::database_crypto::kEnvelopeVersion);
    assert(envelope.at("kdf").get<std::string>() == ccamera::database_crypto::kKdfName);
    assert(envelope.at("iterations").get<std::uint64_t>() > 0);
    assert(envelope.at("salt").is_string());
    assert(envelope.at("nonce").is_string());
    assert(envelope.at("ciphertext").is_string());
    assert(envelope.at("tag").is_string());

    ccamera::FaceDatabase loaded(path.string(), kPassphrase);
    loaded.load_or_create();
    assert_alice_snapshot(loaded.snapshot());

    return file_contents;
}

void test_wrong_passphrase_fails_and_preserves_snapshot(const std::filesystem::path& path) {
    ccamera::FaceDatabase wrong(path.string(), kWrongPassphrase);
    wrong.upsert_person("Cached", {{0.0f, 1.0f, 0.0f}});

    expect_runtime_error([&] { wrong.load_or_create(); });
    assert_cached_snapshot(wrong.snapshot());
}

void test_missing_passphrase_fails_for_save_and_encrypted_load(const std::filesystem::path& encrypted_path,
                                                               const std::filesystem::path& missing_save_path) {
    std::filesystem::remove(missing_save_path);

    ccamera::FaceDatabase no_pass_save(missing_save_path.string(), "");
    no_pass_save.load_or_create();
    no_pass_save.upsert_person("Alice", {{1.0f, 0.0f, 0.0f}});
    expect_runtime_error([&] { no_pass_save.save(); });
    assert(!std::filesystem::exists(missing_save_path));

    ccamera::FaceDatabase no_pass_load(encrypted_path.string(), "");
    no_pass_load.upsert_person("Cached", {{0.0f, 1.0f, 0.0f}});
    expect_runtime_error([&] { no_pass_load.load_or_create(); });
    assert_cached_snapshot(no_pass_load.snapshot());
}

void test_tampered_envelope_fields_fail_and_preserve_snapshot(const std::filesystem::path& path,
                                                             const std::string& valid_envelope) {
    ccamera::FaceDatabase loaded(path.string(), kPassphrase);
    loaded.load_or_create();
    assert_alice_snapshot(loaded.snapshot());

    for (const std::string field : {"ciphertext", "tag", "salt", "nonce"}) {
        write_file(path, tamper_base64_field(valid_envelope, field));
        expect_runtime_error([&] { loaded.load_or_create(); });
        assert_alice_snapshot(loaded.snapshot());
    }

    write_file(path, valid_envelope);
}

void test_old_plaintext_json_is_rejected_and_preserves_snapshot(const std::filesystem::path& path) {
    ccamera::FaceDatabase loaded(path.string(), kPassphrase);
    loaded.load_or_create();
    assert_alice_snapshot(loaded.snapshot());

    write_file(
        path,
        R"({"embedding_dimension":3,"people":[{"id":"person-1","name":"Alice","templates":[[1.0,0.0,0.0]]}]})");

    expect_runtime_error([&] { loaded.load_or_create(); });
    assert_alice_snapshot(loaded.snapshot());
}

}  // namespace

int main() {
    const auto path = unique_test_path();
    const std::string valid_envelope = test_save_encrypts_file_and_roundtrips(path);

    test_loading_missing_database_returns_empty_snapshot(unique_test_path());
    test_wrong_passphrase_fails_and_preserves_snapshot(path);
    test_missing_passphrase_fails_for_save_and_encrypted_load(path, unique_test_path());
    test_tampered_envelope_fields_fail_and_preserve_snapshot(path, valid_envelope);
    test_old_plaintext_json_is_rejected_and_preserves_snapshot(path);

    std::filesystem::remove(path);
    return 0;
}
