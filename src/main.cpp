#include "app/AppController.hpp"

int main() {
    ccamera::AppConfig config;
    ccamera::AppController app(config);
    return app.run();
}
