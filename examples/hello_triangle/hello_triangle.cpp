#include <iostream>
#include "hello_triangle.hpp"


int main()
{
    try
    {
        Application app;
        app.run();
    } catch (const std::exception &e)
    {
        std::cout << "exception: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
