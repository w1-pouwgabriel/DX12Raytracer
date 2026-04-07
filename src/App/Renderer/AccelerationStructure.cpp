#include "AccelerationStructure.h"
#include <stdexcept>
#include <iostream>
#include <cstring>

using namespace DirectX;

// -- Helpers -------------------------------------------------------------------

// Allocates a GPU buffer in the given initial state.
static ComPtr<ID3D12Resource> CreateBuffer(
    Device* device,
    uint64_t size,
    D3D12_RESOURCE_FLAGS flags,
    D3D12_RESOURCE_STATES initialState,
    D3D12_HEAP_TYPE heapType = D3D12_HEAP_TYPE_DEFAULT)
{
    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = heapType;

    D3D12_RESOURCE_DESC desc = {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    desc.Width = size;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    desc.Flags = flags;

    ComPtr<ID3D12Resource> buffer;
    if (FAILED(device->GetDevice()->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &desc,
        initialState, nullptr, IID_PPV_ARGS(&buffer))))
        throw std::runtime_error("Failed to create buffer");
    return buffer;
}

// -- BLAS ----------------------------------------------------------------------

BLAS AccelerationStructure::BuildBLAS(
    Device* device,
    ID3D12GraphicsCommandList4* cmdList,
    const std::vector<float>& vertices,
    const std::vector<uint32_t>& indices)
{
    BLAS blas;

    // -- 1. Upload vertex data (STORE IN BLAS!) -----------------------------
    uint64_t vbSize = vertices.size() * sizeof(float);

    blas.vertexBuffer = CreateBuffer(device, vbSize,
        D3D12_RESOURCE_FLAG_NONE,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        D3D12_HEAP_TYPE_UPLOAD);

    void* pVertexData = nullptr;
    blas.vertexBuffer->Map(0, nullptr, &pVertexData);
    memcpy(pVertexData, vertices.data(), vbSize);
    blas.vertexBuffer->Unmap(0, nullptr);

    // -- 2. Upload index data (STORE IN BLAS!) ------------------------------
    uint64_t ibSize = indices.size() * sizeof(uint32_t);

    blas.indexBuffer = CreateBuffer(device, ibSize,
        D3D12_RESOURCE_FLAG_NONE,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        D3D12_HEAP_TYPE_UPLOAD);

    void* pIndexData = nullptr;
    blas.indexBuffer->Map(0, nullptr, &pIndexData);
    memcpy(pIndexData, indices.data(), ibSize);
    blas.indexBuffer->Unmap(0, nullptr);

    // -- 3. Describe the geometry -------------------------------------------
    D3D12_RAYTRACING_GEOMETRY_DESC geoDesc = {};
    geoDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
    geoDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

    geoDesc.Triangles.VertexBuffer.StartAddress = blas.vertexBuffer->GetGPUVirtualAddress();
    geoDesc.Triangles.VertexBuffer.StrideInBytes = sizeof(float) * 3;  // float3 per vertex
    geoDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
    geoDesc.Triangles.VertexCount = static_cast<UINT>(vertices.size() / 3);

    geoDesc.Triangles.IndexBuffer = blas.indexBuffer->GetGPUVirtualAddress();
    geoDesc.Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;
    geoDesc.Triangles.IndexCount = static_cast<UINT>(indices.size());

    // -- 4. Query memory requirements ---------------------------------------
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
    inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
    inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
    inputs.NumDescs = 1;
    inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    inputs.pGeometryDescs = &geoDesc;

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuild = {};
    device->GetDevice()->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &prebuild);

    // -- 5. Create scratch buffer (STORE IN BLAS!) --------------------------
    blas.scratchBuffer = CreateBuffer(device,
        prebuild.ScratchDataSizeInBytes,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_COMMON);

    // -- 6. Create result buffer (already stored in blas.buffer) ------------
    if (!blas.buffer) {
        blas.buffer = CreateBuffer(device,
            prebuild.ResultDataMaxSizeInBytes,
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE);
    }

    // -- 7. Build the BLAS --------------------------------------------------
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc = {};
    buildDesc.DestAccelerationStructureData = blas.buffer->GetGPUVirtualAddress();
    buildDesc.ScratchAccelerationStructureData = blas.scratchBuffer->GetGPUVirtualAddress();
    buildDesc.Inputs = inputs;

    cmdList->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);

    // -- 8. UAV barrier - ensure build completes ----------------------------
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    barrier.UAV.pResource = blas.buffer.Get();
    cmdList->ResourceBarrier(1, &barrier);

    std::cout << "[BLAS] Built - "
        << (indices.size() / 3) << " triangles, "
        << (prebuild.ResultDataMaxSizeInBytes / 1024) << " KB\n";

    // All buffers (vertex, index, scratch, result) stored in BLAS
    // They stay alive as long as the BLAS exists
    return blas;
}

// -- TLAS ----------------------------------------------------------------------

ComPtr<ID3D12Resource> AccelerationStructure::BuildTLAS(
    Device* device,
    ID3D12GraphicsCommandList4* cmdList,
    const std::vector<TLASInstance>& instances,
    ComPtr<ID3D12Resource>& outScratch,
    ComPtr<ID3D12Resource>& outInstanceBuffer)
{
    // -- 1. Create instance descriptors -------------------------------------
    std::vector<D3D12_RAYTRACING_INSTANCE_DESC> instanceDescs(instances.size());

    for (size_t i = 0; i < instances.size(); i++) {
        auto& src = instances[i];
        auto& dst = instanceDescs[i];

        memset(&dst, 0, sizeof(dst));
        memcpy(dst.Transform, &src.transform, sizeof(dst.Transform));

        dst.InstanceID = src.instanceID;
        dst.InstanceMask = 0xFF;
        dst.InstanceContributionToHitGroupIndex = src.instanceID;
        dst.Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
        dst.AccelerationStructure = src.blas->gpuVA();
    }

    // -- 2. Upload instance buffer (CREATE ONCE, USE IT!) -------------------
    uint64_t instBufSize = instances.size() * sizeof(D3D12_RAYTRACING_INSTANCE_DESC);

    outInstanceBuffer = CreateBuffer(device, instBufSize,
        D3D12_RESOURCE_FLAG_NONE,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        D3D12_HEAP_TYPE_UPLOAD);

    void* pData = nullptr;
    outInstanceBuffer->Map(0, nullptr, &pData);
    memcpy(pData, instanceDescs.data(), instBufSize);
    outInstanceBuffer->Unmap(0, nullptr);

    // -- 3. Query memory requirements ---------------------------------------
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
    inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
    inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
    inputs.NumDescs = static_cast<UINT>(instances.size());
    inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    inputs.InstanceDescs = outInstanceBuffer->GetGPUVirtualAddress();

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuild = {};
    device->GetDevice()->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &prebuild);

    // -- 4. Create scratch buffer (STORE IT!) -------------------------------
    outScratch = CreateBuffer(device,
        prebuild.ScratchDataSizeInBytes,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_COMMON);

    // -- 5. Create result buffer --------------------------------------------
    auto result = CreateBuffer(device,
        prebuild.ResultDataMaxSizeInBytes,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE);

    // -- 6. Build the TLAS --------------------------------------------------
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc = {};
    buildDesc.DestAccelerationStructureData = result->GetGPUVirtualAddress();
    buildDesc.ScratchAccelerationStructureData = outScratch->GetGPUVirtualAddress();
    buildDesc.Inputs = inputs;

    cmdList->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);

    // -- 7. UAV barrier -----------------------------------------------------
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    barrier.UAV.pResource = result.Get();
    cmdList->ResourceBarrier(1, &barrier);

    std::cout << "[TLAS] Built - "
        << instances.size() << " instances, "
        << (prebuild.ResultDataMaxSizeInBytes / 1024) << " KB\n";

    // All buffers now stored in out parameters, stay alive in caller
    return result;
}

// -- Transform helpers ---------------------------------------------------------

XMFLOAT3X4 AccelerationStructure::Identity() {
    return XMFLOAT3X4(
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0
    );
}

XMFLOAT3X4 AccelerationStructure::Translation(float x, float y, float z) {
    return XMFLOAT3X4(
        1, 0, 0, x,
        0, 1, 0, y,
        0, 0, 1, z
    );
}

XMFLOAT3X4 AccelerationStructure::Scale(float s) {
    return XMFLOAT3X4(
        s, 0, 0, 0,
        0, s, 0, 0,
        0, 0, s, 0
    );
}