#include "app/CommandLoop.hpp"
#include "ui/RuntimeControls.hpp"

#include <cassert>

int main() {
    ccamera::RuntimeControls controls;
    const auto layout = controls.layout_for({1280, 720});
    assert(!layout.enroll_button.empty());
    assert(!layout.delete_button.empty());
    assert(!layout.list_button.empty());
    assert(!layout.quit_button.empty());

    auto events = controls.click(layout.enroll_button.x + 4, layout.enroll_button.y + 4, layout);
    assert(events.empty());
    assert(controls.mode() == ccamera::RuntimeControlMode::EnteringEnrollName);

    events = controls.key('A');
    assert(events.empty());
    events = controls.key('l');
    assert(events.empty());
    events = controls.key('\b');
    assert(events.empty());
    events = controls.key('i');
    assert(events.empty());
    events = controls.key('\r');
    assert(events.size() == 1);
    assert(events[0].type == ccamera::CommandType::Enroll);
    assert(events[0].name == "Ai");
    assert(controls.mode() == ccamera::RuntimeControlMode::Idle);

    events = controls.click(layout.enroll_button.x + 4, layout.enroll_button.y + 4, layout);
    assert(events.empty());
    events = controls.key(0x410000 | 'B');
    assert(events.empty());
    assert(controls.input_text() == "B");
    assert(controls.prompt_text().find("B") != std::string::npos);
    events = controls.key('\r');
    assert(events.size() == 1);
    assert(events[0].type == ccamera::CommandType::Enroll);
    assert(events[0].name == "B");
    assert(controls.mode() == ccamera::RuntimeControlMode::Idle);

    events = controls.click(layout.enroll_button.x + 4, layout.enroll_button.y + 4, layout);
    assert(events.empty());
    events = controls.key(0x410000 | 'C');
    assert(events.empty());
    assert(controls.input_text() == "C");
    events = controls.key(0x410000 | '\r');
    assert(events.size() == 1);
    assert(events[0].type == ccamera::CommandType::Enroll);
    assert(events[0].name == "C");
    assert(controls.mode() == ccamera::RuntimeControlMode::Idle);

    events = controls.click(layout.enroll_button.x + 4, layout.enroll_button.y + 4, layout);
    assert(events.empty());
    events = controls.key('D');
    assert(events.empty());
    assert(controls.input_text() == "D");
    events = controls.key(0x410000 | '\b');
    assert(events.empty());
    assert(controls.input_text().empty());
    events = controls.key(27);
    assert(events.empty());
    assert(controls.mode() == ccamera::RuntimeControlMode::Idle);

    events = controls.click(layout.delete_button.x + 4, layout.delete_button.y + 4, layout);
    assert(events.empty());
    events = controls.key('E');
    assert(events.empty());
    assert(controls.input_text() == "E");
    events = controls.key(0x410000 | 27);
    assert(events.empty());
    assert(controls.mode() == ccamera::RuntimeControlMode::Idle);
    assert(controls.input_text().empty());

    events = controls.click(layout.enroll_button.x + 4, layout.enroll_button.y + 4, layout);
    assert(events.empty());
    events = controls.key(0xff51);
    assert(events.empty());
    assert(controls.input_text().empty());
    events = controls.key(27);
    assert(events.empty());
    assert(controls.mode() == ccamera::RuntimeControlMode::Idle);

    events = controls.click(layout.delete_button.x + 4, layout.delete_button.y + 4, layout);
    assert(events.empty());
    assert(controls.mode() == ccamera::RuntimeControlMode::EnteringDeleteName);
    events = controls.key(27);
    assert(events.empty());
    assert(controls.mode() == ccamera::RuntimeControlMode::Idle);

    events = controls.click(layout.list_button.x + 4, layout.list_button.y + 4, layout);
    assert(events.size() == 1);
    assert(events[0].type == ccamera::CommandType::List);

    events = controls.click(layout.quit_button.x + 4, layout.quit_button.y + 4, layout);
    assert(events.size() == 1);
    assert(events[0].type == ccamera::CommandType::Quit);

    return 0;
}
