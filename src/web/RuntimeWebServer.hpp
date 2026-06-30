#pragma once

#include "web/RuntimeWebRouter.hpp"

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

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
    static constexpr std::size_t max_concurrent_connections() noexcept { return 8; }

private:
    struct ClientThread {
        std::thread thread;
        std::shared_ptr<std::atomic<bool>> finished;
    };

    void run();
    void handle_client(std::intptr_t client_handle);
    void reap_finished_clients();
    void join_all_clients();
    void close_listener();
    bool try_reserve_client();
    void release_client();

    std::string auth_token_;
    RuntimeWebRouter router_;
    std::atomic<bool> running_{false};
    std::atomic<std::size_t> active_clients_{0};
    std::thread thread_;
    std::mutex clients_mutex_;
    std::vector<ClientThread> clients_;
    std::intptr_t listener_ = -1;
    std::uint16_t port_ = 0;
};

}  // namespace ccamera
