# Runtime HTML UI Design

## Goal

Add a local browser-based runtime UI for CCamera while keeping the existing OpenCV preview window. The browser UI is for normal users: it shows the live camera stream, recognition status, enrolled people, and safe controls for enrollment, deletion, refresh, and quit. Identity data remains local and continues to be saved through the existing `FaceDatabase` path, normally `data/face_db.json`.

## Scope

In scope:

- Start a local web UI server when `AppController::run()` starts.
- Bind the server only to `127.0.0.1` on an automatically selected port.
- Print the local URL to the console and expose it in status output when possible.
- Serve a built-in HTML/CSS/JS dashboard from `GET /`.
- Serve a live MJPEG stream from `GET /stream.mjpg`.
- Serve JSON status from `GET /api/status`.
- Accept JSON commands from `POST /api/commands`.
- Keep the existing OpenCV preview window and current keyboard/mouse controls working.
- Persist enrollment and deletion through the existing `FaceDatabase::save()` flow.

Out of scope:

- LAN, remote, or authenticated access.
- Replacing the OpenCV preview window.
- Startup/configuration editing in the browser.
- Direct browser access to the database file.
- WebSocket streaming.

## Architecture

Add a focused runtime web UI module under `src/web/`. It owns a small HTTP/1.1 server implemented with platform sockets, not a general-purpose web framework. The server supports only the fixed endpoints needed by the app. On Windows it uses Winsock; on non-Windows it uses POSIX sockets. This avoids adding package-manager or network-fetched dependencies.

`AppController` starts the web server after camera/runtime setup begins and stops it during the existing cleanup path. The server receives a thread-safe facade with read-only snapshot callbacks for frames/status/database and a write callback for runtime commands. The server never mutates `FaceDatabase` directly.

JPEG frame encoding uses `cv::imencode(".jpg", ...)`, so the build must explicitly require and link OpenCV `imgcodecs` for the web module. Overlay drawing should reuse the existing recognition overlay behavior so OpenCV preview and browser preview identify faces consistently.

## User Interface

The approved layout is a single-page runtime dashboard:

- Left side: large live camera stream with recognition boxes and labels.
- Header: app name, running/error indicator, FPS.
- Summary tiles: detected face count, enrolled person count, current status.
- Status strip: latest enrollment, camera, or recognition message.
- Right side: name field and action buttons for Enroll, Delete, Refresh, and Quit.
- People panel: enrolled names and template counts from the local database snapshot.
- Delete action: browser asks for confirmation before sending the delete command.

The UI should be restrained and operational rather than decorative. It should work on a normal desktop browser and collapse reasonably on narrower windows by stacking the video above the controls.

## HTTP Contract

`GET /`

- Returns the embedded HTML page.
- Response type: `text/html; charset=utf-8`.

`GET /stream.mjpg`

- Returns `multipart/x-mixed-replace` with JPEG parts.
- Each part is encoded from the latest available frame.
- The stream frame has recognition overlay drawn before encoding.
- If no camera frame is available, serve a generated placeholder frame with the current status text.
- Slow clients must not block camera capture or recognition. The server may drop frames and always encode the latest snapshot.

`GET /api/status`

- Returns JSON:

```json
{
  "running": true,
  "fps": 29.0,
  "status": "Ready",
  "faces": [
    {"name": "Alice", "similarity": 0.93, "known": true}
  ],
  "people": [
    {"name": "Alice", "templates": 15}
  ],
  "database_path": "data/face_db.json"
}
```

- `database_path` is informational. The endpoint must not expose raw embeddings.

`POST /api/commands`

- Request JSON:

```json
{"type": "enroll", "name": "Alice"}
```

- Accepted command types: `enroll`, `delete`, `list`, `quit`.
- `enroll` and `delete` require a non-empty name after trimming whitespace.
- Responses:
  - `200` with `{"ok": true}` when queued.
  - `400` with `{"ok": false, "error": "message"}` for invalid JSON, unsupported command type, or missing name.
  - `503` with `{"ok": false, "error": "web ui is stopping"}` when shutdown is already in progress.

## Runtime Command Flow

The browser sends commands to the web server. The server validates them and appends them to a thread-safe command queue. `AppController::apply_commands()` consumes that queue in the main loop alongside the existing console command loop and preview-window commands.

All effects stay centralized:

- `Enroll` starts `EnrollmentSession`.
- `Delete` calls `FaceDatabase::delete_person()` and `FaceDatabase::save()`.
- `List` reads the database snapshot.
- `Quit` stops the app.

This keeps console, OpenCV preview, and browser controls behaviorally aligned.

## State Flow

The server reads snapshots only:

- Latest frame from `FrameBuffer::latest()`.
- Latest recognition results from `RecognitionResultsStore`.
- Database people from `FaceDatabase::snapshot()`.
- Status message from `AppController` state.
- FPS from the existing runtime loop or a small shared runtime status struct.

The status endpoint should not hold long locks or call model inference. It returns the latest known state.

## Error Handling

- Web server startup failure must not prevent the existing OpenCV runtime from running. Log the failure and set a visible status message when possible.
- Route-not-found returns `404`.
- Unsupported method returns `405`.
- Invalid JSON returns `400`.
- Runtime command failures are caught in `AppController` as they are today and surfaced in status text.
- MJPEG stream client disconnects are normal and should not log noisy errors.
- The server shutdown path must unblock accept/read loops so `AppController` can exit cleanly.

## Security

- Bind only to `127.0.0.1`.
- Do not serve arbitrary files from disk.
- Do not accept path parameters.
- Do not expose embeddings or raw database JSON through the browser API.
- Do not allow browser-side changes to model paths, database paths, thresholds, or camera settings in this feature.

## Testing

Automated tests:

- Unit test command JSON parsing and validation.
- Unit test status JSON serialization with a fake snapshot.
- Unit test HTTP route handling for `/`, `/api/status`, `/api/commands`, unknown route, and unsupported method.
- Unit test MJPEG formatting with a synthetic frame: multipart boundary, JPEG content type, and non-empty JPEG payload.
- Unit test that invalid commands do not enter the command queue.
- Build-level test that OpenCV `imgcodecs` is linked for JPEG encoding.
- Existing `test_runtime_controls`, `test_face_database`, startup/runtime tests, and full CTest remain part of verification.

Manual checks:

- Launch `build/ccamera.exe`.
- Confirm the OpenCV preview window still opens.
- Open the printed local URL.
- Confirm the browser displays the live stream.
- Enroll a name from the browser and verify it appears in the browser people list and persists to `data/face_db.json`.
- Delete a name from the browser after confirmation and verify it is removed locally.
- Quit from the browser and confirm the app exits cleanly.

Manual camera/model checks must be reported as manual checks, not as automated test pass/fail.

## Acceptance Criteria

- Existing OpenCV runtime UI still works.
- Browser UI is available only on localhost.
- Browser stream shows the live camera with recognition overlays.
- Browser status and people list reflect local runtime state.
- Browser commands reuse existing application command effects.
- Enroll/delete changes are persisted through `FaceDatabase::save()`.
- Web UI startup failure does not break the existing OpenCV UI.
- Narrow automated tests and full CTest pass, subject to existing documented manual camera/model limits.
