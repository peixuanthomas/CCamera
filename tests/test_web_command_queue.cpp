#include "web/WebCommandQueue.hpp"

#include <cassert>
#include <string>

int main() {
    const auto enroll = ccamera::parse_web_command_json(R"({"type":"enroll","name":" Alice "})");
    assert(enroll.ok);
    assert(enroll.command.has_value());
    assert(enroll.command->type == ccamera::CommandType::Enroll);
    assert(enroll.command->name == "Alice");

    const auto remove = ccamera::parse_web_command_json(R"({"type":"delete","name":"Bob"})");
    assert(remove.ok);
    assert(remove.command->type == ccamera::CommandType::Delete);
    assert(remove.command->name == "Bob");

    const auto list = ccamera::parse_web_command_json(R"({"type":"list"})");
    assert(list.ok);
    assert(list.command->type == ccamera::CommandType::List);
    assert(list.command->name.empty());

    const auto quit = ccamera::parse_web_command_json(R"({"type":"quit"})");
    assert(quit.ok);
    assert(quit.command->type == ccamera::CommandType::Quit);
    assert(quit.command->name.empty());

    const auto empty_name = ccamera::parse_web_command_json(R"({"type":"enroll","name":"   "})");
    assert(!empty_name.ok);
    assert(empty_name.status_code == 400);
    assert(empty_name.error == "name is required");

    const auto bad_type = ccamera::parse_web_command_json(R"({"type":"rename","name":"Alice"})");
    assert(!bad_type.ok);
    assert(bad_type.status_code == 400);
    assert(bad_type.error == "unsupported command type");

    const auto bad_json = ccamera::parse_web_command_json(R"({"type":)");
    assert(!bad_json.ok);
    assert(bad_json.status_code == 400);
    assert(bad_json.error == "invalid JSON");

    ccamera::WebCommandQueue queue;
    assert(queue.push_json(R"({"type":"enroll","name":"Carol"})").ok);
    assert(!queue.push_json(R"({"type":"delete","name":""})").ok);
    const auto commands = queue.consume();
    assert(commands.size() == 1);
    assert(commands[0].type == ccamera::CommandType::Enroll);
    assert(commands[0].name == "Carol");
    assert(queue.consume().empty());

    return 0;
}
