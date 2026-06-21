# Enroll Name Input Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the preview-window Enroll/Delete flows show a real name-entry box and accept keyboard input reliably before dispatching commands.

**Architecture:** Keep the existing OpenCV-drawn control panel and `RuntimeControls` state machine. Normalize OpenCV key codes at the input boundary, keep commands as `CommandLoop` command objects, and draw a clear input rectangle with visible typed text when name entry is active.

**Tech Stack:** C++17, OpenCV HighGUI/ImgProc, CTest, CMake.

---

### Task 1: Fix Preview Name Entry

**Files:**
- Modify: `src/ui/RuntimeControls.cpp`
- Modify: `src/ui/PreviewWindow.cpp`
- Test: `tests/test_runtime_controls.cpp`
- Update: `docs/discoveries.md`
- Update: `docs/superpowers/plans/2026-06-21-enroll-name-input.md`

- [x] **Step 1: Write the failing runtime-control regression test**

Add coverage in `tests/test_runtime_controls.cpp` that starts enroll input, sends a printable key code with high bits set, verifies the prompt contains the typed name, then presses Enter and receives an enroll command:

```cpp
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
```

- [x] **Step 2: Run the focused test and verify it fails**

Run:

```powershell
cmake --build build --config Debug --target test_runtime_controls
ctest --test-dir build -C Debug -R test_runtime_controls --output-on-failure
```

Expected before the fix: `test_runtime_controls` fails because the high-bit printable key does not append `B`.

- [x] **Step 3: Implement the minimal input fix and visible input box**

In `src/ui/RuntimeControls.cpp`, normalize printable key codes before checking character support so OpenCV backend-specific high bits do not block normal ASCII input. Keep Enter, Backspace, and Esc handling unchanged.

In `src/ui/PreviewWindow.cpp`, draw the active prompt inside a bordered input rectangle below the buttons. Show an empty field when no characters are typed yet, and keep the enrolled-name list below the field.

- [x] **Step 4: Run focused and full verification**

Run:

```powershell
cmake --build build --config Debug --target test_runtime_controls
ctest --test-dir build -C Debug -R test_runtime_controls --output-on-failure
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

Expected after the fix: focused runtime-control test passes, then the full test suite passes.

- [x] **Step 5: Review and fix findings**

Dispatch a spec-compliance review subagent for this task and fix every reported spec gap. After spec review passes, dispatch a code-quality review subagent and fix every reported issue.

- [x] **Step 6: Update logs and commit**

Append a dated discovery to `docs/discoveries.md`, mark all checkboxes in this plan task complete, stage only files changed for this task, and commit:

```powershell
git add docs/discoveries.md docs/superpowers/plans/2026-06-21-enroll-name-input.md src/ui/RuntimeControls.cpp src/ui/PreviewWindow.cpp tests/test_runtime_controls.cpp
git commit -m "fix: make preview enrollment name input usable"
```
