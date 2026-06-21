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
