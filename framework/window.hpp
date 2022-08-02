#pragma once
#include "vk_common.hpp"


namespace Hiss
{
class Window
{
private:
    struct UserData
    {
        bool resized{false};
        int  width{};    // window 的尺寸，单位并不是 pixel
        int  height{};
    };


    GLFWwindow *_window;
    UserData    _user_data{};


    static void callback_window_resize(GLFWwindow *window, int width, int height);


public:
    Window(const std::string &title, int width, int height);
    ~Window();


    GLFWwindow        *handle_get() { return _window; }
    [[nodiscard]] bool has_resized() const { return _user_data.resized; }
    void               resize_state_clear() { _user_data.resized = false; }
    void               wait_exit_minimize();
    vk::SurfaceKHR     surface_create(vk::Instance instance);
    vk::Extent2D       extent_get() const;


    static std::vector<const char *> extensions_get();
};
}    // namespace Hiss
