#include "Device.h"

Device::Device()
    : m_fenceValue(0)
    , m_fenceEvent(nullptr)
    , m_raytracingTier(D3D12_RAYTRACING_TIER_NOT_SUPPORTED)
{
}

Device::~Device() {
    WaitForGPU();
    
    if (m_fenceEvent) {
        CloseHandle(m_fenceEvent);
    }
}

bool Device::Initialize(bool enableDebugLayer) {
    if (enableDebugLayer) {
        EnableDebugLayer();
    }

    if (!CreateDXGIFactory()) {
        std::cerr << "Failed to create DXGI factory" << std::endl;
        return false;
    }

    if (!SelectAdapter()) {
        std::cerr << "Failed to select adapter" << std::endl;
        return false;
    }

    if (!CreateDevice()) {
        std::cerr << "Failed to create D3D12 device" << std::endl;
        return false;
    }

    if (!CreateCommandQueue()) {
        std::cerr << "Failed to create command queue" << std::endl;
        return false;
    }

    if (!CreateSynchronizationObjects()) {
        std::cerr << "Failed to create synchronization objects" << std::endl;
        return false;
    }

    // Check raytracing support
    if (CheckRaytracingSupport()) {
        std::cout << "Raytracing Tier: " << static_cast<int>(m_raytracingTier) << std::endl;
    } else {
        std::cout << "Warning: Raytracing not supported on this device" << std::endl;
    }

    std::cout << "DX12 Device initialized successfully" << std::endl;
    return true;
}

void Device::EnableDebugLayer() {
#ifdef _DEBUG
    ComPtr<ID3D12Debug> debugController;
    if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
        debugController->EnableDebugLayer();
        std::cout << "Debug layer enabled" << std::endl;

        // Enable additional debug layers
        ComPtr<ID3D12Debug1> debugController1;
        if (SUCCEEDED(debugController.As(&debugController1))) {
            debugController1->SetEnableGPUBasedValidation(true);
        }
    }
#endif
}

bool Device::CreateDXGIFactory() {
    UINT dxgiFactoryFlags = 0;

#ifdef _DEBUG
    dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif

    return SUCCEEDED(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&m_factory)));
}

bool Device::SelectAdapter() {
    ComPtr<IDXGIAdapter1> adapter;
    SIZE_T maxDedicatedVideoMemory = 0;

    for (UINT i = 0; m_factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i) {
        DXGI_ADAPTER_DESC1 desc;
        adapter->GetDesc1(&desc);

        // Skip software adapters
        if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
            continue;
        }

        // Check if adapter supports D3D12
        ComPtr<ID3D12Device> testDevice;
        if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, 
            IID_PPV_ARGS(&testDevice)))) {
            
            // Select adapter with most video memory
            if (desc.DedicatedVideoMemory > maxDedicatedVideoMemory) {
                maxDedicatedVideoMemory = desc.DedicatedVideoMemory;
                m_adapter = adapter;

                std::wcout << L"Selected GPU: " << desc.Description << std::endl;
                std::cout << "Video Memory: " << (desc.DedicatedVideoMemory / 1024 / 1024) << " MB" << std::endl;
            }
        }
    }

    return m_adapter != nullptr;
}

bool Device::CreateDevice() {
    return SUCCEEDED(D3D12CreateDevice(
        m_adapter.Get(),
        D3D_FEATURE_LEVEL_11_0,
        IID_PPV_ARGS(&m_device)
    ));
}

bool Device::CreateCommandQueue() {
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    queueDesc.NodeMask = 0;

    return SUCCEEDED(m_device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)));
}

bool Device::CreateSynchronizationObjects() {
    if (FAILED(m_device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)))) {
        return false;
    }

    m_fenceValue = 1;
    m_fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
    
    return m_fenceEvent != nullptr;
}

bool Device::CheckRaytracingSupport() {
    D3D12_FEATURE_DATA_D3D12_OPTIONS5 options5 = {};
    
    if (SUCCEEDED(m_device->CheckFeatureSupport(
        D3D12_FEATURE_D3D12_OPTIONS5,
        &options5,
        sizeof(options5)))) {
        
        m_raytracingTier = options5.RaytracingTier;
        return m_raytracingTier >= D3D12_RAYTRACING_TIER_1_0;
    }

    return false;
}

void Device::WaitForGPU() {
    const UINT64 fence = m_fenceValue;
    m_commandQueue->Signal(m_fence.Get(), fence);
    m_fenceValue++;

    if (m_fence->GetCompletedValue() < fence) {
        m_fence->SetEventOnCompletion(fence, m_fenceEvent);
        WaitForSingleObject(m_fenceEvent, INFINITE);
    }
}

ComPtr<ID3D12CommandAllocator> Device::CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE type) {
    ComPtr<ID3D12CommandAllocator> allocator;
    
    if (FAILED(m_device->CreateCommandAllocator(type, IID_PPV_ARGS(&allocator)))) {
        throw std::runtime_error("Failed to create command allocator");
    }

    return allocator;
}

ComPtr<ID3D12GraphicsCommandList4> Device::CreateCommandList(D3D12_COMMAND_LIST_TYPE type) {
    auto allocator = CreateCommandAllocator(type);
    ComPtr<ID3D12GraphicsCommandList4> commandList;

    if (FAILED(m_device->CreateCommandList(
        0,
        type,
        allocator.Get(),
        nullptr,
        IID_PPV_ARGS(&commandList)))) {
        throw std::runtime_error("Failed to create command list");
    }

    return commandList;
}