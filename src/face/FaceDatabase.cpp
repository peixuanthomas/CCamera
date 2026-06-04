#include "face/FaceDatabase.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <utility>

namespace ccamera {
namespace {

std::string make_id(const std::string& name) {
    std::string id;
    id.reserve(name.size());
    for (char ch : name) {
        if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9')) {
            id.push_back(ch);
        } else {
            id.push_back('_');
        }
    }
    return id.empty() ? "person" : id;
}

bool is_zero_embedding(const std::vector<float>& values) {
    for (float value : values) {
        if (std::fabs(value) > 1.0e-12f) {
            return false;
        }
    }
    return true;
}

void validate_template(const std::string& person_name,
                       const std::vector<float>& templ,
                       int embedding_dimension) {
    if (templ.empty()) {
        throw std::runtime_error("empty template for " + person_name);
    }
    if (static_cast<int>(templ.size()) != embedding_dimension) {
        throw std::runtime_error("template dimension mismatch for " + person_name);
    }
    if (is_zero_embedding(templ)) {
        throw std::runtime_error("zero template for " + person_name);
    }
}

std::filesystem::path available_sibling_path(const std::filesystem::path& target, const std::string& suffix) {
    const auto parent = target.parent_path();
    const std::string filename = target.filename().string();
    auto candidate = parent / (filename + suffix);
    if (!std::filesystem::exists(candidate)) {
        return candidate;
    }

    for (int index = 1; index < 1000; ++index) {
        candidate = parent / (filename + suffix + "." + std::to_string(index));
        if (!std::filesystem::exists(candidate)) {
            return candidate;
        }
    }

    throw std::runtime_error("failed to create temporary path for face database: " + target.string());
}

}  // namespace

FaceDatabase::FaceDatabase(std::string path) : path_(std::move(path)) {}

void FaceDatabase::load_or_create() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!std::filesystem::exists(path_)) {
        data_ = {};
        return;
    }

    std::ifstream in(path_);
    if (!in) {
        throw std::runtime_error("failed to open face database for reading: " + path_);
    }

    try {
        nlohmann::json json;
        in >> json;
        if (!json.is_object()) {
            throw std::runtime_error("root must be an object");
        }

        FaceDatabaseSnapshot loaded;
        if (json.contains("embedding_dimension")) {
            if (!json.at("embedding_dimension").is_number_integer()) {
                throw std::runtime_error("embedding_dimension must be an integer");
            }
            loaded.embedding_dimension = json.at("embedding_dimension").get<int>();
        }
        if (loaded.embedding_dimension < 0) {
            throw std::runtime_error("negative embedding dimension");
        }

        if (!json.contains("people") || !json.at("people").is_array()) {
            throw std::runtime_error("people must be an array");
        }

        for (const auto& person_json : json.at("people")) {
            if (!person_json.is_object()) {
                throw std::runtime_error("person must be an object");
            }
            if (!person_json.contains("id") || !person_json.at("id").is_string()) {
                throw std::runtime_error("person id must be a string");
            }
            if (!person_json.contains("name") || !person_json.at("name").is_string()) {
                throw std::runtime_error("person name must be a string");
            }
            if (!person_json.contains("templates") || !person_json.at("templates").is_array()) {
                throw std::runtime_error("templates must be an array");
            }
            if (person_json.at("templates").empty()) {
                throw std::runtime_error("templates must not be empty");
            }

            PersonRecord record;
            record.id = person_json.at("id").get<std::string>();
            record.name = person_json.at("name").get<std::string>();

            if (loaded.embedding_dimension == 0) {
                throw std::runtime_error("missing embedding dimension for " + record.name);
            }

            for (const auto& template_json : person_json.at("templates")) {
                if (!template_json.is_array()) {
                    throw std::runtime_error("template must be an array for " + record.name);
                }
                if (template_json.empty()) {
                    throw std::runtime_error("template must not be empty for " + record.name);
                }

                std::vector<float> templ;
                templ.reserve(template_json.size());
                for (const auto& value_json : template_json) {
                    if (!value_json.is_number()) {
                        throw std::runtime_error("template value must be numeric for " + record.name);
                    }
                    templ.push_back(value_json.get<float>());
                }

                validate_template(record.name, templ, loaded.embedding_dimension);
                record.templates.push_back(std::move(templ));
            }

            loaded.people.push_back(std::move(record));
        }

        data_ = std::move(loaded);
    } catch (const nlohmann::json::exception& ex) {
        throw std::runtime_error("invalid face database " + path_ + ": " + ex.what());
    } catch (const std::runtime_error& ex) {
        throw std::runtime_error("invalid face database " + path_ + ": " + ex.what());
    }
}

void FaceDatabase::save() const {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto parent = std::filesystem::path(path_).parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent);
    }

    nlohmann::json json;
    json["embedding_dimension"] = data_.embedding_dimension;
    json["people"] = nlohmann::json::array();
    for (const auto& person : data_.people) {
        json["people"].push_back({
            {"id", person.id},
            {"name", person.name},
            {"templates", person.templates},
        });
    }

    const auto target = std::filesystem::path(path_);
    const auto temp = available_sibling_path(target, ".tmp");

    std::ofstream out(temp);
    if (!out) {
        throw std::runtime_error("failed to open temporary face database for writing: " + temp.string());
    }
    out << json.dump(2) << '\n';
    out.close();
    if (!out) {
        std::filesystem::remove(temp);
        throw std::runtime_error("failed to write temporary face database: " + temp.string());
    }

    std::error_code error;
    if (!std::filesystem::exists(target)) {
        std::filesystem::rename(temp, target, error);
        if (error) {
            std::filesystem::remove(temp);
            throw std::runtime_error("failed to write face database " + path_ + ": " + error.message());
        }
        return;
    }

    const auto backup = available_sibling_path(target, ".bak");
    std::filesystem::rename(target, backup, error);
    if (error) {
        std::filesystem::remove(temp);
        throw std::runtime_error("failed to back up face database " + path_ + ": " + error.message());
    }

    std::filesystem::rename(temp, target, error);
    if (error) {
        std::error_code rollback_error;
        std::filesystem::rename(backup, target, rollback_error);
        std::filesystem::remove(temp);
        if (rollback_error) {
            throw std::runtime_error("failed to replace face database " + path_ + ": " + error.message() +
                                     "; previous database remains at " + backup.string());
        }
        throw std::runtime_error("failed to replace face database " + path_ + ": " + error.message());
    }

    std::error_code cleanup_error;
    std::filesystem::remove(backup, cleanup_error);
}

void FaceDatabase::upsert_person(const std::string& name, const FaceEmbedding& embedding) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (embedding.values.empty()) {
        throw std::invalid_argument("cannot store empty face embedding");
    }
    if (is_zero_embedding(embedding.values)) {
        throw std::invalid_argument("cannot store zero face embedding");
    }
    if (data_.embedding_dimension == 0) {
        data_.embedding_dimension = static_cast<int>(embedding.values.size());
    }
    if (data_.embedding_dimension != static_cast<int>(embedding.values.size())) {
        throw std::invalid_argument("embedding dimension does not match face database");
    }

    for (auto& person : data_.people) {
        if (person.name == name) {
            person.templates.push_back(embedding.values);
            return;
        }
    }

    data_.people.push_back({make_id(name), name, {embedding.values}});
}

bool FaceDatabase::delete_person(const std::string& name) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto old_size = data_.people.size();
    data_.people.erase(
        std::remove_if(data_.people.begin(), data_.people.end(),
                       [&](const PersonRecord& person) { return person.name == name; }),
        data_.people.end());
    return data_.people.size() != old_size;
}

FaceDatabaseSnapshot FaceDatabase::snapshot() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return data_;
}

}  // namespace ccamera
