#include "app/CommandLoop.hpp"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <iostream>
#include <sstream>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace ccamera {
namespace {

constexpr auto kInputPollInterval = std::chrono::milliseconds(20);

void strip_trailing_carriage_return(std::string& line) {
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }
}

}  // namespace

CommandLoop::CommandLoop() = default;

CommandLoop::~CommandLoop() {
    stop();
}

void CommandLoop::start() {
    if (running_.exchange(true)) {
        return;
    }
    thread_ = std::thread(&CommandLoop::run, this);
}

void CommandLoop::stop() {
    running_ = false;
    if (thread_.joinable()) {
        thread_.join();
    }
}

CommandState CommandLoop::consume() {
    std::lock_guard<std::mutex> lock(mutex_);
    CommandState copy = state_;
    state_ = {};
    return copy;
}

std::optional<Command> parse_command_line(const std::string& line) {
    std::istringstream stream(line);
    std::string command;
    stream >> command;

    if (command == "enroll") {
        std::string name;
        stream >> name;
        if (!name.empty()) {
            return Command{CommandType::Enroll, name};
        }
    } else if (command == "delete") {
        std::string name;
        stream >> name;
        if (!name.empty()) {
            return Command{CommandType::Delete, name};
        }
    } else if (command == "list") {
        return Command{CommandType::List, {}};
    } else if (command == "quit") {
        return Command{CommandType::Quit, {}};
    }

    return std::nullopt;
}

void append_command(CommandState& state, const Command& command) {
    state.commands.push_back(command);
    switch (command.type) {
        case CommandType::Enroll:
            state.enroll_name = command.name;
            break;
        case CommandType::Delete:
            state.delete_name = command.name;
            break;
        case CommandType::List:
            state.list_requested = true;
            break;
        case CommandType::Quit:
            state.quit_requested = true;
            break;
    }
}

void CommandLoop::record_line(const std::string& line) {
    const auto command = parse_command_line(line);
    if (!command.has_value()) {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    append_command(state_, *command);
    if (command->type == CommandType::Quit) {
        running_ = false;
    }
}

void CommandLoop::run() {
    std::cout << "Commands: enroll <name>, delete <name>, list, quit\n";

#ifdef _WIN32
    const HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
    DWORD console_mode = 0;
    if (input != INVALID_HANDLE_VALUE && GetConsoleMode(input, &console_mode)) {
        std::string line;
        while (running_) {
            DWORD event_count = 0;
            if (!GetNumberOfConsoleInputEvents(input, &event_count)) {
                running_ = false;
                break;
            }
            if (event_count == 0) {
                std::this_thread::sleep_for(kInputPollInterval);
                continue;
            }

            INPUT_RECORD record{};
            DWORD read_count = 0;
            if (!ReadConsoleInputA(input, &record, 1, &read_count) || read_count == 0) {
                running_ = false;
                break;
            }
            if (record.EventType != KEY_EVENT || !record.Event.KeyEvent.bKeyDown) {
                continue;
            }

            const char ch = record.Event.KeyEvent.uChar.AsciiChar;
            if (ch == '\r' || ch == '\n') {
                std::cout << '\n';
                record_line(line);
                line.clear();
            } else if (ch == '\b') {
                if (!line.empty()) {
                    line.pop_back();
                    std::cout << "\b \b";
                }
            } else if (ch >= ' ') {
                line.push_back(ch);
                std::cout << ch;
            }
        }
        return;
    }

    const DWORD file_type = input == INVALID_HANDLE_VALUE ? FILE_TYPE_UNKNOWN : GetFileType(input);
    if (file_type == FILE_TYPE_PIPE) {
        std::string pending;
        while (running_) {
            DWORD available = 0;
            if (!PeekNamedPipe(input, nullptr, 0, nullptr, &available, nullptr)) {
                running_ = false;
                break;
            }
            if (available == 0) {
                std::this_thread::sleep_for(kInputPollInterval);
                continue;
            }

            char buffer[4096];
            const DWORD to_read = std::min<DWORD>(available, sizeof(buffer));
            DWORD read_count = 0;
            if (!ReadFile(input, buffer, to_read, &read_count, nullptr) || read_count == 0) {
                running_ = false;
                break;
            }

            pending.append(buffer, buffer + read_count);
            std::size_t newline = pending.find('\n');
            while (newline != std::string::npos) {
                std::string line = pending.substr(0, newline);
                strip_trailing_carriage_return(line);
                record_line(line);
                pending.erase(0, newline + 1);
                newline = pending.find('\n');
            }
        }
        if (!pending.empty()) {
            strip_trailing_carriage_return(pending);
            record_line(pending);
        }
        return;
    }
#endif

    std::string line;
    while (running_ && std::getline(std::cin, line)) {
        record_line(line);
    }
    running_ = false;
}

}  // namespace ccamera
