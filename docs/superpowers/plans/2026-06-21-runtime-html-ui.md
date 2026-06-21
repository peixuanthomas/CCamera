# Runtime HTML UI Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add a localhost-only HTML runtime dashboard with MJPEG preview, status JSON, and browser-triggered runtime commands while preserving the existing OpenCV preview.

**Architecture:** Build a focused `src/web/` module in small layers: command/status contracts, HTTP route handling, MJPEG encoding, a localhost socket server, and `AppController` integration. The browser never touches the face database directly; commands flow through existing `Command` handling and database persistence.

**Tech Stack:** C++20, CMake, OpenCV core/imgproc/imgcodecs/highgui/videoio/objdetect, nlohmann/json, Winsock on Windows, POSIX sockets on non-Windows, existing executable tests and CTest.

---

## Current Context

- Approved spec: `docs/superpowers/specs/2026-06-21-runtime-html-ui-design.md`.
- Current OpenCV preview control path lives in `src/ui/PreviewWindow.cpp` and `src/ui/RuntimeControls.cpp`.
- Current app command types are in `src/app/CommandLoop.hpp`: `Enroll`, `Delete`, `List`, `Quit`.
- Current frame source is `FrameBuffer::latest()`.
- Current overlay helper is `PreviewWindow::draw_overlay(cv::Mat&, const std::vector<RecognitionResult>&)`.
- Current database snapshot type is `FaceDatabaseSnapshot`, with `people[].name` and `people[].templates`.
- The worktree has unrelated uncommitted changes. Do not revert or include unrelated edits in task commits.

## File Structure

- Create `src/web/WebCommandQueue.hpp` and `src/web/WebCommandQueue.cpp`: parse browser command JSON, validate names, queue valid `Command` values.
- Create `src/web/WebStatus.hpp` and `src/web/WebStatus.cpp`: define browser-facing status snapshots and serialize them to JSON.
- Create `src/web/Http.hpp` and `src/web/Http.cpp`: parse minimal HTTP requests and format HTTP responses.
- Create `src/web/RuntimeWebRouter.hpp` and `src/web/RuntimeWebRouter.cpp`: implement fixed routes `/`, `/api/status`, `/api/commands`, `/stream.mjpg`.
- Create `src/web/MjpegStream.hpp` and `src/web/MjpegStream.cpp`: create fallback frames, draw overlays, encode JPEG, format multipart MJPEG chunks.
- Create `src/web/RuntimeWebServer.hpp` and `src/web/RuntimeWebServer.cpp`: localhost socket accept loop, client dispatch, lifecycle.
- Modify `src/app/AppController.hpp` and `src/app/AppController.cpp`: own and start/stop the web server, expose runtime snapshots, consume web commands.
- Modify `src/app/AppRunCleanup.hpp` and `src/app/AppRunCleanup.cpp`: include web server cleanup in exception-safe shutdown.
- Modify `CMakeLists.txt`: add OpenCV `imgcodecs`, add `src/web/*.cpp`, link required socket library on Windows, add new tests.
- Create tests: `tests/test_web_command_queue.cpp`, `tests/test_web_status.cpp`, `tests/test_http_router.cpp`, `tests/test_mjpeg_stream.cpp`, `tests/test_runtime_web_server.cpp`.
- Modify `README.md`: add browser UI launch notes.
- Modify `docs/discoveries.md`: append a dated discovery after each completed task.

## Task 1: Web Command Queue

**Files:**
- Create: `src/web/WebCommandQueue.hpp`
- Create: `src/web/WebCommandQueue.cpp`
- Create: `tests/test_web_command_queue.cpp`
- Modify: `CMakeLists.txt`
- Modify after verification: `docs/discoveries.md`
- Update: `docs/superpowers/plans/2026-06-21-runtime-html-ui.md`

- [x] **Step 1: Write the failing test**

Add this test file:

```cpp
// tests/test_web_command_queue.cpp
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
```

Add the target to `CMakeLists.txt` inside `if (CCAMERA_BUILD_TESTS)`:

```cmake
    add_executable(test_web_command_queue tests/test_web_command_queue.cpp)
    target_link_libraries(test_web_command_queue PRIVATE ccamera_core)
    add_test(NAME test_web_command_queue COMMAND test_web_command_queue)
```

- [x] **Step 2: Run the test to verify it fails**

Run:

```powershell
cmake --build build --config Debug --target test_web_command_queue
```

Expected: build fails because `web/WebCommandQueue.hpp` does not exist.

- [x] **Step 3: Write the minimal implementation**

Create `src/web/WebCommandQueue.hpp`:

```cpp
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
```

Create `src/web/WebCommandQueue.cpp`:

```cpp
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
```

Add the source to `ccamera_core` in `CMakeLists.txt`:

```cmake
    src/web/WebCommandQueue.cpp
```

- [x] **Step 4: Run the test to verify it passes**

Run:

```powershell
cmake --build build --config Debug --target test_web_command_queue
ctest --test-dir build -C Debug -R test_web_command_queue --output-on-failure
```

Expected: build succeeds and CTest reports `100% tests passed`.

- [x] **Step 5: Update plan and discoveries**

Check off Task 1 steps in this plan. Append one dated line to `docs/discoveries.md`:

```markdown
- 2026-06-21: Runtime HTML UI Task 1 added `WebCommandQueue`, validating browser JSON commands for enroll/delete/list/quit and queuing only valid `Command` values. Focused `test_web_command_queue` passed.
```

- [x] **Step 6: Commit**

Run:

```powershell
git add CMakeLists.txt src/web/WebCommandQueue.hpp src/web/WebCommandQueue.cpp tests/test_web_command_queue.cpp docs/discoveries.md docs/superpowers/plans/2026-06-21-runtime-html-ui.md
git commit -m "feat: add web command queue"
```

## Task 2: Web Status Serialization

**Files:**
- Create: `src/web/WebStatus.hpp`
- Create: `src/web/WebStatus.cpp`
- Create: `tests/test_web_status.cpp`
- Modify: `CMakeLists.txt`
- Modify after verification: `docs/discoveries.md`
- Update: `docs/superpowers/plans/2026-06-21-runtime-html-ui.md`

- [x] **Step 1: Write the failing test**

Create `tests/test_web_status.cpp`:

```cpp
#include "web/WebStatus.hpp"

#include <cassert>
#include <nlohmann/json.hpp>

int main() {
    ccamera::FaceDatabaseSnapshot database;
    database.embedding_dimension = 3;
    database.people.push_back({"alice", "Alice", {{1.0f, 0.0f, 0.0f}, {0.9f, 0.1f, 0.0f}}});
    database.people.push_back({"bob", "Bob", {{0.0f, 1.0f, 0.0f}}});

    ccamera::RecognitionResult known;
    known.label = "Alice";
    known.similarity = 0.93f;
    known.known = true;
    known.frame_sequence = 42;

    ccamera::RecognitionResult unknown;
    unknown.label = "Unknown";
    unknown.similarity = 0.0f;
    unknown.known = false;
    unknown.frame_sequence = 42;

    ccamera::WebStatusSnapshot snapshot;
    snapshot.running = true;
    snapshot.fps = 29.4;
    snapshot.status = "Ready";
    snapshot.database_path = "data/face_db.json";
    snapshot.results = {known, unknown};
    snapshot.database = database;

    const auto json = nlohmann::json::parse(ccamera::serialize_web_status(snapshot));
    assert(json.at("running").get<bool>());
    assert(json.at("fps").get<double>() == 29.4);
    assert(json.at("status").get<std::string>() == "Ready");
    assert(json.at("database_path").get<std::string>() == "data/face_db.json");

    assert(json.at("faces").size() == 2);
    assert(json.at("faces")[0].at("name").get<std::string>() == "Alice");
    assert(json.at("faces")[0].at("known").get<bool>());
    assert(json.at("faces")[0].at("similarity").get<double>() > 0.92);
    assert(json.at("faces")[0].at("sequence").get<std::uint64_t>() == 42);
    assert(json.at("faces")[1].at("name").get<std::string>() == "Unknown");
    assert(!json.at("faces")[1].at("known").get<bool>());

    assert(json.at("people").size() == 2);
    assert(json.at("people")[0].at("name").get<std::string>() == "Alice");
    assert(json.at("people")[0].at("templates").get<int>() == 2);
    assert(json.at("people")[1].at("name").get<std::string>() == "Bob");
    assert(json.at("people")[1].at("templates").get<int>() == 1);
    assert(!json.at("people")[0].contains("id"));
    assert(!json.at("people")[0].contains("embeddings"));

    return 0;
}
```

Add CMake test target:

```cmake
    add_executable(test_web_status tests/test_web_status.cpp)
    target_link_libraries(test_web_status PRIVATE ccamera_core)
    add_test(NAME test_web_status COMMAND test_web_status)
```

- [x] **Step 2: Run the test to verify it fails**

Run:

```powershell
cmake --build build --config Debug --target test_web_status
```

Expected: build fails because `web/WebStatus.hpp` does not exist.

- [x] **Step 3: Write the minimal implementation**

Create `src/web/WebStatus.hpp`:

```cpp
#pragma once

#include "core/Types.hpp"
#include "face/FaceDatabase.hpp"

#include <string>
#include <vector>

namespace ccamera {

struct WebStatusSnapshot {
    bool running = false;
    double fps = 0.0;
    std::string status;
    std::string database_path;
    std::vector<RecognitionResult> results;
    FaceDatabaseSnapshot database;
};

std::string serialize_web_status(const WebStatusSnapshot& snapshot);

}  // namespace ccamera
```

Create `src/web/WebStatus.cpp`:

```cpp
#include "web/WebStatus.hpp"

#include <nlohmann/json.hpp>

namespace ccamera {

std::string serialize_web_status(const WebStatusSnapshot& snapshot) {
    nlohmann::json json;
    json["running"] = snapshot.running;
    json["fps"] = snapshot.fps;
    json["status"] = snapshot.status;
    json["database_path"] = snapshot.database_path;
    json["faces"] = nlohmann::json::array();
    json["people"] = nlohmann::json::array();

    for (const auto& result : snapshot.results) {
        json["faces"].push_back({
            {"name", result.label},
            {"similarity", result.similarity},
            {"known", result.known},
            {"sequence", result.frame_sequence},
        });
    }

    for (const auto& person : snapshot.database.people) {
        json["people"].push_back({
            {"name", person.name},
            {"templates", person.templates.size()},
        });
    }

    return json.dump();
}

}  // namespace ccamera
```

Add the source to `ccamera_core`:

```cmake
    src/web/WebStatus.cpp
```

- [x] **Step 4: Run the test to verify it passes**

Run:

```powershell
cmake --build build --config Debug --target test_web_status
ctest --test-dir build -C Debug -R test_web_status --output-on-failure
```

Expected: build succeeds and CTest reports `100% tests passed`.

- [x] **Step 5: Update plan and discoveries**

Check off Task 2 steps in this plan. Append:

```markdown
- 2026-06-21: Runtime HTML UI Task 2 added `WebStatusSnapshot` JSON serialization for browser status without exposing person ids or raw embeddings. Focused `test_web_status` passed.
```

- [x] **Step 6: Commit**

Run:

```powershell
git add CMakeLists.txt src/web/WebStatus.hpp src/web/WebStatus.cpp tests/test_web_status.cpp docs/discoveries.md docs/superpowers/plans/2026-06-21-runtime-html-ui.md
git commit -m "feat: serialize web runtime status"
```

## Task 3: HTTP Router and Embedded Dashboard

**Files:**
- Create: `src/web/Http.hpp`
- Create: `src/web/Http.cpp`
- Create: `src/web/RuntimeWebRouter.hpp`
- Create: `src/web/RuntimeWebRouter.cpp`
- Create: `tests/test_http_router.cpp`
- Modify: `CMakeLists.txt`
- Modify after verification: `docs/discoveries.md`
- Update: `docs/superpowers/plans/2026-06-21-runtime-html-ui.md`

- [x] **Step 1: Write the failing test**

Create `tests/test_http_router.cpp`:

```cpp
#include "web/RuntimeWebRouter.hpp"

#include <cassert>
#include <string>

int main() {
    ccamera::WebCommandQueue commands;
    ccamera::WebStatusSnapshot status;
    status.running = true;
    status.fps = 12.5;
    status.status = "Ready";
    status.database_path = "data/face_db.json";

    ccamera::RuntimeWebRouter router(
        [&] { return status; },
        [&](const std::string& body) { return commands.push_json(body); },
        [] { return std::vector<unsigned char>{0xff, 0xd8, 0xff, 0xd9}; });

    const auto index = router.handle({"GET", "/", {}, ""});
    assert(index.status_code == 200);
    assert(index.content_type == "text/html; charset=utf-8");
    assert(index.body.find("CCamera") != std::string::npos);
    assert(index.body.find("/stream.mjpg") != std::string::npos);
    assert(index.body.find("/api/status") != std::string::npos);

    const auto status_response = router.handle({"GET", "/api/status", {}, ""});
    assert(status_response.status_code == 200);
    assert(status_response.content_type == "application/json; charset=utf-8");
    assert(status_response.body.find("\"running\":true") != std::string::npos);

    const auto command_response = router.handle({"POST", "/api/commands", {}, R"({"type":"enroll","name":"Alice"})"});
    assert(command_response.status_code == 200);
    assert(command_response.body == R"({"ok":true})");
    const auto queued = commands.consume();
    assert(queued.size() == 1);
    assert(queued[0].type == ccamera::CommandType::Enroll);
    assert(queued[0].name == "Alice");

    const auto bad_command = router.handle({"POST", "/api/commands", {}, R"({"type":"delete","name":""})"});
    assert(bad_command.status_code == 400);
    assert(bad_command.body.find("name is required") != std::string::npos);

    const auto stream = router.handle({"GET", "/stream.mjpg", {}, ""});
    assert(stream.status_code == 200);
    assert(stream.content_type == "multipart/x-mixed-replace; boundary=ccamera");
    assert(stream.binary_body.size() == 4);

    const auto missing = router.handle({"GET", "/missing", {}, ""});
    assert(missing.status_code == 404);
    assert(missing.body == "Not found");

    const auto wrong_method = router.handle({"POST", "/api/status", {}, ""});
    assert(wrong_method.status_code == 405);
    assert(wrong_method.body == "Method not allowed");

    const auto formatted = ccamera::format_http_response(index);
    assert(formatted.find("HTTP/1.1 200 OK\r\n") == 0);
    assert(formatted.find("Content-Type: text/html; charset=utf-8\r\n") != std::string::npos);

    return 0;
}
```

Add CMake target:

```cmake
    add_executable(test_http_router tests/test_http_router.cpp)
    target_link_libraries(test_http_router PRIVATE ccamera_core)
    add_test(NAME test_http_router COMMAND test_http_router)
```

- [x] **Step 2: Run the test to verify it fails**

Run:

```powershell
cmake --build build --config Debug --target test_http_router
```

Expected: build fails because `web/RuntimeWebRouter.hpp` does not exist.

- [x] **Step 3: Write the minimal implementation**

Create `src/web/Http.hpp`:

```cpp
#pragma once

#include <map>
#include <string>
#include <vector>

namespace ccamera {

struct HttpRequest {
    std::string method;
    std::string path;
    std::map<std::string, std::string> headers;
    std::string body;
};

struct HttpResponse {
    int status_code = 200;
    std::string reason = "OK";
    std::string content_type = "text/plain; charset=utf-8";
    std::string body;
    std::vector<unsigned char> binary_body;
    std::map<std::string, std::string> headers;
};

std::string format_http_response(const HttpResponse& response);

}  // namespace ccamera
```

Create `src/web/Http.cpp`:

```cpp
#include "web/Http.hpp"

#include <sstream>
#include <string>

namespace ccamera {
namespace {

std::size_t content_length(const HttpResponse& response) {
    return response.binary_body.empty() ? response.body.size() : response.binary_body.size();
}

}  // namespace

std::string format_http_response(const HttpResponse& response) {
    std::ostringstream out;
    out << "HTTP/1.1 " << response.status_code << ' ' << response.reason << "\r\n";
    out << "Content-Type: " << response.content_type << "\r\n";
    out << "Content-Length: " << content_length(response) << "\r\n";
    out << "Connection: close\r\n";
    for (const auto& [name, value] : response.headers) {
        out << name << ": " << value << "\r\n";
    }
    out << "\r\n";
    out << response.body;
    if (!response.binary_body.empty()) {
        out.write(reinterpret_cast<const char*>(response.binary_body.data()),
                  static_cast<std::streamsize>(response.binary_body.size()));
    }
    return out.str();
}

}  // namespace ccamera
```

Create `src/web/RuntimeWebRouter.hpp`:

```cpp
#pragma once

#include "web/Http.hpp"
#include "web/WebCommandQueue.hpp"
#include "web/WebStatus.hpp"

#include <functional>
#include <string>
#include <vector>

namespace ccamera {

using WebStatusProvider = std::function<WebStatusSnapshot()>;
using WebCommandHandler = std::function<WebCommandParseResult(const std::string&)>;
using WebStreamProvider = std::function<std::vector<unsigned char>()>;

class RuntimeWebRouter {
public:
    RuntimeWebRouter(WebStatusProvider status_provider,
                     WebCommandHandler command_handler,
                     WebStreamProvider stream_provider);

    HttpResponse handle(const HttpRequest& request) const;

private:
    HttpResponse index() const;
    HttpResponse status() const;
    HttpResponse command(const std::string& body) const;
    HttpResponse stream() const;

    WebStatusProvider status_provider_;
    WebCommandHandler command_handler_;
    WebStreamProvider stream_provider_;
};

}  // namespace ccamera
```

Create `src/web/RuntimeWebRouter.cpp`:

```cpp
#include "web/RuntimeWebRouter.hpp"

#include <nlohmann/json.hpp>

#include <utility>

namespace ccamera {
namespace {

const char kDashboardHtml[] = R"HTML(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>CCamera</title>
<style>
body{margin:0;font-family:Segoe UI,system-ui,sans-serif;background:#eef2f7;color:#111827}
.shell{display:grid;grid-template-columns:minmax(420px,1fr) 320px;gap:16px;padding:18px}
.video{width:100%;aspect-ratio:16/9;background:#111827;border-radius:8px;object-fit:contain}
.panel,.status{background:#fff;border:1px solid #d8dee8;border-radius:8px;padding:14px}
.actions{display:grid;grid-template-columns:1fr 1fr;gap:8px}
button,input{font:inherit}input{width:100%;box-sizing:border-box;margin:6px 0 10px;padding:9px;border:1px solid #cbd5e1;border-radius:6px}
button{padding:9px;border:1px solid #cbd5e1;border-radius:6px;background:#f8fafc;cursor:pointer}.primary{background:#15803d;color:#fff}.danger{background:#dc2626;color:#fff}.dark{background:#334155;color:#fff}
.person{display:flex;justify-content:space-between;border:1px solid #e2e8f0;border-radius:8px;padding:10px;margin-top:8px}
@media(max-width:900px){.shell{grid-template-columns:1fr}}
</style>
</head>
<body>
<main class="shell">
<section><h1>CCamera</h1><img class="video" src="/stream.mjpg" alt="Live camera stream"><div class="status" id="status">Connecting...</div></section>
<aside class="panel"><h2>Actions</h2><label>Name</label><input id="name" autocomplete="off"><div class="actions"><button class="primary" onclick="sendCommand('enroll')">Enroll</button><button class="danger" onclick="confirmDelete()">Delete</button><button onclick="refreshStatus()">Refresh</button><button class="dark" onclick="sendCommand('quit')">Quit</button></div><h2>People</h2><div id="people"></div></aside>
</main>
<script>
async function sendCommand(type){const name=document.getElementById('name').value;const body=(type==='enroll'||type==='delete')?{type,name}:{type};const r=await fetch('/api/commands',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});if(!r.ok){const j=await r.json();alert(j.error||'Command failed')}await refreshStatus()}
function confirmDelete(){const name=document.getElementById('name').value.trim();if(name&&confirm('Delete '+name+' from the local database?'))sendCommand('delete')}
async function refreshStatus(){const r=await fetch('/api/status');const j=await r.json();document.getElementById('status').textContent=(j.running?'Running':'Stopped')+' | FPS '+Math.round(j.fps)+' | '+j.status;document.getElementById('people').innerHTML=j.people.map(p=>'<div class="person"><b>'+p.name+'</b><span>'+p.templates+' templates</span></div>').join('')||'<p>No enrolled people</p>'}
setInterval(refreshStatus,1000);refreshStatus();
</script>
</body>
</html>)HTML";

HttpResponse text_response(int status_code, std::string reason, std::string body) {
    HttpResponse response;
    response.status_code = status_code;
    response.reason = std::move(reason);
    response.body = std::move(body);
    return response;
}

}  // namespace

RuntimeWebRouter::RuntimeWebRouter(WebStatusProvider status_provider,
                                   WebCommandHandler command_handler,
                                   WebStreamProvider stream_provider)
    : status_provider_(std::move(status_provider)),
      command_handler_(std::move(command_handler)),
      stream_provider_(std::move(stream_provider)) {}

HttpResponse RuntimeWebRouter::handle(const HttpRequest& request) const {
    if (request.path == "/" && request.method == "GET") {
        return index();
    }
    if (request.path == "/api/status") {
        return request.method == "GET" ? status() : text_response(405, "Method Not Allowed", "Method not allowed");
    }
    if (request.path == "/api/commands") {
        return request.method == "POST" ? command(request.body) : text_response(405, "Method Not Allowed", "Method not allowed");
    }
    if (request.path == "/stream.mjpg") {
        return request.method == "GET" ? stream() : text_response(405, "Method Not Allowed", "Method not allowed");
    }
    return text_response(404, "Not Found", "Not found");
}

HttpResponse RuntimeWebRouter::index() const {
    HttpResponse response;
    response.content_type = "text/html; charset=utf-8";
    response.body = kDashboardHtml;
    return response;
}

HttpResponse RuntimeWebRouter::status() const {
    HttpResponse response;
    response.content_type = "application/json; charset=utf-8";
    response.body = serialize_web_status(status_provider_());
    return response;
}

HttpResponse RuntimeWebRouter::command(const std::string& body) const {
    const WebCommandParseResult result = command_handler_(body);
    HttpResponse response;
    response.status_code = result.ok ? 200 : result.status_code;
    response.reason = result.ok ? "OK" : "Bad Request";
    response.content_type = "application/json; charset=utf-8";
    response.body = result.ok
        ? std::string(R"({"ok":true})")
        : nlohmann::json({{"ok", false}, {"error", result.error}}).dump();
    return response;
}

HttpResponse RuntimeWebRouter::stream() const {
    HttpResponse response;
    response.content_type = "multipart/x-mixed-replace; boundary=ccamera";
    response.binary_body = stream_provider_();
    return response;
}

}  // namespace ccamera
```

Add sources to `ccamera_core`:

```cmake
    src/web/Http.cpp
    src/web/RuntimeWebRouter.cpp
```

- [x] **Step 4: Run the test to verify it passes**

Run:

```powershell
cmake --build build --config Debug --target test_http_router
ctest --test-dir build -C Debug -R test_http_router --output-on-failure
```

Expected: build succeeds and focused CTest passes.

- [x] **Step 5: Update plan and discoveries**

Check off Task 3 steps in this plan. Append:

```markdown
- 2026-06-21: Runtime HTML UI Task 3 added an embedded browser dashboard and pure HTTP route handling for `/`, `/api/status`, `/api/commands`, and `/stream.mjpg`. Focused `test_http_router` passed.
```

- [x] **Step 6: Commit**

Run:

```powershell
git add CMakeLists.txt src/web/Http.hpp src/web/Http.cpp src/web/RuntimeWebRouter.hpp src/web/RuntimeWebRouter.cpp tests/test_http_router.cpp docs/discoveries.md docs/superpowers/plans/2026-06-21-runtime-html-ui.md
git commit -m "feat: add runtime web router"
```

## Task 4: MJPEG Frame Encoding

**Files:**
- Create: `src/web/MjpegStream.hpp`
- Create: `src/web/MjpegStream.cpp`
- Create: `tests/test_mjpeg_stream.cpp`
- Modify: `CMakeLists.txt`
- Modify after verification: `docs/discoveries.md`
- Update: `docs/superpowers/plans/2026-06-21-runtime-html-ui.md`

- [x] **Step 1: Write the failing test**

Create `tests/test_mjpeg_stream.cpp`:

```cpp
#include "web/MjpegStream.hpp"

#include <cassert>
#include <opencv2/core.hpp>

#include <string>

int main() {
    cv::Mat image(80, 120, CV_8UC3, cv::Scalar(10, 20, 30));

    ccamera::RecognitionResult result;
    result.label = "Alice";
    result.known = true;
    result.similarity = 0.93f;
    result.detection.box = cv::Rect2f(10.0f, 10.0f, 40.0f, 30.0f);

    const auto jpeg = ccamera::encode_overlay_jpeg(image, {result}, "Ready");
    assert(jpeg.size() > 10);
    assert(jpeg[0] == 0xff);
    assert(jpeg[1] == 0xd8);

    const auto part = ccamera::format_mjpeg_part(jpeg);
    const std::string part_text(part.begin(), part.end());
    assert(part_text.find("--ccamera\r\n") == 0);
    assert(part_text.find("Content-Type: image/jpeg\r\n") != std::string::npos);
    assert(part_text.find("Content-Length: " + std::to_string(jpeg.size())) != std::string::npos);

    const auto fallback = ccamera::make_web_fallback_frame({320, 180}, "Waiting for camera");
    assert(fallback.cols == 320);
    assert(fallback.rows == 180);
    assert(fallback.type() == CV_8UC3);

    return 0;
}
```

Update `find_package` in `CMakeLists.txt` to include `imgcodecs`:

```cmake
find_package(OpenCV REQUIRED COMPONENTS core imgproc imgcodecs highgui videoio objdetect)
```

Add test target:

```cmake
    add_executable(test_mjpeg_stream tests/test_mjpeg_stream.cpp)
    target_link_libraries(test_mjpeg_stream PRIVATE ccamera_core)
    add_test(
        NAME test_mjpeg_stream
        COMMAND ${CMAKE_COMMAND} -E env
            "PATH=${CCAMERA_LOCAL_MINGW_PREFIX}/bin$<SEMICOLON>$ENV{PATH}"
            $<TARGET_FILE:test_mjpeg_stream>
    )
```

- [x] **Step 2: Run the test to verify it fails**

Run:

```powershell
cmake -S . -B build -DCCAMERA_BUILD_TESTS=ON
cmake --build build --config Debug --target test_mjpeg_stream
```

Expected: build fails because `web/MjpegStream.hpp` does not exist.

- [x] **Step 3: Write the minimal implementation**

Create `src/web/MjpegStream.hpp`:

```cpp
#pragma once

#include "core/Types.hpp"

#include <opencv2/core.hpp>

#include <string>
#include <vector>

namespace ccamera {

cv::Mat make_web_fallback_frame(cv::Size size, const std::string& status);
std::vector<unsigned char> encode_overlay_jpeg(
    const cv::Mat& frame,
    const std::vector<RecognitionResult>& results,
    const std::string& status);
std::vector<unsigned char> format_mjpeg_part(const std::vector<unsigned char>& jpeg);

}  // namespace ccamera
```

Create `src/web/MjpegStream.cpp`:

```cpp
#include "web/MjpegStream.hpp"

#include "ui/PreviewWindow.hpp"

#include <opencv2/imgcodecs.hpp>
#include <opencv2/imgproc.hpp>

#include <stdexcept>
#include <string>

namespace ccamera {

cv::Mat make_web_fallback_frame(cv::Size size, const std::string& status) {
    const int width = size.width > 0 ? size.width : 640;
    const int height = size.height > 0 ? size.height : 360;
    cv::Mat frame(height, width, CV_8UC3, cv::Scalar(24, 24, 24));
    const std::string text = status.empty() ? "Waiting for camera" : status;
    cv::putText(frame, text, {24, height / 2}, cv::FONT_HERSHEY_SIMPLEX, 0.8, cv::Scalar(230, 230, 230), 2, cv::LINE_AA);
    return frame;
}

std::vector<unsigned char> encode_overlay_jpeg(
    const cv::Mat& frame,
    const std::vector<RecognitionResult>& results,
    const std::string& status) {
    cv::Mat output = frame.empty() ? make_web_fallback_frame({640, 360}, status) : frame.clone();
    PreviewWindow::draw_overlay(output, results);
    if (!status.empty()) {
        cv::putText(output, status, {16, 32}, cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(0, 215, 255), 2, cv::LINE_AA);
    }

    std::vector<unsigned char> jpeg;
    if (!cv::imencode(".jpg", output, jpeg, {cv::IMWRITE_JPEG_QUALITY, 82})) {
        throw std::runtime_error("failed to encode MJPEG frame");
    }
    return jpeg;
}

std::vector<unsigned char> format_mjpeg_part(const std::vector<unsigned char>& jpeg) {
    const std::string header =
        "--ccamera\r\nContent-Type: image/jpeg\r\nContent-Length: " +
        std::to_string(jpeg.size()) + "\r\n\r\n";
    std::vector<unsigned char> part(header.begin(), header.end());
    part.insert(part.end(), jpeg.begin(), jpeg.end());
    const std::string trailer = "\r\n";
    part.insert(part.end(), trailer.begin(), trailer.end());
    return part;
}

}  // namespace ccamera
```

Add source and link library in `CMakeLists.txt`:

```cmake
    src/web/MjpegStream.cpp
```

```cmake
target_link_libraries(ccamera_core PUBLIC
    opencv_imgcodecs
)
```

Also add `opencv_imgcodecs` to Windows runtime deployment loops where OpenCV runtime targets are enumerated.

- [x] **Step 4: Run the test to verify it passes**

Run:

```powershell
cmake -S . -B build -DCCAMERA_BUILD_TESTS=ON
cmake --build build --config Debug --target test_mjpeg_stream
ctest --test-dir build -C Debug -R test_mjpeg_stream --output-on-failure
```

Expected: focused CTest passes.

- [x] **Step 5: Update plan and discoveries**

Check off Task 4 steps in this plan. Append:

```markdown
- 2026-06-21: Runtime HTML UI Task 4 added MJPEG JPEG encoding through OpenCV imgcodecs, overlay reuse, and generated fallback frames for no-camera states. Focused `test_mjpeg_stream` passed.
```

- [x] **Step 6: Commit**

Run:

```powershell
git add CMakeLists.txt src/web/MjpegStream.hpp src/web/MjpegStream.cpp tests/test_mjpeg_stream.cpp docs/discoveries.md docs/superpowers/plans/2026-06-21-runtime-html-ui.md
git commit -m "feat: add mjpeg web stream formatting"
```

## Task 5: Localhost Runtime Web Server

**Files:**
- Create: `src/web/RuntimeWebServer.hpp`
- Create: `src/web/RuntimeWebServer.cpp`
- Create: `tests/test_runtime_web_server.cpp`
- Modify: `CMakeLists.txt`
- Modify after verification: `docs/discoveries.md`
- Update: `docs/superpowers/plans/2026-06-21-runtime-html-ui.md`

- [x] **Step 1: Write the failing test**

Create `tests/test_runtime_web_server.cpp`:

```cpp
#include "web/RuntimeWebServer.hpp"

#include <cassert>
#include <string>

int main() {
    ccamera::RuntimeWebServer server(
        [] {
            ccamera::WebStatusSnapshot status;
            status.running = true;
            status.status = "Ready";
            return status;
        },
        [](const std::string& body) {
            return ccamera::parse_web_command_json(body);
        },
        [] {
            return std::vector<unsigned char>{0xff, 0xd8, 0xff, 0xd9};
        });

    assert(!server.running());
    server.start();
    assert(server.running());
    assert(server.port() > 0);
    assert(server.url().find("http://127.0.0.1:") == 0);
    server.stop();
    assert(!server.running());
    server.stop();
    assert(!server.running());

    return 0;
}
```

Add test target:

```cmake
    add_executable(test_runtime_web_server tests/test_runtime_web_server.cpp)
    target_link_libraries(test_runtime_web_server PRIVATE ccamera_core)
    add_test(NAME test_runtime_web_server COMMAND test_runtime_web_server)
```

Add Windows socket library to `ccamera_core`:

```cmake
if (WIN32)
    target_link_libraries(ccamera_core PUBLIC ws2_32)
endif()
```

- [x] **Step 2: Run the test to verify it fails**

Run:

```powershell
cmake --build build --config Debug --target test_runtime_web_server
```

Expected: build fails because `web/RuntimeWebServer.hpp` does not exist.

- [x] **Step 3: Write the minimal implementation**

Create `src/web/RuntimeWebServer.hpp`:

```cpp
#pragma once

#include "web/RuntimeWebRouter.hpp"

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>

namespace ccamera {

class RuntimeWebServer {
public:
    RuntimeWebServer(WebStatusProvider status_provider,
                     WebCommandHandler command_handler,
                     WebStreamProvider stream_provider);
    ~RuntimeWebServer();

    RuntimeWebServer(const RuntimeWebServer&) = delete;
    RuntimeWebServer& operator=(const RuntimeWebServer&) = delete;

    void start();
    void stop();
    bool running() const;
    std::uint16_t port() const;
    std::string url() const;

private:
    void run();
    void close_listener();

    RuntimeWebRouter router_;
    std::atomic<bool> running_{false};
    std::thread thread_;
    std::intptr_t listener_ = -1;
    std::uint16_t port_ = 0;
};

}  // namespace ccamera
```

Create `src/web/RuntimeWebServer.cpp` with this structure. Keep request parsing simple and bounded:

```cpp
#include "web/RuntimeWebServer.hpp"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstring>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace ccamera {
namespace {

#ifdef _WIN32
using SocketHandle = SOCKET;
using SocketLength = int;
constexpr SocketHandle kInvalidSocket = INVALID_SOCKET;
void close_socket(SocketHandle socket) { closesocket(socket); }
int last_socket_error() { return WSAGetLastError(); }
#else
using SocketHandle = int;
using SocketLength = socklen_t;
constexpr SocketHandle kInvalidSocket = -1;
void close_socket(SocketHandle socket) { close(socket); }
int last_socket_error() { return errno; }
#endif

std::string receive_request(SocketHandle client) {
    std::string request;
    char buffer[4096];
    while (request.size() < 65536) {
        const int received = recv(client, buffer, sizeof(buffer), 0);
        if (received <= 0) {
            break;
        }
        request.append(buffer, buffer + received);
        const auto header_end = request.find("\r\n\r\n");
        if (header_end != std::string::npos) {
            const std::string headers = request.substr(0, header_end);
            const std::string marker = "Content-Length:";
            const auto length_pos = headers.find(marker);
            if (length_pos == std::string::npos) {
                break;
            }
            const auto line_end = headers.find("\r\n", length_pos);
            const std::string value = headers.substr(length_pos + marker.size(), line_end - (length_pos + marker.size()));
            const std::size_t body_length = static_cast<std::size_t>(std::stoul(value));
            if (request.size() >= header_end + 4 + body_length) {
                break;
            }
        }
    }
    return request;
}

HttpRequest parse_request_text(const std::string& text) {
    HttpRequest request;
    const auto header_end = text.find("\r\n\r\n");
    const std::string head = header_end == std::string::npos ? text : text.substr(0, header_end);
    std::istringstream stream(head);
    stream >> request.method >> request.path;
    if (header_end != std::string::npos) {
        request.body = text.substr(header_end + 4);
    }
    return request;
}

bool send_all(SocketHandle client, const std::string& response) {
    const char* data = response.data();
    std::size_t remaining = response.size();
    while (remaining > 0) {
        const int sent = send(client, data, static_cast<int>(std::min<std::size_t>(remaining, 8192)), 0);
        if (sent <= 0) {
            return false;
        }
        data += sent;
        remaining -= static_cast<std::size_t>(sent);
    }
    return true;
}

bool send_all_bytes(SocketHandle client, const std::vector<unsigned char>& response) {
    const char* data = reinterpret_cast<const char*>(response.data());
    std::size_t remaining = response.size();
    while (remaining > 0) {
        const int sent = send(client, data, static_cast<int>(std::min<std::size_t>(remaining, 8192)), 0);
        if (sent <= 0) {
            return false;
        }
        data += sent;
        remaining -= static_cast<std::size_t>(sent);
    }
    return true;
}

void send_mjpeg_stream(SocketHandle client, const RuntimeWebRouter& router, const std::atomic<bool>& running) {
    const std::string header =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: multipart/x-mixed-replace; boundary=ccamera\r\n"
        "Cache-Control: no-cache\r\n"
        "Connection: close\r\n\r\n";
    if (!send_all(client, header)) {
        return;
    }
    while (running) {
        const HttpResponse part = router.handle({"GET", "/stream.mjpg", {}, ""});
        if (!send_all_bytes(client, part.binary_body)) {
            return;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

}  // namespace

RuntimeWebServer::RuntimeWebServer(WebStatusProvider status_provider,
                                   WebCommandHandler command_handler,
                                   WebStreamProvider stream_provider)
    : router_(std::move(status_provider), std::move(command_handler), std::move(stream_provider)) {}

RuntimeWebServer::~RuntimeWebServer() {
    stop();
}

void RuntimeWebServer::start() {
    if (running_.exchange(true)) {
        return;
    }
#ifdef _WIN32
    WSADATA data{};
    if (WSAStartup(MAKEWORD(2, 2), &data) != 0) {
        running_ = false;
        throw std::runtime_error("failed to start Winsock");
    }
#endif
    thread_ = std::thread(&RuntimeWebServer::run, this);
    while (running_ && port_ == 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void RuntimeWebServer::stop() {
    if (!running_.exchange(false)) {
        return;
    }
    close_listener();
    if (thread_.joinable()) {
        thread_.join();
    }
#ifdef _WIN32
    WSACleanup();
#endif
}

bool RuntimeWebServer::running() const {
    return running_;
}

std::uint16_t RuntimeWebServer::port() const {
    return port_;
}

std::string RuntimeWebServer::url() const {
    return port_ == 0 ? std::string{} : ("http://127.0.0.1:" + std::to_string(port_));
}

void RuntimeWebServer::run() {
    SocketHandle server = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server == kInvalidSocket) {
        running_ = false;
        return;
    }
    listener_ = static_cast<std::intptr_t>(server);

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;
    if (bind(server, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
        close_socket(server);
        running_ = false;
        return;
    }

    SocketLength length = sizeof(address);
    getsockname(server, reinterpret_cast<sockaddr*>(&address), &length);
    port_ = ntohs(address.sin_port);

    if (listen(server, 8) != 0) {
        close_socket(server);
        running_ = false;
        return;
    }

    while (running_) {
        SocketHandle client = accept(server, nullptr, nullptr);
        if (client == kInvalidSocket) {
            if (running_) {
                std::cerr << "Web UI accept failed: " << last_socket_error() << '\n';
            }
            continue;
        }
        const std::string request_text = receive_request(client);
        const HttpRequest request = parse_request_text(request_text);
        if (request.method == "GET" && request.path == "/stream.mjpg") {
            send_mjpeg_stream(client, router_, running_);
        } else {
            const HttpResponse response = router_.handle(request);
            send_all(client, format_http_response(response));
        }
        close_socket(client);
    }

    close_socket(server);
    listener_ = -1;
    port_ = 0;
}

void RuntimeWebServer::close_listener() {
    if (listener_ >= 0) {
        close_socket(static_cast<SocketHandle>(listener_));
        listener_ = -1;
    }
}

}  // namespace ccamera
```

Add source:

```cmake
    src/web/RuntimeWebServer.cpp
```

- [x] **Step 4: Run the test to verify it passes**

Run:

```powershell
cmake -S . -B build -DCCAMERA_BUILD_TESTS=ON
cmake --build build --config Debug --target test_runtime_web_server
ctest --test-dir build -C Debug -R test_runtime_web_server --output-on-failure
```

Expected: focused CTest passes and leaves no long-running server process.

- [x] **Step 5: Update plan and discoveries**

Check off Task 5 steps in this plan. Append:

```markdown
- 2026-06-21: Runtime HTML UI Task 5 added `RuntimeWebServer`, binding only to loopback with automatic port selection and clean start/stop lifecycle. Focused `test_runtime_web_server` passed.
```

- [x] **Step 6: Commit**

Run:

```powershell
git add CMakeLists.txt src/web/RuntimeWebServer.hpp src/web/RuntimeWebServer.cpp tests/test_runtime_web_server.cpp docs/discoveries.md docs/superpowers/plans/2026-06-21-runtime-html-ui.md
git commit -m "feat: add localhost runtime web server"
```

## Task 6: AppController Integration

**Files:**
- Modify: `src/app/AppRunCleanup.hpp`
- Modify: `src/app/AppRunCleanup.cpp`
- Modify: `src/app/AppController.hpp`
- Modify: `src/app/AppController.cpp`
- Modify: `tests/test_app_run_cleanup.cpp`
- Create: `tests/test_app_web_commands.cpp`
- Modify: `CMakeLists.txt`
- Modify after verification: `docs/discoveries.md`
- Update: `docs/superpowers/plans/2026-06-21-runtime-html-ui.md`

- [ ] **Step 1: Write the failing tests**

Update `tests/test_app_run_cleanup.cpp` to expect a fourth cleanup callback named `stop_web`. Keep the existing test assertions, and add a `web_stopped` boolean that must become true when cleanup runs.

Create `tests/test_app_web_commands.cpp`:

```cpp
#include "app/AppController.hpp"

#include <cassert>
#include <filesystem>

int main() {
    ccamera::AppConfig config;
    config.database_path = (std::filesystem::temp_directory_path() / "ccamera_web_commands_test.json").string();
    config.detector_model = "models/opencv_zoo/face_detection_yunet_2023mar.onnx";
    config.recognizer_model = "models/opencv_zoo/face_recognition_sface_2021dec.onnx";
    std::filesystem::remove(config.database_path);

    ccamera::AppController controller(config);
    bool running = true;

    controller.enqueue_web_command_for_test({ccamera::CommandType::Enroll, "Alice"});
    controller.enqueue_web_command_for_test({ccamera::CommandType::Quit, {}});
    controller.apply_commands_for_test(running);

    assert(!running);
    assert(controller.enrollment_active_for_test());
    assert(controller.status_message_for_test().find("Enrolling Alice") != std::string::npos);

    std::filesystem::remove(config.database_path);
    return 0;
}
```

Add CMake target:

```cmake
    add_executable(test_app_web_commands tests/test_app_web_commands.cpp)
    target_compile_definitions(test_app_web_commands PRIVATE CCAMERA_TEST_HOOKS)
    target_link_libraries(test_app_web_commands PRIVATE ccamera_core)
    add_test(NAME test_app_web_commands COMMAND test_app_web_commands)
    set_tests_properties(test_app_web_commands PROPERTIES
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    )
```

- [ ] **Step 2: Run tests to verify they fail**

Run:

```powershell
cmake --build build --config Debug --target test_app_run_cleanup test_app_web_commands
```

Expected: build fails because `AppRunCleanup` does not accept `stop_web` and `AppController` does not have `web_commands_`.

- [ ] **Step 3: Write the minimal implementation**

Modify `src/app/AppRunCleanup.hpp` constructor and fields:

```cpp
AppRunCleanup(std::function<void()> stop_recognition,
              std::function<void()> stop_command,
              std::function<void()> stop_web,
              std::function<void()> stop_camera);
```

Add `std::function<void()> stop_web_;` before `stop_camera_`.

Modify `src/app/AppRunCleanup.cpp` so the destructor calls cleanup in this order:

```cpp
AppRunCleanup::~AppRunCleanup() {
    if (!active_) {
        return;
    }
    stop_recognition_();
    stop_command_();
    stop_web_();
    stop_camera_();
}
```

Modify `src/app/AppController.hpp` includes:

```cpp
#include "web/RuntimeWebServer.hpp"
#include "web/WebCommandQueue.hpp"
```

Add these public test hooks after `int run();`:

```cpp
#ifdef CCAMERA_TEST_HOOKS
    void enqueue_web_command_for_test(Command command) { web_commands_.push(command); }
    void apply_commands_for_test(bool& running) { apply_commands(running); }
    bool enrollment_active_for_test() const { return enrollment_session_.active(); }
    std::string status_message_for_test() const { return status_message(); }
#endif
```

Add private members:

```cpp
WebCommandQueue web_commands_;
std::unique_ptr<RuntimeWebServer> web_server_;
double fps_ = 0.0;
```

Add private methods:

```cpp
WebStatusSnapshot web_status_snapshot() const;
std::vector<unsigned char> web_stream_frame() const;
```

Modify `AppController::apply_commands()` to consume web commands before console commands:

```cpp
for (const auto& command : web_commands_.consume()) {
    try {
        apply_command(command, running);
    } catch (const std::exception& error) {
        std::cerr << "Web command failed: " << error.what() << '\n';
        set_status_message(std::string("Web command failed: ") + error.what());
    }
}
```

Implement snapshot and stream helpers in `src/app/AppController.cpp`:

```cpp
WebStatusSnapshot AppController::web_status_snapshot() const {
    WebStatusSnapshot snapshot;
    snapshot.running = recognition_running_;
    snapshot.fps = fps_;
    snapshot.status = status_message();
    snapshot.database_path = config_.database_path;
    snapshot.results = recognition_results_.latest_results();
    snapshot.database = database_.snapshot();
    return snapshot;
}

std::vector<unsigned char> AppController::web_stream_frame() const {
    const auto frame = frame_buffer_.latest();
    const auto results = recognition_results_.latest_results();
    const std::string status = status_message();
    if (frame.has_value()) {
        return format_mjpeg_part(encode_overlay_jpeg(frame->image, results, status));
    }
    return format_mjpeg_part(encode_overlay_jpeg(
        make_web_fallback_frame({config_.frame_width, config_.frame_height}, status),
        {},
        status));
}
```

Include `web/MjpegStream.hpp` and `web/WebStatus.hpp` in `AppController.cpp`.

Start the web server in `run()` after `command_loop_.start()`:

```cpp
try {
    web_server_ = std::make_unique<RuntimeWebServer>(
        [this] { return web_status_snapshot(); },
        [this](const std::string& body) { return web_commands_.push_json(body); },
        [this] { return web_stream_frame(); });
    web_server_->start();
    std::cout << "Web UI: " << web_server_->url() << '\n';
    set_status_message("Web UI: " + web_server_->url());
} catch (const std::exception& error) {
    std::cerr << "Web UI failed to start: " << error.what() << '\n';
    set_status_message(std::string("Web UI failed: ") + error.what());
}
```

Update `AppRunCleanup cleanup(...)` construction:

```cpp
AppRunCleanup cleanup(
    [this] {
        recognition_running_ = false;
        if (recognition_thread_.joinable()) {
            recognition_thread_.join();
        }
    },
    [this] { command_loop_.stop(); },
    [this] {
        if (web_server_) {
            web_server_->stop();
        }
    },
    [&camera] { camera.stop(); });
```

Update the normal shutdown path before `camera.stop()`:

```cpp
if (web_server_) {
    web_server_->stop();
}
```

Store FPS after it is computed:

```cpp
fps_ = fps;
```

- [ ] **Step 4: Run focused tests**

Run:

```powershell
cmake -S . -B build -DCCAMERA_BUILD_TESTS=ON
cmake --build build --config Debug --target test_app_run_cleanup test_app_web_commands
ctest --test-dir build -C Debug -R "test_app_run_cleanup|test_app_web_commands" --output-on-failure
```

Expected: focused tests pass.

- [ ] **Step 5: Update plan and discoveries**

Check off Task 6 steps in this plan. Append:

```markdown
- 2026-06-21: Runtime HTML UI Task 6 integrated web command consumption and web server lifecycle into `AppController`, including cleanup on exception paths. Focused app cleanup/web-command tests passed.
```

- [ ] **Step 6: Commit**

Run:

```powershell
git add CMakeLists.txt src/app/AppRunCleanup.hpp src/app/AppRunCleanup.cpp src/app/AppController.hpp src/app/AppController.cpp tests/test_app_run_cleanup.cpp tests/test_app_web_commands.cpp docs/discoveries.md docs/superpowers/plans/2026-06-21-runtime-html-ui.md
git commit -m "feat: integrate runtime web ui"
```

## Task 7: Documentation and Full Verification

**Files:**
- Modify: `README.md`
- Modify: `docs/discoveries.md`
- Update: `docs/superpowers/plans/2026-06-21-runtime-html-ui.md`

- [ ] **Step 1: Update README**

Add this section after the existing preview-window control description:

```markdown
## Runtime Browser UI

When the app starts, it also starts a local browser UI bound to `127.0.0.1` on an automatically selected port. The console prints the URL:

```text
Web UI: http://127.0.0.1:<port>
```

Open that URL in a local browser to view the live MJPEG preview, current recognition status, enrolled people, and runtime controls. The OpenCV preview window remains open and fully usable.

The browser UI can enqueue `Enroll`, `Delete`, `List`, and `Quit` commands. Enrollment and deletion still use the existing local database flow and persist to the configured `database_path`, normally `data/face_db.json`. The UI is localhost-only; it does not provide LAN or remote access.
```

- [ ] **Step 2: Run full automated verification**

Run:

```powershell
cmake -S . -B build -DCCAMERA_BUILD_TESTS=ON
cmake --build build --config Debug
ctest --test-dir build -C Debug --output-on-failure
```

Expected: full build succeeds and all automated tests pass.

- [ ] **Step 3: Run bounded startup probe**

Run:

```powershell
$process = Start-Process -FilePath ".\build\ccamera.exe" -WorkingDirectory ".\build" -PassThru -WindowStyle Hidden
Start-Sleep -Seconds 15
if (!$process.HasExited) { Stop-Process -Id $process.Id -Force }
```

Expected: process stays alive until stopped or exits with a documented camera/model/runtime error. Do not claim live browser or camera success from this probe alone.

- [ ] **Step 4: Record verification discovery**

Append:

```markdown
- 2026-06-21: Runtime HTML UI final automated verification passed with configure, build, and full CTest. A bounded no-argument startup probe was run; live browser interaction and real camera/model recognition remain manual checks unless explicitly performed.
```

If full CTest fails, record the exact failing test and error instead of the passing line, then fix before committing.

- [ ] **Step 5: Commit**

Run:

```powershell
git add README.md docs/discoveries.md docs/superpowers/plans/2026-06-21-runtime-html-ui.md
git commit -m "docs: document runtime browser ui"
```

## Final Manual Verification

- [ ] Launch `.\build\ccamera.exe`.
- [ ] Confirm the OpenCV preview window remains open.
- [ ] Open the printed browser URL.
- [ ] Confirm the browser shows the live stream.
- [ ] Enroll a name from the browser.
- [ ] Confirm the browser people list updates.
- [ ] Confirm `data/face_db.json` persists the enrolled name.
- [ ] Delete that name from the browser after confirmation.
- [ ] Confirm the local database no longer contains that person.
- [ ] Quit from the browser and confirm the app exits cleanly.

Record completed manual checks in `docs/discoveries.md` with exact results.
