#pragma once

#include "Device.h"
#include <dxgi1_6.h>
#include <cstdint>

class RaytracingPipeline 
{

public:
    bool Initialize(Device* device, uint32_t width, uint32_t height);
    void Dispatch(ID3D12GraphicsCommandList4* cmdList);
    void Resize(uint32_t width, uint32_t height);

    // Renderer needs this to blit to the back buffer
    ID3D12Resource* GetOutputUAV() const { return m_outputUAV.Get(); }

    void Reload(std::string FileName); // recompiles shader and rebuilds RTPSO + SBT

private:
    bool CreateOutputUAV(); 
    bool CreateRootSignature();
    bool CreateRTPSO(const void* shaderCode, size_t shaderCodeSize);
    bool CreateSBT();

    //  Output 
    ComPtr<ID3D12Resource>       m_outputUAV;           // texture raytracing writes into
    ComPtr<ID3D12DescriptorHeap> m_uavHeap;             // descriptor heap to bind it

    //  Pipeline 
    ComPtr<ID3D12RootSignature>  m_globalRootSig;       // binds UAV + TLAS to shaders
    ComPtr<ID3D12StateObject>    m_rtpso;               // compiled raytracing pipeline
    ComPtr<ID3D12StateObjectProperties> m_rtpsoProps;   // used to extract shader IDs for SBT

    // Shader Binding Table
    ComPtr<ID3D12Resource>       m_sbtBuffer;           // GPU buffer holding the SBT
    uint64_t m_rayGenOffset = 0;                        // byte offsets into m_sbtBuffer
    uint64_t m_missOffset = 0;
    uint64_t m_hitGroupOffset = 0;                      // unused until geometry step

    // Back-reference
    Device* m_device = nullptr;
    uint32_t m_width = 0;
    uint32_t m_height = 0;

};