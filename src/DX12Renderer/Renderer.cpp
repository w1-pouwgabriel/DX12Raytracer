#include "Renderer.h"
#include <iostream>

Renderer::Renderer()
    : m_width(0)
    , m_height(0)
{
}

Renderer::~Renderer() {
    WaitForGPU();
    CleanupRenderTargets();
}

bool Renderer::Initialize(HWND hwnd, uint32_t width, uint32_t height, bool enableDebugLayer) {
    m_width = width;
    m_height = height;

    if (!m_device.Initialize(enableDebugLayer)) {
        std::cerr << "[Renderer] Failed to initialize device\n";
        return false;
    }

    if (!CreateSwapChain(hwnd, width, height)) {
        std::cerr << "[Renderer] Failed to create swap chain\n";
        return false;
    }

    if (!CreateRTVHeap()) {
        std::cerr << "[Renderer] Failed to create RTV heap\n";
        return false;
    }

    if (!CreateRenderTargets()) {
        std::cerr << "[Renderer] Failed to create render targets\n";
        return false;
    }

    if (FAILED(m_device.GetDevice()->CreateCommandAllocator(
        D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocator)))) {
        std::cerr << "[Renderer] Failed to create command allocator\n";
        return false;
    }

    if (FAILED(m_device.GetDevice()->CreateCommandList(
        0, D3D12_COMMAND_LIST_TYPE_DIRECT,
        m_commandAllocator.Get(), nullptr,
        IID_PPV_ARGS(&m_commandList)))) {
        std::cerr << "[Renderer] Failed to create command list\n";
        return false;
    }
    m_commandList->Close();

    if (!m_pipeline.Initialize(&m_device, width, height)) {
        std::cerr << "[Renderer] Failed to initialize raytracing pipeline\n";
        return false;
    }

    std::cout << "[Renderer] Initialized\n";
    return true;
}

void Renderer::Render() {
    m_commandAllocator->Reset();
    m_commandList->Reset(m_commandAllocator.Get(), nullptr);

    // 1. Shoot rays > UAV  (no-op until shaders are added, shows black)
    m_pipeline.Dispatch(m_commandList.Get());

    // 2. Blit UAV > back buffer
    CopyUAVToBackBuffer(m_pipeline.GetOutputUAV());

    // 3. Submit and flip
    m_commandList->Close();
    ID3D12CommandList* lists[] = { m_commandList.Get() };
    m_device.GetCommandQueue()->ExecuteCommandLists(1, lists);

    m_swapChain->Present(1, 0);

    m_device.WaitForGPU();
}

void Renderer::CopyUAVToBackBuffer(ID3D12Resource* uavOutput) {
    uint32_t bbIdx = m_swapChain->GetCurrentBackBufferIndex();

    // UAV arrives in COPY_SOURCE (Dispatch always leaves it there).
    // Only the back buffer needs transitioning.
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = m_renderTargets[bbIdx].Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
    m_commandList->ResourceBarrier(1, &barrier);

    m_commandList->CopyResource(m_renderTargets[bbIdx].Get(), uavOutput);

    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
    m_commandList->ResourceBarrier(1, &barrier);
}

void Renderer::WaitForGPU() {
    m_device.WaitForGPU();
}

void Renderer::Resize(uint32_t width, uint32_t height) {
    if (width == m_width && height == m_height) return;

    m_width = width;
    m_height = height;

    WaitForGPU();
    CleanupRenderTargets();

    if (FAILED(m_swapChain->ResizeBuffers(
        BACK_BUFFER_COUNT, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, 0))) {
        std::cerr << "[Renderer] Failed to resize swap chain\n";
        return;
    }

    CreateRenderTargets();
    m_pipeline.Resize(width, height);

    std::cout << "[Renderer] Resized to " << width << "x" << height << "\n";
}

uint32_t Renderer::GetCurrentBackBufferIndex() const {
    return m_swapChain->GetCurrentBackBufferIndex();
}

bool Renderer::CreateSwapChain(HWND hwnd, uint32_t width, uint32_t height) {
    DXGI_SWAP_CHAIN_DESC1 desc = {};
    desc.Width = width;
    desc.Height = height;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = BACK_BUFFER_COUNT;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;

    ComPtr<IDXGISwapChain1> sc1;
    if (FAILED(m_device.GetFactory()->CreateSwapChainForHwnd(
        m_device.GetCommandQueue(), hwnd, &desc, nullptr, nullptr, &sc1)))
        return false;

    return SUCCEEDED(sc1.As(&m_swapChain));
}

bool Renderer::CreateRTVHeap() {
    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    desc.NumDescriptors = BACK_BUFFER_COUNT;
    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

    if (FAILED(m_device.GetDevice()->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_rtvHeap))))
        return false;

    m_rtvDescriptorSize = m_device.GetDevice()->GetDescriptorHandleIncrementSize(
        D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    return true;
}

bool Renderer::CreateRenderTargets() {
    D3D12_CPU_DESCRIPTOR_HANDLE rtv = m_rtvHeap->GetCPUDescriptorHandleForHeapStart();
    for (uint32_t i = 0; i < BACK_BUFFER_COUNT; i++) {
        if (FAILED(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_renderTargets[i]))))
            return false;
        m_device.GetDevice()->CreateRenderTargetView(m_renderTargets[i].Get(), nullptr, rtv);
        rtv.ptr += m_rtvDescriptorSize;
    }
    return true;
}

void Renderer::CleanupRenderTargets() {
    for (auto& rt : m_renderTargets) rt.Reset();
}