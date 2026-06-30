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
                     WebStreamProvider stream_provider,
                     std::string auth_token = {},
                     std::string allowed_origin = {});

    HttpResponse handle(const HttpRequest& request) const;
    void set_allowed_origin(std::string allowed_origin);

private:
    HttpResponse index() const;
    HttpResponse status() const;
    HttpResponse command(const std::string& body) const;
    HttpResponse stream() const;
    bool has_valid_token(const HttpRequest& request) const;
    bool has_allowed_origin(const HttpRequest& request) const;

    WebStatusProvider status_provider_;
    WebCommandHandler command_handler_;
    WebStreamProvider stream_provider_;
    std::string auth_token_;
    std::string allowed_origin_;
};

}  // namespace ccamera
