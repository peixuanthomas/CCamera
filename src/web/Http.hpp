#pragma once

#include <map>
#include <string>
#include <vector>

namespace ccamera {

struct HttpRequest {
    std::string method;
    std::string path;
    std::map<std::string, std::string> headers;
    std::string body;
};

struct HttpResponse {
    int status_code = 200;
    std::string reason = "OK";
    std::string content_type = "text/plain; charset=utf-8";
    std::string body;
    std::vector<unsigned char> binary_body;
    std::map<std::string, std::string> headers;
};

std::string format_http_response(const HttpResponse& response);

}  // namespace ccamera
