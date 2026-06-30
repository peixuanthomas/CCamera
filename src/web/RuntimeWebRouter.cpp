#include "web/RuntimeWebRouter.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <string>
#include <utility>

namespace ccamera {
namespace {

constexpr char kTokenPlaceholder[] = "__CCAMERA_TOKEN__";

const char kDashboardHtml[] = R"HTML(<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>CCamera</title>
<style>
body{margin:0;font-family:Segoe UI,system-ui,sans-serif;background:#eef2f7;color:#111827}
.shell{display:grid;grid-template-columns:minmax(420px,1fr) 320px;gap:16px;padding:18px}
.video{width:100%;aspect-ratio:16/9;background:#111827;border-radius:8px;object-fit:contain}
.panel,.status{background:#fff;border:1px solid #d8dee8;border-radius:8px;padding:14px}
.actions{display:grid;grid-template-columns:1fr 1fr;gap:8px}
button,input{font:inherit}input{width:100%;box-sizing:border-box;margin:6px 0 10px;padding:9px;border:1px solid #cbd5e1;border-radius:6px}
button{padding:9px;border:1px solid #cbd5e1;border-radius:6px;background:#f8fafc;cursor:pointer}.primary{background:#15803d;color:#fff}.danger{background:#dc2626;color:#fff}.dark{background:#334155;color:#fff}
.person{display:flex;align-items:center;justify-content:space-between;gap:8px;border:1px solid #e2e8f0;border-radius:8px;padding:8px 10px;margin-top:8px}
.person .meta{min-width:0}.person .meta b{word-break:break-word}.person .meta small{color:#64748b}
.person button{flex:none;padding:5px 10px;font-size:13px}
@media(max-width:900px){.shell{grid-template-columns:1fr}}
</style>
</head>
<body>
<main class="shell">
<section><h1>CCamera</h1><img class="video" src="/stream.mjpg?token=__CCAMERA_TOKEN__" alt="Live camera stream"><div class="status" id="status">Connecting...</div></section>
<aside class="panel"><h2>Actions</h2><label>Name</label><input id="name" autocomplete="off"><div class="actions"><button class="primary" onclick="sendCommand('enroll')">Enroll</button><button onclick="refreshStatus()">Refresh</button><button class="dark" onclick="sendCommand('quit')">Quit</button></div><h2>People</h2><div id="people"></div></aside>
</main>
<script>
const CCAMERA_TOKEN='__CCAMERA_TOKEN__';
const TOKEN_QUERY='token='+encodeURIComponent(CCAMERA_TOKEN);
function esc(s){return String(s).replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]))}
async function postCommand(body){const r=await fetch('/api/commands',{method:'POST',headers:{'Content-Type':'application/json','X-CCamera-Token':CCAMERA_TOKEN},body:JSON.stringify(body)});if(!r.ok){const j=await r.json().catch(()=>({}));alert(j.error||'Command failed')}await refreshStatus()}
async function sendCommand(type){const name=document.getElementById('name').value;return postCommand(type==='enroll'?{type,name}:{type})}
async function deletePerson(name){if(confirm('Delete '+name+' from the local database?'))await postCommand({type:'delete',name})}
async function refreshStatus(){const r=await fetch('/api/status?'+TOKEN_QUERY);const j=await r.json();document.getElementById('status').textContent=(j.running?'Running':'Stopped')+' | FPS '+Math.round(j.fps)+' | '+j.status;const people=document.getElementById('people');if(!j.people.length){people.innerHTML='<p>No enrolled people</p>';return}people.innerHTML=j.people.map(p=>'<div class="person"><span class="meta"><b>'+esc(p.name)+'</b><br><small>'+p.templates+' templates</small></span><button class="danger" data-name="'+esc(p.name)+'">Delete</button></div>').join('');people.querySelectorAll('button[data-name]').forEach(b=>b.onclick=()=>deletePerson(b.dataset.name))}
setInterval(refreshStatus,1000);refreshStatus();
</script>
</body>
</html>)HTML";

std::string to_lower_ascii(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}

std::string request_path(const HttpRequest& request) {
    const auto query_pos = request.path.find('?');
    return query_pos == std::string::npos ? request.path : request.path.substr(0, query_pos);
}

std::string request_query(const HttpRequest& request) {
    if (!request.query.empty()) {
        return request.query;
    }
    const auto query_pos = request.path.find('?');
    return query_pos == std::string::npos ? std::string{} : request.path.substr(query_pos + 1);
}

std::string query_parameter(const HttpRequest& request, const std::string& name) {
    const std::string query = request_query(request);
    std::size_t start = 0;
    while (start <= query.size()) {
        const auto end = query.find('&', start);
        const std::string item = query.substr(start, end == std::string::npos ? std::string::npos : end - start);
        const auto equals = item.find('=');
        const std::string key = equals == std::string::npos ? item : item.substr(0, equals);
        if (key == name) {
            return equals == std::string::npos ? std::string{} : item.substr(equals + 1);
        }
        if (end == std::string::npos) {
            break;
        }
        start = end + 1;
    }
    return {};
}

std::string header_value(const HttpRequest& request, const std::string& name) {
    const std::string lower_name = to_lower_ascii(name);
    for (const auto& [header_name, value] : request.headers) {
        if (to_lower_ascii(header_name) == lower_name) {
            return value;
        }
    }
    return {};
}

void replace_all(std::string& text, const std::string& needle, const std::string& replacement) {
    std::size_t position = 0;
    while ((position = text.find(needle, position)) != std::string::npos) {
        text.replace(position, needle.size(), replacement);
        position += replacement.size();
    }
}

HttpResponse text_response(int status_code, std::string reason, std::string body) {
    HttpResponse response;
    response.status_code = status_code;
    response.reason = std::move(reason);
    response.body = std::move(body);
    return response;
}

}  // namespace

RuntimeWebRouter::RuntimeWebRouter(WebStatusProvider status_provider,
                                   WebCommandHandler command_handler,
                                   WebStreamProvider stream_provider,
                                   std::string auth_token,
                                   std::string allowed_origin)
    : status_provider_(std::move(status_provider)),
      command_handler_(std::move(command_handler)),
      stream_provider_(std::move(stream_provider)),
      auth_token_(std::move(auth_token)),
      allowed_origin_(std::move(allowed_origin)) {}

HttpResponse RuntimeWebRouter::handle(const HttpRequest& request) const {
    const std::string path = request_path(request);
    if (path == "/" && request.method == "GET") {
        return index();
    }
    if (path == "/api/status") {
        if (!has_valid_token(request)) {
            return text_response(401, "Unauthorized", "Unauthorized");
        }
        return request.method == "GET" ? status() : text_response(405, "Method Not Allowed", "Method not allowed");
    }
    if (path == "/api/commands") {
        if (!has_valid_token(request)) {
            return text_response(401, "Unauthorized", "Unauthorized");
        }
        if (request.method == "POST" && !has_allowed_origin(request)) {
            return text_response(403, "Forbidden", "Forbidden");
        }
        return request.method == "POST" ? command(request.body) : text_response(405, "Method Not Allowed", "Method not allowed");
    }
    if (path == "/stream.mjpg") {
        if (!has_valid_token(request)) {
            return text_response(401, "Unauthorized", "Unauthorized");
        }
        return request.method == "GET" ? stream() : text_response(405, "Method Not Allowed", "Method not allowed");
    }
    return text_response(404, "Not Found", "Not found");
}

void RuntimeWebRouter::set_allowed_origin(std::string allowed_origin) {
    allowed_origin_ = std::move(allowed_origin);
}

HttpResponse RuntimeWebRouter::index() const {
    HttpResponse response;
    response.content_type = "text/html; charset=utf-8";
    response.body = kDashboardHtml;
    replace_all(response.body, kTokenPlaceholder, auth_token_);
    return response;
}

HttpResponse RuntimeWebRouter::status() const {
    HttpResponse response;
    response.content_type = "application/json; charset=utf-8";
    response.body = serialize_web_status(status_provider_());
    return response;
}

HttpResponse RuntimeWebRouter::command(const std::string& body) const {
    const WebCommandParseResult result = command_handler_(body);
    HttpResponse response;
    response.status_code = result.ok ? 200 : result.status_code;
    response.reason = result.ok ? "OK" : "Bad Request";
    response.content_type = "application/json; charset=utf-8";
    response.body = result.ok
        ? std::string(R"({"ok":true})")
        : nlohmann::json({{"ok", false}, {"error", result.error}}).dump();
    return response;
}

HttpResponse RuntimeWebRouter::stream() const {
    HttpResponse response;
    response.content_type = "multipart/x-mixed-replace; boundary=ccamera";
    response.binary_body = stream_provider_();
    return response;
}

bool RuntimeWebRouter::has_valid_token(const HttpRequest& request) const {
    if (auth_token_.empty()) {
        return false;
    }
    if (header_value(request, "x-ccamera-token") == auth_token_) {
        return true;
    }
    return query_parameter(request, "token") == auth_token_;
}

bool RuntimeWebRouter::has_allowed_origin(const HttpRequest& request) const {
    const std::string origin = header_value(request, "origin");
    if (origin.empty()) {
        return true;
    }
    return !allowed_origin_.empty() && origin == allowed_origin_;
}

}  // namespace ccamera
