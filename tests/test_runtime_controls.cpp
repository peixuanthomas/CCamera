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
