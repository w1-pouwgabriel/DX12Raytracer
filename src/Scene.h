#include "utils.h"

struct Camera {
    DirectX::XMFLOAT3 Position;
    DirectX::XMFLOAT3 Target;
    DirectX::XMFLOAT3 Up;
};

struct Model {
    Model() {
        Vertices = {
            -0.5f,-0.5f,-0.5f,  
            0.5f,-0.5f,-0.5f,  
            0.5f, 0.5f,-0.5f, 
            -0.5f, 0.5f,-0.5f,
            -0.5f,-0.5f, 0.5f,  
            0.5f,-0.5f, 0.5f,  
            0.5f, 0.5f, 0.5f, 
            -0.5f, 0.5f, 0.5f, 
        };
        indices = {     
            0,1,2, 0,2,3,  // -Z face
            5,4,7, 5,7,6,  // +Z face
            4,0,3, 4,3,7,  // -X face
            1,5,6, 1,6,2,  // +X face
            4,5,1, 4,1,0,  // -Y face
            3,2,6, 3,6,7,  // +Y face
        };
    }
    std::vector<float> Vertices;
    std::vector<uint32_t> indices;
    std::shared_ptr<Camera> camera;
};