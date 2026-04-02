#pragma once

#include "Device.h"
#include <cstdint>
#include <string>

class RaytracingPipeline {
public:
    bool Initialize(Device* device, uint32_t width, uint32_t height);
    void Dispatch(ID3D12GraphicsCommandList4* cmdList);
    void Resize(uint32_t width, uint32_t height);

    // Shader hot-reload rebuilds RTPSO + SBT only, geometry is untouched
    void Reload(const std::string& shaderPath = "assets/shaders/landelare.hlsl");

    // Called by Renderer after BuildScene() stores the GPU address for Dispatch()
    void SetTLAS(D3D12_GPU_VIRTUAL_ADDRESS tlas) { m_tlas = tlas; }

    ID3D12Resource* GetOutputUAV() const { return m_outputUAV.Get(); }

private:
    bool CreateOutputUAV();
    bool CreateRootSignature();
    bool CreateRTPSO(const void* shaderCode, size_t shaderSize);
    bool CreateSBT();

    // -- Output ----------------------------------------------------------------
    ComPtr<ID3D12Resource>       m_outputUAV;
    ComPtr<ID3D12DescriptorHeap> m_uavHeap;

    // -- Pipeline --------------------------------------------------------------
    ComPtr<ID3D12RootSignature>         m_globalRootSig;
    ComPtr<ID3D12StateObject>           m_rtpso;
    ComPtr<ID3D12StateObjectProperties> m_rtpsoProps;

    // -- Shader Binding Table --------------------------------------------------
    ComPtr<ID3D12Resource> m_sbtBuffer;
    uint64_t m_rayGenOffset = 0;
    uint64_t m_missOffset = 0;
    uint64_t m_hitGroupOffset = 0;

    // -- Back-references (non-owning) ------------------------------------------
    Device* m_device = nullptr;
    uint32_t                  m_width = 0;
    uint32_t                  m_height = 0;
    D3D12_GPU_VIRTUAL_ADDRESS m_tlas = 0; // set by Renderer after scene build
};