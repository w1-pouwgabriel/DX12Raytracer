#include "AccelerationStructure.h"
#include <stdexcept>
#include <iostream>

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
    // -- 1. Upload vertex and index data to GPU -----------------------------
    // Vertices are float3 (3 floats × 4 bytes = 12 bytes per vertex)
    uint64_t vbSize = vertices.size() * sizeof(float);
    uint64_t ibSize = indices.size() * sizeof(uint32_t);

    auto vbUpload = CreateBuffer(device, vbSize, D3D12_RESOURCE_FLAG_NONE,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        D3D12_HEAP_TYPE_UPLOAD);
    auto ibUpload = CreateBuffer(device, ibSize, D3D12_RESOURCE_FLAG_NONE,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        D3D12_HEAP_TYPE_UPLOAD);

    void* pData = nullptr;
    vbUpload->Map(0, nullptr, &pData);
    memcpy(pData, vertices.data(), vbSize);
    vbUpload->Unmap(0, nullptr);

    ibUpload->Map(0, nullptr, &pData);
    memcpy(pData, indices.data(), ibSize);
    ibUpload->Unmap(0, nullptr);

    // -- 2. Describe the geometry -------------------------------------------
    D3D12_RAYTRACING_GEOMETRY_DESC geoDesc = {};
    geoDesc.Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;
    geoDesc.Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE; // disables any-hit shader

    geoDesc.Triangles.VertexBuffer.StartAddress = vbUpload->GetGPUVirtualAddress();
    geoDesc.Triangles.VertexBuffer.StrideInBytes = sizeof(float) * 3; // float3
    geoDesc.Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
    geoDesc.Triangles.VertexCount = static_cast<uint32_t>(vertices.size() / 3);

    geoDesc.Triangles.IndexBuffer = ibUpload->GetGPUVirtualAddress();
    geoDesc.Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;
    geoDesc.Triangles.IndexCount = static_cast<uint32_t>(indices.size());

    // -- 3. Query how much scratch + result memory the driver needs ---------
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
    inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;
    inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
    inputs.NumDescs = 1;
    inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    inputs.pGeometryDescs = &geoDesc;

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuild = {};
    device->GetDevice()->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &prebuild);

    // -- 4. Allocate scratch (temporary) and result (permanent) buffers -----
    // Scratch is only needed during the build — can be freed afterwards.
    // Result must stay alive as long as the BLAS is used.
    auto scratch = CreateBuffer(device,
        prebuild.ScratchDataSizeInBytes,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_COMMON);

    auto result = CreateBuffer(device,
        prebuild.ResultDataMaxSizeInBytes,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE);

    // -- 5. Record the build command ----------------------------------------
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc = {};
    buildDesc.DestAccelerationStructureData = result->GetGPUVirtualAddress();
    buildDesc.ScratchAccelerationStructureData = scratch->GetGPUVirtualAddress();
    buildDesc.Inputs = inputs;

    cmdList->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);

    // UAV barrier — ensures the BLAS build finishes before the TLAS reads it
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    barrier.UAV.pResource = result.Get();
    cmdList->ResourceBarrier(1, &barrier);

    std::cout << "[BLAS] Built — "
        << (indices.size() / 3) << " triangles, "
        << prebuild.ResultDataMaxSizeInBytes / 1024 << " KB\n";

    BLAS blas;
    blas.buffer = result;
    return blas;
    // Note: scratch and upload buffers go out of scope here.
    // They're still alive on the GPU until WaitForGPU() is called — that's fine.
}

// -- TLAS ----------------------------------------------------------------------

ComPtr<ID3D12Resource> AccelerationStructure::BuildTLAS(
    Device* device,
    ID3D12GraphicsCommandList4* cmdList,
    const std::vector<TLASInstance>& instances)
{
    // -- 1. Fill one D3D12_RAYTRACING_INSTANCE_DESC per instance -----------
    // This struct tells the GPU which BLAS to use, what transform to apply,
    // and what InstanceID() / InstanceMask to return in the shader.
    std::vector<D3D12_RAYTRACING_INSTANCE_DESC> instanceDescs(instances.size());

    for (size_t i = 0; i < instances.size(); i++) {
        auto& src = instances[i];
        auto& dst = instanceDescs[i];

        memset(&dst, 0, sizeof(dst));

        // Copy the 3x4 row-major transform (DX12 expects row-major)
        memcpy(dst.Transform, &src.transform, sizeof(dst.Transform));

        dst.InstanceID = src.instanceID;
        dst.InstanceMask = 0xFF; // visible to all ray masks
        dst.InstanceContributionToHitGroupIndex = src.instanceID; // maps to SBT slot
        dst.Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;
        dst.AccelerationStructure = src.blas->gpuVA();
    }

    // -- 2. Upload instance descs to an upload buffer -----------------------
    uint64_t instBufSize = instances.size() * sizeof(D3D12_RAYTRACING_INSTANCE_DESC);
    auto instanceBuf = CreateBuffer(device, instBufSize,
        D3D12_RESOURCE_FLAG_NONE,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        D3D12_HEAP_TYPE_UPLOAD);

    void* pData = nullptr;
    instanceBuf->Map(0, nullptr, &pData);
    memcpy(pData, instanceDescs.data(), instBufSize);
    instanceBuf->Unmap(0, nullptr);

    // -- 3. Query memory requirements --------------------------------------
    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
    inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;
    inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_PREFER_FAST_TRACE;
    inputs.NumDescs = static_cast<uint32_t>(instances.size());
    inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
    inputs.InstanceDescs = instanceBuf->GetGPUVirtualAddress();

    D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO prebuild = {};
    device->GetDevice()->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &prebuild);

    // -- 4. Allocate buffers and build --------------------------------------
    auto scratch = CreateBuffer(device,
        prebuild.ScratchDataSizeInBytes,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_COMMON);

    auto result = CreateBuffer(device,
        prebuild.ResultDataMaxSizeInBytes,
        D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE);

    D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC buildDesc = {};
    buildDesc.DestAccelerationStructureData = result->GetGPUVirtualAddress();
    buildDesc.ScratchAccelerationStructureData = scratch->GetGPUVirtualAddress();
    buildDesc.Inputs = inputs;

    cmdList->BuildRaytracingAccelerationStructure(&buildDesc, 0, nullptr);

    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    barrier.UAV.pResource = result.Get();
    cmdList->ResourceBarrier(1, &barrier);

    std::cout << "[TLAS] Built — "
        << instances.size() << " instances, "
        << prebuild.ResultDataMaxSizeInBytes / 1024 << " KB\n";

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