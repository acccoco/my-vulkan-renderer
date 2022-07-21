#include <iostream>
#include "./application.hpp"


int main()
{
    Application app;

    try {
        app.run();
    } catch (const std::exception &e) {
        std::cout << "exception: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
