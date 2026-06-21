#include "web/WebCommandQueue.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string>
#include <utility>

namespace ccamera {
namespace {

std::string trim(std::string value) {
    const auto not_space = [](unsigned char ch) { return !std::isspace(ch); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
    return value;
}

WebCommandParseResult success(Command command) {
    WebCommandParseResult result;
    result.ok = true;
    result.status_code = 200;
    result.command = std::move(command);
    return result;
}

WebCommandParseResult failure(std::string error) {
    WebCommandParseResult result;
    result.ok = false;
    result.status_code = 400;
    result.error = std::move(error);
    return result;
}

}  // namespace

WebCommandParseResult parse_web_command_json(const std::string& body) {
    nlohmann::json json;
    try {
        json = nlohmann::json::parse(body);
    } catch (const nlohmann::json::exception&) {
        return failure("invalid JSON");
    }

    if (!json.is_object() || !json.contains("type") || !json.at("type").is_string()) {
        return failure("type is required");
    }

    const std::string type = json.at("type").get<std::string>();
    if (type == "list") {
        return success({CommandType::List, {}});
    }
    if (type == "quit") {
        return success({CommandType::Quit, {}});
    }
    if (type != "enroll" && type != "delete") {
        return failure("unsupported command type");
    }
    if (!json.contains("name") || !json.at("name").is_string()) {
        return failure("name is required");
    }

    const std::string name = trim(json.at("name").get<std::string>());
    if (name.empty()) {
        return failure("name is required");
    }

    return success({type == "enroll" ? CommandType::Enroll : CommandType::Delete, name});
}

WebCommandParseResult WebCommandQueue::push_json(const std::string& body) {
    WebCommandParseResult result = parse_web_command_json(body);
    if (result.ok && result.command.has_value()) {
        push(*result.command);
    }
    return result;
}

void WebCommandQueue::push(Command command) {
    std::lock_guard<std::mutex> lock(mutex_);
    commands_.push_back(std::move(command));
}

std::vector<Command> WebCommandQueue::consume() {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<Command> copy;
    copy.swap(commands_);
    return copy;
}

}  // namespace ccamera
