#include "web/WebStatus.hpp"

#include <nlohmann/json.hpp>

namespace ccamera {

std::string serialize_web_status(const WebStatusSnapshot& snapshot) {
    nlohmann::json json;
    json["running"] = snapshot.running;
    json["fps"] = snapshot.fps;
    json["status"] = snapshot.status;
    json["database_path"] = snapshot.database_path;
    json["faces"] = nlohmann::json::array();
    json["people"] = nlohmann::json::array();

    for (const auto& result : snapshot.results) {
        json["faces"].push_back({
            {"name", result.label},
            {"similarity", result.similarity},
            {"known", result.known},
            {"sequence", result.frame_sequence},
        });
    }

    for (const auto& person : snapshot.database.people) {
        json["people"].push_back({
            {"name", person.name},
            {"templates", person.templates.size()},
        });
    }

    return json.dump();
}

}  // namespace ccamera
