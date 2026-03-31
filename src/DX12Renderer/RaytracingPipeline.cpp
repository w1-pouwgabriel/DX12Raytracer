#include "RaytracingPipeline.h"
#include "ShaderCompiler.h"
#include "AccelerationStructure.h"
#include <iostream>
#include <fstream>
#include <cstring>
#include <filesystem>

static uint32_t AlignUp(uint32_t v, uint32_t a) { return (v + a - 1) & ~(a - 1); }

static std::string ResolveShaderPath(const std::string& path) {
    if (std::filesystem::exists(path)) return path;
    char exePath[MAX_PATH] = {};
    GetModuleFileNameA(nullptr, exePath, MAX_PATH);
    return (std::filesystem::path(exePath).parent_path() / path).string();
}

bool RaytracingPipeline::Initialize(Device* device, uint32_t width, uint32_t height) {
    m_device = device;
    m_width = width;
    m_height = height;

    if (!CreateOutputUAV()) {
        std::cerr << "[RaytracingPipeline] Failed to create output UAV\n";
        return false;
    }

    ShaderCompiler compiler;
    if (!compiler.Initialize()) return false;

    std::string resolved = ResolveShaderPath("assets/shaders/landelare.hlsl");
    std::ifstream file(resolved);
    if (!file.is_open()) {
        std::cerr << "[RaytracingPipeline] Cannot open: " << resolved << "\n"
            << "[RaytracingPipeline] Working dir: "
            << std::filesystem::current_path() << "\n";
        return false;
    }
    std::string hlsl(std::istreambuf_iterator<char>(file), {});

    auto blob = compiler.Compile(hlsl, L"lib_6_3");

    if (!CreateRootSignature()) {
        std::cerr << "[RaytracingPipeline] Failed to create root signature\n";
        return false;
    }
    if (!CreateRTPSO(blob->GetBufferPointer(), blob->GetBufferSize())) {
        std::cerr << "[RaytracingPipeline] Failed to create RTPSO\n";
        return false;
    }
    /*
    
    Manually listed export "raygeneration", doesn't exist in DXILLibrary.pShaderBytecode: 0x00000231D4BE3410. [ STATE_CREATION ERROR #1194: CREATE_STATE_OBJECT_ERROR]
    Manually listed export "closesthit", doesn't exist in DXILLibrary.pShaderBytecode: 0x00000231D4BE3410. [ STATE_CREATION ERROR #1194: CREATE_STATE_OBJECT_ERROR]
    Manually listed export "miss", doesn't exist in DXILLibrary.pShaderBytecode: 0x00000231D4BE3410. [ STATE_CREATION ERROR #1194: CREATE_STATE_OBJECT_ERROR]
    HitGroupExport "raygeneration" imports ClosestHitShaderImport named "closesthit" but there are no exports matching that name. [ STATE_CREATION ERROR #1194: CREATE_STATE_OBJECT_ERROR]
    
    */





    if (!CreateSBT()) {
        std::cerr << "[RaytracingPipeline] Failed to create SBT\n";
        return false;
    }

    std::cout << "[RaytracingPipeline] Initialized (" << width << "x" << height << ")\n";
    return true;
}

void RaytracingPipeline::Reload(const std::string& shaderPath) {
    std::cout << "[RaytracingPipeline] Reloading: " << shaderPath << "\n";
    m_device->WaitForGPU();

    m_rtpso.Reset();
    m_rtpsoProps.Reset();
    m_globalRootSig.Reset();
    m_sbtBuffer.Reset();
    // m_tlas is NOT reset — geometry hasn't changed

    ShaderCompiler compiler;
    if (!compiler.Initialize()) return;

    std::string resolved = ResolveShaderPath(shaderPath);
    std::ifstream file(resolved);
    if (!file.is_open()) {
        std::cerr << "[RaytracingPipeline] Cannot open: " << resolved << "\n";
        return;
    }
    std::string hlsl(std::istreambuf_iterator<char>(file), {});

    try {
        auto blob = compiler.Compile(hlsl, L"lib_6_3");
        if (!CreateRootSignature() ||
            !CreateRTPSO(blob->GetBufferPointer(), blob->GetBufferSize()) ||
            !CreateSBT()) {
            std::cerr << "[RaytracingPipeline] Reload failed\n";
            return;
        }
    }
    catch (const std::exception& e) {
        std::cerr << "[RaytracingPipeline] Shader error: " << e.what() << "\n";
    }
}

void RaytracingPipeline::Dispatch(ID3D12GraphicsCommandList4* cmdList) {
    if (!m_rtpso) return;

    // UAV: COPY_SOURCE  UNORDERED_ACCESS
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = m_outputUAV.Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    cmdList->ResourceBarrier(1, &barrier);

    cmdList->SetComputeRootSignature(m_globalRootSig.Get());

    // param 0: UAV descriptor table (output texture)
    ID3D12DescriptorHeap* heaps[] = { m_uavHeap.Get() };
    cmdList->SetDescriptorHeaps(1, heaps);
    cmdList->SetComputeRootDescriptorTable(
        0, m_uavHeap->GetGPUDescriptorHandleForHeapStart());

    // param 1: TLAS root SRV (0 = no geometry, all rays will miss)
    cmdList->SetComputeRootShaderResourceView(1, m_tlas);

    cmdList->SetPipelineState1(m_rtpso.Get());

    const uint32_t idSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;

    D3D12_DISPATCH_RAYS_DESC desc = {};
    desc.Width = m_width;
    desc.Height = m_height;
    desc.Depth = 1;

    desc.RayGenerationShaderRecord.StartAddress =
        m_sbtBuffer->GetGPUVirtualAddress() + m_rayGenOffset;
    desc.RayGenerationShaderRecord.SizeInBytes = idSize;

    desc.MissShaderTable.StartAddress =
        m_sbtBuffer->GetGPUVirtualAddress() + m_missOffset;
    desc.MissShaderTable.SizeInBytes = idSize;
    desc.MissShaderTable.StrideInBytes = idSize;

    desc.HitGroupTable.StartAddress =
        m_sbtBuffer->GetGPUVirtualAddress() + m_hitGroupOffset;
    desc.HitGroupTable.SizeInBytes = idSize;
    desc.HitGroupTable.StrideInBytes = idSize;

    cmdList->DispatchRays(&desc);

    // UAV: UNORDERED_ACCESS  COPY_SOURCE
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
    cmdList->ResourceBarrier(1, &barrier);
}

void RaytracingPipeline::Resize(uint32_t width, uint32_t height) {
    m_width = width;
    m_height = height;
    m_outputUAV.Reset();
    m_uavHeap.Reset();
    CreateOutputUAV();
}

//  Private 
bool RaytracingPipeline::CreateOutputUAV() {
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heapDesc.NumDescriptors = 1;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    if (FAILED(m_device->GetDevice()->CreateDescriptorHeap(
        &heapDesc, IID_PPV_ARGS(&m_uavHeap)))) return false;

    D3D12_RESOURCE_DESC texDesc = {};
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Width = m_width;
    texDesc.Height = m_height;
    texDesc.DepthOrArraySize = 1;
    texDesc.MipLevels = 1;
    texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    texDesc.SampleDesc.Count = 1;
    texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

    if (FAILED(m_device->GetDevice()->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &texDesc,
        D3D12_RESOURCE_STATE_COPY_SOURCE, nullptr,
        IID_PPV_ARGS(&m_outputUAV)))) return false;

    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    m_device->GetDevice()->CreateUnorderedAccessView(
        m_outputUAV.Get(), nullptr, &uavDesc,
        m_uavHeap->GetCPUDescriptorHandleForHeapStart());
    return true;
}

bool RaytracingPipeline::CreateRootSignature() {
    // param 0: descriptor table  UAV at u0 (output texture)
    D3D12_DESCRIPTOR_RANGE uavRange = {};
    uavRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    uavRange.NumDescriptors = 1;
    uavRange.BaseShaderRegister = 0; // u0
    uavRange.RegisterSpace = 0;
    uavRange.OffsetInDescriptorsFromTableStart = 0;

    D3D12_ROOT_PARAMETER params[2] = {};

    params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    params[0].DescriptorTable.NumDescriptorRanges = 1;
    params[0].DescriptorTable.pDescriptorRanges = &uavRange;
    params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    // param 1: root SRV  TLAS at t0
    params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
    params[1].Descriptor.ShaderRegister = 0; // t0
    params[1].Descriptor.RegisterSpace = 0;
    params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    D3D12_ROOT_SIGNATURE_DESC desc = {};
    desc.NumParameters = 2;
    desc.pParameters = params;
    desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

    ComPtr<ID3DBlob> serialized, error;
    HRESULT hr = D3D12SerializeRootSignature(
        &desc, D3D_ROOT_SIGNATURE_VERSION_1, &serialized, &error);
    if (FAILED(hr)) {
        if (error) std::cerr << static_cast<char*>(error->GetBufferPointer());
        return false;
    }

    return SUCCEEDED(m_device->GetDevice()->CreateRootSignature(
        0, serialized->GetBufferPointer(), serialized->GetBufferSize(),
        IID_PPV_ARGS(&m_globalRootSig)));
}

bool RaytracingPipeline::CreateRTPSO(const void* shaderCode, size_t shaderSize) {
    // Export all three shader entry points from the DXIL library.
    // Names must exactly match the [shader("...")] functions in the HLSL.


    D3D12_EXPORT_DESC exports[3] = {
        { L"RayGeneration", nullptr, D3D12_EXPORT_FLAG_NONE },
        { L"Miss",          nullptr, D3D12_EXPORT_FLAG_NONE },
        { L"ClosestHit",    nullptr, D3D12_EXPORT_FLAG_NONE },
    };
    D3D12_DXIL_LIBRARY_DESC libDesc = {};
    libDesc.DXILLibrary.pShaderBytecode = shaderCode;
    libDesc.DXILLibrary.BytecodeLength = shaderSize;
    libDesc.NumExports = 3;
    libDesc.pExports = exports;

    D3D12_HIT_GROUP_DESC hitGroup = {};
    hitGroup.HitGroupExport = L"HitGroup";
    hitGroup.Type = D3D12_HIT_GROUP_TYPE_TRIANGLES;
    hitGroup.ClosestHitShaderImport = L"ClosestHit";
    hitGroup.AnyHitShaderImport = nullptr;
    hitGroup.IntersectionShaderImport = nullptr;

    // Payload layout: float3 color (12) + bool allowReflection (4) + bool missed (4) = 20 bytes
    D3D12_RAYTRACING_SHADER_CONFIG shaderConfig = {};
    shaderConfig.MaxPayloadSizeInBytes = 20;
    shaderConfig.MaxAttributeSizeInBytes = 8; // barycentrics
    D3D12_RAYTRACING_PIPELINE_CONFIG pipelineConfig = {};
    pipelineConfig.MaxTraceRecursionDepth = 3;// Primary ray reflection shadow = 3 levels of recursion

    D3D12_GLOBAL_ROOT_SIGNATURE globalRootSig = {};
    globalRootSig.pGlobalRootSignature = m_globalRootSig.Get();

    D3D12_STATE_SUBOBJECT subobjects[5] = {
        { D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY,               &libDesc        },
        { D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP,                   &hitGroup       },
        { D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG,    &shaderConfig   },
        { D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG,  &pipelineConfig },
        { D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE,       &globalRootSig  },
    };

    D3D12_STATE_OBJECT_DESC stateObjDesc = {};
    stateObjDesc.Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;
    stateObjDesc.NumSubobjects = 5;
    stateObjDesc.pSubobjects = subobjects;

    if (FAILED(m_device->GetDevice()->CreateStateObject(
        &stateObjDesc, IID_PPV_ARGS(&m_rtpso))))
        return false;

    return SUCCEEDED(m_rtpso.As(&m_rtpsoProps));
}

bool RaytracingPipeline::CreateSBT() {
    const uint32_t idSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;
    const uint32_t tableAlign = D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT;

    // Three tables: RayGen, Miss, HitGroup � each padded to 64-byte boundary
    m_rayGenOffset = 0;
    m_missOffset = AlignUp(idSize, tableAlign);
    m_hitGroupOffset = AlignUp(m_missOffset + idSize, tableAlign);
    const uint32_t sbtSize = m_hitGroupOffset + AlignUp(idSize, tableAlign);

    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

    D3D12_RESOURCE_DESC bufDesc = {};
    bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    bufDesc.Width = sbtSize;
    bufDesc.Height = 1;
    bufDesc.DepthOrArraySize = 1;
    bufDesc.MipLevels = 1;
    bufDesc.SampleDesc.Count = 1;
    bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

    if (FAILED(m_device->GetDevice()->CreateCommittedResource(
        &heapProps, D3D12_HEAP_FLAG_NONE, &bufDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
        IID_PPV_ARGS(&m_sbtBuffer)))) return false;

    // Names must match HLSL entry points / hit group export name exactly
    void* rayGenId = m_rtpsoProps->GetShaderIdentifier(L"RayGeneration");
    void* missId = m_rtpsoProps->GetShaderIdentifier(L"Miss");
    void* hitGroupId = m_rtpsoProps->GetShaderIdentifier(L"HitGroup");

    if (!rayGenId || !missId || !hitGroupId) {
        std::cerr << "[RaytracingPipeline] Shader identifier not found � "
            "check entry point names match the HLSL exactly\n";
        return false;
    }

    uint8_t* pData = nullptr;
    m_sbtBuffer->Map(0, nullptr, reinterpret_cast<void**>(&pData));
    memcpy(pData + m_rayGenOffset, rayGenId, idSize);
    memcpy(pData + m_missOffset, missId, idSize);
    memcpy(pData + m_hitGroupOffset, hitGroupId, idSize);
    m_sbtBuffer->Unmap(0, nullptr);

    return true;
}