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
    m_nearPlane(1.0f),
    m_farPlane(5000.0f)
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
    // 1. Crear la matriz de rotacin a partir de yaw y pitch.
    Matrix rotationMatrix = Matrix::CreateFromYawPitchRoll(m_yaw, m_pitch, 0.0f);

    // 2. An es necesario actualizar los vectores m_forward, m_right, m_up
    //    porque otras partes del cdigo (como MoveRelative) los usan.
    m_forward = Vector3::TransformNormal(WORLD_FORWARD, rotationMatrix);
    m_right = Vector3::TransformNormal(WORLD_RIGHT, rotationMatrix);
    m_up = Vector3::TransformNormal(WORLD_UP, rotationMatrix);

    // 3. Construir la matriz de mundo de la cmara (su posicin y orientacin en el mundo).
    //    El orden es Rotacin * Traslacin.
    Matrix translationMatrix = Matrix::CreateTranslation(m_position);
    Matrix cameraWorldMatrix = rotationMatrix * translationMatrix;

    // 4. La matriz de vista es la INVERSA de la matriz de mundo de la cmara.
    //    Esto transforma todo el mundo para que la cmara est en el origen, mirando hacia adelante.
    //    Este mtodo es el ms estable y directo.
    m_viewMatrix = cameraWorldMatrix.Invert();
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