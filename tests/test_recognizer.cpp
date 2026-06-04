#include "face/Recognizer.hpp"

#include <cassert>
#include <string>
#include <vector>

int main() {
    ccamera::FaceDatabaseSnapshot db;
    db.embedding_dimension = 3;
    db.people.push_back({"person-1", "Alice", {{1.0f, 0.0f, 0.0f}}});
    db.people.push_back({"person-2", "Bob", {{0.0f, 1.0f, 0.0f}}});

    ccamera::Recognizer recognizer(0.8f);

    const auto known = recognizer.match({{0.98f, 0.02f, 0.0f}}, db);
    assert(known.known);
    assert(known.label == "Alice");
    assert(known.similarity > 0.9f);

    const auto unknown = recognizer.match({{0.0f, 0.0f, 1.0f}}, db);
    assert(!unknown.known);
    assert(unknown.label == "Unknown");

    ccamera::FaceDatabaseSnapshot empty_templates;
    empty_templates.embedding_dimension = 3;
    empty_templates.people.push_back({"person-3", "Carol", {}});

    const auto no_template_match = recognizer.match({{1.0f, 0.0f, 0.0f}}, empty_templates);
    assert(!no_template_match.known);
    assert(no_template_match.label == "Unknown");
    assert(no_template_match.similarity == 0.0f);

    return 0;
}
