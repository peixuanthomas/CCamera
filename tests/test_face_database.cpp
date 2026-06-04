#include "face/FaceDatabase.hpp"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

void expect_load_rejects(const std::filesystem::path& path, const std::string& json) {
    {
        std::ofstream out(path);
        out << json;
    }

    ccamera::FaceDatabase db(path.string());
    bool rejected = false;
    try {
        db.load_or_create();
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    assert(rejected);
    std::filesystem::remove(path);
}

void expect_reload_rejects_and_preserves(ccamera::FaceDatabase& db,
                                         const std::filesystem::path& path,
                                         const std::string& json) {
    {
        std::ofstream out(path);
        out << json;
    }

    bool rejected = false;
    try {
        db.load_or_create();
    } catch (const std::runtime_error&) {
        rejected = true;
    }
    assert(rejected);

    const auto preserved_snapshot = db.snapshot();
    assert(preserved_snapshot.people.size() == 1);
    assert(preserved_snapshot.people[0].name == "Alice");
    assert(preserved_snapshot.people[0].templates.size() == 2);
    assert(preserved_snapshot.embedding_dimension == 3);
}

}  // namespace

int main() {
    const auto path = std::filesystem::temp_directory_path() / "ccamera_face_db_test.json";
    std::filesystem::remove(path);

    ccamera::FaceDatabase db(path.string());
    db.load_or_create();
    db.upsert_person("Alice", {{1.0f, 0.0f, 0.0f}});
    db.upsert_person("Alice", {{0.9f, 0.1f, 0.0f}});
    db.save();

    ccamera::FaceDatabase loaded(path.string());
    loaded.load_or_create();
    const auto snapshot = loaded.snapshot();

    assert(snapshot.people.size() == 1);
    assert(snapshot.people[0].name == "Alice");
    assert(snapshot.people[0].templates.size() == 2);
    assert(snapshot.embedding_dimension == 3);

    expect_reload_rejects_and_preserves(
        loaded,
        path,
        R"({"embedding_dimension":3,"people":[{"id":"person-1","name":"Alice","templates":[[0.0,0.0,0.0]]}]})");
    expect_reload_rejects_and_preserves(
        loaded,
        path,
        R"({"embedding_dimension":3,"people":{}})");
    expect_reload_rejects_and_preserves(
        loaded,
        path,
        R"({"embedding_dimension":3,"people":[{"id":"person-1","name":"Alice","templates":[]}]})");

    loaded.delete_person("Alice");
    assert(loaded.snapshot().people.empty());

    std::filesystem::remove(path);

    const auto malformed_path = std::filesystem::temp_directory_path() / "ccamera_face_db_malformed_test.json";
    std::filesystem::remove(malformed_path);

    expect_load_rejects(
        malformed_path,
        R"({"embedding_dimension":3,"people":[{"id":"person-1","name":"Alice","templates":[[1.0,0.0]]}]})");
    expect_load_rejects(
        malformed_path,
        R"({"embedding_dimension":3,"people":[{"id":"person-1","name":"Alice","templates":[[0.0,0.0,0.0]]}]})");

    return 0;
}
