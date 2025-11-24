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
    bool useAutoCollision;

    GameObjectInstance(Model* model, const DirectX::SimpleMath::Matrix& transform, bool autoCol=true)
        : baseModel(model), worldTransform(transform), useAutoCollision(autoCol){
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

struct SmokeParticle
{
    DirectX::SimpleMath::Vector3 position;
    DirectX::SimpleMath::Vector3 velocity;
    float lifetime;
    float maxLifetime;
    float scale;     
    float alpha;     
};

struct GameItem
{
    DirectX::SimpleMath::Vector3 position;
    bool isActive = true;          // Si es true, existe en el mundo
    int type = 0;                  // 0 = Pieza Mística, 1 = Leña
    float radius = 20.0f;           // Distancia para recogerlo
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
        float modelSpecificOffsetY,
		bool useAutoCollision = true
    );

    void RenderShadowPass();
    void RenderMinimapPass();

    // --- Update() Function Helpers
    void UpdateInput(float elapsedTime);              // Leer teclado/mouse
    void UpdatePlayer(float elapsedTime); // Movimiento WASD, Montar caballo, Física
    void UpdateCamera(float elapsedTime); // Toda la lógica que acabamos de arreglar
    void UpdateGameplay(float elapsedTime); // Win/Lose, Coleccionables, Tiempo
    void UpdateWorld(float elapsedTime);    // Ciclo día/noche, Luciérnagas

    // --- Render() Function Helpers
    void RenderScene();           // Terreno, Modelos, Jugador, Skybox, Debug 3D
    void RenderParticles();       // Luciérnagas (Transparencias)
    void RenderPostProcessing();  // Bloom
    void RenderUI();              // HUD 2D y Minimapa

    void BuildCustomColliders();
    void RenderInteractionBillboards();

    // -- Editor Mode
	void UpdateEditor(float elapsedTime);


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

    // UI ELEMENTS

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_blankTexture;

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
    std::unique_ptr<Model> m_blacksmith; //HERRERIA
    std::unique_ptr<Model> m_green_tree1; //ARBOL ESTANDAR
    std::unique_ptr<Model> m_forest_pine1;
    std::unique_ptr<Model> m_forest_pine2;
    std::unique_ptr<Model> m_forest_pine3; //PINO
    std::unique_ptr<Model> m_cart; //CARRITO
    std::unique_ptr<Model> m_windmill; //MOLINO
    std::unique_ptr<Model> m_rock1;
    std::unique_ptr<Model> m_rock2;
    std::unique_ptr<Model> m_rock3;
    std::unique_ptr<Model> m_rock4;
    std::unique_ptr<Model> m_rock5;
    std::unique_ptr<Model> m_rock6;  //ROCAS
	std::unique_ptr<Model> m_house1; //HALL
	std::unique_ptr<Model> m_house2; //CASA PEQUEÑA
	std::unique_ptr<Model> m_house3; //CASA GRANDE
	std::unique_ptr<Model> m_house4; //CASA MEDIANA
	std::unique_ptr<Model> m_knight; //CABALLERO
	std::unique_ptr<Model> m_crystal; //CRISTAL MISTICO
    std::unique_ptr<Model> m_branch; //RAMA
	std::unique_ptr<Model> m_axe; //HOGUERA
	std::unique_ptr<Model> m_dungeonGate; //DUNGEON ENTRANCE
	std::unique_ptr<Model> m_dungeonInterior; //DUNGEON 
	std::unique_ptr<Model> m_log; //TRONCO 
	std::unique_ptr<Model> m_sword; //ESPADA


    // MODEL POSES
    std::vector<std::unique_ptr<Model>> m_animIdle;   // Lista dinámica para Idle
    std::vector<std::unique_ptr<Model>> m_animWalk;   // Lista dinámica para Walk
    std::vector<std::unique_ptr<Model>> m_animMelee;
    Model* m_currentModel = nullptr;                  // Puntero al modelo actual

    // Variables de tiempo
    float m_animTimer = 0.0f;
    int m_currentFrame = 0;
    bool m_wasMoving = false; // Para detectar cambio de estado

    void LoadAnimationSequence(
        std::vector<std::unique_ptr<Model>>& targetVector,
        std::string basePath,
        int count,
        ID3D11Device* device,
        ID3D11DeviceContext* context
    );

    // player logic

    float m_walkBobTimer = 0.0f;  
    float m_playerOffsetY = 2.5f;  
    bool m_isThirdPerson = true;

    DirectX::SimpleMath::Vector3 m_playerPos;

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

    //Shaders
    Microsoft::WRL::ComPtr<ID3D11PixelShader> m_mysticPS;


    static const int SHADOW_MAP_SIZE = 1024;

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

    // USER INTERFACE (UI)

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_uiAxeTexture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_uiSwordTexture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_uiBranchTexture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_uiRockTexture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_uiMysticTexture;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_InteractionTexture;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_AnvilTexture;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_uiLogTexture;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_uiMysticCookedTexture;

    float m_itemPopupTimer = 0.0f;
    int m_popupItemType = -1;

    // PARTICLES
    //SMOKE

    std::vector<SmokeParticle> m_smokeParticles;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_smokeTexture;
    static const int NUM_SMOKE_PARTICLES = 50; 
    DirectX::SimpleMath::Vector3 m_chimneyPos; 

    void InitializeSmoke();
    void UpdateSmoke(float elapsedTime);
    void RenderSmoke(); 

    struct CB_BloomParameters
    {
        float bloomThreshold = 0.5f;
        float bloomIntensity = 2.8f;
        float sceneIntensity = 1.0f;
        float saturation = 1.4f;
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

    // GAME LOGIC

    std::vector<GameItem> m_gameItems; // Lista de todos los objetos

    int m_collectedPieces = 0;      // Cuántas piezas llevas
    bool m_hasFirewood = false;     // ¿Tienes la leña?
    bool m_swordRepaired = false;  

    // Inventory

    int m_stonesCount = 0;
    int m_branchesCount = 0;

    const int COST_STONES = 3;
    const int COST_BRANCHES = 3;

    bool m_gameWon = false;
    bool m_gameLost = false;
    float m_gameTimer = 300.0f;     

    bool m_isChopping = false;
    float m_treeInteractTimer = 0.0f; 
    const float TREE_COOLDOWN = 1.0f; 
    bool m_hasAxe = false;
    int m_currentToolId = 0;

    int m_woodCount;         
    int m_rawStonesCount;    
    int m_refinedStonesCount; 

    // Lógica de Tala
    bool m_hasHitTreeThisSwing;

    // Lógica del Horno
    bool m_isCooking;
    float m_furnaceTimer;
    DirectX::BoundingBox m_furnaceTriggerBox;

    // Lógica de Mazmorra y Victoria
    bool m_isInDungeon;
    DirectX::SimpleMath::Vector3 m_dungeonSpawnPos;
    DirectX::BoundingBox m_catacombsEntranceTrigger;
    DirectX::BoundingBox m_dungeonAltarTrigger;



    DirectX::SimpleMath::Vector3 m_blacksmithPos = DirectX::SimpleMath::Vector3(-70.7f, 3.5f, 563.0f);
    DirectX::SimpleMath::Vector3 m_catacombsPos = DirectX::SimpleMath::Vector3(-50.0f, 0.0f, -50.0f);

    // INTERACTION BOUNDING BOXES

    DirectX::BoundingBox m_anvilTriggerBox;


    // LOCATION BOUNDING BOXES
    std::vector<DirectX::BoundingBox> m_customColliders;

    // END GAME LOGIC

    // DEBUG UI

    std::wstring m_debugModelName = L"";


    // EDITOR MODE
    bool m_inEditorMode = false;
    int m_selectedInstanceIndex = 0; 
    float m_editorMoveSpeed = 20.0f;
    float m_editorRotSpeed = 1.5f;


};
