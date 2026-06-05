#include "app/AppConfig.hpp"
#include "app/AppController.hpp"

#include <exception>
#include <iostream>

#ifdef _WIN32
#define CCAMERA_RUNTIME_EXPORT __declspec(dllexport)
#else
#define CCAMERA_RUNTIME_EXPORT
#endif

extern "C" CCAMERA_RUNTIME_EXPORT int ccamera_run_app(int argc, char** argv) noexcept {
    try {
        ccamera::AppConfig config = ccamera::load_app_config_from_args(argc, argv);
        ccamera::AppController app(config);
        return app.run();
    } catch (const std::exception& error) {
        std::cerr << "ccamera: " << error.what() << '\n';
        return 1;
    } catch (...) {
        std::cerr << "ccamera: unknown runtime error\n";
        return 1;
    }
}
