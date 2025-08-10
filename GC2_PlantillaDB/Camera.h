#pragma once // O usa #ifndef CAMERA_H / #define CAMERA_H / #endif

#include <SimpleMath.h> // De DirectX ToolKit

class Camera
{
public:
    // Constructor
    Camera(int screenWidth, int screenHeight);

    // M�todos para actualizar la c�mara
    void SetPosition(const DirectX::SimpleMath::Vector3& position);
    void SetRotation(float yaw, float pitch); // Yaw en radianes, Pitch en radianes

    void Move(const DirectX::SimpleMath::Vector3& translation); // Moverse en coordenadas del mundo
    void MoveRelative(const DirectX::SimpleMath::Vector3& translation); // Moverse relativo a la orientaci�n de la c�mara
    void Rotate(float yawDelta, float pitchDelta); // Rotar en radianes

    void UpdateViewMatrix();
    void UpdateProjectionMatrix(int screenWidth, int screenHeight);

    // M�todos para obtener matrices y propiedades
    const DirectX::SimpleMath::Matrix& GetViewMatrix() const;
    const DirectX::SimpleMath::Matrix& GetProjectionMatrix() const;
    const DirectX::SimpleMath::Vector3& GetPosition() const;
    DirectX::SimpleMath::Vector3 GetForward() const;
    DirectX::SimpleMath::Vector3 GetRight() const;
    DirectX::SimpleMath::Vector3 GetUp() const; // El vector "arriba" local de la c�mara

    float GetYaw() const;
    float GetPitch() const;
    float GetFarPlane() const;
    DirectX::SimpleMath::Quaternion GetRotation() const;

private:
    // Propiedades de la c�mara
    DirectX::SimpleMath::Vector3 m_position;
    float m_yaw;   // Rotaci�n alrededor del eje Y global (radianes)
    float m_pitch; // Rotaci�n alrededor del eje X local (radianes)

    // Vectores de direcci�n (calculados a partir de yaw y pitch)
    DirectX::SimpleMath::Vector3 m_forward;
    DirectX::SimpleMath::Vector3 m_right;
    DirectX::SimpleMath::Vector3 m_up; // Vector "arriba" local de la c�mara

    // Matrices de transformaci�n
    DirectX::SimpleMath::Matrix m_viewMatrix;
    DirectX::SimpleMath::Matrix m_projectionMatrix;

    // Propiedades de la proyecci�n
    float m_fieldOfView;  // Campo de visi�n en radianes
    float m_aspectRatio;
    float m_nearPlane;
    float m_farPlane;

    // Constantes
    const DirectX::SimpleMath::Vector3 WORLD_UP = DirectX::SimpleMath::Vector3(0.0f, 1.0f, 0.0f);
    const DirectX::SimpleMath::Vector3 WORLD_FORWARD = DirectX::SimpleMath::Vector3(0.0f, 0.0f, 1.0f);
    const DirectX::SimpleMath::Vector3 WORLD_RIGHT = DirectX::SimpleMath::Vector3(1.0f, 0.0f, 0.0f);
};