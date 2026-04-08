#pragma once

#include <cstdint>
#include <vector>

#include "Device.h"
#include "AccelerationStructure.h"

class Scene {
public:
    Scene() = default;
    ~Scene() = default;

    // Disable copy
    Scene(const Scene&) = delete;
    Scene& operator=(const Scene&) = delete;

    // Build the entire scene (BLAS + TLAS)
    bool Build(Device* device, ID3D12GraphicsCommandList4* cmdList);

    // Get TLAS GPU address (for binding to raytracing pipeline)
    D3D12_GPU_VIRTUAL_ADDRESS GetTLASAddress() const;

    // Clear and rebuild scene
    void Clear();

private:
    // Scene geometry (BLASes)
    BLAS m_cubeBLAS;
    BLAS m_mirrorBLAS;
    BLAS m_floorBLAS;

    // Scene structure (TLAS)
    ComPtr<ID3D12Resource> m_tlas;

    // Temporary build buffers (kept alive for safety)
    ComPtr<ID3D12Resource> m_tlasScratch;
    ComPtr<ID3D12Resource> m_tlasInstanceBuffer;
};