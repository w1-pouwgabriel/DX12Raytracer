#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include "DX12/Renderer.h"
#include <iostream>

#define WindowWidth 720
#define WindowHeight 480

int main() {
    // Initialize GLFW
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(WindowWidth, WindowHeight, "DX12 Raytracer", nullptr, nullptr);
    HWND hwnd = glfwGetWin32Window(window);

    // Create renderer (only class you interact with!)
    Renderer renderer;
    if (!renderer.Initialize(hwnd, WindowWidth, WindowHeight, true)) {
        return -1;
    }

    // Main loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        
        // renderer.BeginFrame();
        // renderer.EndFrame();
        // renderer.Present();
        renderer.Render();
    }

    renderer.WaitForGPU();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}