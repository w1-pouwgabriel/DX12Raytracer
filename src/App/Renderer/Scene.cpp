#include <iostream>
#include "Scene.h"
#include "AccelerationStructure.h"



bool Scene::Build(Device* device, ID3D12GraphicsCommandList4* cmdList) {
    // Define geometry (hardcoded for now)
    static const std::vector<float> cubeVerts = {
        -0.5f,-0.5f,-0.5f,  0.5f,-0.5f,-0.5f,  0.5f, 0.5f,-0.5f, -0.5f, 0.5f,-0.5f,
        -0.5f,-0.5f, 0.5f,  0.5f,-0.5f, 0.5f,  0.5f, 0.5f, 0.5f, -0.5f, 0.5f, 0.5f,
    };
    static const std::vector<uint32_t> cubeIdx = {
        0,1,2, 0,2,3,  5,4,7, 5,7,6,
        4,0,3, 4,3,7,  1,5,6, 1,6,2,
        4,5,1, 4,1,0,  3,2,6, 3,6,7,
    };
    
    static const std::vector<float> quadVerts = { 
        -1,0,-1, 1,0,-1, 1,0,1, -1,0,1 
    };
    static const std::vector<uint32_t> quadIdx = { 
        0,1,2, 0,2,3 
    };

    // Build BLASes (per-object geometry)
    m_cubeBLAS = AccelerationStructure::BuildBLAS(device, cmdList, cubeVerts, cubeIdx);
    m_mirrorBLAS = AccelerationStructure::BuildBLAS(device, cmdList, quadVerts, quadIdx);
    m_floorBLAS = AccelerationStructure::BuildBLAS(device, cmdList, quadVerts, quadIdx);

    // Define scene instances (BLAS + transform + ID)
    std::vector<TLASInstance> instances = {
        { &m_cubeBLAS,   AccelerationStructure::Translation(0, 1, 0), 0 },   // Cube at (0, 1, 0)
        { &m_mirrorBLAS, AccelerationStructure::Translation(0, 3, 2), 1 },   // Mirror at (0, 3, 2)
        { &m_floorBLAS,  AccelerationStructure::Scale(20),            2 },   // Floor (scaled 20x)
    };

    // Build TLAS (scene structure)
    m_tlas = AccelerationStructure::BuildTLAS(
        device, cmdList, instances,
        m_tlasScratch, m_tlasInstanceBuffer
    );

    if (!m_tlas) {
        std::cerr << "[Scene] Failed to build TLAS\n";
        return false;
    }

    std::cout << "[Scene] Built successfully: 3 objects\n";
    return true;
}

D3D12_GPU_VIRTUAL_ADDRESS Scene::GetTLASAddress() const {
    return m_tlas ? m_tlas->GetGPUVirtualAddress() : 0;
}

void Scene::Clear() {
    m_cubeBLAS = BLAS();
    m_mirrorBLAS = BLAS();
    m_floorBLAS = BLAS();
    m_tlas.Reset();
    m_tlasScratch.Reset();
    m_tlasInstanceBuffer.Reset();
}