#include "web/WebStatus.hpp"

#include <cassert>
#include <nlohmann/json.hpp>

int main() {
    ccamera::FaceDatabaseSnapshot database;
    database.embedding_dimension = 3;
    database.people.push_back({"alice", "Alice", {{1.0f, 0.0f, 0.0f}, {0.9f, 0.1f, 0.0f}}});
    database.people.push_back({"bob", "Bob", {{0.0f, 1.0f, 0.0f}}});

    ccamera::RecognitionResult known;
    known.label = "Alice";
    known.similarity = 0.93f;
    known.known = true;
    known.frame_sequence = 42;

    ccamera::RecognitionResult unknown;
    unknown.label = "Unknown";
    unknown.similarity = 0.0f;
    unknown.known = false;
    unknown.frame_sequence = 42;

    ccamera::WebStatusSnapshot snapshot;
    snapshot.running = true;
    snapshot.fps = 29.4;
    snapshot.status = "Ready";
    snapshot.database_path = "data/face_db.json";
    snapshot.results = {known, unknown};
    snapshot.database = database;

    const auto json = nlohmann::json::parse(ccamera::serialize_web_status(snapshot));
    assert(json.at("running").get<bool>());
    assert(json.at("fps").get<double>() == 29.4);
    assert(json.at("status").get<std::string>() == "Ready");
    assert(json.at("database_path").get<std::string>() == "data/face_db.json");

    assert(json.at("faces").size() == 2);
    assert(json.at("faces")[0].at("name").get<std::string>() == "Alice");
    assert(json.at("faces")[0].at("known").get<bool>());
    assert(json.at("faces")[0].at("similarity").get<double>() > 0.92);
    assert(json.at("faces")[0].at("sequence").get<std::uint64_t>() == 42);
    assert(json.at("faces")[1].at("name").get<std::string>() == "Unknown");
    assert(!json.at("faces")[1].at("known").get<bool>());

    assert(json.at("people").size() == 2);
    assert(json.at("people")[0].at("name").get<std::string>() == "Alice");
    assert(json.at("people")[0].at("templates").get<int>() == 2);
    assert(json.at("people")[1].at("name").get<std::string>() == "Bob");
    assert(json.at("people")[1].at("templates").get<int>() == 1);
    assert(!json.at("people")[0].contains("id"));
    assert(!json.at("people")[0].contains("embeddings"));

    return 0;
}
