#include "Camera.h"
#include <algorithm>
#include <cmath>

using namespace DirectX;

Camera::Camera()
    : m_position(0.0f, 2.0f, -5.0f)
    , m_forward(0.0f, 0.0f, 1.0f)
    , m_right(1.0f, 0.0f, 0.0f)
    , m_up(0.0f, 1.0f, 0.0f)
    , m_yaw(0.0f)
    , m_pitch(0.0f)
    , m_fov(XM_PIDIV4)  // 45 degrees
    , m_aspectRatio(16.0f / 9.0f)
    , m_moveSpeed(5.0f)
    , m_lookSpeed(0.003f)
    , m_keyW(false), m_keyA(false), m_keyS(false), m_keyD(false)
    , m_keySpace(false), m_keyShift(false)
{
    UpdateVectors();
}

void Camera::Update(float deltaTime) {
    // Movement vector in camera space
    XMFLOAT3 move(0, 0, 0);

    if (m_keyW) move.z += 1.0f;  // Forward
    if (m_keyS) move.z -= 1.0f;  // Backward
    if (m_keyD) move.x += 1.0f;  // Right
    if (m_keyA) move.x -= 1.0f;  // Left
    if (m_keySpace) move.y += 1.0f;   // Up
    if (m_keyShift) move.y -= 1.0f;   // Down

    // Convert to world space and apply
    XMVECTOR moveVec = XMLoadFloat3(&move);
    XMVECTOR forward = XMLoadFloat3(&m_forward);
    XMVECTOR right = XMLoadFloat3(&m_right);
    XMVECTOR up = XMVectorSet(0, 1, 0, 0);  // Always world up

    XMVECTOR pos = XMLoadFloat3(&m_position);
    pos += forward * XMVectorGetZ(moveVec) * m_moveSpeed * deltaTime;
    pos += right * XMVectorGetX(moveVec) * m_moveSpeed * deltaTime;
    pos += up * XMVectorGetY(moveVec) * m_moveSpeed * deltaTime;

    XMStoreFloat3(&m_position, pos);
}

void Camera::OnMouseMove(int deltaX, int deltaY) {
    // Update yaw and pitch based on mouse movement
    m_yaw += deltaX * m_lookSpeed;
    m_pitch -= deltaY * m_lookSpeed;  // Invert Y (down = look up)

    // Clamp pitch to prevent flipping
    const float maxPitch = XM_PIDIV2 - 0.01f;  // 89 degrees
    m_pitch = std::clamp(m_pitch, -maxPitch, maxPitch);

    UpdateVectors();
}

void Camera::OnKeyDown(uint32_t key) {
    switch (key) {
        case 'W': m_keyW = true; break;
        case 'A': m_keyA = true; break;
        case 'S': m_keyS = true; break;
        case 'D': m_keyD = true; break;
        case VK_SPACE: m_keySpace = true; break;
        case VK_SHIFT: m_keyShift = true; break;
    }
}

void Camera::OnKeyUp(uint32_t key) {
    switch (key) {
        case 'W': m_keyW = false; break;
        case 'A': m_keyA = false; break;
        case 'S': m_keyS = false; break;
        case 'D': m_keyD = false; break;
        case VK_SPACE: m_keySpace = false; break;
        case VK_SHIFT: m_keyShift = false; break;
    }
}

void Camera::UpdateVectors() {
    // Calculate forward vector from yaw and pitch
    float cosYaw = cosf(m_yaw);
    float sinYaw = sinf(m_yaw);
    float cosPitch = cosf(m_pitch);
    float sinPitch = sinf(m_pitch);

    m_forward.x = sinYaw * cosPitch;
    m_forward.y = sinPitch;
    m_forward.z = cosYaw * cosPitch;

    // Normalize forward
    XMVECTOR forward = XMLoadFloat3(&m_forward);
    forward = XMVector3Normalize(forward);
    XMStoreFloat3(&m_forward, forward);

    // Calculate right vector (cross with world up)
    XMVECTOR worldUp = XMVectorSet(0, 1, 0, 0);
    XMVECTOR right = XMVector3Cross(forward, worldUp);
    right = XMVector3Normalize(right);
    XMStoreFloat3(&m_right, right);

    // Calculate up vector (cross of right and forward)
    XMVECTOR up = XMVector3Cross(right, forward);
    XMStoreFloat3(&m_up, up);
}

XMFLOAT3 Camera::GetRayDirection(float ndcX, float ndcY) const {
    // ndcX, ndcY are in range [-1, 1]
    // Convert to camera ray direction

    float tanHalfFOV = tanf(m_fov * 0.5f);
    
    // Scale by aspect ratio and FOV
    float x = ndcX * m_aspectRatio * tanHalfFOV;
    float y = ndcY * tanHalfFOV;
    float z = 1.0f;

    // Ray direction in camera space
    XMVECTOR rayDir = XMVectorSet(x, y, z, 0);

    // Transform to world space
    XMVECTOR forward = XMLoadFloat3(&m_forward);
    XMVECTOR right = XMLoadFloat3(&m_right);
    XMVECTOR up = XMLoadFloat3(&m_up);

    XMVECTOR worldDir = right * XMVectorGetX(rayDir) +
                        up * XMVectorGetY(rayDir) +
                        forward * XMVectorGetZ(rayDir);

    worldDir = XMVector3Normalize(worldDir);

    XMFLOAT3 result;
    XMStoreFloat3(&result, worldDir);
    return result;
}

void Camera::SetLookAt(const XMFLOAT3& target) {
    XMVECTOR pos = XMLoadFloat3(&m_position);
    XMVECTOR tgt = XMLoadFloat3(&target);
    XMVECTOR dir = XMVector3Normalize(tgt - pos);

    XMFLOAT3 direction;
    XMStoreFloat3(&direction, dir);

    // Calculate yaw and pitch from direction
    m_yaw = atan2f(direction.x, direction.z);
    m_pitch = asinf(direction.y);

    UpdateVectors();
}

void Camera::SetFOV(float fovDegrees) {
    m_fov = XMConvertToRadians(fovDegrees);
}