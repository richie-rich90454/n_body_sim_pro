#include "application/Application.hpp"

#include <cstdio>
#include <exception>

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    try {
        hpcsim::application::Application application;
        return application.run();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "HPCSim: fatal error: %s\n", error.what());
        return 1;
    }
}
