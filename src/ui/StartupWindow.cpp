#include "ui/StartupWindow.hpp"

#include "app/AppConfig.hpp"

#include <filesystem>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <commdlg.h>
#endif

namespace ccamera {

#ifdef _WIN32
namespace {

constexpr int kWindowWidth = 760;
constexpr int kWindowHeight = 560;
constexpr int kLabelX = 24;
constexpr int kFieldX = 180;
constexpr int kButtonX = 628;
constexpr int kRowHeight = 34;
constexpr int kFieldWidth = 430;
constexpr int kFieldHeight = 24;

constexpr int kIdConfigPath = 100;
constexpr int kIdDetectorModel = 101;
constexpr int kIdRecognizerModel = 102;
constexpr int kIdDatabasePath = 103;
constexpr int kIdCameraIndex = 104;
constexpr int kIdFrameWidth = 105;
constexpr int kIdFrameHeight = 106;
constexpr int kIdDetectionThreshold = 107;
constexpr int kIdRecognitionThreshold = 108;
constexpr int kIdEnrollmentSamples = 109;
constexpr int kIdEnrollmentTimeout = 110;
constexpr int kIdLoadConfig = 201;
constexpr int kIdBrowseDetector = 202;
constexpr int kIdBrowseRecognizer = 203;
constexpr int kIdBrowseDatabase = 204;
constexpr int kIdStart = 301;
constexpr int kIdCancel = 302;

std::wstring utf8_to_wide(const std::string& text) {
    if (text.empty()) {
        return {};
    }
    const int length = MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0);
    if (length <= 0) {
        return {};
    }
    std::wstring result(static_cast<std::size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), length);
    return result;
}

std::string wide_to_utf8(const std::wstring& text) {
    if (text.empty()) {
        return {};
    }
    const int length = WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), nullptr, 0, nullptr, nullptr);
    if (length <= 0) {
        return {};
    }
    std::string result(static_cast<std::size_t>(length), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.data(), static_cast<int>(text.size()), result.data(), length, nullptr, nullptr);
    return result;
}

std::string hwnd_text(HWND handle) {
    const int length = GetWindowTextLengthW(handle);
    std::wstring text(static_cast<std::size_t>(length) + 1, L'\0');
    GetWindowTextW(handle, text.data(), length + 1);
    text.resize(static_cast<std::size_t>(length));
    return wide_to_utf8(text);
}

void set_hwnd_text(HWND handle, const std::string& text) {
    SetWindowTextW(handle, utf8_to_wide(text).c_str());
}

std::string to_string(float value) {
    std::ostringstream stream;
    stream << value;
    return stream.str();
}

std::optional<std::string> open_file_dialog(HWND owner, const wchar_t* title, const wchar_t* filter) {
    std::wstring buffer(4096, L'\0');
    OPENFILENAMEW file{};
    file.lStructSize = sizeof(file);
    file.hwndOwner = owner;
    file.lpstrTitle = title;
    file.lpstrFilter = filter;
    file.lpstrFile = buffer.data();
    file.nMaxFile = static_cast<DWORD>(buffer.size());
    file.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    if (!GetOpenFileNameW(&file)) {
        return std::nullopt;
    }
    return wide_to_utf8(buffer.c_str());
}

std::optional<std::string> save_file_dialog(HWND owner, const wchar_t* title, const wchar_t* filter) {
    std::wstring buffer(4096, L'\0');
    OPENFILENAMEW file{};
    file.lStructSize = sizeof(file);
    file.hwndOwner = owner;
    file.lpstrTitle = title;
    file.lpstrFilter = filter;
    file.lpstrFile = buffer.data();
    file.nMaxFile = static_cast<DWORD>(buffer.size());
    file.Flags = OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT;
    if (!GetSaveFileNameW(&file)) {
        return std::nullopt;
    }
    return wide_to_utf8(buffer.c_str());
}

int parse_int(HWND handle, const char* name) {
    try {
        return std::stoi(hwnd_text(handle));
    } catch (const std::exception&) {
        throw std::invalid_argument(std::string(name) + " must be an integer");
    }
}

float parse_float(HWND handle, const char* name) {
    try {
        return std::stof(hwnd_text(handle));
    } catch (const std::exception&) {
        throw std::invalid_argument(std::string(name) + " must be a number");
    }
}

class StartupDialog {
public:
    explicit StartupDialog(AppConfig config) : config_(std::move(config)) {}

    std::optional<AppConfig> run() {
        const HINSTANCE instance = GetModuleHandleW(nullptr);
        WNDCLASSW window_class{};
        window_class.lpfnWndProc = &StartupDialog::window_proc;
        window_class.hInstance = instance;
        window_class.lpszClassName = L"CCameraStartupWindow";
        window_class.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
        window_class.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        RegisterClassW(&window_class);

        window_ = CreateWindowExW(
            0,
            window_class.lpszClassName,
            L"CCamera Startup",
            WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            kWindowWidth,
            kWindowHeight,
            nullptr,
            nullptr,
            instance,
            this);
        if (window_ == nullptr) {
            throw std::runtime_error("failed to create startup window");
        }

        ShowWindow(window_, SW_SHOW);
        UpdateWindow(window_);

        MSG message{};
        while (!done_ && GetMessageW(&message, nullptr, 0, 0) > 0) {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        return result_;
    }

private:
    static LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
        StartupDialog* dialog = nullptr;
        if (message == WM_NCCREATE) {
            const auto* create = reinterpret_cast<CREATESTRUCTW*>(lparam);
            dialog = static_cast<StartupDialog*>(create->lpCreateParams);
            SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(dialog));
            dialog->window_ = window;
        } else {
            dialog = reinterpret_cast<StartupDialog*>(GetWindowLongPtrW(window, GWLP_USERDATA));
        }

        if (dialog == nullptr) {
            return DefWindowProcW(window, message, wparam, lparam);
        }
        return dialog->handle_message(message, wparam, lparam);
    }

    LRESULT handle_message(UINT message, WPARAM wparam, LPARAM) {
        switch (message) {
            case WM_CREATE:
                create_controls();
                refresh_fields();
                return 0;
            case WM_COMMAND:
                handle_command(LOWORD(wparam));
                return 0;
            case WM_CLOSE:
                done_ = true;
                DestroyWindow(window_);
                return 0;
            case WM_DESTROY:
                done_ = true;
                return 0;
            default:
                return DefWindowProcW(window_, message, wparam, 0);
        }
    }

    HWND make_label(const wchar_t* text, int y) {
        return CreateWindowExW(0, L"STATIC", text, WS_CHILD | WS_VISIBLE, kLabelX, y + 4, 160, kFieldHeight, window_, nullptr, nullptr, nullptr);
    }

    HWND make_edit(int id, int y) {
        return CreateWindowExW(
            WS_EX_CLIENTEDGE,
            L"EDIT",
            L"",
            WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL,
            kFieldX,
            y,
            kFieldWidth,
            kFieldHeight,
            window_,
            reinterpret_cast<HMENU>(id),
            nullptr,
            nullptr);
    }

    HWND make_button(int id, const wchar_t* text, int x, int y, int width = 84) {
        return CreateWindowExW(
            0,
            L"BUTTON",
            text,
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            x,
            y,
            width,
            28,
            window_,
            reinterpret_cast<HMENU>(id),
            nullptr,
            nullptr);
    }

    void create_row(int id, const wchar_t* label, HWND& field, int row, int browse_id = 0) {
        const int y = 22 + row * kRowHeight;
        make_label(label, y);
        field = make_edit(id, y);
        if (browse_id != 0) {
            make_button(browse_id, L"Browse", kButtonX, y - 2);
        }
    }

    void create_controls() {
        create_row(kIdConfigPath, L"Config file", config_path_, 0, kIdLoadConfig);
        create_row(kIdDetectorModel, L"Detector model", detector_model_, 1, kIdBrowseDetector);
        create_row(kIdRecognizerModel, L"Recognizer model", recognizer_model_, 2, kIdBrowseRecognizer);
        create_row(kIdDatabasePath, L"Database path", database_path_, 3, kIdBrowseDatabase);
        create_row(kIdCameraIndex, L"Camera index", camera_index_, 4);
        create_row(kIdFrameWidth, L"Frame width", frame_width_, 5);
        create_row(kIdFrameHeight, L"Frame height", frame_height_, 6);
        create_row(kIdDetectionThreshold, L"Detection threshold", detection_threshold_, 7);
        create_row(kIdRecognitionThreshold, L"Recognition threshold", recognition_threshold_, 8);
        create_row(kIdEnrollmentSamples, L"Enrollment samples", enrollment_samples_, 9);
        create_row(kIdEnrollmentTimeout, L"Enrollment timeout", enrollment_timeout_, 10);

        make_button(kIdStart, L"Start", kFieldX, 440, 120);
        make_button(kIdCancel, L"Cancel", kFieldX + 140, 440, 120);
        make_label(L"Command-line startup is unchanged: ccamera.exe --config config/app.example.json", 492);
    }

    void refresh_fields() {
        set_hwnd_text(config_path_, config_path_text_);
        set_hwnd_text(detector_model_, config_.detector_model);
        set_hwnd_text(recognizer_model_, config_.recognizer_model);
        set_hwnd_text(database_path_, config_.database_path);
        set_hwnd_text(camera_index_, std::to_string(config_.camera_index));
        set_hwnd_text(frame_width_, std::to_string(config_.frame_width));
        set_hwnd_text(frame_height_, std::to_string(config_.frame_height));
        set_hwnd_text(detection_threshold_, to_string(config_.detection_threshold));
        set_hwnd_text(recognition_threshold_, to_string(config_.recognition_threshold));
        set_hwnd_text(enrollment_samples_, std::to_string(config_.enrollment_sample_count));
        set_hwnd_text(enrollment_timeout_, std::to_string(config_.enrollment_timeout_seconds));
    }

    void read_fields() {
        config_.detector_model = hwnd_text(detector_model_);
        config_.recognizer_model = hwnd_text(recognizer_model_);
        config_.database_path = hwnd_text(database_path_);
        config_.camera_index = parse_int(camera_index_, "camera_index");
        config_.frame_width = parse_int(frame_width_, "frame_width");
        config_.frame_height = parse_int(frame_height_, "frame_height");
        config_.detection_threshold = parse_float(detection_threshold_, "detection_threshold");
        config_.recognition_threshold = parse_float(recognition_threshold_, "recognition_threshold");
        config_.enrollment_sample_count = parse_int(enrollment_samples_, "enrollment_sample_count");
        config_.enrollment_timeout_seconds = parse_int(enrollment_timeout_, "enrollment_timeout_seconds");
        validate_app_config(config_);
    }

    void show_error(const std::exception& error) {
        MessageBoxW(window_, utf8_to_wide(error.what()).c_str(), L"CCamera", MB_ICONERROR | MB_OK);
    }

    void handle_command(WORD id) {
        try {
            if (id == kIdLoadConfig) {
                if (const auto path = open_file_dialog(window_, L"Load CCamera config", L"JSON config (*.json)\0*.json\0All files\0*.*\0")) {
                    config_ = load_app_config(*path);
                    config_path_text_ = *path;
                    refresh_fields();
                }
            } else if (id == kIdBrowseDetector) {
                if (const auto path = open_file_dialog(window_, L"Select detector ONNX model", L"ONNX model (*.onnx)\0*.onnx\0All files\0*.*\0")) {
                    set_hwnd_text(detector_model_, *path);
                }
            } else if (id == kIdBrowseRecognizer) {
                if (const auto path = open_file_dialog(window_, L"Select recognizer ONNX model", L"ONNX model (*.onnx)\0*.onnx\0All files\0*.*\0")) {
                    set_hwnd_text(recognizer_model_, *path);
                }
            } else if (id == kIdBrowseDatabase) {
                if (const auto path = save_file_dialog(window_, L"Select face database path", L"JSON database (*.json)\0*.json\0All files\0*.*\0")) {
                    set_hwnd_text(database_path_, *path);
                }
            } else if (id == kIdStart) {
                read_fields();
                result_ = config_;
                done_ = true;
                DestroyWindow(window_);
            } else if (id == kIdCancel) {
                done_ = true;
                DestroyWindow(window_);
            }
        } catch (const std::exception& error) {
            show_error(error);
        }
    }

    AppConfig config_;
    std::string config_path_text_;
    HWND window_ = nullptr;
    HWND config_path_ = nullptr;
    HWND detector_model_ = nullptr;
    HWND recognizer_model_ = nullptr;
    HWND database_path_ = nullptr;
    HWND camera_index_ = nullptr;
    HWND frame_width_ = nullptr;
    HWND frame_height_ = nullptr;
    HWND detection_threshold_ = nullptr;
    HWND recognition_threshold_ = nullptr;
    HWND enrollment_samples_ = nullptr;
    HWND enrollment_timeout_ = nullptr;
    bool done_ = false;
    std::optional<AppConfig> result_;
};

}  // namespace

std::optional<AppConfig> show_startup_window(const AppConfig& initial_config) {
    StartupDialog dialog(initial_config);
    return dialog.run();
}

#else

std::optional<AppConfig> show_startup_window(const AppConfig& initial_config) {
    return initial_config;
}

#endif

}  // namespace ccamera
