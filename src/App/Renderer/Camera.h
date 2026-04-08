#pragma once

#include <DirectXMath.h>
#include <cstdint>

class Camera {
public:
    Camera();
    ~Camera() = default;

    // Update camera (call each frame)
    void Update(float deltaTime);

    // Input handling
    void OnMouseMove(int deltaX, int deltaY);
    void OnKeyDown(uint32_t key);
    void OnKeyUp(uint32_t key);

    // Getters for raytracing
    DirectX::XMFLOAT3 GetPosition() const { return m_position; }
    DirectX::XMFLOAT3 GetForward() const { return m_forward; }
    DirectX::XMFLOAT3 GetRight() const { return m_right; }
    DirectX::XMFLOAT3 GetUp() const { return m_up; }
    
    // Get ray direction for a given pixel (normalized device coords -1 to 1)
    DirectX::XMFLOAT3 GetRayDirection(float ndcX, float ndcY) const;

    // Camera parameters
    void SetPosition(const DirectX::XMFLOAT3& pos) { m_position = pos; }
    void SetLookAt(const DirectX::XMFLOAT3& target);
    void SetFOV(float fovDegrees);
    void SetAspectRatio(float aspect) { m_aspectRatio = aspect; }
    void SetMoveSpeed(float speed) { m_moveSpeed = speed; }
    void SetLookSpeed(float speed) { m_lookSpeed = speed; }

    float GetFOV() const { return m_fov; }
    float GetAspectRatio() const { return m_aspectRatio; }

    struct CameraData {
        DirectX::XMFLOAT3 position;
        float padding1;
        DirectX::XMFLOAT3 forward;
        float padding2;
        DirectX::XMFLOAT3 right;
        float padding3;
        DirectX::XMFLOAT3 up;
        float fov;
    };
    
private:
    void UpdateVectors();  // Recalculate forward/right/up from yaw/pitch

    // Position and orientation
    DirectX::XMFLOAT3 m_position;
    DirectX::XMFLOAT3 m_forward;
    DirectX::XMFLOAT3 m_right;
    DirectX::XMFLOAT3 m_up;

    // Rotation (Euler angles)
    float m_yaw;    // Rotation around Y axis (left/right)
    float m_pitch;  // Rotation around X axis (up/down)

    // Camera parameters
    float m_fov;          // Field of view in radians
    float m_aspectRatio;  // Width / height
    float m_moveSpeed;    // Units per second
    float m_lookSpeed;    // Radians per pixel

    // Input state (WASD + mouse)
    bool m_keyW, m_keyA, m_keyS, m_keyD;
    bool m_keySpace, m_keyShift;  // Up/down movement
};