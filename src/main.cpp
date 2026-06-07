#include "app/AppConfig.hpp"
#include "ui/StartupWindow.hpp"

#include <exception>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#include <cstdlib>
#include <sstream>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#ifdef __linux__
#include <array>
#include <unistd.h>
#endif
#endif

namespace {

#ifndef CCAMERA_RUNTIME_MODULE_NAME
#error "CCAMERA_RUNTIME_MODULE_NAME must be defined by the build"
#endif

using RunAppFunction = int (*)(int, char**);

void require_existing_file(const char* name, const std::string& path) {
    if (!std::filesystem::exists(path)) {
        throw std::runtime_error(std::string(name) + " does not exist: " + path);
    }
}

void validate_runtime_inputs(const ccamera::AppConfig& config) {
    require_existing_file("detector model", config.detector_model);
    require_existing_file("recognizer model", config.recognizer_model);
}

ccamera::AppConfig default_startup_config() {
    const std::string example_path = "config/app.example.json";
    if (std::filesystem::exists(example_path)) {
        try {
            return ccamera::load_app_config(example_path);
        } catch (const std::exception&) {
        }
    }
    ccamera::AppConfig config;
    ccamera::validate_app_config(config);
    return config;
}

std::filesystem::path temporary_startup_config_path() {
    const auto ticks = ccamera::Clock::now().time_since_epoch().count();
    return std::filesystem::temp_directory_path() / ("ccamera-startup-" + std::to_string(ticks) + ".json");
}

std::vector<char*> mutable_argv(std::vector<std::string>& args) {
    std::vector<char*> result;
    result.reserve(args.size());
    for (auto& arg : args) {
        result.push_back(arg.data());
    }
    return result;
}

class TemporaryConfigCleanup {
public:
    void set(std::filesystem::path path) {
        path_ = std::move(path);
    }

    ~TemporaryConfigCleanup() {
        if (!path_.empty()) {
            std::error_code ignored;
            std::filesystem::remove(path_, ignored);
        }
    }

private:
    std::filesystem::path path_;
};

#ifdef _WIN32
std::string windows_error_message(DWORD error_code) {
    char* buffer = nullptr;
    const DWORD length = FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
        nullptr,
        error_code,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        reinterpret_cast<char*>(&buffer),
        0,
        nullptr);
    std::string message = length > 0 && buffer != nullptr ? std::string(buffer, length) : "unknown error";
    if (buffer != nullptr) {
        LocalFree(buffer);
    }
    while (!message.empty() && (message.back() == '\r' || message.back() == '\n' || message.back() == '.')) {
        message.pop_back();
    }
    return message;
}

std::filesystem::path executable_directory(char** argv) {
    wchar_t path[MAX_PATH];
    const DWORD length = GetModuleFileNameW(nullptr, path, MAX_PATH);
    if (length == 0 || length == MAX_PATH) {
        return std::filesystem::path(argv[0]).parent_path();
    }
    return std::filesystem::path(path).parent_path();
}

int run_runtime_module(int argc, char** argv) {
    const std::filesystem::path module_path = executable_directory(argv) / CCAMERA_RUNTIME_MODULE_NAME;
    HMODULE module = LoadLibraryW(module_path.wstring().c_str());
    if (module == nullptr) {
        throw std::runtime_error(
            "failed to load runtime module " + module_path.string() + ": " +
            windows_error_message(GetLastError()) +
            ". Ensure OpenCV highgui/videoio GUI and video backend DLLs are available beside the executable or on PATH");
    }

    const auto run_app = reinterpret_cast<RunAppFunction>(GetProcAddress(module, "ccamera_run_app"));
    if (run_app == nullptr) {
        const DWORD error_code = GetLastError();
        FreeLibrary(module);
        throw std::runtime_error(
            "failed to find ccamera_run_app in runtime module: " + windows_error_message(error_code));
    }

    const int result = run_app(argc, argv);
    FreeLibrary(module);
    return result;
}
#else
std::filesystem::path fallback_executable_directory(char** argv) {
    if (argv != nullptr && argv[0] != nullptr) {
        const std::filesystem::path executable = argv[0];
        if (executable.is_absolute() || executable.has_parent_path()) {
            return std::filesystem::weakly_canonical(executable).parent_path();
        }

        if (const char* path_env = std::getenv("PATH")) {
            std::stringstream paths(path_env);
            std::string directory;
            while (std::getline(paths, directory, ':')) {
                const std::filesystem::path candidate = std::filesystem::path(directory) / executable;
                if (std::filesystem::exists(candidate)) {
                    return std::filesystem::weakly_canonical(candidate).parent_path();
                }
            }
        }
    }

    return std::filesystem::current_path();
}

std::filesystem::path executable_directory(char** argv) {
#ifdef __linux__
    std::array<char, 4096> buffer{};
    const ssize_t length = readlink("/proc/self/exe", buffer.data(), buffer.size() - 1);
    if (length > 0) {
        buffer[static_cast<std::size_t>(length)] = '\0';
        return std::filesystem::path(buffer.data()).parent_path();
    }
#endif
#ifdef __APPLE__
    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::string path(size, '\0');
    if (_NSGetExecutablePath(path.data(), &size) == 0) {
        return std::filesystem::weakly_canonical(std::filesystem::path(path)).parent_path();
    }
#endif
    return fallback_executable_directory(argv);
}

int run_runtime_module(int argc, char** argv) {
    const std::filesystem::path module_path = executable_directory(argv) / CCAMERA_RUNTIME_MODULE_NAME;
    void* module = dlopen(module_path.string().c_str(), RTLD_NOW);
    if (module == nullptr) {
        throw std::runtime_error(std::string("failed to load runtime module: ") + dlerror());
    }

    const auto run_app = reinterpret_cast<RunAppFunction>(dlsym(module, "ccamera_run_app"));
    if (run_app == nullptr) {
        const char* error = dlerror();
        dlclose(module);
        throw std::runtime_error(std::string("failed to find ccamera_run_app in runtime module: ") +
                                 (error != nullptr ? error : "unknown error"));
    }

    const int result = run_app(argc, argv);
    dlclose(module);
    return result;
}
#endif

}  // namespace

int main(int argc, char** argv) {
    try {
        std::vector<std::string> generated_args;
        std::vector<char*> generated_argv;
        TemporaryConfigCleanup temporary_config;

        ccamera::AppConfig config;
        if (argc <= 1) {
            const auto selected_config = ccamera::show_startup_window(default_startup_config());
            if (!selected_config.has_value()) {
                return 0;
            }
            config = *selected_config;
            const std::filesystem::path generated_config_path = temporary_startup_config_path();
            ccamera::save_app_config(generated_config_path.string(), config);
            temporary_config.set(generated_config_path);
            generated_args = {
                argv != nullptr && argv[0] != nullptr ? argv[0] : "ccamera.exe",
                "--config",
                generated_config_path.string(),
            };
            generated_argv = mutable_argv(generated_args);
            argc = static_cast<int>(generated_argv.size());
            argv = generated_argv.data();
        } else {
            config = ccamera::load_app_config_from_args(argc, argv);
        }

        validate_runtime_inputs(config);
        return run_runtime_module(argc, argv);
    } catch (const std::exception& error) {
        std::cerr << "ccamera: " << error.what() << '\n';
        return 1;
    }
}
