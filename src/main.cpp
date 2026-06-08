#include "app/AppConfig.hpp"
#include "app/StartupMode.hpp"

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
#include <unistd.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#ifdef __linux__
#include <array>
#endif
#endif

namespace {

#ifndef CCAMERA_RUNTIME_MODULE_NAME
#error "CCAMERA_RUNTIME_MODULE_NAME must be defined by the build"
#endif

using RunAppFunction = int (*)(int, char**);

std::filesystem::path executable_directory(char** argv);

void require_existing_file(const char* name, const std::string& path) {
    if (!std::filesystem::exists(path)) {
        throw std::runtime_error(std::string(name) + " does not exist: " + path);
    }
}

void validate_runtime_inputs(const ccamera::AppConfig& config) {
    require_existing_file("detector model", config.detector_model);
    require_existing_file("recognizer model", config.recognizer_model);
}

struct DefaultStartupConfig {
    ccamera::AppConfig config;
    std::filesystem::path resource_root;
    std::filesystem::path config_path;
};

std::string path_string(const std::filesystem::path& path) {
    return path.lexically_normal().string();
}

void append_unique_directory(std::vector<std::filesystem::path>& directories, std::filesystem::path directory) {
    if (directory.empty()) {
        return;
    }

    directory = std::filesystem::absolute(directory).lexically_normal();
    for (const auto& existing : directories) {
        if (existing == directory) {
            return;
        }
    }
    directories.push_back(std::move(directory));
}

std::vector<std::filesystem::path> default_resource_directories(char** argv) {
    std::vector<std::filesystem::path> directories;
    append_unique_directory(directories, std::filesystem::current_path());

    const std::filesystem::path executable_dir = executable_directory(argv);
    append_unique_directory(directories, executable_dir.parent_path());
    append_unique_directory(directories, executable_dir);
    return directories;
}

void absolutize_default_paths(ccamera::AppConfig& config, const std::filesystem::path& base_directory) {
    auto absolutize = [&](std::string& value) {
        const std::filesystem::path path(value);
        if (path.is_relative()) {
            value = path_string(base_directory / path);
        }
    };

    absolutize(config.detector_model);
    absolutize(config.recognizer_model);
    absolutize(config.database_path);
}

DefaultStartupConfig default_startup_config(char** argv) {
    const auto directories = default_resource_directories(argv);
    std::vector<std::string> diagnostics;
    for (const auto& directory : directories) {
        const std::filesystem::path example_path = directory / "config" / "app.example.json";
        if (std::filesystem::exists(example_path)) {
            try {
                ccamera::AppConfig config = ccamera::load_app_config(path_string(example_path));
                absolutize_default_paths(config, directory);
                if (std::filesystem::exists(config.detector_model) && std::filesystem::exists(config.recognizer_model)) {
                    return {std::move(config), directory, std::filesystem::absolute(example_path).lexically_normal()};
                }
                diagnostics.push_back(
                    "default config " + path_string(example_path) +
                    " does not point at existing committed OpenCV Zoo models");
            } catch (const std::exception& error) {
                diagnostics.push_back(
                    "failed to load default config " + path_string(example_path) + ": " + error.what());
            }
        }
    }

    std::string message =
        "failed to find default resource root with config/app.example.json and committed OpenCV Zoo models";
    if (!diagnostics.empty()) {
        message += "; candidates checked:";
        for (const auto& diagnostic : diagnostics) {
            message += " " + diagnostic + ";";
        }
        message.pop_back();
    }
    throw std::runtime_error(message);
}

std::string argv0_or_default(char** argv) {
    if (argv != nullptr && argv[0] != nullptr) {
        return argv[0];
    }
    return "ccamera.exe";
}

std::vector<char*> mutable_argv(std::vector<std::string>& args) {
    std::vector<char*> result;
    result.reserve(args.size());
    for (auto& arg : args) {
        result.push_back(arg.data());
    }
    return result;
}

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

        ccamera::AppConfig config;
        const ccamera::StartupMode startup_mode = ccamera::classify_startup_mode(argc, argv);
        if (startup_mode == ccamera::StartupMode::DefaultRuntime) {
            DefaultStartupConfig startup_config = default_startup_config(argv);
            std::filesystem::current_path(startup_config.resource_root);
            config = std::move(startup_config.config);
            generated_args = {argv0_or_default(argv), "--config", path_string(startup_config.config_path)};
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
