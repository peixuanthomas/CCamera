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
