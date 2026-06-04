#pragma once

#include <atomic>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace ccamera {

enum class CommandType {
    Enroll,
    Delete,
    List,
    Quit,
};

struct Command {
    CommandType type;
    std::string name;
};

struct CommandState {
    std::optional<std::string> enroll_name;
    std::optional<std::string> delete_name;
    bool list_requested = false;
    bool quit_requested = false;
    std::vector<Command> commands;
};

std::optional<Command> parse_command_line(const std::string& line);
void append_command(CommandState& state, const Command& command);

class CommandLoop {
public:
    CommandLoop();
    ~CommandLoop();

    void start();
    void stop();
    CommandState consume();

private:
    void record_line(const std::string& line);
    void run();

    std::atomic<bool> running_{false};
    std::thread thread_;
    std::mutex mutex_;
    CommandState state_;
};

}  // namespace ccamera
