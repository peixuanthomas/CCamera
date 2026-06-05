#include "app/AppConfig.hpp"
#include "app/AppController.hpp"

#include <exception>
#include <iostream>

int main(int argc, char** argv) {
    try {
        ccamera::AppConfig config = ccamera::load_app_config_from_args(argc, argv);
        ccamera::AppController app(config);
        return app.run();
    } catch (const std::exception& error) {
        std::cerr << "ccamera: " << error.what() << '\n';
        return 1;
    }
}
