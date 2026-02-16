#include <iostream>
#include <wrl/client.h>

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include "DX12Device.h"

using Microsoft::WRL::ComPtr;

int main()
{
    // Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    // Tell GLFW not to create an OpenGL context
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    
    // Create window
    GLFWwindow* window = glfwCreateWindow(1280, 720, "DX12 Raytracer", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    // Get Win32 HWND from GLFW
    HWND hwnd = glfwGetWin32Window(window);

    // Initialize DX12 Device
    DX12Device device;
    if (!device.Initialize(true)) {  // true = enable debug layer
        std::cerr << "Failed to initialize DX12 device" << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }

    // Check raytracing support
    if (device.GetRaytracingTier() == D3D12_RAYTRACING_TIER_NOT_SUPPORTED) {
        std::cerr << "WARNING: Raytracing not supported on this GPU!" << std::endl;
        std::cerr << "Continuing anyway, but raytracing features won't work." << std::endl;
    } else {
        std::cout << "Raytracing supported! Tier: " << static_cast<int>(device.GetRaytracingTier()) << std::endl;
    }

    // Create swap chain
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.Width = 1280;
    swapChainDesc.Height = 720;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = 2;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    ComPtr<IDXGISwapChain1> swapChain1;
    HRESULT hr = device.GetFactory()->CreateSwapChainForHwnd(
        device.GetCommandQueue(),
        hwnd,
        &swapChainDesc,
        nullptr,
        nullptr,
        &swapChain1
    );

    if (FAILED(hr)) {
        std::cerr << "Failed to create swap chain" << std::endl;
        glfwDestroyWindow(window);
        glfwTerminate();
        return -1;
    }
    
    ComPtr<IDXGISwapChain3> swapChain;
    swapChain1.As(&swapChain);

    std::cout << "DirectX 12 initialized successfully!" << std::endl;
    std::cout << "Press ESC to exit" << std::endl;

    // Main loop
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        
        // Check for ESC key
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }
        
        // Your rendering code will go here
        
        // Present
        swapChain->Present(1, 0);
    }

    // Wait for GPU to finish before cleanup
    device.WaitForGPU();

    // Cleanup
    glfwDestroyWindow(window);
    glfwTerminate();

    std::cout << "Application closed cleanly" << std::endl;
    return 0;
}