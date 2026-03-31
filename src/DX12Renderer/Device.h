#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <stdexcept>
#include <iostream>

template<typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;

class Device {
public:
    Device();
    ~Device();

    // Disable copy
    Device(const Device&) = delete;
    Device& operator=(const Device&) = delete;

    // Initialize the device
    bool Initialize(bool enableDebugLayer);

    // Check for raytracing support
    bool CheckRaytracingSupport();

    // Getters
    ID3D12Device5* GetDevice() const { return m_device.Get(); }
    ID3D12CommandQueue* GetCommandQueue() const { return m_commandQueue.Get(); }
    IDXGIFactory4* GetFactory() const { return m_factory.Get(); }
    IDXGIAdapter1* GetAdapter() const { return m_adapter.Get(); }
    D3D12_RAYTRACING_TIER GetRaytracingTier() const { return m_raytracingTier; }

    // Synchronization
    void WaitForGPU();

    // Utility: Create command allocator
    ComPtr<ID3D12CommandAllocator> CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE type);

    // Utility: Create command list
    ComPtr<ID3D12GraphicsCommandList4> CreateCommandList(D3D12_COMMAND_LIST_TYPE type);

private:
    // Core DX12 objects
    ComPtr<ID3D12Device5> m_device;
    ComPtr<ID3D12CommandQueue> m_commandQueue;
    ComPtr<IDXGIFactory4> m_factory;
    ComPtr<IDXGIAdapter1> m_adapter;

    // Synchronization
    ComPtr<ID3D12Fence> m_fence;
    UINT64 m_fenceValue;
    HANDLE m_fenceEvent;

    // Capabilities
    D3D12_RAYTRACING_TIER m_raytracingTier;

    // Helper functions
    bool CreateDXGIFactory();
    bool SelectAdapter();
    bool CreateDevice();
    bool CreateCommandQueue();
    bool CreateSynchronizationObjects();
    void EnableDebugLayer();
};

