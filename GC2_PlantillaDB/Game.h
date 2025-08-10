//
// Game.h
//

#pragma once

#include "DeviceResources.h"
#include "StepTimer.h"
#include "Camera.h"
#include <GeometricPrimitive.h>
#include <CommonStates.h>
#include <SpriteBatch.h>
#include <SpriteFont.h>
#include <Effects.h>
#include "Terrain.h"
#include "Model.h"
#include <vector>   
#include <string>   
#include <memory> 

struct GameObjectInstance
{
    Model* baseModel = nullptr;
    DirectX::SimpleMath::Matrix worldTransform;

    GameObjectInstance(Model* model, const DirectX::SimpleMath::Matrix& transform)
        : baseModel(model), worldTransform(transform) {
    }
};
// A basic game implementation that creates a D3D11 device and
// provides a game loop.
class Game final : public DX::IDeviceNotify
{
public:

    Game() noexcept(false);
    ~Game() = default;

    Game(Game&&) = default;
    Game& operator= (Game&&) = default;

    Game(Game const&) = delete;
    Game& operator= (Game const&) = delete;

    // Initialization and management
    void Initialize(HWND window, int width, int height);

    // Basic game loop
    void Tick();

    // IDeviceNotify
    void OnDeviceLost() override;
    void OnDeviceRestored() override;

    // Messages
    void OnActivated();
    void OnDeactivated();
    void OnSuspending();
    void OnResuming();
    void OnWindowMoved();
    void OnDisplayChange();
    void OnWindowSizeChanged(int width, int height);

    // Properties
    void GetDefaultSize( int& width, int& height ) const noexcept;

private:

    void Update(DX::StepTimer const& timer);
    void Render();

    void Clear();

    void CreateDeviceDependentResources();
    void CreateWindowSizeDependentResources();

    void AddInstancedObject(
        Model* modelPtr,
        const DirectX::SimpleMath::Matrix& baseTransform,
        float instanceX,
        float instanceZ,
        float fallbackY,
        float modelSpecificOffsetY
    );

    // Device resources.
    std::unique_ptr<DX::DeviceResources> m_deviceResources;

    // Rendering loop timer.
    DX::StepTimer m_timer;

    // Camera items
    std::unique_ptr<Camera> m_camera;
    std::unique_ptr<DirectX::Keyboard> m_keyboard;
    std::unique_ptr<DirectX::Mouse> m_mouse;

    float m_normalSpeed;
    float m_sprintSpeed;
    float m_currentSpeed; 
    bool m_isSprinting;
    int m_wTapCount;
    float m_wTapTimer; 
    const float m_doubleTapTimeLimit = 0.3f; 
    bool m_wKeyWasPressedInPreviousFrame;

    //--- KEYBOARD AND MOUSE STATES / TRACKERS
    DirectX::Keyboard::State m_kbState;          
    DirectX::Mouse::State m_mouseState;
    DirectX::Keyboard::KeyboardStateTracker m_kbTracker;
    DirectX::Mouse::ButtonStateTracker m_mouseTracker;

    // Primitives
    std::unique_ptr<DirectX::GeometricPrimitive> m_cube;
    DirectX::SimpleMath::Matrix                  m_cubeWorldMatrix;
    // Opcional pero recomendado para estados de renderizado estándar:
    std::unique_ptr<DirectX::CommonStates>       m_states;
   
    // Fonts
    std::unique_ptr<DirectX::SpriteBatch>   m_spriteBatch;
    std::unique_ptr<DirectX::SpriteFont>    m_font;

    // SkyDome
    std::unique_ptr<DirectX::GeometricPrimitive>    m_skySphere;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_skyTextureSRV;
    std::unique_ptr<DirectX::BasicEffect>           m_skyEffect;
    Microsoft::WRL::ComPtr<ID3D11InputLayout>       m_skyInputLayout;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> m_skyDepthState;

    // Terrain
    std::unique_ptr<Terrain> m_terrain;

    // Models
    std::unique_ptr<Model> m_blacksmith;
    std::unique_ptr<Model> m_green_tree1;
    std::unique_ptr<Model> m_forest_pine1;
    std::unique_ptr<Model> m_forest_pine2;
    std::unique_ptr<Model> m_forest_pine3;
    std::unique_ptr<Model> m_cart;
    std::unique_ptr<Model> m_windmill;
    std::unique_ptr<Model> m_rock1;
    std::unique_ptr<Model> m_rock2;
    std::unique_ptr<Model> m_rock3;
    std::unique_ptr<Model> m_rock4;
    std::unique_ptr<Model> m_rock5;
    std::unique_ptr<Model> m_rock6;

    // Collisions
    std::unique_ptr<DirectX::GeometricPrimitive> m_debugBoxDrawer;
    std::unique_ptr<DirectX::GeometricPrimitive> m_debugSphereDrawer;
    bool m_drawDebugCollisions;
    DirectX::BoundingBox m_cameraBoxToDraw;
    std::vector<DirectX::BoundingSphere> m_modelSpheresToDraw;
    std::vector<DirectX::BoundingBox> m_modelPartBoxesToDraw;

    Microsoft::WRL::ComPtr<ID3D11Buffer> m_lightPropertiesCB;
    Microsoft::WRL::ComPtr<ID3D11SamplerState> m_samplerState;
    PSLightPropertiesData m_lightData;

    // Model instances
    std::vector<GameObjectInstance> m_worldInstances;

};
