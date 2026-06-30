#include "web/RuntimeWebServer.hpp"

#include <cassert>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <winsock2.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>
#endif

namespace {

#ifdef _WIN32
using SocketHandle = SOCKET;
constexpr SocketHandle kInvalidSocket = INVALID_SOCKET;
void close_socket(SocketHandle socket) { closesocket(socket); }
#else
using SocketHandle = int;
constexpr SocketHandle kInvalidSocket = -1;
void close_socket(SocketHandle socket) { close(socket); }
#endif

void set_socket_timeout(SocketHandle socket) {
#ifdef _WIN32
    const DWORD timeout = 2000;
    setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
    setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
#else
    timeval timeout{};
    timeout.tv_sec = 2;
    setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(socket, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
#endif
}

SocketHandle connect_socket(std::uint16_t port) {
    SocketHandle socket_handle = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    assert(socket_handle != kInvalidSocket);
    set_socket_timeout(socket_handle);

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = htons(port);
    assert(connect(socket_handle, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0);
    return socket_handle;
}

void send_all(SocketHandle socket, std::string_view request) {
    const char* data = request.data();
    std::size_t remaining = request.size();
    while (remaining > 0) {
        const int sent = send(socket, data, static_cast<int>(std::min<std::size_t>(remaining, 4096)), 0);
        assert(sent > 0);
        data += sent;
        remaining -= static_cast<std::size_t>(sent);
    }
}

std::string receive_response(SocketHandle socket) {
    std::string response;
    char buffer[4096];
    while (true) {
        const int received = recv(socket, buffer, sizeof(buffer), 0);
        if (received <= 0) {
            break;
        }
        response.append(buffer, buffer + received);
    }
    return response;
}

std::string receive_some(SocketHandle socket) {
    char buffer[4096];
    const int received = recv(socket, buffer, sizeof(buffer), 0);
    assert(received > 0);
    return std::string(buffer, buffer + received);
}

std::string http_exchange(std::uint16_t port, const std::string& request) {
    const SocketHandle socket = connect_socket(port);
    send_all(socket, request);
    const std::string response = receive_response(socket);
    close_socket(socket);
    return response;
}

bool has_status(const std::string& response, int status_code) {
    return response.find("HTTP/1.1 " + std::to_string(status_code) + " ") == 0;
}

std::string token_from_url(const std::string& url) {
    const std::string marker = "?token=";
    const auto marker_pos = url.find(marker);
    assert(marker_pos != std::string::npos);
    return url.substr(marker_pos + marker.size());
}

std::string get_request(const std::string& target) {
    return "GET " + target + " HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n\r\n";
}

std::string post_request(const std::string& target,
                         const std::string& body,
                         const std::string& token,
                         const std::string& origin) {
    std::string request =
        "POST " + target + " HTTP/1.1\r\n"
        "Host: 127.0.0.1\r\n"
        "Content-Type: application/json\r\n"
        "X-CCamera-Token: " + token + "\r\n";
    if (!origin.empty()) {
        request += "Origin: " + origin + "\r\n";
    }
    request += "Content-Length: " + std::to_string(body.size()) + "\r\nConnection: close\r\n\r\n" + body;
    return request;
}

bool eventually_rejects_extra_connection(std::uint16_t port, const std::string& token) {
    for (int attempt = 0; attempt < 20; ++attempt) {
        if (has_status(http_exchange(port, get_request("/api/status?token=" + token)), 503)) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    return false;
}

}  // namespace

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
    const std::string token = token_from_url(server.url());
    assert(!token.empty());
    const std::string origin = "http://127.0.0.1:" + std::to_string(server.port());

    assert(has_status(http_exchange(server.port(), get_request("/api/status")), 401));
    assert(has_status(http_exchange(server.port(), get_request("/api/status?token=wrong")), 401));
    assert(has_status(http_exchange(server.port(), get_request("/stream.mjpg")), 401));
    assert(has_status(http_exchange(server.port(), get_request("/api/status?token=" + token)), 200));

    assert(has_status(
        http_exchange(server.port(), post_request("/api/commands", R"({"type":"quit"})", token, "http://evil.example")),
        403));
    assert(has_status(
        http_exchange(server.port(), post_request("/api/commands", R"({"type":"list"})", token, origin)),
        200));

    assert(has_status(
        http_exchange(server.port(),
                      "POST /api/commands?token=" + token + " HTTP/1.1\r\n"
                      "Host: 127.0.0.1\r\n"
                      "Content-Length: nope\r\n"
                      "Connection: close\r\n\r\n"),
        400));
    assert(has_status(http_exchange(server.port(), get_request("/api/status?token=" + token)), 200));

    assert(has_status(
        http_exchange(server.port(),
                      "POST /api/commands?token=" + token + " HTTP/1.1\r\n"
                      "Host: 127.0.0.1\r\n"
                      "Content-Length: 65537\r\n"
                      "Connection: close\r\n\r\n"),
        413));
    assert(has_status(http_exchange(server.port(), get_request("/api/status?token=" + token)), 200));

    const SocketHandle slow = connect_socket(server.port());
    assert(has_status(http_exchange(server.port(), get_request("/api/status?token=" + token)), 200));
    close_socket(slow);

    const SocketHandle stream = connect_socket(server.port());
    send_all(stream, get_request("/stream.mjpg?token=" + token));
    assert(has_status(receive_some(stream), 200));
    assert(has_status(http_exchange(server.port(), get_request("/api/status?token=" + token)), 200));
    close_socket(stream);

    std::vector<SocketHandle> idle_sockets;
    for (std::size_t index = 0; index < ccamera::RuntimeWebServer::max_concurrent_connections(); ++index) {
        const SocketHandle idle = connect_socket(server.port());
        send_all(idle, std::string("GET /api/status?token=") + token + " HTTP/1.1\r\nHost: 127.0.0.1\r\n");
        idle_sockets.push_back(idle);
    }
    assert(eventually_rejects_extra_connection(server.port(), token));
    for (SocketHandle socket : idle_sockets) {
        close_socket(socket);
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(150));
    assert(has_status(http_exchange(server.port(), get_request("/api/status?token=" + token)), 200));

    server.stop();
    assert(!server.running());
    server.stop();
    assert(!server.running());

    return 0;
}
