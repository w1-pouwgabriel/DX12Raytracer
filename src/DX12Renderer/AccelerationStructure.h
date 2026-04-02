#pragma once
#include "Device.h"
#include <vector>
#include <DirectXMath.h>

// One BLAS wraps the triangle geometry for a single mesh.
// Build it once; reuse it across many TLAS instances.
struct BLAS {
    ComPtr<ID3D12Resource> buffer;      // AS data on the GPU
    D3D12_GPU_VIRTUAL_ADDRESS gpuVA() const { return buffer->GetGPUVirtualAddress(); }

    ComPtr<ID3D12Resource> scratchBuffer;   // ADD THIS - keep alive!
    ComPtr<ID3D12Resource> vertexBuffer;    // ADD THIS - keep alive!
    ComPtr<ID3D12Resource> indexBuffer;     // ADD THIS - keep alive!
};

// One entry in the TLAS — pairs a BLAS with a world transform and an instance ID.
// InstanceID() in the shader returns the id field, used to dispatch hit shaders.
struct TLASInstance {
    BLAS*                    blas;
    DirectX::XMFLOAT3X4      transform;  // row-major 3x4 world matrix
    uint32_t                 instanceID; // InstanceID() in HLSL
};

class AccelerationStructure {
public:
    // Build a BLAS from a flat vertex list (float3 positions) and index list.
    // Submits a GPU build command — call WaitForGPU() after.
    static BLAS BuildBLAS(
        Device*                        device,
        ID3D12GraphicsCommandList4*    cmdList,
        const std::vector<float>&      vertices,   // x,y,z per vertex
        const std::vector<uint32_t>&   indices     // 3 indices per triangle
    );

    // Build the TLAS from a list of instances.
    // Returns the GPU virtual address to pass to SetTLAS().
    static ComPtr<ID3D12Resource> BuildTLAS(
        Device*                           device,
        ID3D12GraphicsCommandList4*       cmdList,
        const std::vector<TLASInstance>&  instances,
        ComPtr<ID3D12Resource>& outScratch,
        ComPtr<ID3D12Resource>& outInstanceBuffer
    );

    // Helper: identity 3x4 transform (no translation, no rotation, no scale)
    static DirectX::XMFLOAT3X4 Identity();

    // Helper: translation matrix
    static DirectX::XMFLOAT3X4 Translation(float x, float y, float z);

    // Helper: uniform scale matrix
    static DirectX::XMFLOAT3X4 Scale(float s);
};