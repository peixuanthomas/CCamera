#include "web/RuntimeWebRouter.hpp"

#include <nlohmann/json.hpp>

#include <utility>

namespace ccamera {
namespace {

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
.person{display:flex;justify-content:space-between;border:1px solid #e2e8f0;border-radius:8px;padding:10px;margin-top:8px}
@media(max-width:900px){.shell{grid-template-columns:1fr}}
</style>
</head>
<body>
<main class="shell">
<section><h1>CCamera</h1><img class="video" src="/stream.mjpg" alt="Live camera stream"><div class="status" id="status">Connecting...</div></section>
<aside class="panel"><h2>Actions</h2><label>Name</label><input id="name" autocomplete="off"><div class="actions"><button class="primary" onclick="sendCommand('enroll')">Enroll</button><button class="danger" onclick="confirmDelete()">Delete</button><button onclick="refreshStatus()">Refresh</button><button class="dark" onclick="sendCommand('quit')">Quit</button></div><h2>People</h2><div id="people"></div></aside>
</main>
<script>
async function sendCommand(type){const name=document.getElementById('name').value;const body=(type==='enroll'||type==='delete')?{type,name}:{type};const r=await fetch('/api/commands',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify(body)});if(!r.ok){const j=await r.json();alert(j.error||'Command failed')}await refreshStatus()}
function confirmDelete(){const name=document.getElementById('name').value.trim();if(name&&confirm('Delete '+name+' from the local database?'))sendCommand('delete')}
async function refreshStatus(){const r=await fetch('/api/status');const j=await r.json();document.getElementById('status').textContent=(j.running?'Running':'Stopped')+' | FPS '+Math.round(j.fps)+' | '+j.status;document.getElementById('people').innerHTML=j.people.map(p=>'<div class="person"><b>'+p.name+'</b><span>'+p.templates+' templates</span></div>').join('')||'<p>No enrolled people</p>'}
setInterval(refreshStatus,1000);refreshStatus();
</script>
</body>
</html>)HTML";

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
                                   WebStreamProvider stream_provider)
    : status_provider_(std::move(status_provider)),
      command_handler_(std::move(command_handler)),
      stream_provider_(std::move(stream_provider)) {}

HttpResponse RuntimeWebRouter::handle(const HttpRequest& request) const {
    if (request.path == "/" && request.method == "GET") {
        return index();
    }
    if (request.path == "/api/status") {
        return request.method == "GET" ? status() : text_response(405, "Method Not Allowed", "Method not allowed");
    }
    if (request.path == "/api/commands") {
        return request.method == "POST" ? command(request.body) : text_response(405, "Method Not Allowed", "Method not allowed");
    }
    if (request.path == "/stream.mjpg") {
        return request.method == "GET" ? stream() : text_response(405, "Method Not Allowed", "Method not allowed");
    }
    return text_response(404, "Not Found", "Not found");
}

HttpResponse RuntimeWebRouter::index() const {
    HttpResponse response;
    response.content_type = "text/html; charset=utf-8";
    response.body = kDashboardHtml;
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

}  // namespace ccamera
