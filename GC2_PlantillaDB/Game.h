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

struct FireflyParticle
{
    DirectX::SimpleMath::Vector3 position;
    DirectX::SimpleMath::Vector3 velocity;
    float lifetime;
    float maxLifetime;
    float blinkTimer;
    float rotation;
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

    void UpdateDayNightCycle(float elapsedTime);

    void AddInstancedObject(
        Model* modelPtr,
        const DirectX::SimpleMath::Matrix& baseTransform,
        float instanceX,
        float instanceZ,
        float fallbackY,
        float modelSpecificOffsetY
    );

    void RenderShadowPass();
    void RenderMinimapPass();
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
    std::unique_ptr<DirectX::SpriteBatch>   m_spriteBatch3D; // Para elementos en el mundo 3D (lucirnagas)
    std::unique_ptr<DirectX::SpriteBatch>   m_spriteBatchUI; // Para la interfaz 2D
    std::unique_ptr<DirectX::SpriteFont>    m_font;
    std::unique_ptr<DirectX::BasicEffect>   m_spriteEffect;
    Microsoft::WRL::ComPtr<ID3D11InputLayout> m_spriteInputLayout;

    // Recursos para el Sistema de Partculas de Lucirnagas
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_fireflyVertexBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_fireflyIndexBuffer;

    Microsoft::WRL::ComPtr<ID3D11VertexShader> m_fireflyVS;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>  m_fireflyPS;
    Microsoft::WRL::ComPtr<ID3D11InputLayout>  m_fireflyInputLayout;

    Microsoft::WRL::ComPtr<ID3D11Buffer> m_cbFireflyPerFrame;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_cbFireflyPerParticle;

    // Estructuras de Constant Buffers para las lucirnagas
    struct CB_Firefly_PerFrame
    {
        DirectX::SimpleMath::Matrix ViewProjection;
        DirectX::SimpleMath::Vector3 CameraUp_World;
        float _pad0;
        DirectX::SimpleMath::Vector3 CameraRight_World;
        float _pad1;
    };

    struct CB_Firefly_PerParticle
    {
        DirectX::SimpleMath::Vector3 ParticleCenter_World;
        float _pad0;
        DirectX::SimpleMath::Vector2 ParticleSize;
        float _pad1[2];
        DirectX::SimpleMath::Vector4 ParticleColor;
    };

    // SkyDome
    std::unique_ptr<DirectX::GeometricPrimitive>    m_skySphere;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_skyTextureSRV;
    std::unique_ptr<DirectX::BasicEffect>           m_skyEffect;
    Microsoft::WRL::ComPtr<ID3D11InputLayout>       m_skyInputLayout;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> m_skyDepthState;

    // Day-Night Cycle light properties
    float m_timeOfDay;
    float m_dayNightCycleSpeed;
    float m_sunPower;

    Microsoft::WRL::ComPtr<ID3D11RasterizerState>   m_shadowRasterizerState_CullFront;

	// Fireflies
    void InitializeFireflies();
    void UpdateFireflies(float elapsedTime);
    void ResetFirefly(FireflyParticle& particle);

    std::vector<FireflyParticle> m_fireflies;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_fireflyTexture;
    DirectX::BoundingBox m_fireflyVolume;
    static const int NUM_FIREFLIES = 300;

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

    // Iluminacin exclusiva para el minimapa
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_minimapLightPropertiesCB;
    PSLightPropertiesData m_minimapLightData;

    // Shadow mapping
    Microsoft::WRL::ComPtr<ID3D11Texture2D>           m_shadowMapTexture;
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView>  m_shadowMapDSV;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_shadowMapSRV;
    Microsoft::WRL::ComPtr<ID3D11SamplerState>      m_shadowSamplerState; 
    Microsoft::WRL::ComPtr<ID3D11RasterizerState>   m_shadowRasterizerState; 
    Microsoft::WRL::ComPtr<ID3D11DepthStencilState> m_shadowDepthState;

    DirectX::SimpleMath::Matrix m_lightViewMatrix;
    DirectX::SimpleMath::Matrix m_lightProjectionMatrix;

    Microsoft::WRL::ComPtr<ID3D11VertexShader> m_shadowVertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>  m_shadowPixelShader;
    Microsoft::WRL::ComPtr<ID3D11InputLayout>  m_shadowInputLayout;

    Microsoft::WRL::ComPtr<ID3D11VertexShader> m_shadowVertexShader_AlphaClip; 
    Microsoft::WRL::ComPtr<ID3D11PixelShader>  m_shadowPixelShader_AlphaClip;

    static const int SHADOW_MAP_SIZE = 4096;

    // Minimap Resources
    Microsoft::WRL::ComPtr<ID3D11Texture2D>           m_minimapTexture;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView>  m_minimapRTV;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_minimapSRV;
    Microsoft::WRL::ComPtr<ID3D11Texture2D>           m_minimapDepthTexture; 
    Microsoft::WRL::ComPtr<ID3D11DepthStencilView>  m_minimapDSV;          
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_playerIconTexture;
    D3D11_VIEWPORT                                   m_minimapViewport;
    static const int MINIMAP_SIZE = 256;

    // Post-processing BLOOM
    Microsoft::WRL::ComPtr<ID3D11Texture2D>           m_sceneTexture;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView>  m_sceneRTV;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_sceneSRV;

    Microsoft::WRL::ComPtr<ID3D11Texture2D>           m_bloomExtractTexture;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView>  m_bloomExtractRTV;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_bloomExtractSRV;

    Microsoft::WRL::ComPtr<ID3D11Texture2D>           m_blurTexture;
    Microsoft::WRL::ComPtr<ID3D11RenderTargetView>  m_blurRTV;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_blurSRV;

    Microsoft::WRL::ComPtr<ID3D11VertexShader> m_fullscreenQuadVS;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>  m_bloomExtractPS;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>  m_gaussianBlurHorizontalPS;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>  m_gaussianBlurVerticalPS;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>  m_bloomCompositePS;

    Microsoft::WRL::ComPtr<ID3D11Buffer> m_cbBloomParameters;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_cbBlurParameters;

    struct CB_BloomParameters
    {
        float bloomThreshold = 0.9f;
        float bloomIntensity = 1.8f;
        float sceneIntensity = 0.8f;
        float saturation = 1.2f;
    };

    CB_BloomParameters m_bloomParamsData;

    struct CB_BlurParameters
    {
        DirectX::SimpleMath::Vector2 texelSize;
        float _padding1;
        float _padding2;
    };
    CB_BlurParameters m_blurParamsData;

    // GRADIENTS
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_colorGradingLutSRV;

    
};
