#include "Renderer.h"
#include <iostream>

Renderer::Renderer()
    : m_width(0)
    , m_height(0)
    , m_frameIndex(0)
{
}

Renderer::~Renderer() {
    WaitForGPU();
    CleanupRenderTargets();
}

bool Renderer::Initialize(HWND hwnd, uint32_t width, uint32_t height, bool enableDebugLayer) {
    m_width = width;
    m_height = height;

    // Initialize device first
    if (!m_device.Initialize(enableDebugLayer)) {
        std::cerr << "Failed to initialize DX12 device" << std::endl;
        return false;
    }

    // Check raytracing support
    if (m_device.GetRaytracingTier() == D3D12_RAYTRACING_TIER_NOT_SUPPORTED) {
        std::cerr << "WARNING: Raytracing not supported on this GPU!" << std::endl;
    }

    // Create swap chain
    if (!CreateSwapChain(hwnd, width, height)) {
        std::cerr << "Failed to create swap chain" << std::endl;
        return false;
    }

    // Create RTV descriptor heap
    if (!CreateRTVHeap()) {
        std::cerr << "Failed to create RTV descriptor heap" << std::endl;
        return false;
    }

    // Create render targets
    if (!CreateRenderTargets()) {
        std::cerr << "Failed to create render targets" << std::endl;
        return false;
    }

    // Create allocator (just once)
    m_device.GetDevice()->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        IID_PPV_ARGS(&m_commandAllocator)
    );
    
    // Create command list (just once)
    m_device.GetDevice()->CreateCommandList(
        0,
        D3D12_COMMAND_LIST_TYPE_DIRECT,
        m_commandAllocator.Get(),
        nullptr,
        IID_PPV_ARGS(&m_commandList)
    );
    
    m_commandList->Close();  // Start closed

    std::cout << "Renderer initialized successfully!" << std::endl;
    return true;
}

bool Renderer::CreateSwapChain(HWND hwnd, uint32_t width, uint32_t height) {
    DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
    swapChainDesc.Width = width;
    swapChainDesc.Height = height;
    swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc.SampleDesc.Count = 1;
    swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc.BufferCount = BACK_BUFFER_COUNT;
    swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    ComPtr<IDXGISwapChain1> swapChain1;
    HRESULT hr = m_device.GetFactory()->CreateSwapChainForHwnd(
        m_device.GetCommandQueue(),
        hwnd,
        &swapChainDesc,
        nullptr,
        nullptr,
        &swapChain1
    );

    if (FAILED(hr)) {
        return false;
    }

    return SUCCEEDED(swapChain1.As(&m_swapChain));
}

bool Renderer::CreateRenderTargets() {
    // Get the first descriptor handle
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();

    // Create a RTV for each back buffer
    for (uint32_t i = 0; i < BACK_BUFFER_COUNT; i++) {
        // Get the back buffer resource
        if (FAILED(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_renderTargets[i])))) {
            return false;
        }

        // Create render target view for this buffer
        m_device.GetDevice()->CreateRenderTargetView(
            m_renderTargets[i].Get(),
            nullptr,  // Use default RTV desc
            rtvHandle
        );

        // Move to next descriptor
        rtvHandle.ptr += m_rtvDescriptorSize;
    }
    return true;
}

void Renderer::CleanupRenderTargets() {
    for (uint32_t i = 0; i < BACK_BUFFER_COUNT; i++) {
        m_renderTargets[i].Reset();
    }
}

bool Renderer::CreateRTVHeap() {
    // Describe the RTV descriptor heap
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
    rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.NumDescriptors = BACK_BUFFER_COUNT;  // One for each back buffer
    rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    rtvHeapDesc.NodeMask = 0;

    // Create the heap
    HRESULT hr = m_device.GetDevice()->CreateDescriptorHeap(
        &rtvHeapDesc,
        IID_PPV_ARGS(&m_rtvHeap)
    );

    if (FAILED(hr)) {
        return false;
    }

    // Get descriptor size (varies by GPU)
    m_rtvDescriptorSize = m_device.GetDevice()->GetDescriptorHandleIncrementSize(
        D3D12_DESCRIPTOR_HEAP_TYPE_RTV
    );

    return true;
}

void Renderer::Render() {
    m_commandAllocator->Reset();
    m_commandList->Reset(m_commandAllocator.Get(), nullptr);
    
    // Get current back buffer
    uint32_t backBufferIndex = m_swapChain->GetCurrentBackBufferIndex();
    
    // Transition to render target
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = m_renderTargets[backBufferIndex].Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
    m_commandList->ResourceBarrier(1, &barrier);
    
    // Get RTV handle for current back buffer
    D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    rtvHandle.ptr += backBufferIndex * m_rtvDescriptorSize;
    
    // Clear to RED (it's working!)
    float clearColor[] = { 1.0f, 0.0f, 0.0f, 1.0f };
    m_commandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
    
    // Transition back to present
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    m_commandList->ResourceBarrier(1, &barrier);
    
    m_commandList->Close();
    
    ID3D12CommandList* lists[] = { m_commandList.Get() };
    m_device.GetCommandQueue()->ExecuteCommandLists(1, lists);
    
    m_swapChain->Present(1, 0);
    m_device.WaitForGPU();
}

/*
void Renderer::BeginFrame() {
    // Get the current back buffer index for this frame
    // Swap chain alternates between buffers (double/triple buffering)
    m_frameIndex = m_swapChain->GetCurrentBackBufferIndex();
    
    // Reset command allocator and list here
    // m_commandAllocator->Reset();
    // m_commandList->Reset(m_commandAllocator.Get(), nullptr);
    
    // Set up frame-specific state:
    // - Transition render targets to correct state
    // - Set render targets
    // - Set viewport and scissor rect
    // - Clear render targets
}

void Renderer::EndFrame() {
    // Close command list after recording all commands
    // m_commandList->Close();
    
    // Execute command list on GPU
    // ID3D12CommandList* lists[] = { m_commandList.Get() };
    // m_device.GetCommandQueue()->ExecuteCommandLists(1, lists);
    
    // Signal fence for synchronization
    // const uint64_t fenceValue = m_fenceValue;
    // m_device.GetCommandQueue()->Signal(m_fence.Get(), fenceValue);
    // m_fenceValue++;
}

void Renderer::Present() {
    // Present the rendered frame to the screen
    // Parameters: (SyncInterval, Flags)
    // SyncInterval: 0 = no vsync, 1 = vsync (60fps), 2 = 30fps, etc.
    // Flags: 0 = normal, DXGI_PRESENT_DO_NOT_WAIT, etc.
    m_swapChain->Present(1, 0);
    
    // After present, swap chain automatically switches to next back buffer
    // Next frame will use different buffer (double buffering)
}
*/

void Renderer::WaitForGPU() {
    m_device.WaitForGPU();
}

void Renderer::Resize(uint32_t width, uint32_t height) {
    if (width == m_width && height == m_height) {
        return;
    }

    m_width = width;
    m_height = height;

    // Wait for GPU to finish
    WaitForGPU();

    // Release old render targets
    CleanupRenderTargets();

    // Resize swap chain buffers
    HRESULT hr = m_swapChain->ResizeBuffers(
        BACK_BUFFER_COUNT,
        width,
        height,
        DXGI_FORMAT_R8G8B8A8_UNORM,
        0
    );

    if (FAILED(hr)) {
        std::cerr << "Failed to resize swap chain" << std::endl;
        return;
    }

    // Recreate render targets
    CreateRenderTargets();

    std::cout << "Resized to " << width << "x" << height << std::endl;
}

uint32_t Renderer::GetCurrentBackBufferIndex() const {
    return m_swapChain->GetCurrentBackBufferIndex();
}