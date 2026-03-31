#pragma once

#include "Device.h"
#include "RaytracingPipeline.h"
#include "AccelerationStructure.h"
#include <dxgi1_6.h>
#include <cstdint>
#include <vector>

class Renderer {
public:
    Renderer();
    ~Renderer();

    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    bool Initialize(HWND hwnd, uint32_t width, uint32_t height, bool enableDebugLayer = true);

    void Render();
    void WaitForGPU();
    void Resize(uint32_t width, uint32_t height);

    // Return by reference — returning by value would copy a non-copyable type
    RaytracingPipeline& GetPipeline() { return m_pipeline; }

    Device& GetDevice() { return m_device; }
    ID3D12Device5* GetD3D12Device()    const { return m_device.GetDevice(); }
    ID3D12CommandQueue* GetCommandQueue()   const { return m_device.GetCommandQueue(); }
    IDXGISwapChain3* GetSwapChain()      const { return m_swapChain.Get(); }
    uint32_t            GetCurrentBackBufferIndex() const;

private:
    bool CreateSwapChain(HWND hwnd, uint32_t width, uint32_t height);
    bool CreateRTVHeap();
    bool CreateRenderTargets();
    void CleanupRenderTargets();
    void CopyUAVToBackBuffer(ID3D12Resource* uavOutput);

    // Builds all BLASes + TLAS and calls m_pipeline.SetTLAS()
    // Called once from Initialize(), after the pipeline is ready
    bool BuildScene();

    // -- Core infrastructure ---------------------------------------------------
    Device                              m_device;
    ComPtr<ID3D12CommandAllocator>      m_commandAllocator;
    ComPtr<ID3D12GraphicsCommandList4>  m_commandList;

    // -- Presentation ----------------------------------------------------------
    ComPtr<IDXGISwapChain3>      m_swapChain;
    ComPtr<ID3D12DescriptorHeap> m_rtvHeap;
    uint32_t                     m_rtvDescriptorSize = 0;
    static const uint32_t        BACK_BUFFER_COUNT = 2;
    ComPtr<ID3D12Resource>       m_renderTargets[BACK_BUFFER_COUNT];

    // -- Raytracing ------------------------------------------------------------
    RaytracingPipeline m_pipeline;

    // -- Scene — owned here, never in RaytracingPipeline ----------------------
    BLAS                    m_cubeBLAS;
    BLAS                    m_mirrorBLAS;
    BLAS                    m_floorBLAS;
    ComPtr<ID3D12Resource>  m_tlas;         // keep alive — GPU reads it every frame

    // -- State -----------------------------------------------------------------
    uint32_t m_width = 0;
    uint32_t m_height = 0;
};