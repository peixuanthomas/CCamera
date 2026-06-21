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
