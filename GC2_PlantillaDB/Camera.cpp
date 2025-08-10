// Camera.cpp
#include "pch.h" 
#include "Camera.h"
#include <algorithm> 

// Usar DirectX::SimpleMath directamente para evitar ambigüedades si hay otros Vector3, etc.
using DirectX::SimpleMath::Vector3;
using DirectX::SimpleMath::Matrix;

// Constructor
Camera::Camera(int screenWidth, int screenHeight)
    : m_position(0.0f, 0.0f, -5.0f), // Posición inicial por defecto
    m_yaw(0.0f),
    m_pitch(0.0f),
    m_fieldOfView(DirectX::XM_PIDIV4), // 45 grados
    m_nearPlane(0.1f),
    m_farPlane(1000.0f)
{
    UpdateProjectionMatrix(screenWidth, screenHeight);
    UpdateViewMatrix(); // Calcula m_forward, m_right, m_up y m_viewMatrix iniciales
}

// Establecer posición
void Camera::SetPosition(const Vector3& position)
{
    m_position = position;
    UpdateViewMatrix();
}

// Establecer rotación (yaw y pitch en radianes)
void Camera::SetRotation(float yaw, float pitch)
{
    m_yaw = yaw;
    m_pitch = pitch;

    // Restringir pitch para evitar que la cámara se voltee
    m_pitch = std::clamp(m_pitch, -DirectX::XM_PIDIV2 + 0.001f, DirectX::XM_PIDIV2 - 0.001f);

    UpdateViewMatrix();
}

// Mover en coordenadas del mundo
void Camera::Move(const Vector3& translation)
{
    m_position += translation;
    UpdateViewMatrix();
}

// Moverse relativo a la orientación de la cámara
void Camera::MoveRelative(const Vector3& translation)
{
    // Nota: m_forward, m_right, y m_up deben estar actualizados por UpdateViewMatrix
    // antes de que se llame a esta función si la rotación ha cambiado justo antes.
    // Sin embargo, como UpdateViewMatrix se llama al final de Rotate(), esto está bien.

    m_position += translation.x * m_right;
    m_position += translation.y * m_up;     // Movimiento vertical local
    m_position += translation.z * m_forward;
    UpdateViewMatrix();
}

// Rotar la cámara (deltas en radianes)
void Camera::Rotate(float yawDelta, float pitchDelta)
{
    m_yaw += yawDelta;
    m_pitch += pitchDelta;

    // Normalizar yaw para mantenerlo en [0, 2*PI) opcionalmente, pero no estrictamente necesario
    // m_yaw = fmodf(m_yaw, DirectX::XM_2PI);
    // if (m_yaw < 0.0f) {
    //     m_yaw += DirectX::XM_2PI;
    // }

    // Restringir pitch para evitar que la cámara se voltee
    m_pitch = std::clamp(m_pitch, -DirectX::XM_PIDIV2 + 0.001f, DirectX::XM_PIDIV2 - 0.001f);

    UpdateViewMatrix();
}

// Actualizar la matriz de vista
void Camera::UpdateViewMatrix()
{
    Matrix rotationMatrix = Matrix::CreateFromYawPitchRoll(m_yaw, m_pitch, 0.0f);

    // Calcular el vector "adelante" de la cámara
    m_forward = Vector3::TransformNormal(WORLD_FORWARD, rotationMatrix);
    m_forward.Normalize();

    // Calcular el vector "derecha" de la cámara
    // Este 'm_right' será horizontal respecto al mundo
    m_right = WORLD_UP.Cross(m_forward);
    m_right.Normalize();

    // Calcular el vector "arriba" local de la cámara
    // Este 'm_up' se inclinará con el pitch
    m_up = m_right.Cross(m_forward); // Originalmente m_forward.Cross(m_right); pero para LH, Right x Forward = Up
    m_up.Normalize();                // Si m_right = WORLD_UP.Cross(m_forward), entonces m_up = m_forward.Cross(m_right) da un vector que mira "hacia arriba" relativo a la inclinación de la cámara.

    // El punto hacia donde la cámara está mirando
    Vector3 lookAtTarget = m_position + m_forward;

    // Crear la matriz de vista
    m_viewMatrix = Matrix::CreateLookAt(m_position, lookAtTarget, m_up);
}

// Actualizar la matriz de proyección
void Camera::UpdateProjectionMatrix(int screenWidth, int screenHeight)
{
    if (screenHeight == 0) // Evitar división por cero
    {
        screenHeight = 1;
    }
    m_aspectRatio = static_cast<float>(screenWidth) / static_cast<float>(screenHeight);

    m_projectionMatrix = Matrix::CreatePerspectiveFieldOfView(
        m_fieldOfView,
        m_aspectRatio,
        m_nearPlane,
        m_farPlane
    );
}

// Getters
const Matrix& Camera::GetViewMatrix() const
{
    return m_viewMatrix;
}

const Matrix& Camera::GetProjectionMatrix() const
{
    return m_projectionMatrix;
}

const Vector3& Camera::GetPosition() const
{
    return m_position;
}

Vector3 Camera::GetForward() const
{
    return m_forward;
}

Vector3 Camera::GetRight() const
{
    return m_right;
}

Vector3 Camera::GetUp() const
{
    return m_up; // Devuelve el vector "arriba" local de la cámara
}

float Camera::GetYaw() const
{
    return m_yaw;
}

float Camera::GetPitch() const
{
    return m_pitch;
}

float Camera::GetFarPlane() const
{
    return m_farPlane;
}

DirectX::SimpleMath::Quaternion Camera::GetRotation() const
{
    // DirectX::SimpleMath::Quaternion::CreateFromYawPitchRoll espera los ángulos en radianes.
    // Asumimos que tus miembros Yaw y Pitch ya están en radianes.
    // Si están en grados, necesitarías convertirlos: DirectX::XMConvertToRadians(this->Yaw)
    return DirectX::SimpleMath::Quaternion::CreateFromYawPitchRoll(this->m_yaw, this->m_pitch, 0.0f); // Roll es 0.0
}