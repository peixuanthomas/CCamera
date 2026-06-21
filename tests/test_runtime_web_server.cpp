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
