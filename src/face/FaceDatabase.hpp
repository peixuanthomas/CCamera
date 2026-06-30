#pragma once

#include "core/Types.hpp"

#include <mutex>
#include <string>
#include <vector>

namespace ccamera {

struct PersonRecord {
    std::string id;
    std::string name;
    std::vector<std::vector<float>> templates;
};

struct FaceDatabaseSnapshot {
    int embedding_dimension = 0;
    std::vector<PersonRecord> people;
};

class FaceDatabase {
public:
    explicit FaceDatabase(std::string path);
    FaceDatabase(std::string path, std::string passphrase);

    void load_or_create();
    void save() const;
    void upsert_person(const std::string& name, const FaceEmbedding& embedding);
    bool delete_person(const std::string& name);
    FaceDatabaseSnapshot snapshot() const;

private:
    std::string path_;
    std::string passphrase_;
    mutable std::mutex mutex_;
    FaceDatabaseSnapshot data_;
};

}  // namespace ccamera
