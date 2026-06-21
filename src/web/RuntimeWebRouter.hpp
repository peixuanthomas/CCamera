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
