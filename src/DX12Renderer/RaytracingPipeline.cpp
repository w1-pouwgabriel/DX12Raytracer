#include "RaytracingPipeline.h"
#include "ShaderCompiler.h"
#include <iostream>
#include <fstream>
#include <cstring>

// Rounds v up to the next multiple of a (a must be a power of two)
static uint32_t AlignUp(uint32_t v, uint32_t a) { return (v + a - 1) & ~(a - 1); }

// -- Public --------------------------------------------------------------------

bool RaytracingPipeline::Initialize(Device* device, uint32_t width, uint32_t height) {
    m_device = device;
    m_width = width;
    m_height = height;

    if (!CreateOutputUAV()) {
        std::cerr << "[RaytracingPipeline] Failed to create output UAV\n";
        return false;
    }

    // -- Compile shader --------------------------------------------------------
    ShaderCompiler compiler;
    if (!compiler.Initialize()) return false;

    std::ifstream file("assets/shaders/screenShader.hlsl");
    if (!file.is_open()) {
        std::cerr << "[RaytracingPipeline] Cannot open assets/shaders/screenShader.hlsl\n";
        return false;
    }
    std::string hlsl(std::istreambuf_iterator<char>(file), {});

    // lib_6_3 = raytracing library target (no single entry point — exports all shaders)
    auto blob = compiler.Compile(hlsl, L"lib_6_3");

    // -- Build pipeline --------------------------------------------------------
    if (!CreateRootSignature()) {
        std::cerr << "[RaytracingPipeline] Failed to create root signature\n";
        return false;
    }

    if (!CreateRTPSO(blob->GetBufferPointer(), blob->GetBufferSize())) {
        std::cerr << "[RaytracingPipeline] Failed to create RTPSO\n";
        return false;
    }

    if (!CreateSBT()) {
        std::cerr << "[RaytracingPipeline] Failed to create SBT\n";
        return false;
    }

    std::cout << "[RaytracingPipeline] Initialized (" << width << "x" << height << ")\n";
    return true;
}

void RaytracingPipeline::Dispatch(ID3D12GraphicsCommandList4* cmdList) {
    if (!m_rtpso) return;

    // -- 1. UAV: COPY_SOURCE > UNORDERED_ACCESS --------------------------------
    D3D12_RESOURCE_BARRIER barrier = {};
    barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    barrier.Transition.pResource = m_outputUAV.Get();
    barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_SOURCE;
    barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    cmdList->ResourceBarrier(1, &barrier);

    // -- 2. Bind root signature, UAV heap, UAV descriptor ----------------------
    cmdList->SetComputeRootSignature(m_globalRootSig.Get());

    ID3D12DescriptorHeap* heaps[] = { m_uavHeap.Get() };
    cmdList->SetDescriptorHeaps(1, heaps);
    cmdList->SetComputeRootDescriptorTable(
        0,  // root parameter index — matches CreateRootSignature()
        m_uavHeap->GetGPUDescriptorHandleForHeapStart()
    );

    // -- 3. Set raytracing pipeline --------------------------------------------
    cmdList->SetPipelineState1(m_rtpso.Get());

    // -- 4. Dispatch rays ------------------------------------------------------
    const uint32_t idSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;

    D3D12_DISPATCH_RAYS_DESC desc = {};
    desc.Width = m_width;
    desc.Height = m_height;
    desc.Depth = 1;

    // One RayGen record (no stride field — there is always exactly one)
    desc.RayGenerationShaderRecord.StartAddress =
        m_sbtBuffer->GetGPUVirtualAddress() + m_rayGenOffset;
    desc.RayGenerationShaderRecord.SizeInBytes = idSize;

    // One Miss record
    desc.MissShaderTable.StartAddress =
        m_sbtBuffer->GetGPUVirtualAddress() + m_missOffset;
    desc.MissShaderTable.SizeInBytes = idSize;
    desc.MissShaderTable.StrideInBytes = idSize;

    // No hit group yet — no geometry
    desc.HitGroupTable = {};

    cmdList->DispatchRays(&desc);

    // -- 5. UAV: UNORDERED_ACCESS > COPY_SOURCE --------------------------------
    // Renderer::CopyUAVToBackBuffer() expects the UAV in this state
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


void RaytracingPipeline::Reload(std::string FileName)
{
    std::cout << "[RaytracingPipeline] Reloading shaders...\n";

    // wait for GPU to finish using the current pipeline before we replace it
    m_device->WaitForGPU();

    // drop old pipeline objects — UAV stays, we only rebuilt the shader side
    m_rtpso.Reset();
    m_rtpsoProps.Reset();
    m_globalRootSig.Reset();
    m_sbtBuffer.Reset();

    ShaderCompiler compiler;
    if (!compiler.Initialize()) return;

    if (FileName.size() <= 0) return;

    std::ifstream file("assets/shaders/" + FileName +".hlsl");
    if (!file.is_open()) {
        std::cerr << "[RaytracingPipeline] Cannot open shader file\n";
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
        // shader has a compile error — print it and keep the old pipeline running
        std::cerr << "[RaytracingPipeline] Shader error: " << e.what() << "\n";
        return;
    }

    std::cout << "[RaytracingPipeline] Reload successful\n";
}

// -- Private -------------------------------------------------------------------

bool RaytracingPipeline::CreateOutputUAV() {
    D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
    heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heapDesc.NumDescriptors = 1;
    heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;

    if (FAILED(m_device->GetDevice()->CreateDescriptorHeap(
        &heapDesc, IID_PPV_ARGS(&m_uavHeap))))
        return false;

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
        IID_PPV_ARGS(&m_outputUAV))))
        return false;

    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

    m_device->GetDevice()->CreateUnorderedAccessView(
        m_outputUAV.Get(), nullptr, &uavDesc,
        m_uavHeap->GetCPUDescriptorHandleForHeapStart()
    );
    return true;
}

bool RaytracingPipeline::CreateRootSignature() {
    // One descriptor table: UAV at register u0
    // This matches `RWTexture2D<float4> gOutput : register(u0)` in the HLSL
    D3D12_DESCRIPTOR_RANGE range = {};
    range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    range.NumDescriptors = 1;
    range.BaseShaderRegister = 0; // u0
    range.RegisterSpace = 0;
    range.OffsetInDescriptorsFromTableStart = 0;

    D3D12_ROOT_PARAMETER param = {};
    param.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    param.DescriptorTable.NumDescriptorRanges = 1;
    param.DescriptorTable.pDescriptorRanges = &range;
    param.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    D3D12_ROOT_SIGNATURE_DESC desc = {};
    desc.NumParameters = 1;
    desc.pParameters = &param;
    desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

    ComPtr<ID3DBlob> serialized, error;
    HRESULT hr = D3D12SerializeRootSignature(
        &desc, D3D_ROOT_SIGNATURE_VERSION_1, &serialized, &error);

    if (FAILED(hr)) {
        if (error) std::cerr << static_cast<char*>(error->GetBufferPointer());
        return false;
    }

    return SUCCEEDED(m_device->GetDevice()->CreateRootSignature(
        0,
        serialized->GetBufferPointer(),
        serialized->GetBufferSize(),
        IID_PPV_ARGS(&m_globalRootSig)
    ));
}

bool RaytracingPipeline::CreateRTPSO(const void* shaderCode, size_t shaderCodeSize) {
    // Export the two entry points from the DXIL library.
    // Names must match the function names in screenShader.hlsl exactly.
    D3D12_EXPORT_DESC exports[2] = {
        { L"RayGen", nullptr, D3D12_EXPORT_FLAG_NONE },
        { L"Miss",   nullptr, D3D12_EXPORT_FLAG_NONE },
    };
    D3D12_DXIL_LIBRARY_DESC libDesc = {};
    libDesc.DXILLibrary.pShaderBytecode = shaderCode;
    libDesc.DXILLibrary.BytecodeLength = shaderCodeSize;
    libDesc.NumExports = 2;
    libDesc.pExports = exports;

    // MaxPayloadSizeInBytes must match the Payload struct in the HLSL (float4 = 16 bytes)
    // MaxAttributeSizeInBytes covers built-in triangle barycentrics (float2 = 8 bytes)
    D3D12_RAYTRACING_SHADER_CONFIG shaderConfig = {};
    shaderConfig.MaxPayloadSizeInBytes = sizeof(float) * 4; // float4 color
    shaderConfig.MaxAttributeSizeInBytes = sizeof(float) * 2; // float2 barycentrics

    // No secondary rays in gradient mode — recursion depth 1 is the minimum
    D3D12_RAYTRACING_PIPELINE_CONFIG pipelineConfig = {};
    pipelineConfig.MaxTraceRecursionDepth = 1;

    D3D12_GLOBAL_ROOT_SIGNATURE globalRootSig = {};
    globalRootSig.pGlobalRootSignature = m_globalRootSig.Get();

    D3D12_STATE_SUBOBJECT subobjects[4] = {
        { D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY,              &libDesc        },
        { D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG,  &shaderConfig   },
        { D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG,&pipelineConfig },
        { D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE,     &globalRootSig  },
    };

    D3D12_STATE_OBJECT_DESC stateObjDesc = {};
    stateObjDesc.Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;
    stateObjDesc.NumSubobjects = 4;
    stateObjDesc.pSubobjects = subobjects;

    if (FAILED(m_device->GetDevice()->CreateStateObject(
        &stateObjDesc, IID_PPV_ARGS(&m_rtpso))))
        return false;

    // rtpsoProps lets us look up shader identifiers by name for the SBT
    return SUCCEEDED(m_rtpso.As(&m_rtpsoProps));
}

bool RaytracingPipeline::CreateSBT() {
    const uint32_t idSize = D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES;         // 32 bytes
    const uint32_t tableAlign = D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT;  // 64 bytes

    // Each table's GPU start address must be 64-byte aligned.
    // Our records have no local root arguments so they are exactly idSize (32 bytes),
    // but we still pad the offsets so each table starts on a 64-byte boundary.
    m_rayGenOffset = 0;
    m_missOffset = AlignUp(idSize, tableAlign); // 64

    const uint32_t sbtSize = m_missOffset + AlignUp(idSize, tableAlign); // 128

    D3D12_HEAP_PROPERTIES heapProps = {};
    heapProps.Type = D3D12_HEAP_TYPE_UPLOAD; // CPU-writable so we can memcpy shader IDs

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
        IID_PPV_ARGS(&m_sbtBuffer))))
        return false;

    // GetShaderIdentifier returns a pointer to a 32-byte opaque blob.
    // The name must exactly match the HLSL function name.
    void* rayGenId = m_rtpsoProps->GetShaderIdentifier(L"RayGen");
    void* missId = m_rtpsoProps->GetShaderIdentifier(L"Miss");

    if (!rayGenId || !missId) {
        std::cerr << "[RaytracingPipeline] Shader identifier not found — "
            "entry point names must match screenShader.hlsl exactly\n";
        return false;
    }

    uint8_t* pData = nullptr;
    m_sbtBuffer->Map(0, nullptr, reinterpret_cast<void**>(&pData));
    memcpy(pData + m_rayGenOffset, rayGenId, idSize);
    memcpy(pData + m_missOffset, missId, idSize);
    m_sbtBuffer->Unmap(0, nullptr);

    return true;
}