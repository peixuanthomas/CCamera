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
