#pragma once

#include "app/CommandLoop.hpp"

#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace ccamera {

struct WebCommandParseResult {
    bool ok = false;
    int status_code = 400;
    std::string error;
    std::optional<Command> command;
};

WebCommandParseResult parse_web_command_json(const std::string& body);

class WebCommandQueue {
public:
    WebCommandParseResult push_json(const std::string& body);
    void push(Command command);
    std::vector<Command> consume();

private:
    std::mutex mutex_;
    std::vector<Command> commands_;
};

}  // namespace ccamera
