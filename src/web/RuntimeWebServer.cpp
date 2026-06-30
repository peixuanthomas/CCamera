#include "web/RuntimeWebServer.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <chrono>
#include <cctype>
#include <cstring>
#include <iostream>
#include <limits>
#include <memory>
#include <mutex>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
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
#include <sys/time.h>
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

constexpr std::size_t kMaxHeaderBytes = 16 * 1024;
constexpr std::size_t kMaxBodyBytes = 64 * 1024;
constexpr int kSocketTimeoutMs = 3000;

struct RequestReadResult {
    bool ok = false;
    HttpRequest request;
    HttpResponse error;
};

HttpResponse error_response(int status_code, std::string reason, std::string body) {
    HttpResponse response;
    response.status_code = status_code;
    response.reason = std::move(reason);
    response.body = std::move(body);
    return response;
}

std::string trim_ascii(std::string_view value) {
    std::size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start]))) {
        ++start;
    }
    std::size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1]))) {
        --end;
    }
    return std::string(value.substr(start, end - start));
}

std::string to_lower_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

bool parse_content_length(const std::string& value, std::size_t& body_length) {
    const std::string trimmed = trim_ascii(value);
    if (trimmed.empty()) {
        return false;
    }
    for (char ch : trimmed) {
        if (!std::isdigit(static_cast<unsigned char>(ch))) {
            return false;
        }
    }
    try {
        const unsigned long long parsed = std::stoull(trimmed);
        if (parsed > std::numeric_limits<std::size_t>::max()) {
            return false;
        }
        body_length = static_cast<std::size_t>(parsed);
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

RequestReadResult parse_request_head(const std::string& head, std::size_t& body_length) {
    RequestReadResult result;
    std::istringstream stream(head);
    std::string request_line;
    if (!std::getline(stream, request_line)) {
        result.error = error_response(400, "Bad Request", "Bad request");
        return result;
    }
    if (!request_line.empty() && request_line.back() == '\r') {
        request_line.pop_back();
    }

    std::istringstream request_line_stream(request_line);
    std::string version;
    if (!(request_line_stream >> result.request.method >> result.request.path >> version)) {
        result.error = error_response(400, "Bad Request", "Bad request");
        return result;
    }
    const auto query_pos = result.request.path.find('?');
    if (query_pos != std::string::npos) {
        result.request.query = result.request.path.substr(query_pos + 1);
    }

    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty()) {
            continue;
        }
        const auto colon = line.find(':');
        if (colon == std::string::npos) {
            result.error = error_response(400, "Bad Request", "Bad request");
            return result;
        }
        std::string name = to_lower_ascii(trim_ascii(std::string_view(line).substr(0, colon)));
        std::string value = trim_ascii(std::string_view(line).substr(colon + 1));
        if (name.empty()) {
            result.error = error_response(400, "Bad Request", "Bad request");
            return result;
        }
        result.request.headers[std::move(name)] = std::move(value);
    }

    body_length = 0;
    const auto length_it = result.request.headers.find("content-length");
    if (length_it != result.request.headers.end()) {
        if (!parse_content_length(length_it->second, body_length)) {
            result.error = error_response(400, "Bad Request", "Bad Content-Length");
            return result;
        }
        if (body_length > kMaxBodyBytes) {
            result.error = error_response(413, "Payload Too Large", "Request body too large");
            return result;
        }
    }

    result.ok = true;
    return result;
}

RequestReadResult receive_request(SocketHandle client) {
    std::string raw;
    char buffer[4096];
    while (true) {
        const int received = recv(client, buffer, sizeof(buffer), 0);
        if (received <= 0) {
            return {false, {}, error_response(raw.empty() ? 408 : 400,
                                              raw.empty() ? "Request Timeout" : "Bad Request",
                                              raw.empty() ? "Request timeout" : "Bad request")};
        }

        raw.append(buffer, buffer + received);
        const auto header_end = raw.find("\r\n\r\n");
        if (header_end == std::string::npos) {
            if (raw.size() > kMaxHeaderBytes) {
                return {false, {}, error_response(431, "Request Header Fields Too Large", "Request headers too large")};
            }
            continue;
        }

        if (header_end > kMaxHeaderBytes) {
            return {false, {}, error_response(431, "Request Header Fields Too Large", "Request headers too large")};
        }

        std::size_t body_length = 0;
        RequestReadResult parsed = parse_request_head(raw.substr(0, header_end), body_length);
        if (!parsed.ok) {
            return parsed;
        }

        const std::size_t total_length = header_end + 4 + body_length;
        if (raw.size() >= total_length) {
            parsed.request.body = raw.substr(header_end + 4, body_length);
            return parsed;
        }
    }
}

void set_socket_timeouts(SocketHandle client) {
#ifdef _WIN32
    const DWORD timeout = kSocketTimeoutMs;
    setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
    setsockopt(client, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout));
#else
    timeval timeout{};
    timeout.tv_sec = kSocketTimeoutMs / 1000;
    timeout.tv_usec = (kSocketTimeoutMs % 1000) * 1000;
    setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(client, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
#endif
}

std::string request_path(const HttpRequest& request) {
    const auto query_pos = request.path.find('?');
    return query_pos == std::string::npos ? request.path : request.path.substr(0, query_pos);
}

std::string generate_auth_token() {
    std::array<unsigned char, 32> bytes{};
    std::random_device random;
    for (unsigned char& byte : bytes) {
        byte = static_cast<unsigned char>(random());
    }

    static constexpr char kHex[] = "0123456789abcdef";
    std::string token;
    token.reserve(bytes.size() * 2);
    for (unsigned char byte : bytes) {
        token.push_back(kHex[(byte >> 4) & 0x0F]);
        token.push_back(kHex[byte & 0x0F]);
    }
    return token;
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

void send_mjpeg_stream(SocketHandle client,
                       const RuntimeWebRouter& router,
                       const HttpRequest& request,
                       const std::atomic<bool>& running,
                       std::vector<unsigned char> first_part) {
    const std::string header =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: multipart/x-mixed-replace; boundary=ccamera\r\n"
        "Cache-Control: no-cache\r\n"
        "Connection: close\r\n\r\n";
    if (!send_all(client, header)) {
        return;
    }
    if (!first_part.empty() && !send_all_bytes(client, first_part)) {
        return;
    }
    while (running) {
        const HttpResponse part = router.handle(request);
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
    : auth_token_(generate_auth_token()),
      router_(std::move(status_provider), std::move(command_handler), std::move(stream_provider), auth_token_) {}

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
    join_all_clients();
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
    return port_ == 0 ? std::string{} : ("http://127.0.0.1:" + std::to_string(port_) + "/?token=" + auth_token_);
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
    router_.set_allowed_origin("http://127.0.0.1:" + std::to_string(port_));

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

        set_socket_timeouts(client);

        {
            std::lock_guard<std::mutex> lock(clients_mutex_);
            reap_finished_clients();
        }
        if (!try_reserve_client()) {
            send_all(client, format_http_response(error_response(503, "Service Unavailable", "Too many connections")));
            close_socket(client);
            continue;
        }

        // Each connection is served on its own thread so a long-lived MJPEG
        // stream cannot starve the status/command endpoints.
        auto finished = std::make_shared<std::atomic<bool>>(false);
        const auto client_handle = static_cast<std::intptr_t>(client);
        std::thread worker([this, client_handle, finished] {
            handle_client(client_handle);
            release_client();
            finished->store(true);
        });

        std::lock_guard<std::mutex> lock(clients_mutex_);
        clients_.push_back({std::move(worker), std::move(finished)});
    }

    close_socket(server);
    listener_ = -1;
    port_ = 0;
}

void RuntimeWebServer::handle_client(std::intptr_t client_handle) {
    const SocketHandle client = static_cast<SocketHandle>(client_handle);
    try {
        const RequestReadResult read = receive_request(client);
        if (!read.ok) {
            send_all(client, format_http_response(read.error));
            close_socket(client);
            return;
        }

        const HttpRequest& request = read.request;
        if (request.method == "GET" && request_path(request) == "/stream.mjpg") {
            const HttpResponse stream_response = router_.handle(request);
            if (stream_response.status_code == 200) {
                send_mjpeg_stream(client, router_, request, running_, stream_response.binary_body);
            } else {
                send_all(client, format_http_response(stream_response));
            }
        } else {
            const HttpResponse response = router_.handle(request);
            send_all(client, format_http_response(response));
        }
    } catch (const std::exception& error) {
        std::cerr << "Web UI client failed: " << error.what() << '\n';
    } catch (...) {
        std::cerr << "Web UI client failed: unknown error\n";
    }
    close_socket(client);
}

void RuntimeWebServer::reap_finished_clients() {
    clients_.erase(
        std::remove_if(clients_.begin(), clients_.end(), [](ClientThread& worker) {
            if (worker.finished->load()) {
                if (worker.thread.joinable()) {
                    worker.thread.join();
                }
                return true;
            }
            return false;
        }),
        clients_.end());
}

void RuntimeWebServer::join_all_clients() {
    std::lock_guard<std::mutex> lock(clients_mutex_);
    for (auto& worker : clients_) {
        if (worker.thread.joinable()) {
            worker.thread.join();
        }
    }
    clients_.clear();
}

void RuntimeWebServer::close_listener() {
    if (listener_ >= 0) {
        close_socket(static_cast<SocketHandle>(listener_));
        listener_ = -1;
    }
}

bool RuntimeWebServer::try_reserve_client() {
    std::size_t current = active_clients_.load();
    while (current < max_concurrent_connections()) {
        if (active_clients_.compare_exchange_weak(current, current + 1)) {
            return true;
        }
    }
    return false;
}

void RuntimeWebServer::release_client() {
    active_clients_.fetch_sub(1);
}

}  // namespace ccamera
