#pragma once

#include "Device.h"
#include <dxgi1_6.h>
#include <cstdint> // For uint32_t

class Renderer {
public:
    Renderer();
    ~Renderer();

    // Disable copy
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    // Main initialization - call this from your app
    bool Initialize(HWND hwnd, uint32_t width, uint32_t height, bool enableDebugLayer = true);

    // Rendering
    void Render();

    // Utility
    void WaitForGPU();
    void Resize(uint32_t width, uint32_t height);

    // Device access
    Device& GetDevice() { return m_device; }
    ID3D12Device5* GetD3D12Device() const { return m_device.GetDevice(); }
    ID3D12CommandQueue* GetCommandQueue() const { return m_device.GetCommandQueue(); }

    // Swap chain access
    IDXGISwapChain3* GetSwapChain() const { return m_swapChain.Get(); }
    uint32_t GetCurrentBackBufferIndex() const;

private:
    // Initialization helpers
    bool CreateSwapChain(HWND hwnd, uint32_t width, uint32_t height);
    bool CreateRTVHeap();
    bool CreateRenderTargets();
    void CleanupRenderTargets();

    // Core components
    Device m_device;

    // Just use the DX12 types directly for simplicity
    ComPtr<ID3D12CommandAllocator> m_commandAllocator;
    ComPtr<ID3D12GraphicsCommandList4> m_commandList;

    // Descriptor heap for render target views
    ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
    uint32_t m_rtvDescriptorSize;

    // Swap chain and render targets
    ComPtr<IDXGISwapChain3> m_swapChain;
    static const uint32_t BACK_BUFFER_COUNT = 2;
    ComPtr<ID3D12Resource> m_renderTargets[BACK_BUFFER_COUNT];

    // Rendering state
    uint32_t m_width;
    uint32_t m_height;
    uint32_t m_frameIndex;
};