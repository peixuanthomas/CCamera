#include "app/CommandLoop.hpp"

#include <cassert>

int main() {
    const auto enroll = ccamera::parse_command_line("enroll Alice extra");
    assert(enroll.has_value());
    assert(enroll->type == ccamera::CommandType::Enroll);
    assert(enroll->name == "Alice");

    const auto empty_enroll = ccamera::parse_command_line("enroll");
    assert(!empty_enroll.has_value());

    ccamera::CommandState state;
    ccamera::append_command(state, {ccamera::CommandType::Delete, "Alice"});
    ccamera::append_command(state, {ccamera::CommandType::Delete, "Bob"});
    ccamera::append_command(state, {ccamera::CommandType::List, {}});
    ccamera::append_command(state, {ccamera::CommandType::Quit, {}});

    assert(state.commands.size() == 4);
    assert(state.commands[0].type == ccamera::CommandType::Delete);
    assert(state.commands[0].name == "Alice");
    assert(state.commands[1].type == ccamera::CommandType::Delete);
    assert(state.commands[1].name == "Bob");
    assert(state.commands[2].type == ccamera::CommandType::List);
    assert(state.commands[3].type == ccamera::CommandType::Quit);

    assert(state.delete_name.has_value());
    assert(*state.delete_name == "Bob");
    assert(state.list_requested);
    assert(state.quit_requested);

    return 0;
}
