#pragma once

#include <d3d12.h>
#include <wrl/client.h>
#include <cstdint>
#include <vector>
#include <queue>

template<typename T>
using ComPtr = Microsoft::WRL::ComPtr<T>;

// Forward declaration
class Device;

// Manages a pool of command allocators for efficient reuse
class CommandAllocatorPool {
public:
    CommandAllocatorPool(Device* device, D3D12_COMMAND_LIST_TYPE type);
    ~CommandAllocatorPool();

    // Get an allocator (creates new or reuses from pool)
    ComPtr<ID3D12CommandAllocator> RequestAllocator(uint64_t completedFenceValue);

    // Return allocator to pool for reuse
    void DiscardAllocator(uint64_t fenceValue, ComPtr<ID3D12CommandAllocator> allocator);

private:
    struct AllocatorEntry {
        uint64_t fenceValue;
        ComPtr<ID3D12CommandAllocator> allocator;
    };

    Device* m_device;
    D3D12_COMMAND_LIST_TYPE m_type;
    std::queue<AllocatorEntry> m_allocatorPool;
};

// Represents a command list with its allocator
class CommandContext {
public:
    CommandContext(Device* device, D3D12_COMMAND_LIST_TYPE type);
    ~CommandContext();

    // Get the command list for recording
    ID3D12GraphicsCommandList4* GetCommandList() const { return m_commandList.Get(); }
    ID3D12CommandAllocator* GetAllocator() const { return m_allocator.Get(); }

    // Reset the command list for new recording
    void Reset();

    // Close the command list (ready for execution)
    void Close();

    // Execute this context on the command queue
    void Execute(ID3D12CommandQueue* commandQueue);

private:
    Device* m_device;
    D3D12_COMMAND_LIST_TYPE m_type;
    
    ComPtr<ID3D12CommandAllocator> m_allocator;
    ComPtr<ID3D12GraphicsCommandList4> m_commandList;
};