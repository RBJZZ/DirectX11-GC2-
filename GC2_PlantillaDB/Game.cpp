//
// Game.cpp
//

#include "pch.h"
#include "Game.h"
#include <VertexTypes.h>
#include <d3dcompiler.h>
using namespace DirectX::SimpleMath;

extern void ExitGame() noexcept;

using namespace DirectX;

using Microsoft::WRL::ComPtr;

Game::Game() noexcept(false) :
    m_kbState{},
    m_mouseState{},
    m_normalSpeed(20.0f),
    m_sprintSpeed(45.0f),
    m_currentSpeed(10.0f),
    m_isSprinting(false),
    m_wTapCount(0),
    m_wTapTimer(0.0f),
    m_wKeyWasPressedInPreviousFrame(false),
    m_drawDebugCollisions(true),
    m_timeOfDay(0.25f),
    m_dayNightCycleSpeed(0.002f),
    m_sunPower(0.0f),

    m_woodCount(0),
    m_rawStonesCount(0),
    m_refinedStonesCount(0),
    m_hasHitTreeThisSwing(false),
    m_isCooking(false),
    m_furnaceTimer(0.0f),
    m_isInDungeon(false)
{

    m_deviceResources = std::make_unique<DX::DeviceResources>();
    // TODO: Provide parameters for swapchain format, depth/stencil format, and backbuffer count.
    //   Add DX::DeviceResources::c_AllowTearing to opt-in to variable rate displays.
    //   Add DX::DeviceResources::c_EnableHDR for HDR10 display.
    m_deviceResources->RegisterDeviceNotify(this);
    m_currentSpeed = m_normalSpeed;

    m_fireflyVolume = DirectX::BoundingBox(
        DirectX::SimpleMath::Vector3(50.f, -5.f, -150.f),
        DirectX::SimpleMath::Vector3(150.f, 20.f, 200.f)
    );
}

// Initialize the Direct3D resources required to run.
void Game::Initialize(HWND window, int width, int height)
{

    m_keyboard = std::make_unique<DirectX::Keyboard>();
    m_mouse = std::make_unique<DirectX::Mouse>();
    m_mouse->SetWindow(window);

    m_deviceResources->SetWindow(window, width, height);

    m_deviceResources->CreateDeviceResources();
    CreateDeviceDependentResources();

    m_deviceResources->CreateWindowSizeDependentResources();
    CreateWindowSizeDependentResources();

    // TODO: Change the timer settings if you want something other than the default variable timestep mode.
    // e.g. for 60 FPS fixed timestep update logic, call:
    /*
    m_timer.SetFixedTimeStep(true);
    m_timer.SetTargetElapsedSeconds(1.0 / 60);
    */

    m_gameItems.clear();

    m_gameItems.push_back({ Vector3(10.0f, 2.0f, 10.0f), true, 0 });
    m_gameItems.push_back({ Vector3(-10.0f, 2.0f, 20.0f), true, 0 });
    m_gameItems.push_back({ Vector3(20.0f, 2.0f, -10.0f), true, 0 });
    m_gameItems.push_back({ Vector3(-20.0f, 2.0f, -20.0f), true, 0 });
    m_gameItems.push_back({ Vector3(0.0f, 2.0f, 30.0f), true, 0 });

    m_gameItems.push_back({ Vector3(5.0f, 1.0f, 5.0f), true, 1 });

    // RAMAS (Tipo 3)

    m_gameItems.push_back({ Vector3(5.0f, 0.5f, -65.0f), true, 3 });
    m_gameItems.push_back({ Vector3(8.0f, 0.5f, -60.0f), true, 3 });
    m_gameItems.push_back({ Vector3(2.0f, 0.5f, -62.0f), true, 3 });

    // PIEDRAS (Tipo 4)
    m_gameItems.push_back({ Vector3(-5.0f, 0.5f, -65.0f), true, 4 });
    m_gameItems.push_back({ Vector3(-8.0f, 0.5f, -60.0f), true, 4 });
    m_gameItems.push_back({ Vector3(-2.0f, 0.5f, -62.0f), true, 4 });

    m_collectedPieces = 0;
    m_hasFirewood = false;
    m_swordRepaired = false;
    m_gameWon = false;
    m_gameLost = false;
    m_gameTimer = 300.0f;
    m_stonesCount = 0;
    m_branchesCount = 0;
    m_hasAxe = false;

    m_timer.SetFixedTimeStep(true);
    m_timer.SetTargetElapsedSeconds(1.0 / 60.0);

}

#pragma region Frame Update
// Executes the basic game loop.
void Game::Tick()
{
    m_timer.Tick([&]()
    {
        Update(m_timer);
    });

    Render();
}

// Updates the world.
void Game::Update(DX::StepTimer const& timer)
{
    float elapsedTime = float(timer.GetElapsedSeconds());

    // 1. Entradas y Control General
    UpdateInput(elapsedTime);

    UpdateEditor(elapsedTime);

    // 2. Movimiento y Física del Jugador

    if (!m_inEditorMode) 
    {
        UpdatePlayer(elapsedTime);
    }


    // 3. Lógica de Cámara (Depende de la posición actualizada del jugador)
    UpdateCamera(elapsedTime);

    // 4. Ambiente y Luces (Depende de la cámara actualizada)
    UpdateWorld(elapsedTime);

	// 5. Lógica de Juego
	UpdateGameplay(elapsedTime);
}

#pragma endregion

#pragma region Frame Render
// Draws the scene.
void Game::Render()
{
    // No intentar renderizar antes de la primera actualización.
    if (m_timer.GetFrameCount() == 0) return;

    // 1. Pases Previos (Fuera de pantalla)
    RenderShadowPass();
    RenderMinimapPass();

    // 2. Configurar Render Target de ESCENA (HDR)
    auto context = m_deviceResources->GetD3DDeviceContext();
    auto depthStencil = m_deviceResources->GetDepthStencilView();
    const auto mainViewport = m_deviceResources->GetScreenViewport();

    // Dibujamos en la textura de escena, NO en la pantalla todavía
    context->OMSetRenderTargets(1, m_sceneRTV.GetAddressOf(), depthStencil);
    context->ClearRenderTargetView(m_sceneRTV.Get(), DirectX::Colors::CornflowerBlue);
    context->ClearDepthStencilView(depthStencil, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
    context->RSSetViewports(1, &mainViewport);

    // 3. Dibujar Mundo 3D (Sólidos y Cielo)
    m_deviceResources->PIXBeginEvent(L"Render Scene 3D");
    RenderScene();
    m_deviceResources->PIXEndEvent();

    // 4. Dibujar Partículas (Transparencias)
    m_deviceResources->PIXBeginEvent(L"Render Particles");
    RenderParticles();
    RenderSmoke();
    RenderInteractionBillboards();
    m_deviceResources->PIXEndEvent();

    // 5. Post-Procesamiento (Bloom) -> Esto escribe al BackBuffer final
    m_deviceResources->PIXBeginEvent(L"Post-Processing");
    RenderPostProcessing();
    m_deviceResources->PIXEndEvent();

    // 6. UI (Sobre todo lo demás)
    // Nota: RenderPostProcessing ya dejó configurado el FinalRTV, así que dibujamos encima.
    m_deviceResources->PIXBeginEvent(L"Render UI");
    RenderUI();
    m_deviceResources->PIXEndEvent();

    // 7. Presentar
    m_deviceResources->Present();
}

// Helper method to clear the back buffers.
void Game::Clear()
{
    m_deviceResources->PIXBeginEvent(L"Clear");

    // Clear the views.
    auto context = m_deviceResources->GetD3DDeviceContext();
    auto renderTarget = m_deviceResources->GetRenderTargetView();
    auto depthStencil = m_deviceResources->GetDepthStencilView();

    context->ClearRenderTargetView(renderTarget, Colors::CornflowerBlue);
    context->ClearDepthStencilView(depthStencil, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
    context->OMSetRenderTargets(1, &renderTarget, depthStencil);

    // Set the viewport.
    const auto viewport = m_deviceResources->GetScreenViewport();
    context->RSSetViewports(1, &viewport);

    m_deviceResources->PIXEndEvent();
}
#pragma endregion

#pragma region Message Handlers
// Message handlers
void Game::OnActivated()
{
    // TODO: Game is becoming active window.

    m_kbTracker.Reset();
    m_mouseTracker.Reset();

    // Opcional: Si quieres que al volver el mouse se comporte normal hasta que hagas click
    if (m_mouse)
    {
        m_mouse->SetMode(DirectX::Mouse::MODE_ABSOLUTE);
    }
}

void Game::OnDeactivated()
{
    // TODO: Game is becoming background window.
    if (m_mouse) m_mouse->SetMode(DirectX::Mouse::MODE_ABSOLUTE);
}

void Game::OnSuspending()
{
    // TODO: Game is being power-suspended (or minimized).
    if (m_mouse) m_mouse->SetMode(DirectX::Mouse::MODE_ABSOLUTE);
}

void Game::OnResuming()
{
    m_timer.ResetElapsedTime();

    // TODO: Game is being power-resumed (or returning from minimize).
}

void Game::OnWindowMoved()
{
    const auto r = m_deviceResources->GetOutputSize();
    m_deviceResources->WindowSizeChanged(r.right, r.bottom);
}

void Game::OnDisplayChange()
{
    m_deviceResources->UpdateColorSpace();
}

void Game::OnWindowSizeChanged(int width, int height)
{
    if (!m_deviceResources->WindowSizeChanged(width, height))
        return;

    CreateWindowSizeDependentResources();

    // TODO: Game window is being resized.
}

// Properties
void Game::GetDefaultSize(int& width, int& height) const noexcept
{
    // TODO: Change to desired default window size (note minimum size is 320x200).
    width = 800;
    height = 600;
}
#pragma endregion

#pragma region Direct3D Resources
// These are the resources that depend on the device.
void Game::CreateDeviceDependentResources()
{
    auto device = m_deviceResources->GetD3DDevice();
    auto context = m_deviceResources->GetD3DDeviceContext();
    HRESULT hr;

    D3D11_TEXTURE2D_DESC minimapTexDesc = {};
    minimapTexDesc.Width = MINIMAP_SIZE;
    minimapTexDesc.Height = MINIMAP_SIZE;
    minimapTexDesc.MipLevels = 1;
    minimapTexDesc.ArraySize = 1;
    minimapTexDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // Textura de color estndar
    minimapTexDesc.SampleDesc.Count = 1;
    minimapTexDesc.Usage = D3D11_USAGE_DEFAULT;
    // IMPORTANTE: Debe poder ser un Render Target Y un Shader Resource
    minimapTexDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    hr = device->CreateTexture2D(&minimapTexDesc, nullptr, m_minimapTexture.ReleaseAndGetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("Fallo al crear la textura del minimapa.");

    // Crear la Vista de Render Target (RTV)
    hr = device->CreateRenderTargetView(m_minimapTexture.Get(), nullptr, m_minimapRTV.ReleaseAndGetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("Fallo al crear el RTV del minimapa.");

    // Crear la Vista de Shader Resource (SRV)
    hr = device->CreateShaderResourceView(m_minimapTexture.Get(), nullptr, m_minimapSRV.ReleaseAndGetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("Fallo al crear el SRV del minimapa.");

    // Definir el viewport para el minimapa
    m_minimapViewport = { 0.0f, 0.0f, MINIMAP_SIZE, MINIMAP_SIZE, 0.0f, 1.0f };

    // Cargar el icono del jugador
    hr = CreateWICTextureFromFile(device, L"GameAssets\\textures\\player.png", nullptr, m_playerIconTexture.ReleaseAndGetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("Fallo al cargar el icono del jugador.");

    hr = DirectX::CreateWICTextureFromFile(device, L"GameAssets\\textures\\blank.png", nullptr, m_blankTexture.ReleaseAndGetAddressOf());
    if (FAILED(hr))
    {
        OutputDebugString(L"ADVERTENCIA: No se encontró blank.png\n");
    }

    hr = DirectX::CreateWICTextureFromFile(device, L"GameAssets\\textures\\axeicon.png", nullptr, m_uiAxeTexture.ReleaseAndGetAddressOf());
    if (FAILED(hr))
    {
        OutputDebugString(L"ADVERTENCIA: No se encontró axe.png\n");
    }

    hr = DirectX::CreateWICTextureFromFile(device, L"GameAssets\\textures\\sword.png", nullptr, m_uiSwordTexture.ReleaseAndGetAddressOf());
    if (FAILED(hr))
    {
        OutputDebugString(L"ADVERTENCIA: No se encontró sword.png\n");
    }

    hr = DirectX::CreateWICTextureFromFile(device, L"GameAssets\\textures\\branch.png", nullptr, m_uiBranchTexture.ReleaseAndGetAddressOf());
    if (FAILED(hr))
    {
        OutputDebugString(L"ADVERTENCIA: No se encontró branch.png\n");
    }

    hr = DirectX::CreateWICTextureFromFile(device, L"GameAssets\\textures\\roca.png", nullptr, m_uiRockTexture.ReleaseAndGetAddressOf());
    if (FAILED(hr))
    {
        OutputDebugString(L"ADVERTENCIA: No se encontró roca.png\n");
    }

    hr = DirectX::CreateWICTextureFromFile(device, L"GameAssets\\textures\\mystic.png", nullptr, m_uiMysticTexture.ReleaseAndGetAddressOf());
    if (FAILED(hr))
    {
        OutputDebugString(L"ADVERTENCIA: No se encontró mystic.png\n");
    }

    hr = DirectX::CreateWICTextureFromFile(device, L"GameAssets\\textures\\E.png", nullptr, m_InteractionTexture.ReleaseAndGetAddressOf());
    if (FAILED(hr))
    {
        OutputDebugString(L"ADVERTENCIA: No se encontró E.png\n");
    }

    hr = DirectX::CreateWICTextureFromFile(device, L"GameAssets\\textures\\anvil.png", nullptr, m_AnvilTexture.ReleaseAndGetAddressOf());
    if (FAILED(hr))
    {
        OutputDebugString(L"ADVERTENCIA: No se encontró anvil.png\n");
    }

    hr = DirectX::CreateWICTextureFromFile(device, L"GameAssets\\textures\\madera.png", nullptr, m_uiLogTexture.ReleaseAndGetAddressOf());
    if (FAILED(hr))
    {
        OutputDebugString(L"ADVERTENCIA: No se encontró madera.png\n");
    }

    hr = DirectX::CreateWICTextureFromFile(device, L"GameAssets\\textures\\mystic_cooked.png", nullptr, m_uiMysticCookedTexture.ReleaseAndGetAddressOf());
    if (FAILED(hr))
    {
        OutputDebugString(L"ADVERTENCIA: No se encontró mystic_cooked.png\n");
    }

    D3D11_TEXTURE2D_DESC depthTexDesc = {};
    depthTexDesc.Width = MINIMAP_SIZE;
    depthTexDesc.Height = MINIMAP_SIZE;
    depthTexDesc.MipLevels = 1;
    depthTexDesc.ArraySize = 1;
    depthTexDesc.Format = DXGI_FORMAT_D32_FLOAT; // Formato de profundidad estndar
    depthTexDesc.SampleDesc.Count = 1;
    depthTexDesc.Usage = D3D11_USAGE_DEFAULT;
    depthTexDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL; // Solo se usa para profundidad/stencil

    hr = device->CreateTexture2D(&depthTexDesc, nullptr, m_minimapDepthTexture.ReleaseAndGetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("Fallo al crear la textura de profundidad del minimapa.");

    // Crear la Vista de Profundidad/Stencil (DSV)
    hr = device->CreateDepthStencilView(m_minimapDepthTexture.Get(), nullptr, m_minimapDSV.ReleaseAndGetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("Fallo al crear el DSV del minimapa.");

    m_states = std::make_unique<DirectX::CommonStates>(device);
    m_samplerState = m_states->LinearWrap();

    D3D11_BUFFER_DESC cbd_lights = {};
    cbd_lights.Usage = D3D11_USAGE_DYNAMIC;
    cbd_lights.ByteWidth = sizeof(PSLightPropertiesData); 
    cbd_lights.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbd_lights.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    hr = device->CreateBuffer(&cbd_lights, nullptr, m_lightPropertiesCB.ReleaseAndGetAddressOf());
    if (FAILED(hr)) { 
        throw std::runtime_error("Failed to create light properties constant buffer.");
    }

    hr = device->CreateBuffer(&cbd_lights, nullptr, m_minimapLightPropertiesCB.ReleaseAndGetAddressOf());
    if (FAILED(hr)) {
        throw std::runtime_error("Fallo al crear el constant buffer de luces del minimapa.");
    }

    // Definimos una iluminacin brillante y uniforme para el minimapa
    // Luz ambiental muy alta para que todo sea visible
    m_minimapLightData.ambientLightColor = DirectX::SimpleMath::Vector4(0.8f, 0.8f, 0.8f, 1.0f);
    m_minimapLightData.directionalLightVector = DirectX::SimpleMath::Vector3(0.0f, -1.0f, 0.0f);
    m_minimapLightData.directionalLightColor = DirectX::SimpleMath::Vector4(0.5f, 0.5f, 0.5f, 1.0f);
    m_minimapLightData.cameraPositionWorld = DirectX::SimpleMath::Vector3::Zero;


    m_cube = DirectX::GeometricPrimitive::CreateCube(context, 1.0f);
    m_cubeWorldMatrix = DirectX::SimpleMath::Matrix::Identity;

    m_debugBoxDrawer = DirectX::GeometricPrimitive::CreateBox(context, DirectX::SimpleMath::Vector3(1.f, 1.f, 1.f));
    m_debugSphereDrawer = DirectX::GeometricPrimitive::CreateSphere(context, 0.5f, 16);

    // TODO: Initialize device dependent objects here (independent of window size).

    RECT outputSize = m_deviceResources->GetOutputSize();
    int width = outputSize.right - outputSize.left;
    int height = outputSize.bottom - outputSize.top;

    m_camera = std::make_unique<Camera>(width, height);
    //Player spawn point
    m_playerPos = DirectX::SimpleMath::Vector3(11.2f, 0.0f, -72.0f);

    float distanciaAtras = 8.0f;
    float alturaArriba = 4.0f;
    DirectX::SimpleMath::Vector3 offsetInicial(0.0f, alturaArriba, -distanciaAtras);

    m_playerPos = DirectX::SimpleMath::Vector3(11.2f, 0.0f, -72.0f);
    /*m_camera->SetPosition(m_playerPos + DirectX::SimpleMath::Vector3(0.0f, 5.0f, -10.0f));*/

    /*m_camera->SetRotation(0.0f, 0.0f);*/
    m_camera->UpdateViewMatrix(); // Asegura que la matriz de vista se calcule inicialmente
    

    m_spriteBatch3D = std::make_unique<DirectX::SpriteBatch>(context);
    m_spriteBatchUI = std::make_unique<DirectX::SpriteBatch>(context);
    m_font = std::make_unique<DirectX::SpriteFont>(device, L"GameAssets\\Fonts\\GameFont.spritefont");
    if (m_font)
    {
        m_font->SetDefaultCharacter('?');
    }

    m_spriteEffect = std::make_unique<BasicEffect>(device);
    m_spriteEffect->SetTextureEnabled(true);
    m_spriteEffect->SetVertexColorEnabled(true);

    void const* shaderBytecode;
    size_t byteCodeLength;

    m_spriteEffect->GetVertexShaderBytecode(&shaderBytecode, &byteCodeLength);

    hr = device->CreateInputLayout(
        DirectX::VertexPositionColorTexture::InputElements,
        DirectX::VertexPositionColorTexture::InputElementCount,
        shaderBytecode,
        byteCodeLength,
        m_spriteInputLayout.ReleaseAndGetAddressOf());

    if (FAILED(hr))
    {
        throw std::runtime_error("Failed to create sprite input layout.");
    }

    hr = DirectX::CreateWICTextureFromFile(device, L"GameAssets\\textures\\dome2.png", nullptr,
        m_skyTextureSRV.ReleaseAndGetAddressOf());
    if (FAILED(hr))
    {
        // Manejar error de carga de textura (ej. lanzar una excepción o mostrar un mensaje)
        throw std::runtime_error("Failed to load sky texture.");
    }

    // 2. Crear la esfera para el SkyDome
    // Radio grande, pocos segmentos son suficientes, false para coordenadas LH, true para invertir normales (ver el interior)
    m_skySphere = DirectX::GeometricPrimitive::CreateSphere(context, 1.0f, 16, false, true);

    // 3. Crear y configurar BasicEffect para el cielo
    m_skyEffect = std::make_unique<BasicEffect>(device);
    m_skyEffect->SetTextureEnabled(true);
    m_skyEffect->SetTexture(m_skyTextureSRV.Get());
    m_skyEffect->SetLightingEnabled(false); 

    // 4. Crear el Input Layout para el BasicEffect
    m_skyEffect->GetVertexShaderBytecode(&shaderBytecode, &byteCodeLength); 

    hr = device->CreateInputLayout(
        DirectX::VertexPositionNormalTexture::InputElements, 
        DirectX::VertexPositionNormalTexture::InputElementCount,
        shaderBytecode,
        byteCodeLength,
        m_skyInputLayout.ReleaseAndGetAddressOf());
    if (FAILED(hr))
    {
        throw std::runtime_error("Failed to create sky input layout.");
    }

    // 5. Crear el estado de profundidad para el SkyDome
    // Se dibujará el cielo, pero no escribirá en el buffer de profundidad,
    // y la prueba de profundidad será LESS_EQUAL. Esto se usa cuando se dibuja el cielo al final.
    D3D11_DEPTH_STENCIL_DESC skyDepthDesc = {};
    skyDepthDesc.DepthEnable = TRUE;
    skyDepthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO; // No escribe en el Z-Buffer
    skyDepthDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;    // Pasa si es menor o igual que lo que hay

    hr = device->CreateDepthStencilState(&skyDepthDesc, m_skyDepthState.ReleaseAndGetAddressOf());
    if (FAILED(hr))
    {
        throw std::runtime_error("Failed to create sky depth stencil state.");
    }
    
    m_terrain = std::make_unique<Terrain>();
    // ASEGÚRATE DE QUE ESTOS NOMBRES DE ARCHIVO Y RUTAS SEAN CORRECTOS:
    // 1. Que los archivos existan en "GameAssets/Textures/" en tu proyecto.
    // 2. Que su propiedad "Copiar en el directorio de salida" esté en "Copiar si es posterior".
    if (!m_terrain->Initialize(device, context,
        L"GameAssets\\Textures\\heightmap1.png",
        L"GameAssets\\Textures\\dirt.jpg",                       
        L"GameAssets\\Textures\\terrain\\tilable-IMG_0044-grey.png",  
        L"GameAssets\\Textures\\terrain\\tilable-IMG_0044-grey1.png",               
        L"C:TerrainVS.cso",
        L"C:TerrainPS.cso"))
    {
        OutputDebugString(L"ERROR: Falló la inicialización del terreno desde Game.cpp\n");
        throw std::runtime_error("Failed to initialize terrain.");
    }

    if (m_terrain) // Asegurarse de que el terreno se inicializó
    {
        // Obtener las dimensiones del terreno (en número de vértices/píxeles del heightmap)
        // Si no añadiste los getters, tendrías que saber estas dimensiones de otra forma.
        float terrainGridActualWidth = static_cast<float>(m_terrain->GetTerrainWidth() - 1); // El ancho real es N-1 unidades si hay N vértices
        float terrainGridActualDepth = static_cast<float>(m_terrain->GetTerrainHeight() - 1); // La profundidad real

        // Calcular el desplazamiento para centrar el terreno en X y Z
        float offsetX = terrainGridActualWidth / 2.0f;
        float offsetZ = terrainGridActualDepth / 2.0f;

        float desiredBaseY = -20.0f; // Ejemplo: El "nivel 0" del terreno estará en Y=-10 del mundo.
        // AJUSTA ESTE VALOR SEGÚN NECESITES.

        float deseadoAnchoDelTerrenoEnElMundo = terrainGridActualWidth * 10.0f; // Ejemplo: Hacerlo el doble de ancho
        float deseadoProfundidadDelTerrenoEnElMundo = terrainGridActualDepth * 10.0f; // Ejemplo: Hacerlo el doble de profundo
        float escalaYAdicionalParaElMundo = 1.0f;

        DirectX::SimpleMath::Matrix terrainScaleMatrix = DirectX::SimpleMath::Matrix::CreateScale(
            deseadoAnchoDelTerrenoEnElMundo / terrainGridActualWidth, // Escala X efectiva
            escalaYAdicionalParaElMundo,                             // Escala Y efectiva
            deseadoProfundidadDelTerrenoEnElMundo / terrainGridActualDepth  // Escala Z efectiva
        );

        // Esta traslación centra el *punto medio* del grid del heightmap (en sus coordenadas originales 0 a W-1, 0 a H-1)
        // en el origen (0,0,0) del espacio al que se aplica el escalado.
        DirectX::SimpleMath::Matrix centeringTranslation = DirectX::SimpleMath::Matrix::CreateTranslation(
            -terrainGridActualWidth / 2.0f,
            0.0f,
            -terrainGridActualDepth / 2.0f
        );

        // Esta traslación mueve el terreno verticalmente a su posición base deseada.
        DirectX::SimpleMath::Matrix verticalWorldTranslation = DirectX::SimpleMath::Matrix::CreateTranslation(
            0.0f,
            desiredBaseY,
            0.0f
        );

        // Orden de transformación:
        // 1. Los vértices del terreno están originalmente en un grid (i, altura_local, j).
        // 2. `centeringTranslation` mueve el centro de este grid al origen.
        // 3. `terrainScaleMatrix` escala este grid centrado.
        // 4. `verticalWorldTranslation` mueve el terreno escalado y centrado a su posición Y final en el mundo.
        DirectX::SimpleMath::Matrix terrainWorld = centeringTranslation * terrainScaleMatrix * verticalWorldTranslation;


        m_terrain->SetWorldMatrix(terrainWorld);
    }
    
    // 3D Models

    m_blacksmith = std::make_unique<Model>();
    if (!m_blacksmith->Load(device, context,
        "GameAssets/models/blacksmith/Blacksmith2.obj"))
    {
        throw std::runtime_error("Failed to load m_blacksmith!");
    }

    if (m_blacksmith) // Asegúrate de que el modelo principal se cargó
    {
        // Asegúrate de que DebugVS.cso y DebugPS.cso estén en tu directorio de salida
        if (!m_blacksmith->LoadEvolvingShaders(device, L"C:EvolvingVS.cso", L"C:EvolvingPS.cso"))
        {
            throw std::runtime_error("Failed to load debug shaders for m_blacksmith!");
        }
    }

    // Ajusta escala, rotación, posición para m_miPrimerModelo como lo tenías
    m_blacksmith->SetScale(2000.0f); // Ejemplo
    m_blacksmith->SetRotationEuler(0.0f, 0.0, 0.0f); // Ejemplo


    m_dungeonInterior = std::make_unique<Model>();
    if (!m_dungeonInterior->Load(device, context, "GameAssets/models/dungeon/dungeon.obj"))
    {
        OutputDebugString(L"ADVERTENCIA: No se encontró el modelo dungeon.obj\n");
    }

    if (m_dungeonInterior)
    {
        m_dungeonInterior->LoadEvolvingShaders(device, L"C:EvolvingVS.cso", L"C:EvolvingPS.cso");

        m_dungeonInterior->SetScale(30.0f); 
        m_dungeonInterior->SetRotationEuler(0.0f, 0.0f, 0.0f);
    }

    m_dungeonGate = std::make_unique<Model>();
    if (!m_dungeonGate->Load(device, context, "GameAssets/models/temple/Temple.obj"))
    {
        OutputDebugString(L"ADVERTENCIA: No se encontró el modelo temple.obj\n");
    }

    if (m_dungeonGate)
    {
        m_dungeonGate->LoadEvolvingShaders(device, L"C:EvolvingVS.cso", L"C:EvolvingPS.cso");

        m_dungeonGate->SetScale(30.0f);
        m_dungeonGate->SetRotationEuler(0.0f, 0.0f, 0.0f);
    }
    
    m_green_tree1 = std::make_unique<Model>();
    if (!m_green_tree1->Load(device, context,
        "GameAssets/models/green_tree/green_tree.obj"))
    {
        throw std::runtime_error("Failed to load m_green_tree1!");
    }

    if (m_green_tree1)
    {
        if (!m_green_tree1->LoadEvolvingShaders(device, L"C:EvolvingVS.cso", L"C:EvolvingPS.cso"))
        {
            throw std::runtime_error("Failed to load debug shaders for m_green_tree1!");
        }
    }

    m_green_tree1->SetScale(7.0f);
    m_green_tree1->SetRotationEuler(0.0f, DirectX::XM_PI, 0.0f); 

   
    m_forest_pine1 = std::make_unique<Model>();
    if (!m_forest_pine1->Load(device, context,
        "GameAssets/models/trees/pine1.obj"))
    {
        throw std::runtime_error("Failed to load m_forest_pine1!");
    }

    if (m_forest_pine1)
    {
        if (!m_forest_pine1->LoadEvolvingShaders(device, L"C:EvolvingVS.cso", L"C:EvolvingPS.cso"))
        {
            throw std::runtime_error("Failed to load debug shaders for m_forest_pine1!");
        }
    }

    m_forest_pine1->SetScale(5.0f);
    m_forest_pine1->SetRotationEuler(0.0f, DirectX::XM_PI, 0.0f);


    m_forest_pine2 = std::make_unique<Model>();
    if (!m_forest_pine2->Load(device, context,
        "GameAssets/models/trees/pine2.obj"))
    {
        throw std::runtime_error("Failed to load m_forest_pine2!");
    }

    if (m_forest_pine2)
    {
        if (!m_forest_pine2->LoadEvolvingShaders(device, L"C:EvolvingVS.cso", L"C:EvolvingPS.cso"))
        {
            throw std::runtime_error("Failed to load debug shaders for m_forest_pine2!");
        }
    }

    m_forest_pine2->SetScale(3.0f);
    m_forest_pine2->SetRotationEuler(0.0f, DirectX::XM_PI, 0.0f);


    m_forest_pine3 = std::make_unique<Model>();
    if (!m_forest_pine3->Load(device, context,
        "GameAssets/models/trees/pine3.obj"))
    {
        throw std::runtime_error("Failed to load m_forest_pine3!");
    }

    if (m_forest_pine3)
    {
        if (!m_forest_pine3->LoadEvolvingShaders(device, L"C:EvolvingVS.cso", L"C:EvolvingPS.cso"))
        {
            throw std::runtime_error("Failed to load debug shaders for m_forest_pine3!");
        }
    }

    m_forest_pine3->SetScale(3.0f);
    m_forest_pine3->SetRotationEuler(0.0f, DirectX::XM_PI, 0.0f);




    m_cart = std::make_unique<Model>();
    if (!m_cart->Load(device, context,
        "GameAssets/models/cart/Cart.obj"))
    {
        throw std::runtime_error("Failed to load m_cart!");
    }

    if (m_cart)
    {
        if (!m_cart->LoadEvolvingShaders(device, L"C:EvolvingVS.cso", L"C:EvolvingPS.cso"))
        {
            throw std::runtime_error("Failed to load debug shaders for m_cart!");
        }
    }

    m_cart->SetScale(0.1f);
    m_cart->SetRotationEuler(0.0f, DirectX::XM_PIDIV2, 0.0f);

    m_windmill = std::make_unique<Model>();
    if (!m_windmill->Load(device, context,
        "GameAssets/models/windmill/windmill.obj"))
    {
        throw std::runtime_error("Failed to load m_windmill!");
    }

    if (m_windmill)
    {
        if (!m_windmill->LoadEvolvingShaders(device, L"C:EvolvingVS.cso", L"C:EvolvingPS.cso"))
        {
            throw std::runtime_error("Failed to load debug shaders for m_windmill!");
        }
    }

    m_windmill->SetScale(1.0f);
    m_windmill->SetRotationEuler(0.0f, 0.0f, 0.0f);

    m_rock1 = std::make_unique<Model>();
    if (!m_rock1->Load(device, context,
        "GameAssets/models/rocks/rock1.obj"))
    {
        throw std::runtime_error("Failed to load m_rock1!");
    }

    if (m_rock1)
    {
        if (!m_rock1->LoadEvolvingShaders(device, L"C:EvolvingVS.cso", L"C:EvolvingPS.cso"))
        {
            throw std::runtime_error("Failed to load debug shaders for m_rock1!");
        }
    }

    m_rock1->SetScale(3.0f);
    m_rock1->SetRotationEuler(0.0f, 0.0f, 0.0f);


    m_axe = std::make_unique<Model>();
    if (!m_axe->Load(device, context,
        "GameAssets/models/axe/Axe.obj"))
    {
        throw std::runtime_error("Failed to load m_axe!");
    }

    if (m_axe)
    {
        if (!m_axe->LoadEvolvingShaders(device, L"C:EvolvingVS.cso", L"C:EvolvingPS.cso"))
        {
            throw std::runtime_error("Failed to load debug shaders for m_axe!");
        }
    }

    m_axe->SetScale(3.0f);
    m_axe->SetRotationEuler(0.0f, 0.0f, 0.0f);

    m_sword = std::make_unique<Model>();
    if (!m_sword->Load(device, context,
        "GameAssets/models/sword/Sword.obj"))
    {
        throw std::runtime_error("Failed to load m_sword!");
    }

    if (m_sword)
    {
        if (!m_sword->LoadEvolvingShaders(device, L"C:EvolvingVS.cso", L"C:EvolvingPS.cso"))
        {
            throw std::runtime_error("Failed to load debug shaders for m_sword!");
        }
    }

    m_axe->SetScale(3.0f);
    m_axe->SetRotationEuler(0.0f, 0.0f, 0.0f);

    m_rock2 = std::make_unique<Model>();
    if (!m_rock2->Load(device, context,
        "GameAssets/models/rocks/rock2.obj"))
    {
        throw std::runtime_error("Failed to load m_rock2!");
    }

    if (m_rock2)
    {
        if (!m_rock2->LoadEvolvingShaders(device, L"C:EvolvingVS.cso", L"C:EvolvingPS.cso"))
        {
            throw std::runtime_error("Failed to load debug shaders for m_rock2!");
        }
    }

    m_rock2->SetScale(1.0f);
    m_rock2->SetRotationEuler(0.0f, 0.0f, 0.0f);

    m_rock3 = std::make_unique<Model>();
    if (!m_rock3->Load(device, context,
        "GameAssets/models/rocks/rock3.obj"))
    {
        throw std::runtime_error("Failed to load m_rock3!");
    }

    if (m_rock3)
    {
        if (!m_rock3->LoadEvolvingShaders(device, L"C:EvolvingVS.cso", L"C:EvolvingPS.cso"))
        {
            throw std::runtime_error("Failed to load debug shaders for m_rock3!");
        }
    }

    m_rock3->SetScale(1.0f);
    m_rock3->SetRotationEuler(0.0f, 0.0f, 0.0f);

    m_rock4 = std::make_unique<Model>();
    if (!m_rock4->Load(device, context,
        "GameAssets/models/rocks/rock4.obj"))
    {
        throw std::runtime_error("Failed to load m_rock4!");
    }

    if (m_rock4)
    {
        if (!m_rock1->LoadEvolvingShaders(device, L"C:EvolvingVS.cso", L"C:EvolvingPS.cso"))
        {
            throw std::runtime_error("Failed to load debug shaders for m_rock4!");
        }
    }

    m_rock4->SetScale(1.0f);
    m_rock4->SetRotationEuler(0.0f, 0.0f, 0.0f);

    m_rock5 = std::make_unique<Model>();
    if (!m_rock5->Load(device, context,
        "GameAssets/models/rocks/rock5.obj"))
    {
        throw std::runtime_error("Failed to load m_rock5!");
    }

    if (m_rock5)
    {
        if (!m_rock5->LoadEvolvingShaders(device, L"C:EvolvingVS.cso", L"C:EvolvingPS.cso"))
        {
            throw std::runtime_error("Failed to load debug shaders for m_rock5!");
        }
    }

    m_rock5->SetScale(1.0f);
    m_rock5->SetRotationEuler(0.0f, 0.0f, 0.0f);

    m_rock6 = std::make_unique<Model>();
    if (!m_rock6->Load(device, context,
        "GameAssets/models/rocks/rock6.obj"))
    {
        throw std::runtime_error("Failed to load m_rock6!");
    }

    if (m_rock6)
    {
        if (!m_rock6->LoadEvolvingShaders(device, L"C:EvolvingVS.cso", L"C:EvolvingPS.cso"))
        {
            throw std::runtime_error("Failed to load debug shaders for m_rock6!");
        }
    }

    m_rock6->SetScale(1.0f);
    m_rock6->SetRotationEuler(0.0f, 0.0f, 0.0f);

    m_crystal = std::make_unique<Model>();
    if (!m_crystal->Load(device, context,
        "GameAssets/models/mystic/crystal.obj"))
    {
        throw std::runtime_error("Failed to load m_crystal!");
    }

    if (m_crystal)
    {
        if (!m_crystal->LoadEvolvingShaders(device, L"C:EvolvingVS.cso", L"C:EvolvingPS.cso"))
        {
            throw std::runtime_error("Failed to load debug shaders for m_crystal!");
        }
    }

    m_crystal->SetScale(5.0f);
    m_crystal->SetRotationEuler(0.0f, 0.0f, 0.0f);

    // Modelo de casa 1
    m_house1 = std::make_unique<Model>();
    if (!m_house1->Load(device, context,
        "GameAssets/models/houses/CASA01.obj"))
    {
        throw std::runtime_error("Failed to load m_house1!");
    }

    if (m_house1)
    {
        if (!m_house1->LoadEvolvingShaders(device, L"C:EvolvingVS.cso", L"C:EvolvingPS.cso"))
        {
            throw std::runtime_error("Failed to load debug shaders for m_house1!");
        }
    }

    m_house1->SetScale(5.0f);
    m_house1->SetRotationEuler(0.0f, DirectX::XM_PI, 0.0f);

    //Modelo de casa 2

    m_house2 = std::make_unique<Model>();
    if (!m_house2->Load(device, context,
        "GameAssets/models/houses/CASA02.obj"))
    {
        throw std::runtime_error("Failed to load m_house2!");
    }

    if (m_house2)
    {
        if (!m_house2->LoadEvolvingShaders(device, L"C:EvolvingVS.cso", L"C:EvolvingPS.cso"))
        {
            throw std::runtime_error("Failed to load debug shaders for m_house2!");
        }
    }

    m_house2->SetScale(5.0f);
    m_house2->SetRotationEuler(0.0f, DirectX::XM_PI, 0.0f);

    //Modelo de casa 3
	m_house3 = std::make_unique<Model>();
    if (!m_house3->Load(device, context,
        "GameAssets/models/houses/CASA03.obj"))
    {
        throw std::runtime_error("Failed to load m_house3!");
    }

    if (m_house3)
    {
        if (!m_house3->LoadEvolvingShaders(device, L"C:EvolvingVS.cso", L"C:EvolvingPS.cso"))
        {
            throw std::runtime_error("Failed to load debug shaders for m_house3!");
        }
    }

    m_house3->SetScale(5.0f);
    m_house3->SetRotationEuler(0.0f, -DirectX::XM_PIDIV2, 0.0f);

    //Modelo de casa 4
    m_house4 = std::make_unique<Model>();
    if (!m_house4->Load(device, context,
        "GameAssets/models/houses/CASA04.obj"))
    {
        throw std::runtime_error("Failed to load m_house4!");
    }

    if (m_house4)
    {
        if (!m_house4->LoadEvolvingShaders(device, L"C:EvolvingVS.cso", L"C:EvolvingPS.cso"))
        {
            throw std::runtime_error("Failed to load debug shaders for m_house4!");
        }
    }

    m_house4->SetScale(5.0f);
    m_house4->SetRotationEuler(0.0f, -DirectX::XM_PIDIV2, 0.0f);
    
    m_knight = std::make_unique<Model>();
    if (!m_knight->Load(device, context,
        "GameAssets/models/knight/knight.obj"))
    {
        throw std::runtime_error("Failed to load m_knight!");
    }
    if (m_knight)
    {
        if (!m_knight->LoadEvolvingShaders(device, L"C:EvolvingVS.cso", L"C:EvolvingPS.cso"))
        {
            throw std::runtime_error("Failed to load debug shaders for m_knight!");
        }
    }
    m_knight->SetScale(2.0f);
    m_knight->SetRotationEuler(0.0f, DirectX::XM_PI, 0.0f);

    m_branch = std::make_unique<Model>();
    if (!m_branch->Load(device, context, "GameAssets/models/branch/branch.obj"))
    {
        OutputDebugString(L"ADVERTENCIA: No se encontró el modelo de la rama.\n");
    }

    if (m_branch)
    {
        m_branch->LoadEvolvingShaders(device,
            L"C:EvolvingVS.cso",
            L"C:EvolvingPS.cso");

        m_branch->SetScale(1.0f); 
    }

    m_log = std::make_unique<Model>();
    if (!m_branch->Load(device, context, "GameAssets/models/log/log.obj"))
    {
        OutputDebugString(L"ADVERTENCIA: No se encontró el modelo del tronco.\n");
    }

    if (m_log)
    {
        m_log->LoadEvolvingShaders(device,
            L"C:EvolvingVS.cso",
            L"C:EvolvingPS.cso");

        m_log->SetScale(1.0f);
    }

    LoadAnimationSequence(m_animIdle, "GameAssets/models/knight/Idle/IdleF", 60, device, context);

    LoadAnimationSequence(m_animWalk, "GameAssets/models/knight/Walking/Walking_F", 32, device, context);

    LoadAnimationSequence(m_animMelee, "GameAssets/models/knight/Melee/MeleeF", 70, device, context);

    if (!m_animIdle.empty()) {
        m_currentModel = m_animIdle[0].get();
    }
    else if (m_knight) {
        m_currentModel = m_knight.get(); 
    }


    

    hr = CreateWICTextureFromFile(device, L"GameAssets\\textures\\firefly.png", nullptr, m_fireflyTexture.ReleaseAndGetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("Fallo al cargar la textura de la luciernaga.");

    hr = CreateWICTextureFromFile(device, L"GameAssets\\textures\\smoke1.png", nullptr, m_smokeTexture.ReleaseAndGetAddressOf());
    if (FAILED(hr)) CreateWICTextureFromFile(device, L"GameAssets\\textures\\smoke.png", nullptr, m_smokeTexture.ReleaseAndGetAddressOf());

    m_chimneyPos = DirectX::SimpleMath::Vector3(-101.3f, 20.9f, 570.2f);


    InitializeFireflies();
    InitializeSmoke();

    m_worldInstances.clear();

    const float offsetY_pine1 = -7.0f;
    const float offsetY_pine2 = -1.0f;
    const float offsetY_pine3 = -1.2f;
    const float offsetY_green_tree1 = 0.0f;
    const float offsetY_rock = -0.5f; 

    DirectX::SimpleMath::Matrix baseTransform;

    // =================================================================================
    // GENERACIÓN PROCEDURAL DE BOSQUE (CALIBRADO)
    // =================================================================================

    // 1. ZONAS PROHIBIDAS
    struct ExclusionZone { float x, z, radius; };
    std::vector<ExclusionZone> forbiddenZones;

    forbiddenZones.push_back({ 0.0f, 0.0f, 20.0f });       // Spawn (Más espacio)
    forbiddenZones.push_back({ 20.0f, 20.0f, 35.0f });     // Herrería
    forbiddenZones.push_back({ -40.0f, -40.0f, 35.0f });   // Catacumbas
    // Agrega aquí coordenadas de otras casas para que no salgan árboles dentro

    // 2. PALETA DE ÁRBOLES CON ESCALA INDIVIDUAL
    struct TreeOption {
        Model* model;
        float offsetY;
        float baseScale; // <--- NUEVO: Para controlar el tamaño de cada tipo
    };

    std::vector<TreeOption> treePalette;

    // --- CALIBRACIÓN DE TAMAÑOS ---
    // Ajusta estos números (baseScale) si ves que uno sigue siendo gigante
    if (m_forest_pine1) treePalette.push_back({ m_forest_pine1.get(), -7.0f, 5.0f }); // Pinos delgados
    if (m_forest_pine2) treePalette.push_back({ m_forest_pine2.get(), -1.0f, 2.0f }); // Pinos medianos
    if (m_forest_pine3) treePalette.push_back({ m_forest_pine3.get(), -1.2f, 2.0f }); // Pinos chicos

    // El Green Tree suele ser gigante, probemos con una escala más chica (0.5 o 1.0)
    if (m_green_tree1)  treePalette.push_back({ m_green_tree1.get(), 0.0f, 6.0f });

    // 3. GENERAR EL BOSQUE
    int cantidadArboles = 50;   // Bajamos un poco la cantidad para que no saturen
    float mapRange = 800.0f;     // Aumentamos el rango (de 500 a 800) para esparcirlos más

    if (!treePalette.empty() && m_terrain)
    {
        for (int i = 0; i < cantidadArboles; i++)
        {
            float x, z;
            bool positionIsValid = false;
            int attempts = 0;

            while (!positionIsValid && attempts < 20)
            {
                // Generación más dispersa
                float rx = (rand() % 1000) / 1000.0f;
                float rz = (rand() % 1000) / 1000.0f;

                x = (rx * mapRange) - (mapRange / 2.0f);
                z = (rz * mapRange) - (mapRange / 2.0f);

                // Colisiones
                bool collisionFound = false;
                for (const auto& zone : forbiddenZones)
                {
                    float dx = x - zone.x;
                    float dz = z - zone.z;
                    if ((dx * dx + dz * dz) < (zone.radius * zone.radius)) {
                        collisionFound = true;
                        break;
                    }
                }

                if (!collisionFound) positionIsValid = true;
                attempts++;
            }

            if (positionIsValid)
            {
                int treeIndex = rand() % treePalette.size();
                TreeOption selectedTree = treePalette[treeIndex];

                // Variación aleatoria ligera (±20% del tamaño base)
                float randomVar = 0.8f + ((rand() % 100) / 100.0f) * 0.4f;
                float finalScale = selectedTree.baseScale * randomVar;

                float rotationY = ((rand() % 360) / 360.0f) * DirectX::XM_2PI;

                // Construimos la matriz DESDE CERO para ignorar escalas previas raras
                DirectX::SimpleMath::Matrix instanceMatrix =
                    DirectX::SimpleMath::Matrix::CreateScale(finalScale) *
                    DirectX::SimpleMath::Matrix::CreateRotationY(rotationY);

                AddInstancedObject(
                    selectedTree.model,
                    instanceMatrix,
                    x, z,
                    -5.0f,
                    selectedTree.offsetY
                );
            }
        }
    }

    if (m_blacksmith && m_terrain) { 
        baseTransform = m_blacksmith->GetWorldMatrix();
        AddInstancedObject(m_blacksmith.get(), baseTransform, -70.7f, 563.0f, 43.5f, 0.0f, false);
    }

    //if (m_dungeonInterior && m_terrain)
    //{
    //    DirectX::SimpleMath::Matrix baseTransform = m_dungeonInterior->GetWorldMatrix();

    //    AddInstancedObject(
    //        m_dungeonInterior.get(),
    //        baseTransform,
    //        600.0f, 1000.0f, // X, Z lejanos
    //        2.0f,             // Y (Altura base)
    //        2.0f,             // Offset Y
    //        true              // Auto-Collision (Intenta true primero)
    //    );
    //}

    if (m_dungeonInterior)
    {
        // 1. Creamos la matriz con los datos de tu Editor
        DirectX::SimpleMath::Matrix dungeonTransform =
            DirectX::SimpleMath::Matrix::CreateScale(60.50f) *
            DirectX::SimpleMath::Matrix::CreateRotationY(-0.05f); // El ángulo pequeño

        // 2. La agregamos al mundo
        // Nota: Pasamos la Y (-17.47f) como altura base
        AddInstancedObject(
            m_dungeonInterior.get(),
            dungeonTransform,
            75.34f, 3727.67f, // X, Z
            -17.47f,          // Y (Altura exacta del editor)
            0.0f,             // Offset
            true              // AutoCollision
        );
    }


    //if (m_dungeonGate && m_terrain)
    //{
    //    DirectX::SimpleMath::Matrix baseTransform = m_dungeonInterior->GetWorldMatrix();

    //    AddInstancedObject(
    //        m_dungeonGate.get(),
    //        baseTransform,
    //        3.5f, -435.0f, // X, Z lejanos
    //        -12.0f,            // Y (Altura base)
    //        2.0f,             // Offset Y
    //        true              // Auto-Collision (Intenta true primero)
    //    );
    //}


    if (m_dungeonGate) // O el puntero que sea tu "Templo"
    {
        DirectX::SimpleMath::Matrix dungeonGateTransform =
            DirectX::SimpleMath::Matrix::CreateScale(16.72f) *
            DirectX::SimpleMath::Matrix::CreateRotationY(0.13f);

        AddInstancedObject(
            m_dungeonGate.get(),
            dungeonGateTransform,
            188.50f, -732.66f, // X, Z
            -18.18f,           // Y
            0.0f,
            true
        );
    }

    if (m_windmill) // <--- Pon aquí el puntero correcto
    {
        DirectX::SimpleMath::Matrix windmillTransform =
            DirectX::SimpleMath::Matrix::CreateScale(1.70f) *
            DirectX::SimpleMath::Matrix::CreateRotationY(2.81f);

        AddInstancedObject(
            m_house2.get(),
            windmillTransform,
            439.50f, 533.70f,  // X, Z
            -15.31f,           // Y
            0.0f,         // Pon el ID correcto
            true
        );
    }





    if (m_house1 && m_terrain) { 
        baseTransform = m_house1->GetWorldMatrix();
        AddInstancedObject(m_house1.get(), baseTransform, 188.0f, 100.0f,-9.0f, 0.0f);
    }

    if (m_house2 && m_terrain) {
        baseTransform = m_house2->GetWorldMatrix();
        AddInstancedObject(m_house2.get(), baseTransform, -97.2f, 161.0f, -9.0f, 0.0f);
    }

    if (m_house3 && m_terrain) {
        baseTransform = m_house3->GetWorldMatrix();
        AddInstancedObject(m_house3.get(), baseTransform, -88.2f, -209.0f, -9.0f, 0.0f);
    }

    if (m_house4 && m_terrain) {
        baseTransform = m_house4->GetWorldMatrix();
        AddInstancedObject(m_house4.get(), baseTransform, 243.79f, -32.0f, -9.0f, 0.0f);
    }

    if (m_cart && m_terrain) {
        baseTransform = m_cart->GetWorldMatrix();
        AddInstancedObject(m_cart.get(), baseTransform, 154.8f, -137.55f, -4.7f, 0.0f);
    }

    if (m_windmill && m_terrain) {
        baseTransform = m_windmill->GetWorldMatrix();
        AddInstancedObject(m_windmill.get(), baseTransform, 615.5f, 439.7f, 1.0f, 0.0f); // offsetY=0 ejemplo
    }

    // Rocas:
    // Para m_rock1 (y las demás rocas si las vas a instanciar o colocar individualmente)
    if (m_rock1 && m_terrain) {
        baseTransform = m_rock1->GetWorldMatrix();
        AddInstancedObject(m_rock1.get(), baseTransform, -79.10f, -118.56f, 5.0f, offsetY_rock);
    }

	BuildCustomColliders();

    DirectX::VertexPositionTexture quadVertices[] =
    {
        { DirectX::SimpleMath::Vector3(-0.5f,  0.5f, 0.f), DirectX::SimpleMath::Vector2(0, 0) }, // Top-Left
        { DirectX::SimpleMath::Vector3(0.5f,  0.5f, 0.f), DirectX::SimpleMath::Vector2(1, 0) }, // Top-Right
        { DirectX::SimpleMath::Vector3(0.5f, -0.5f, 0.f), DirectX::SimpleMath::Vector2(1, 1) }, // Bottom-Right
        { DirectX::SimpleMath::Vector3(-0.5f, -0.5f, 0.f), DirectX::SimpleMath::Vector2(0, 1) }, // Bottom-Left
    };

    D3D11_BUFFER_DESC vbd = {};
    vbd.Usage = D3D11_USAGE_DEFAULT;
    vbd.ByteWidth = sizeof(DirectX::VertexPositionTexture) * 4;
    vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    D3D11_SUBRESOURCE_DATA vinitData = { quadVertices, 0, 0 };
    hr = device->CreateBuffer(&vbd, &vinitData, m_fireflyVertexBuffer.ReleaseAndGetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("Fallo al crear el vertex buffer de las lucirnagas.");

    unsigned short quadIndices[] = { 0, 1, 2, 0, 2, 3 };

    D3D11_BUFFER_DESC ibd = {};
    ibd.Usage = D3D11_USAGE_DEFAULT;
    ibd.ByteWidth = sizeof(unsigned short) * 6;
    ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    D3D11_SUBRESOURCE_DATA iinitData = { quadIndices, 0, 0 };
    hr = device->CreateBuffer(&ibd, &iinitData, m_fireflyIndexBuffer.ReleaseAndGetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("Fallo al crear el index buffer de las lucirnagas.");

    // 2. Cargar y crear los shaders de las lucirnagas
    Microsoft::WRL::ComPtr<ID3DBlob> vsBlob, psBlob;
    hr = D3DReadFileToBlob(L"C:FireflyVS.cso", vsBlob.GetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("Fallo al cargar FireflyVS.cso.");
    hr = device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, m_fireflyVS.ReleaseAndGetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("Fallo al crear el Firefly Vertex Shader.");

    hr = D3DReadFileToBlob(L"C:FireflyPS.cso", psBlob.GetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("Fallo al cargar FireflyPS.cso.");
    hr = device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, m_fireflyPS.ReleaseAndGetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("Fallo al crear el Firefly Pixel Shader.");

    // 3. Crear el Input Layout para los vrtices del quad
    const D3D11_INPUT_ELEMENT_DESC fireflyLayoutDesc[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    };

    hr = device->CreateInputLayout(
        fireflyLayoutDesc,
        ARRAYSIZE(fireflyLayoutDesc),
        vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
        m_fireflyInputLayout.ReleaseAndGetAddressOf());

    if (FAILED(hr)) throw std::runtime_error("Fallo al crear el input layout de las lucirnagas.");
    // 4. Crear los Constant Buffers
    CD3D11_BUFFER_DESC cbd(sizeof(CB_Firefly_PerFrame), D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);
    hr = device->CreateBuffer(&cbd, nullptr, m_cbFireflyPerFrame.ReleaseAndGetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("Fallo al crear el CB PerFrame de las lucirnagas.");

    cbd.ByteWidth = sizeof(CB_Firefly_PerParticle);
    hr = device->CreateBuffer(&cbd, nullptr, m_cbFireflyPerParticle.ReleaseAndGetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("Fallo al crear el CB PerParticle de las lucirnagas.");


    D3D11_TEXTURE2D_DESC texDesc = {};
    texDesc.Width = SHADOW_MAP_SIZE;
    texDesc.Height = SHADOW_MAP_SIZE;
    texDesc.MipLevels = 1;
    texDesc.ArraySize = 1;
    texDesc.Format = DXGI_FORMAT_R32_TYPELESS; 
    texDesc.SampleDesc.Count = 1;
    texDesc.Usage = D3D11_USAGE_DEFAULT;
    texDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE; 

    hr = device->CreateTexture2D(&texDesc, nullptr, m_shadowMapTexture.ReleaseAndGetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("Fallo al crear la textura del shadow map.");

    D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
    dsvDesc.Format = DXGI_FORMAT_D32_FLOAT; 
    dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Flags = 0;

    hr = device->CreateDepthStencilView(m_shadowMapTexture.Get(), &dsvDesc, m_shadowMapDSV.ReleaseAndGetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("Fallo al crear el DSV del shadow map.");

    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R32_FLOAT; 
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;

    hr = device->CreateShaderResourceView(m_shadowMapTexture.Get(), &srvDesc, m_shadowMapSRV.ReleaseAndGetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("Fallo al crear el SRV del shadow map.");

    D3D11_SAMPLER_DESC samplerDesc = {};
    samplerDesc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
    samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_BORDER;
    samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_BORDER;
    samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_BORDER;
    samplerDesc.BorderColor[0] = 1.0f;
    samplerDesc.BorderColor[1] = 1.0f;
    samplerDesc.BorderColor[2] = 1.0f;
    samplerDesc.BorderColor[3] = 1.0f;
    samplerDesc.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL;

    hr = device->CreateSamplerState(&samplerDesc, m_shadowSamplerState.ReleaseAndGetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("Fallo al crear el sampler de comparación para sombras.");

    // RASTERIZER STATE DEFINITIVO Y SEGURO
    D3D11_RASTERIZER_DESC rasterDesc = {};
    rasterDesc.FillMode = D3D11_FILL_SOLID;
    rasterDesc.FrontCounterClockwise = true;
    rasterDesc.DepthClipEnable = true;
    rasterDesc.CullMode = D3D11_CULL_NONE; // Modo estándar y seguro

    // Bias para evitar shadow acne
    rasterDesc.DepthBias = 4000;
    rasterDesc.SlopeScaledDepthBias = 2.0f;
    rasterDesc.DepthBiasClamp = 0.0f;

    hr = device->CreateRasterizerState(&rasterDesc, &m_shadowRasterizerState);
    if (FAILED(hr)) throw std::runtime_error("Fallo al crear el estado de rasterizador para sombras.");


    hr = D3DReadFileToBlob(L"C:ShadowVS.cso", vsBlob.GetAddressOf());
    if (FAILED(hr))
    {
        throw std::runtime_error("Fallo al cargar ShadowVS.cso. Revisa la ruta y asegúrate de que compila.");
    }

    hr = device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, m_shadowVertexShader.ReleaseAndGetAddressOf());
    if (FAILED(hr))
    {
        throw std::runtime_error("Fallo al crear el vertex shader de sombras.");
    }

    hr = D3DReadFileToBlob(L"C:ShadowPS.cso", psBlob.GetAddressOf()); // O la ruta completa si es necesario
    if (FAILED(hr))
    {
        throw std::runtime_error("Fallo al cargar ShadowPS.cso.");
    }

    hr = device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, m_shadowPixelShader.ReleaseAndGetAddressOf());
    if (FAILED(hr))
    {
        throw std::runtime_error("Fallo al crear el pixel shader de sombras.");
    }

    Microsoft::WRL::ComPtr<ID3DBlob> vsAlphaBlob;
    hr = D3DReadFileToBlob(L"C:ShadowVS_AlphaClip.cso", vsAlphaBlob.GetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("Fallo al cargar ShadowVS_AlphaClip.cso.");
    hr = device->CreateVertexShader(vsAlphaBlob->GetBufferPointer(), vsAlphaBlob->GetBufferSize(), nullptr, m_shadowVertexShader_AlphaClip.ReleaseAndGetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("Fallo al crear el vertex shader de sombras con alfa.");

    // CREAMOS EL NICO INPUT LAYOUT QUE NECESITAMOS USANDO EL BLOB CORRECTO
    const D3D11_INPUT_ELEMENT_DESC shadowLayoutDesc[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };

    hr = device->CreateInputLayout(
        shadowLayoutDesc,
        ARRAYSIZE(shadowLayoutDesc),
        vsAlphaBlob->GetBufferPointer(), // <-- Ahora usa el blob correcto
        vsAlphaBlob->GetBufferSize(),
        m_shadowInputLayout.ReleaseAndGetAddressOf()
    );
    if (FAILED(hr))
    {
        throw std::runtime_error("Fallo al crear el input layout de sombras unificado.");
    }

    Microsoft::WRL::ComPtr<ID3DBlob> psAlphaBlob;
    hr = D3DReadFileToBlob(L"ShadowPS_AlphaClip.cso", psAlphaBlob.GetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("Fallo al cargar ShadowPS_AlphaClip.cso.");
    hr = device->CreatePixelShader(psAlphaBlob->GetBufferPointer(), psAlphaBlob->GetBufferSize(), nullptr, m_shadowPixelShader_AlphaClip.ReleaseAndGetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("Fallo al crear el pixel shader de sombras con alfa.");

    // --- Crear el Input Layout para el Pase de Sombras ---
    // Aunque el shader solo usa la posición, el layout debe describir la estructura completa del buffer
    // de vértices (ModelVertex o TerrainVertex) para que la GPU sepa el tamaño de cada vértice.

    D3D11_DEPTH_STENCIL_DESC dsDesc = {};
    dsDesc.DepthEnable = TRUE;                         // Habilitar pruebas de profundidad
    dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL; // LA CLAVE: Permitir escribir en el buffer
    dsDesc.DepthFunc = D3D11_COMPARISON_LESS;           // Pasa si el nuevo pxel est ms cerca

    // No usamos el stencil, as que lo dejamos por defecto
    dsDesc.StencilEnable = FALSE;
    dsDesc.StencilReadMask = D3D11_DEFAULT_STENCIL_READ_MASK;
    dsDesc.StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;

    hr = device->CreateDepthStencilState(&dsDesc, m_shadowDepthState.ReleaseAndGetAddressOf());
    if (FAILED(hr))
    {
        throw std::runtime_error("Fallo al crear el estado de profundidad para sombras.");
    }

    // Shaders 
    hr = D3DReadFileToBlob(L"FullscreenQuadVS.cso", vsBlob.GetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("Fallo al cargar FullscreenQuadVS.cso");
    hr = device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, m_fullscreenQuadVS.ReleaseAndGetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("Fallo al crear FullscreenQuadVS");

    hr = D3DReadFileToBlob(L"BloomExtractPS.cso", psBlob.GetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("Fallo al cargar BloomExtractPS.cso");
    hr = device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, m_bloomExtractPS.ReleaseAndGetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("Fallo al crear BloomExtractPS");
    
    hr = D3DReadFileToBlob(L"GaussianBlurHorizontalPS.cso", psBlob.GetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("Fallo al cargar GaussianBlurHorizontalPS.cso");
    hr = device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, m_gaussianBlurHorizontalPS.ReleaseAndGetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("Fallo al crear GaussianBlurHorizontalPS");

    hr = D3DReadFileToBlob(L"GaussianBlurVerticalPS.cso", psBlob.GetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("Fallo al cargar GaussianBlurVerticalPS.cso");
    hr = device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, m_gaussianBlurVerticalPS.ReleaseAndGetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("Fallo al crear GaussianBlurVerticalPS");
    
    hr = D3DReadFileToBlob(L"BloomCompositePS.cso", psBlob.GetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("Fallo al cargar BloomCompositePS.cso");
    hr = device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, m_bloomCompositePS.ReleaseAndGetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("Fallo al crear BloomCompositePS");

    hr = D3DReadFileToBlob(L"C:MysticItemPS.cso", psBlob.ReleaseAndGetAddressOf());

    if (SUCCEEDED(hr))
    {
        device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, m_mysticPS.ReleaseAndGetAddressOf());
    }

    // --- NUEVO: CREAR CONSTANT BUFFERS DE POST-PROCESAMIENTO ---
    CD3D11_BUFFER_DESC cbDesc(sizeof(CB_BloomParameters), D3D11_BIND_CONSTANT_BUFFER, D3D11_USAGE_DYNAMIC, D3D11_CPU_ACCESS_WRITE);
    hr = device->CreateBuffer(&cbDesc, nullptr, m_cbBloomParameters.ReleaseAndGetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("Fallo al crear CB de Bloom");

    cbDesc.ByteWidth = sizeof(CB_BlurParameters);
    hr = device->CreateBuffer(&cbDesc, nullptr, m_cbBlurParameters.ReleaseAndGetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("Fallo al crear CB de Blur");


    device;
}

void Game::CreateWindowSizeDependentResources()
{
    // Liberar recursos antiguos
    m_sceneTexture.Reset(); m_sceneRTV.Reset(); m_sceneSRV.Reset();
    m_bloomExtractTexture.Reset(); m_bloomExtractRTV.Reset(); m_bloomExtractSRV.Reset();
    m_blurTexture.Reset(); m_blurRTV.Reset(); m_blurSRV.Reset();

    RECT outputSize = m_deviceResources->GetOutputSize();
    int width = outputSize.right - outputSize.left;
    int height = outputSize.bottom - outputSize.top;

    if (m_camera)
    {
        m_camera->UpdateProjectionMatrix(width, height);
    }

    if (width == 0 || height == 0) return;

    auto device = m_deviceResources->GetD3DDevice();
    HRESULT hr;

    // Usamos un formato de alta precisión para permitir colores más brillantes que 1.0 (HDR)
    DXGI_FORMAT hdrFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;

    // Textura de la escena principal (resolución completa)
    CD3D11_TEXTURE2D_DESC sceneDesc(hdrFormat, width, height, 1, 1, D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE, D3D11_USAGE_DEFAULT, 0, 1);
    hr = device->CreateTexture2D(&sceneDesc, nullptr, m_sceneTexture.ReleaseAndGetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("Fallo al crear textura de escena");
    hr = device->CreateRenderTargetView(m_sceneTexture.Get(), nullptr, m_sceneRTV.ReleaseAndGetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("Fallo al crear RTV de escena");
    hr = device->CreateShaderResourceView(m_sceneTexture.Get(), nullptr, m_sceneSRV.ReleaseAndGetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("Fallo al crear SRV de escena");

    // Texturas de Bloom y Blur (a menor resolución para mejor rendimiento y un look más suave)
    int bloomWidth = width / 2;
    int bloomHeight = height / 2;
    CD3D11_TEXTURE2D_DESC bloomDesc(hdrFormat, bloomWidth, bloomHeight, 1, 1, D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE, D3D11_USAGE_DEFAULT, 0, 1);

    hr = device->CreateTexture2D(&bloomDesc, nullptr, m_bloomExtractTexture.ReleaseAndGetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("Fallo al crear textura de extracción de bloom");
    hr = device->CreateRenderTargetView(m_bloomExtractTexture.Get(), nullptr, m_bloomExtractRTV.ReleaseAndGetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("Fallo al crear RTV de extracción de bloom");
    hr = device->CreateShaderResourceView(m_bloomExtractTexture.Get(), nullptr, m_bloomExtractSRV.ReleaseAndGetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("Fallo al crear SRV de extracción de bloom");

    hr = device->CreateTexture2D(&bloomDesc, nullptr, m_blurTexture.ReleaseAndGetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("Fallo al crear textura de blur");
    hr = device->CreateRenderTargetView(m_blurTexture.Get(), nullptr, m_blurRTV.ReleaseAndGetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("Fallo al crear RTV de blur");
    hr = device->CreateShaderResourceView(m_blurTexture.Get(), nullptr, m_blurSRV.ReleaseAndGetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("Fallo al crear SRV de blur");
}

void Game::OnDeviceLost()
{
    // TODO: Add Direct3D resource cleanup here.
}

void Game::OnDeviceRestored()
{
    CreateDeviceDependentResources();

    CreateWindowSizeDependentResources();
}
#pragma endregion

#pragma region Model Instances

void Game::AddInstancedObject(
    Model* modelPtr,
    const DirectX::SimpleMath::Matrix& baseTransform, 
    float instanceX,
    float instanceZ,
    float fallbackY,
    float modelSpecificOffsetY,
    bool useAutoCollision)
{
    if (!modelPtr || !m_terrain) { 
        return;
    }

    float finalInstanceY;
    float terrainHeightHere;

    if (m_terrain->GetWorldHeightAt(instanceX, instanceZ, terrainHeightHere)) {
        finalInstanceY = terrainHeightHere + modelSpecificOffsetY;
    }
    else {
        finalInstanceY = fallbackY;
    }

    DirectX::SimpleMath::Matrix instanceWorldMatrix = baseTransform; // Comienza con escala/rotación base del modelo
    // Establece la posición de esta instancia
    instanceWorldMatrix.Translation(DirectX::SimpleMath::Vector3(instanceX, finalInstanceY, instanceZ));

    m_worldInstances.emplace_back(modelPtr, instanceWorldMatrix, useAutoCollision);
}

#pragma endregion

#pragma region Shadow Mapping

void Game::RenderShadowPass()
{
    auto context = m_deviceResources->GetD3DDeviceContext();
    if (!m_shadowDepthState || !m_shadowRasterizerState) return;

    // --- 1. Configuración de la Pipeline ---
    context->OMSetRenderTargets(0, nullptr, m_shadowMapDSV.Get());
    context->OMSetDepthStencilState(m_shadowDepthState.Get(), 0);
    context->ClearDepthStencilView(m_shadowMapDSV.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
    context->RSSetState(m_shadowRasterizerState.Get());

    D3D11_VIEWPORT shadowViewport = { 0.0f, 0.0f, SHADOW_MAP_SIZE, SHADOW_MAP_SIZE, 0.0f, 1.0f };
    context->RSSetViewports(1, &shadowViewport);

    // --- 2. Definición de la Convención de la Luz ---
    // Convención: m_lightData.directionalLightVector es un vector unitario que representa
    // la DIRECCIÓN EN LA QUE VIAJA LA LUZ (ej. desde el sol hacia el mundo).
    // Para la mayoría de los cálculos de iluminación (NdotL), necesitaremos su inverso.
    Vector3 lightDirection = m_lightData.directionalLightVector;


    Vector3 focusPoint = m_camera->GetPosition();

    Vector3 lightPosition = focusPoint - (lightDirection * 500.0f); 

    // Vector "arriba" robusto para evitar problemas cuando la luz es cenital.
    Vector3 lightUp;
    if (abs(lightDirection.Dot(Vector3::Up)) < 0.9f) {
        lightUp = Vector3::Up;
    }
    else {
        lightUp = Vector3::Forward;
    }

    m_lightViewMatrix = Matrix::CreateLookAt(lightPosition, focusPoint, lightUp);

    // --- 4. Creación de la Matriz de Proyección de la Luz (CreateOrthographic) ---
    // Esta es una proyección ortográfica porque la luz del sol es direccional (rayos paralelos).
    // Los valores de ancho/alto definen el área que cubrirá la sombra.
    float orthoWidth = 500.0f;  // <-- Aumenta el rea de cobertura (antes 250)
    float orthoHeight = 500.0f; // <-- Aumenta el rea de cobertura (antes 250)
    float nearPlane = 1.0f;
    float farPlane = 2000.0f;

    m_lightProjectionMatrix = Matrix::CreateOrthographic(orthoWidth, orthoHeight, nearPlane, farPlane);

    // --- 5. Dibujado de Objetos en el Pase de Sombras ---
    context->IASetInputLayout(m_shadowInputLayout.Get()); // Usamos un layout unificado

    // Bucle para dibujar los modelos
    for (const auto& instance : m_worldInstances)
    {
        if (instance.baseModel)
        {
            bool usesAlphaClip = (instance.baseModel == m_green_tree1.get() ||
                instance.baseModel == m_forest_pine1.get() ||
                instance.baseModel == m_forest_pine2.get() ||
                instance.baseModel == m_forest_pine3.get());

            if (usesAlphaClip)
            {
                context->VSSetShader(m_shadowVertexShader_AlphaClip.Get(), nullptr, 0);
                context->PSSetShader(m_shadowPixelShader_AlphaClip.Get(), nullptr, 0);
            }
            else
            {
                context->VSSetShader(m_shadowVertexShader.Get(), nullptr, 0);
                context->PSSetShader(nullptr, nullptr, 0); // No se necesita Pixel Shader para sólidos
            }

            instance.baseModel->ShadowDrawAlphaClip(context, instance.worldTransform, m_lightViewMatrix, m_lightProjectionMatrix, m_samplerState.Get());
        }
    }


    // Dibujar el terreno
    if (m_terrain)
    {
        context->VSSetShader(m_shadowVertexShader.Get(), nullptr, 0);
        context->PSSetShader(nullptr, nullptr, 0);
        m_terrain->ShadowDraw(context, m_lightViewMatrix, m_lightProjectionMatrix);
    }

    if (m_currentModel)
    {
        // Usamos el shader normal de sombras (sin alpha clip porque es armadura sólida)
        context->VSSetShader(m_shadowVertexShader.Get(), nullptr, 0);
        context->PSSetShader(nullptr, nullptr, 0); // No pixel shader needed for opaque shadows

        // La matriz de mundo ya se actualizó en UpdatePlayer, así que la pedimos directo
        DirectX::SimpleMath::Matrix playerWorld = m_currentModel->GetWorldMatrix();

        m_currentModel->ShadowDraw(
            context,
            playerWorld,
            m_lightViewMatrix,
            m_lightProjectionMatrix
        );
    }
}

#pragma endregion

#pragma region minimap

void Game::RenderMinimapPass()
{
    auto context = m_deviceResources->GetD3DDeviceContext();

    // 1. Subir los datos de luz del minimapa a su Constant Buffer
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    HRESULT hr = context->Map(m_minimapLightPropertiesCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    if (SUCCEEDED(hr))
    {
        memcpy(mappedResource.pData, &m_minimapLightData, sizeof(PSLightPropertiesData));
        context->Unmap(m_minimapLightPropertiesCB.Get(), 0);
    }

    // 2. Configurar la pipeline para renderizar AL MINIMAPA
    context->OMSetRenderTargets(1, m_minimapRTV.GetAddressOf(), m_minimapDSV.Get());
    context->ClearRenderTargetView(m_minimapRTV.Get(), Colors::DarkSlateGray);
    context->ClearDepthStencilView(m_minimapDSV.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);
    context->RSSetViewports(1, &m_minimapViewport);

    // 3. Configurar la "cmara" del minimapa
    Vector3 playerPos = m_camera->GetPosition();
    Vector3 mapCamPos = Vector3(playerPos.x, 150.0f, playerPos.z);
    Vector3 mapCamTarget = playerPos;
    Matrix minimapView = Matrix::CreateLookAt(mapCamPos, mapCamTarget, Vector3(0.0f, 1.0f, 0.0001f));
    Matrix minimapProj = Matrix::CreateOrthographic(150.f, 150.f, 1.0f, 400.0f);

    // 4. Dibujar la escena en el minimapa USANDO LA LUZ DEL MINIMAPA
    if (m_terrain)
    {
        m_terrain->SetViewMatrix(minimapView);
        m_terrain->SetProjectionMatrix(minimapProj);

        m_terrain->Render(context, m_minimapLightPropertiesCB.Get(), m_samplerState.Get(), playerPos, m_lightViewMatrix * m_lightProjectionMatrix, nullptr, m_shadowSamplerState.Get());
    }

    for (const auto& instance : m_worldInstances)
    {
        if (instance.baseModel)
        {
            // Le pasamos el sampler real, aunque la textura sea nula.
            instance.baseModel->EvolvingDraw(
                context,
                minimapView,
                minimapProj,
                m_minimapLightPropertiesCB.Get(),
                m_samplerState.Get(),
                m_lightViewMatrix,
                m_lightProjectionMatrix,
                nullptr,
                m_shadowSamplerState.Get()
            );
        }
    }
}

#pragma endregion


#pragma region DayNight Cycle

DirectX::SimpleMath::Vector4 LerpColor(const DirectX::SimpleMath::Vector4& a, const DirectX::SimpleMath::Vector4& b, float t)
{
    return DirectX::SimpleMath::Vector4::Lerp(a, b, t);
}

void Game::UpdateDayNightCycle(float elapsedTime)
{
    // 1. AVANZAR LA HORA DEL DÍA
    m_timeOfDay += elapsedTime * m_dayNightCycleSpeed;
    m_timeOfDay = fmodf(m_timeOfDay, 1.0f); // Se mantiene entre 0.0 y 1.0

    // 2. CALCULAR LA TRAYECTORIA CONTINUA DE LA LUZ
    // La luz sigue un círculo completo. El amanecer ocurre en t=0.25, mediodía en t=0.5, etc.
    const float cycleAngle = m_timeOfDay * 2.0f * DirectX::XM_PI - DirectX::XM_PIDIV2;

    // La trayectoria forma un arco en el cielo.
    // Y = altitud, Z = movimiento Este/Oeste, X = inclinación Norte/Sur
    Vector3 finalLightDirection = Vector3(sin(cycleAngle) * 0.4f, -sin(cycleAngle), cos(cycleAngle));
    finalLightDirection.Normalize();

    if (finalLightDirection.y > 0.0f)
    {
        finalLightDirection.y = -0.05f;
        finalLightDirection.Normalize(); 
    }

    m_lightData.directionalLightVector = finalLightDirection;

    // 3. DETERMINAR LA INFLUENCIA DEL SOL BASADO EN SU ALTURA
    // m_sunPower será 1.0 en el punto más alto del sol y 0.0 cuando esté en el horizonte o por debajo.
    m_sunPower = std::clamp(-finalLightDirection.y, 0.0f, 1.0f);

    // 4. MEZCLAR COLORES E INTENSIDADES

    // --- Paleta de colores ---
    const Vector4 HORIZON_RED(1.0f, 0.2f, 0.1f, 1.0f);
    const Vector4 GOLDEN_HOUR_ORANGE(1.0f, 0.6f, 0.2f, 1.0f);
    const Vector4 MIDDAY_SUN_YELLOW(1.0f, 1.0f, 0.9f, 1.0f);
    const Vector4 MOON_COLOR(0.5f, 0.6f, 0.8f, 1.0f); // Luna un poco más brillante y azulada

    const Vector4 SUNSET_AMBIENT_PURPLE(0.4f, 0.25f, 0.45f, 1.0f);
    const Vector4 DAY_AMBIENT(0.35f, 0.35f, 0.4f, 1.0f);
    const Vector4 NIGHT_AMBIENT(0.02f, 0.025f, 0.04f, 1.0f);

    // --- Intensidades ---
    const float SUN_INTENSITY = 1.9f;
    const float MOON_INTENSITY = 0.15f;
    const float AMBIENT_DAY_INTENSITY = 1.0f;
    const float AMBIENT_NIGHT_INTENSITY = 0.12f;

    // --- Lógica de Transición de Color del Sol (solo cuando está visible) ---
    Vector4 currentSunColor;
    Vector4 currentAmbientColor;
    const float goldenHourThreshold = 0.3f;

    if (m_sunPower < goldenHourThreshold)
    {
        float t = m_sunPower / goldenHourThreshold;
        currentSunColor = Vector4::Lerp(HORIZON_RED, GOLDEN_HOUR_ORANGE, t);
        currentAmbientColor = Vector4::Lerp(SUNSET_AMBIENT_PURPLE, DAY_AMBIENT, t);
    }
    else
    {
        float t = (m_sunPower - goldenHourThreshold) / (1.0f - goldenHourThreshold);
        currentSunColor = Vector4::Lerp(GOLDEN_HOUR_ORANGE, MIDDAY_SUN_YELLOW, t);
        currentAmbientColor = DAY_AMBIENT;
    }

    // 5. COMBINACIÓN FINAL (LA CLAVE DE LA SUAVIDAD)
    // Usamos m_sunPower para mezclar suavemente entre las propiedades del sol y la luna.
    // Cuando m_sunPower es 0 (noche), solo la contribución de la luna es visible.
    // Cuando m_sunPower es 1 (mediodía), solo la contribución del sol es visible.

    // Luz Direccional: Mezcla entre el color del sol calculado y el color de la luna.
    Vector4 sunContribution = currentSunColor * SUN_INTENSITY;
    Vector4 moonContribution = MOON_COLOR * MOON_INTENSITY;
    m_lightData.directionalLightColor = Vector4::Lerp(moonContribution, sunContribution, m_sunPower);

    // Luz Ambiental: Mezcla entre el ambiente de noche y el ambiente de día.
    Vector4 dayAmbientFinal = currentAmbientColor * AMBIENT_DAY_INTENSITY;
    Vector4 nightAmbientFinal = NIGHT_AMBIENT * AMBIENT_NIGHT_INTENSITY;
    m_lightData.ambientLightColor = Vector4::Lerp(nightAmbientFinal, dayAmbientFinal, m_sunPower);

    // 6. ACTUALIZAR SKYDOME
    if (m_skyEffect)
    {
        Vector3 skyTintColor(m_lightData.ambientLightColor);
        float skyBrightnessFactor = m_sunPower * 1.8f;
        skyTintColor *= (1.0f + skyBrightnessFactor);

        skyTintColor.x = std::max(skyTintColor.x, 0.01f);
        skyTintColor.y = std::max(skyTintColor.y, 0.015f);
        skyTintColor.z = std::max(skyTintColor.z, 0.025f);

        m_skyEffect->SetDiffuseColor(skyTintColor);
    }
}

void Game::ResetFirefly(FireflyParticle& particle)
{
    // Posición aleatoria dentro del volumen definido
    particle.position.x = m_fireflyVolume.Center.x + (((float)rand() / RAND_MAX) * 2.f - 1.f) * m_fireflyVolume.Extents.x;
    particle.position.y = m_fireflyVolume.Center.y + (((float)rand() / RAND_MAX) * 2.f - 1.f) * m_fireflyVolume.Extents.y;
    particle.position.z = m_fireflyVolume.Center.z + (((float)rand() / RAND_MAX) * 2.f - 1.f) * m_fireflyVolume.Extents.z;

    // Velocidad aleatoria suave para que floten
    particle.velocity.x = (((float)rand() / RAND_MAX) * 2.f - 1.f) * 0.5f; // Movimiento lento en X
    particle.velocity.y = (((float)rand() / RAND_MAX) * 2.f - 1.f) * 0.3f; // Movimiento lento en Y
    particle.velocity.z = (((float)rand() / RAND_MAX) * 2.f - 1.f) * 0.5f; // Movimiento lento en Z

    // Tiempo de vida aleatorio para que no desaparezcan todas a la vez
    particle.maxLifetime = 4.0f + ((float)rand() / RAND_MAX) * 5.f; // Entre 4 y 9 segundos
    particle.lifetime = particle.maxLifetime;

    // Temporizador de parpadeo con un desfase aleatorio
    particle.blinkTimer = ((float)rand() / RAND_MAX) * 2.0f * DirectX::XM_PI;
    particle.rotation = ((float)rand() / RAND_MAX) * DirectX::XM_2PI;
}

void Game::InitializeFireflies()
{
    m_fireflies.resize(NUM_FIREFLIES);
    for (auto& firefly : m_fireflies)
    {
        ResetFirefly(firefly);
    }
}

void Game::UpdateFireflies(float elapsedTime)
{

    if (m_sunPower > 0.1f) // Si el sol tiene algo de fuerza, las luciérnagas se ocultan
    {
        return;
    }

    for (auto& firefly : m_fireflies)
    {
        firefly.lifetime -= elapsedTime;
        if (firefly.lifetime <= 0.f)
        {
            ResetFirefly(firefly);
        }

        // Actualizar posición
        firefly.position += firefly.velocity * elapsedTime;

        // Añadir un movimiento suave y ondulante
        firefly.position.y += sin(firefly.blinkTimer * 2.0f) * 0.2f * elapsedTime;

        // Actualizar temporizador de parpadeo
        firefly.blinkTimer += elapsedTime * 3.0f;
    }
}

#pragma endregion

#pragma region Player & Camera Logic

void Game::UpdateInput(float elapsedTime)
{
    if (m_kbState.Escape) ExitGame();

    // Actualizar Trackers
    if (m_keyboard) {
        m_kbState = m_keyboard->GetState();
        m_kbTracker.Update(m_kbState);
    }
    if (m_mouse) {
        m_mouseState = m_mouse->GetState();
        m_mouseTracker.Update(m_mouseState);
    }

    // Lógica de Sprint (Doble Tap W)
    bool wPressed = m_kbState.W;
    if (m_wTapTimer > 0.0f) {
        m_wTapTimer -= elapsedTime;
        if (m_wTapTimer <= 0.0f) { m_wTapCount = 0; m_wTapTimer = 0.0f; }
    }

    if (m_kbTracker.pressed.W) {
        if (m_wTapCount == 1 && m_wTapTimer > 0.0f) {
            m_isSprinting = true;
            m_wTapCount = 0;
            m_wTapTimer = 0.0f;
        }
        else {
            m_wTapCount = 1;
            m_wTapTimer = m_doubleTapTimeLimit;
        }
    }

    // Cancelar sprint si se deja de mover
    if (!wPressed || (m_kbState.A || m_kbState.S || m_kbState.D)) {
        m_isSprinting = false;
    }

    // Cambio de Cámara (C)
    if (m_kbTracker.pressed.C) {
        m_isThirdPerson = !m_isThirdPerson;
    }

    m_currentSpeed = (m_isSprinting && wPressed) ? m_sprintSpeed : m_normalSpeed;

    if (m_kbState.P)
    {
        m_dayNightCycleSpeed = 0.2f; 
    }
    else
    {
        m_dayNightCycleSpeed = 0.001f; 
    }

    if (m_kbTracker.pressed.D1 || m_kbTracker.pressed.NumPad1)
    {
        if (m_hasAxe) {
            m_currentToolId = 1;
            OutputDebugString(L"-> Equipado: HACHA\n");
        }
    }

    if (m_kbTracker.pressed.D2 || m_kbTracker.pressed.NumPad2)
    {
        if (m_swordRepaired) { 
            m_currentToolId = 2;
            OutputDebugString(L"-> Equipado: ESPADA REAL\n");
        }
    }
}

void Game::UpdatePlayer(float elapsedTime)
{
    // ---------------------------------------------------------
    // 1. LÓGICA DE ATAQUE (Sin cambios)
    // ---------------------------------------------------------
    if (m_isChopping)
    {
        m_animTimer += elapsedTime;
        if (m_animTimer >= 0.025f)
        {
            m_animTimer = 0.0f;
            m_currentFrame++;

            if (m_currentFrame >= m_animMelee.size())
            {
                m_isChopping = false;
                m_currentFrame = 0;
            }
        }

        if (!m_animMelee.empty()) {
            int safeIndex = std::min((int)m_currentFrame, (int)m_animMelee.size() - 1);
            m_currentModel = m_animMelee[safeIndex].get();
        }

        if (m_currentModel) {
            m_currentModel->SetPosition(m_playerPos);
            m_currentModel->SetRotationEuler(0.0f, m_camera->GetYaw(), 0.0f);
            m_currentModel->SetScale(10.0f);
        }
        return; // Salimos para no movernos mientras atacamos
    }

    // ---------------------------------------------------------
    // 2. INPUT Y CÁLCULO DE MOVIMIENTO
    // ---------------------------------------------------------
    DirectX::SimpleMath::Vector3 moveDir = DirectX::SimpleMath::Vector3::Zero;
    if (m_kbState.W) moveDir.z -= 1.f;
    if (m_kbState.S) moveDir.z += 1.f;
    if (m_kbState.A) moveDir.x -= 1.f;
    if (m_kbState.D) moveDir.x += 1.f;

    bool isMoving = (moveDir.LengthSquared() > 0);

    if (isMoving)
    {
        moveDir.Normalize();
        // Moverse respecto a la cámara (Yaw)
        Matrix camYRotation = Matrix::CreateRotationY(m_camera->GetYaw());
        Vector3 worldMove = Vector3::Transform(moveDir, camYRotation);

        // --- AQUÍ EMPIEZA EL SISTEMA DE COLISIONES ---

        // A. Predecir a dónde queremos ir
        Vector3 proposedPos = m_playerPos + (worldMove * m_currentSpeed * elapsedTime);

        // B. Crear una caja para el jugador en esa posición futura
        // Tamaño aprox: 1m ancho, 2m alto, 1m profundidad
        Vector3 playerSize(0.5f, 2.0f, 0.5f);
        // Ajustamos el centro para que la caja suba desde los pies
        Vector3 boxCenter = proposedPos;
        boxCenter.y += 1.0f;
        DirectX::BoundingBox playerBox(boxCenter, playerSize);

        bool collisionDetected = false;

        // C. FASE 1: Revisión Automática (Árboles, Rocas, Casas estándar)
        for (const auto& instance : m_worldInstances)
        {
            if (!instance.baseModel) continue;

            // ¡CRUCIAL! Si es la herrería (false), la ignoramos aquí para poder entrar
            if (!instance.useAutoCollision) continue;

            float dist = Vector3::Distance(proposedPos, instance.worldTransform.Translation());
            if (dist > 20.0f) continue;

            if (dist < 8.0f)
            {
                if (instance.baseModel == m_green_tree1.get())       m_debugModelName = L"MODELO: GREEN TREE";
                else if (instance.baseModel == m_forest_pine1.get()) m_debugModelName = L"MODELO: PINO 1";
                else if (instance.baseModel == m_forest_pine2.get()) m_debugModelName = L"MODELO: PINO 2";
                else if (instance.baseModel == m_forest_pine3.get()) m_debugModelName = L"MODELO: PINO 3";
            }

            // --- IDENTIFICAR SI ES UN ÁRBOL ---
            // Creamos un vector de "engorde" por defecto (0,0,0)
            Vector3 collisionPadding = Vector3::Zero;

            if (instance.baseModel == m_green_tree1.get())
            {
                collisionPadding = Vector3(0.0f, 10.0f, 0.0f);
            }
            else if (instance.baseModel == m_forest_pine3.get())
            {
                collisionPadding = Vector3(3.0f, 12.0f, 3.0f);
            }
            else if (instance.baseModel == m_forest_pine1.get() ||
                instance.baseModel == m_forest_pine2.get())
            {
                // Tamaño estándar más ajustado
                collisionPadding = Vector3(1.2f, 10.0f, 1.2f);
            }
            else if (instance.baseModel == m_rock1.get() || instance.baseModel == m_rock2.get())
            {
                collisionPadding = Vector3(0.5f, 0.5f, 0.5f);
            }

            

            // Pasamos el padding a la función
            if (instance.baseModel->CheckCollisionAgainstParts(
                playerBox,
                instance.worldTransform,
                m_modelPartBoxesToDraw,
                m_drawDebugCollisions,
                collisionPadding)) // <--- AQUÍ PASAMOS EL VECTOR
            {
                collisionDetected = true;
                break;
            }
        }

        // D. FASE 2: Revisión Manual (Paredes invisibles, Yunque, Muebles)
        if (!collisionDetected)
        {
            for (const auto& box : m_customColliders)
            {
                if (box.Contains(playerBox) != DirectX::DISJOINT)
                {
                    collisionDetected = true;
                    break;
                }
            }
        }

        // E. Aplicar movimiento SOLO si el camino está libre
        if (!collisionDetected)
        {
            m_playerPos = proposedPos;
            m_walkBobTimer += elapsedTime * 10.0f;
        }
        else
        {
            // Chocamos: El jugador se queda donde estaba. 
            // (Opcional: Podrías poner m_walkBobTimer = 0 aquí si quieres que deje de "caminar")
        }
    }
    else
    {
        m_walkBobTimer = 0.0f;
    }

    // ---------------------------------------------------------
    // 3. FÍSICA DE TERRENO (Gravedad / Ajuste Y)
    // ---------------------------------------------------------
    if (m_terrain)
    {
        float terrainHeight = 0.0f;
        if (m_terrain->GetWorldHeightAt(m_playerPos.x, m_playerPos.z, terrainHeight)) {
            m_playerPos.y = terrainHeight;
        }
    }

    // ---------------------------------------------------------
    // 4. SISTEMA DE ANIMACIÓN (Igual que antes)
    // ---------------------------------------------------------
    std::vector<std::unique_ptr<Model>>* currentAnimList = isMoving ? &m_animWalk : &m_animIdle;

    if (currentAnimList->empty()) {
        if (m_knight) {
            m_knight->SetPosition(m_playerPos);
            m_knight->SetRotationEuler(0.0f, m_camera->GetYaw(), 0.0f);
            m_knight->SetScale(0.1f);
        }
        return;
    }

    float velocidadCaminar = 0.03f;
    float velocidadIdle = 0.02f;
    float timePerFrame = isMoving ? velocidadCaminar : velocidadIdle;

    if (m_wasMoving != isMoving)
    {
        m_currentFrame = 0;
        m_animTimer = 0.0f;
        m_wasMoving = isMoving;
    }

    m_animTimer += elapsedTime;
    if (m_animTimer >= timePerFrame)
    {
        m_animTimer = 0.0f;
        m_currentFrame++;
        m_currentFrame %= currentAnimList->size();
    }

    m_currentModel = (*currentAnimList)[m_currentFrame].get();

    if (m_currentModel)
    {
        m_currentModel->SetPosition(m_playerPos);
        m_currentModel->SetRotationEuler(0.0f, m_camera->GetYaw(), 0.0f);
        m_currentModel->SetScale(10.0f);
    }
}

void Game::UpdateCamera(float elapsedTime)
{
    if (!m_camera) return;

    // -------------------------------------------------------
    // 1. ROTACIÓN (MOUSE) - Funciona igual en ambos modos
    // -------------------------------------------------------
    if (m_mouse)
    {
        // Detectar si acabamos de presionar (PRESSED) o si estamos manteniendo (HELD)
        // Usamos el tracker que ya actualizas en UpdateInput
        bool justPressed = (m_mouseTracker.leftButton == DirectX::Mouse::ButtonStateTracker::PRESSED) ||
            (m_mouseTracker.rightButton == DirectX::Mouse::ButtonStateTracker::PRESSED);

        bool isHolding = m_mouseState.leftButton || m_mouseState.rightButton;

        if (justPressed)
        {
            // FRAME 1: Solo cambiamos el modo. NO rotamos.
            // Esto "resetea" el contador interno del mouse a 0.
            m_mouse->SetMode(DirectX::Mouse::MODE_RELATIVE);
        }
        else if (isHolding)
        {
            // FRAME 2 en adelante: Ahora el mouse nos da "deltas" (movimiento suave)
            // Como ya estamos en modo relativo, x e y son pequeños.

            float yawDelta = -static_cast<float>(m_mouseState.x) * 0.001f;
            float pitchDelta = -static_cast<float>(m_mouseState.y) * 0.001f;

            // Opcional: Protección extra contra saltos locos (si el delta es absurdo, ignóralo)
            if (abs(yawDelta) < 1.0f && abs(pitchDelta) < 1.0f)
            {
                m_camera->Rotate(yawDelta * 2.0f, pitchDelta * 2.0f);
            }
        }
        else
        {
            // Si soltamos, devolvemos el cursor
            m_mouse->SetMode(DirectX::Mouse::MODE_ABSOLUTE);
        }
    }

    // -------------------------------------------------------
    // 2. MODO DRON (SOLO EN EDITOR)
    // -------------------------------------------------------
    if (m_inEditorMode)
    {
        // Velocidad de vuelo (rápida para llegar lejos)
        float camSpeed = 100.0f * elapsedTime;

        // Turbo con Shift (para cruzar el mapa rápido)
        if (m_kbState.LeftShift) camSpeed *= 5.0f;

        DirectX::SimpleMath::Vector3 move = DirectX::SimpleMath::Vector3::Zero;

        // WASD para moverse adelante/atrás/lados
        if (m_kbState.W) move.z -= 1.f;
        if (m_kbState.S) move.z += 1.f;
        if (m_kbState.A) move.x -= 1.f;
        if (m_kbState.D) move.x += 1.f;

        // Q y E para subir/bajar (Elevación)
        if (m_kbState.Q) move.y += 1.f; // Arriba
        if (m_kbState.E) move.y -= 1.f; // Abajo

        // Aplicar movimiento relativo a hacia donde mira la cámara
        m_camera->MoveRelative(move * camSpeed);

        // IMPORTANTE: Actualizar matriz y SALIR para no ejecutar la lógica de seguir al jugador
        m_camera->UpdateViewMatrix();
        return;
    }

    // -------------------------------------------------------
    // 3. MODO JUEGO NORMAL (SEGUIR JUGADOR)
    // -------------------------------------------------------
    if (m_isThirdPerson)
    {
        float ALTURA_PIVOTE = 24.0f;
        float DISTANCIA = 40.0f;

        Vector3 targetPivot = m_playerPos;
        targetPivot.y += ALTURA_PIVOTE;

        Matrix rotationMatrix = Matrix::CreateFromYawPitchRoll(m_camera->GetYaw(), m_camera->GetPitch(), 0.0f);
        Vector3 offset = Vector3::Transform(Vector3(0, 0, 1), rotationMatrix) * DISTANCIA;
        Vector3 finalCamPos = targetPivot + offset;

        // Corrección Terreno (evitar que la cámara entre al suelo)
        if (m_terrain) {
            float groundY = 0.0f;
            if (m_terrain->GetWorldHeightAt(finalCamPos.x, finalCamPos.z, groundY)) {
                if (finalCamPos.y < groundY + 5.0f) finalCamPos.y = groundY + 5.0f;
            }
        }
        m_camera->SetPosition(finalCamPos);
    }
    else // Primera Persona
    {
        Vector3 eyePos = m_playerPos;
        eyePos.y += 26.0f;

        Matrix rotY = Matrix::CreateRotationY(m_camera->GetYaw());
        Vector3 forwardOffset = Vector3::Transform(Vector3(0, 0, -1), rotY) * 4.0f;

        m_camera->SetPosition(eyePos + forwardOffset);
    }

    m_camera->UpdateViewMatrix();
}

void Game::UpdateWorld(float elapsedTime)
{
    // 1. Actualizar Sistemas de Ambiente
    UpdateDayNightCycle(elapsedTime);
    UpdateFireflies(elapsedTime);
    UpdateSmoke(elapsedTime); // Tu sistema de humo

    // 2. Actualizar Luces (Contexto necesario para Map/Unmap)
    if (m_camera && m_lightPropertiesCB)
    {
        auto context = m_deviceResources->GetD3DDeviceContext();

        // --- Datos Básicos ---
        m_lightData.cameraPositionWorld = m_camera->GetPosition();

        // Enviamos el tiempo para efectos de shaders (como el Mystic Item)
        // Nota: Si en tu struct de C++ todavía se llama "_pad0", usa m_lightData._pad0 = ...
        m_lightData.time = static_cast<float>(m_timer.GetTotalSeconds());

        // =============================================================
        // LÓGICA DE LA LUZ DEL HORNO (POINT LIGHT)
        // =============================================================

        // A. Posición: Usamos las coordenadas del humo pero un poco ajustadas
        // X=-79.6, Y=40.0 (altura de la llama), Z=476.5
        m_lightData.pointLightPos = DirectX::SimpleMath::Vector3(-104.0f, 25.0f, 568.0f);

        // B. Rango: Distancia hasta donde ilumina (60 metros)
        m_lightData.pointLightRange = 100.0f;

        // 3. INTENSIDAD SUAVE (Fuego de brasas/carbón)
        float t = static_cast<float>(m_timer.GetTotalSeconds());

        // -- EXPLICACIÓN DE LA NUEVA FÓRMULA --
        // Base sólida (4.0): La luz nunca baja de aquí, para que no se vea oscuro.
        // Onda lenta (sin * 2.0): Simula el fuego "respirando" suavemente.
        // Onda media (sin * 7.0): Pequeñas variaciones aleatorias.
        // Multiplicamos por números pequeños (0.25 y 0.15) para que sea sutil.

        float baseIntensity = 2.0f;
        float smoothFlicker = baseIntensity + (0.25f * sin(t * 2.0f)) + (0.15f * cos(t * 7.0f));

        // Un pequeño "ruido" extra aleatorio para que no sea repetitivo
        float randomJitter = ((float)rand() / RAND_MAX) * 0.1f;

        float finalIntensity = smoothFlicker + randomJitter;

        // Color: Naranja cálido (R=1.0, G=0.45, B=0.05)
        m_lightData.pointLightColor = DirectX::SimpleMath::Vector4(1.0f, 0.45f, 0.05f, finalIntensity);
        // =============================================================

        // 3. Subir datos a la GPU
        D3D11_MAPPED_SUBRESOURCE mappedResource;
        if (SUCCEEDED(context->Map(m_lightPropertiesCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource)))
        {
            memcpy(mappedResource.pData, &m_lightData, sizeof(PSLightPropertiesData));
            context->Unmap(m_lightPropertiesCB.Get(), 0);
        }
    }

    // 3. Actualizar iluminación en la clase Terreno (para shaders básicos si los usa)
    if (m_terrain) {
        m_terrain->UpdateGlobalLighting(
            m_lightData.directionalLightVector,
            m_lightData.directionalLightColor,
            m_lightData.ambientLightColor,
            m_lightData.directionalLightColor
        );
    }
}

void Game::UpdateGameplay(float elapsedTime)
{
    // --- 0. DEBUG Y ESTADOS GLOBALES ---
    if (m_kbTracker.pressed.R)
    {
        m_drawDebugCollisions = !m_drawDebugCollisions;

        BuildCustomColliders();

        if (m_drawDebugCollisions)
            OutputDebugString(L"--> [DEBUG] Colisiones: VISIBLES\n");
        else
            OutputDebugString(L"--> [DEBUG] Colisiones: OCULTAS\n");
    }

    // Si el juego terminó, no procesamos nada
    /*if (m_gameWon || m_gameLost) return;*/

    // Temporizador del Juego
    /*m_gameTimer -= elapsedTime;
    if (m_gameTimer <= 0.0f)
    {
        m_gameLost = true;
        return;
    }*/

    // Temporizador de Popups (UI)
    if (m_itemPopupTimer > 0.0f)
    {
        m_itemPopupTimer -= elapsedTime;
    }

    // =====================================================================
    // 1. LÓGICA DEL HORNO (AUTOMÁTICA)
    // =====================================================================
    if (m_isCooking)
    {
        m_furnaceTimer -= elapsedTime;
        if (m_furnaceTimer <= 0.0f)
        {
            m_isCooking = false;
            m_furnaceTimer = 0.0f;

            // Generar la Pieza Refinada
            GameItem refined;
            refined.type = 5; // ID 5 = Pieza Refinada
            refined.isActive = true;

            // --- CORRECCIÓN DE POSICIÓN ---
            // 1. Tomamos el centro del trigger del horno
            Vector3 spawnPoint = m_furnaceTriggerBox.Center;

            // 2. AJUSTE DE ALTURA (Y): Lo subimos 4.5 unidades (aprox a la cara del jugador)
            spawnPoint.y += 6.0f;
            spawnPoint.z -= 6.0f;

            refined.position = spawnPoint;

            m_gameItems.push_back(refined);

            // Feedback sonoro/visual en consola
            OutputDebugString(L"-> ¡Horno terminó! Cristal generado en el aire.\n");
        }
    }

    // =====================================================================
    // 2. MECÁNICA DE TALA (SE ACTIVA AL ATACAR CON 'T' O CLICK)
    // =====================================================================
    // Si presionas T, inicias la animación (si no estabas atacando ya)
    if (m_kbTracker.pressed.T && !m_isChopping)
    {
        if (m_hasAxe)
        {
            m_isChopping = true;
            m_currentFrame = 0;
            m_animTimer = 0.0f;
            m_hasHitTreeThisSwing = false;
        }
        else
        {
            // Opcional: Feedback visual o sonoro de "No puedes hacer esto"
            // Ejemplo: Mostrar mensaje "Necesito un hacha" en RenderUI si quieres pulirlo luego
            OutputDebugString(L"-> ¡Necesitas un Hacha para talar!\n");
        }
    }

    if (m_isChopping && m_hasAxe && !m_hasHitTreeThisSwing && m_currentFrame > 20 && m_currentFrame < 45)
    {
        for (const auto& instance : m_worldInstances)
        {
            bool isTree = (instance.baseModel == m_green_tree1.get() ||
                instance.baseModel == m_forest_pine1.get() ||
                instance.baseModel == m_forest_pine2.get() ||
                instance.baseModel == m_forest_pine3.get());

            if (isTree)
            {
                float dx = m_playerPos.x - instance.worldTransform.Translation().x;
                float dz = m_playerPos.z - instance.worldTransform.Translation().z;
                float dist2D = sqrt(dx * dx + dz * dz);

                if (dist2D < 12.0f)
                {
                    m_hasHitTreeThisSwing = true;

                    if ((rand() % 100) < 80)
                    {
                        GameItem newLog;
                        newLog.type = 1;
                        newLog.isActive = true;

                        DirectX::SimpleMath::Vector3 dirToPlayer(dx, 0, dz);
                        dirToPlayer.Normalize();

                        float spawnDistance = 4.0f;

                        newLog.position = instance.worldTransform.Translation() + (dirToPlayer * spawnDistance);

                        newLog.position.y = m_playerPos.y + 1.0f;

                        m_gameItems.push_back(newLog);
                        OutputDebugString(L"-> ¡Madera obtenida frente al jugador!\n");
                    }
                    break;
                }
            }
        }
    }

    // =====================================================================
    // 3. INTERACCIÓN CON TECLA 'E' (RECOGER Y USAR)
    // =====================================================================
    if (m_kbTracker.pressed.E)
    {
        bool actionDone = false; // Para no hacer dos cosas a la vez (ej. recoger item y abrir puerta)

        // A. RECOGER ITEMS DEL SUELO
        for (auto& item : m_gameItems)
        {
            if (item.isActive)
            {
                float dist = DirectX::SimpleMath::Vector3::Distance(m_playerPos, item.position);
                if (dist < 12.0f) // Radio de recolección
                {
                    item.isActive = false; // Desaparece
                    actionDone = true;

                    // Inventario
                    switch (item.type)
                    {
                    case 0: m_rawStonesCount++; OutputDebugString(L"-> Recogido: Pieza Mística Cruda\n"); break;
                    case 1: m_woodCount++; OutputDebugString(L"-> Recogido: Leña\n"); break;
                    case 2: m_hasAxe = true; OutputDebugString(L"-> Recogido: Hacha\n"); break;
                    case 3: m_branchesCount++; break;
                    case 4: m_stonesCount++; break;
                    case 5: m_refinedStonesCount++; OutputDebugString(L"-> Recogido: Cristal Refinado\n"); break;
                    }
                    break; // Solo recoger uno por pulsación
                }
            }
        }

        // B. USAR HORNO (Si no recogimos nada)
        if (!actionDone && m_furnaceTriggerBox.Contains(m_playerPos))
        {
            if (!m_isCooking)
            {
                // RECETA: 2 Leña + 1 Piedra Cruda
                if (m_woodCount >= 2 && m_rawStonesCount >= 1)
                {
                    m_woodCount -= 2;
                    m_rawStonesCount -= 1;
                    m_isCooking = true;
                    m_furnaceTimer = 5.0f; // 5 Segundos
                    OutputDebugString(L"-> Horno Encendido: Refinando cristal...\n");
                }
                else
                {
                    OutputDebugString(L"-> Horno: Faltan materiales (2 Leña + 1 Pieza Cruda)\n");
                }
            }
            else
            {
                OutputDebugString(L"-> Horno: Ya está cocinando.\n");
            }
            actionDone = true;
        }

        // C. USAR YUNQUE (Forjar o Reparar)
        if (!actionDone && m_anvilTriggerBox.Contains(m_playerPos))
        {
            // Prioridad 1: Hacer Hacha
            if (!m_hasAxe)
            {
                if (m_branchesCount >= 3 && m_stonesCount >= 3)
                {
                    m_branchesCount -= 3;
                    m_stonesCount -= 3;
                    m_hasAxe = true;
                    m_currentToolId = 1;
                    m_popupItemType = 1; 
                    m_itemPopupTimer = 4.0f;
                    OutputDebugString(L"-> Yunque: ¡Hacha Crafteada!\n");
                }
                else
                {
                    OutputDebugString(L"-> Yunque: Faltan materiales para Hacha (3 Ramas, 3 Piedras)\n");
                }
            }
            // Prioridad 2: Reparar Espada
            else if (!m_swordRepaired)
            {
                if (m_refinedStonesCount >= 5)
                {
                    m_refinedStonesCount -= 5;
                    m_swordRepaired = true;
                    m_swordRepaired = true;
                    m_popupItemType = 2; // Mostrar popup Espada
                    m_itemPopupTimer = 5.0f;
                    OutputDebugString(L"-> Yunque: ¡ESPADA REAL RESTAURADA!\n");
                }
                else
                {
                    wchar_t buf[64];
                    swprintf_s(buf, L"-> Yunque: Faltan Cristales Refinados (%d/5)\n", m_refinedStonesCount);
                    OutputDebugString(buf);
                }
            }
            actionDone = true;
        }

        // D. MAZMORRA (Teleport)
        // Usamos Contains si tienes trigger box, o distancia si tienes punto

        // E. ALTAR FINAL (Victoria)
        if (!actionDone && m_isInDungeon && m_dungeonAltarTrigger.Contains(m_playerPos))
        {
            if (m_swordRepaired)
            {
                m_gameWon = true;
                OutputDebugString(L"-> ¡VICTORIA!\n");
            }
            else
            {
                OutputDebugString(L"-> Altar: Necesitas la Espada Real reparada.\n");
            }
        }
    }

    if (!m_isInDungeon && m_catacombsEntranceTrigger.Contains(m_playerPos))
    {
        // 1. Teletransportar
        m_playerPos = m_dungeonSpawnPos;
        m_isInDungeon = true;

        // 2. Cambiar Atmósfera (Oscuridad total)
        m_lightData.ambientLightColor = DirectX::SimpleMath::Vector4(0.05f, 0.05f, 0.2f, 1.0f);
        m_lightData.directionalLightColor = DirectX::SimpleMath::Vector4(0.0f, 0.0f, 0.0f, 1.0f); // Apagar el sol

        OutputDebugString(L"-> ¡TELEPORT AUTOMÁTICO A MAZMORRA!\n");
    }
}

#pragma endregion

#pragma region Rendering

void Game::RenderScene()
{
    auto context = m_deviceResources->GetD3DDeviceContext();
    DirectX::SimpleMath::Matrix viewMatrix = m_camera->GetViewMatrix();
    DirectX::SimpleMath::Matrix projectionMatrix = m_camera->GetProjectionMatrix();
    float time = static_cast<float>(m_timer.GetTotalSeconds());

    // --- ESTADOS COMUNES ---
    if (m_states)
    {
        context->OMSetBlendState(m_states->Opaque(), nullptr, 0xFFFFFFFF);
        context->OMSetDepthStencilState(m_states->DepthDefault(), 0);
        context->RSSetState(m_states->CullCounterClockwise());
    }

    // 1. TERRENO
    if (m_terrain)
    {
        m_terrain->SetViewMatrix(viewMatrix);
        m_terrain->SetProjectionMatrix(projectionMatrix);
        m_terrain->Render(context, m_lightPropertiesCB.Get(), m_samplerState.Get(), m_camera->GetPosition(),
            m_lightViewMatrix * m_lightProjectionMatrix, m_shadowMapSRV.Get(), m_shadowSamplerState.Get());
    }

    // 2. INSTANCIAS (Árboles, Rocas, Casas)
    context->RSSetState(m_states->CullNone()); 
    for (const auto& instance : m_worldInstances)
    {
        if (instance.baseModel)
        {
            instance.baseModel->SetWorldMatrix(instance.worldTransform);
            instance.baseModel->EvolvingDraw(
                context, viewMatrix, projectionMatrix,
                m_lightPropertiesCB.Get(), m_samplerState.Get(),
                m_lightViewMatrix, m_lightProjectionMatrix,
                m_shadowMapSRV.Get(), m_shadowSamplerState.Get()
            );
        }
    }

    // 3. JUGADOR (Caballero)
    
    if (m_currentModel && m_isThirdPerson)
    {
        context->OMSetBlendState(m_states->Opaque(), nullptr, 0xFFFFFFFF);
        context->OMSetDepthStencilState(m_states->DepthDefault(), 0);

        m_currentModel->SetPosition(m_playerPos);
        m_currentModel->SetRotationEuler(0.0f, m_camera->GetYaw(), 0.0f);

        m_currentModel->EvolvingDraw(
            context, viewMatrix, projectionMatrix,
            m_lightPropertiesCB.Get(), m_samplerState.Get(),
            m_lightViewMatrix, m_lightProjectionMatrix,
            m_shadowMapSRV.Get(), m_shadowSamplerState.Get()
        );
    }

    Model* toolToRender = nullptr;
    float toolScale = 1.0f; // Factor de escala por si la espada es más chica/grande que el hacha

    if (m_currentToolId == 1 && m_axe)
    {
        toolToRender = m_axe.get();
        toolScale = 10.0f; // Escala original de tu hacha
    }
    else if (m_currentToolId == 2 && m_sword) // Asumiendo que m_sword ya está cargado
    {
        toolToRender = m_sword.get(); // <--- Aquí usamos tu nuevo modelo
        toolScale = 10.0f; // Ajusta esto si la espada se ve muy grande o chica
    }

    // 2. Si hay algo que dibujar y no estamos talando/atacando...
    if (toolToRender && m_isThirdPerson && !m_isChopping)
    {
        // --- ANIMACIÓN DE REBOTE (BOBBING) ---
        // (Copiado de tu lógica original)
        float time = static_cast<float>(m_timer.GetTotalSeconds());
        float breatheBob = sin(time * 2.0f) * 0.10f;
        float walkBob = (m_walkBobTimer > 0.0f) ? sin(m_walkBobTimer) * 0.5f : 0.0f;

        // --- PARÁMETROS DE POSICIÓN (Los mismos para ambos) ---
        float offX = -0.5f;
        float offY = 18.2f;
        float offZ = 3.5f;

        // --- ROTACIÓN ---
        float degX_Idle = -160.0f;
        float degX_Walk = 190.0f;
        float degY = 0.0f;
        float degZ = 120.0f;

        float currentDegX = (m_walkBobTimer > 0.0f) ? degX_Walk : degX_Idle;

        // Conversión a radianes
        float rotX = DirectX::XMConvertToRadians(currentDegX);
        float rotY = DirectX::XMConvertToRadians(degY);
        float rotZ = DirectX::XMConvertToRadians(degZ);

        // Offset local final
        DirectX::SimpleMath::Vector3 offsetLocal(offX, offY + breatheBob + walkBob, offZ);

        // 3. CONSTRUCCIÓN DE MATRIZ Y DIBUJADO
        // Usamos 'toolToRender' en lugar de 'm_axe' explícitamente
        DirectX::SimpleMath::Matrix toolWorld =
            DirectX::SimpleMath::Matrix::CreateScale(toolScale) * DirectX::SimpleMath::Matrix::CreateRotationX(rotX) * DirectX::SimpleMath::Matrix::CreateRotationY(rotY) * DirectX::SimpleMath::Matrix::CreateRotationZ(rotZ) * DirectX::SimpleMath::Matrix::CreateTranslation(offsetLocal) * DirectX::SimpleMath::Matrix::CreateRotationY(m_camera->GetYaw()) * DirectX::SimpleMath::Matrix::CreateTranslation(m_playerPos);

        toolToRender->SetWorldMatrix(toolWorld);

        toolToRender->EvolvingDraw(context, viewMatrix, projectionMatrix,
            m_lightPropertiesCB.Get(), m_samplerState.Get(),
            m_lightViewMatrix, m_lightProjectionMatrix,
            m_shadowMapSRV.Get(), m_shadowSamplerState.Get());
    }

    context->RSSetState(m_states->CullCounterClockwise());
    

    // 4. DEBUG DE ITEMS 
    // DIBUJAR ITEMS CON SHADER MÍSTICO


    // En Game.cpp -> RenderScene

    for (const auto& item : m_gameItems)
    {
        if (item.isActive)
        {
            DirectX::SimpleMath::Matrix itemWorld;

            // --- TIPO 0: PIEZA MÍSTICA CRUDA (Cristal Flotante) ---
            if (item.type == 0 && m_crystal)
            {
                float rotY = time * 1.5f;
                float bobOffset = sin(time * 2.5f) * 0.25f;

                itemWorld =
                    DirectX::SimpleMath::Matrix::CreateScale(20.0f) *
                    DirectX::SimpleMath::Matrix::CreateRotationY(rotY) *
                    DirectX::SimpleMath::Matrix::CreateTranslation(item.position.x, item.position.y + 1.0f + bobOffset, item.position.z);

                if (m_mysticPS) {
                    m_crystal->SetWorldMatrix(itemWorld);
                    m_crystal->EvolvingDraw(context, viewMatrix, projectionMatrix,
                        m_lightPropertiesCB.Get(), m_samplerState.Get(),
                        m_lightViewMatrix, m_lightProjectionMatrix,
                        m_shadowMapSRV.Get(), m_shadowSamplerState.Get(),
                        m_mysticPS.Get());
                }
            }

            // --- TIPO 1: LEÑA / LOG (Modelo m_log en el suelo) ---
            else if (item.type == 1 && m_log)
            {

                itemWorld =
                    DirectX::SimpleMath::Matrix::CreateScale(1.0f) * // Ajusta escala si se ve muy grande/chico
                    DirectX::SimpleMath::Matrix::CreateRotationY(item.position.x) *
                    DirectX::SimpleMath::Matrix::CreateTranslation(item.position);

                m_log->SetWorldMatrix(itemWorld);
                m_log->EvolvingDraw(context, viewMatrix, projectionMatrix,
                    m_lightPropertiesCB.Get(), m_samplerState.Get(),
                    m_lightViewMatrix, m_lightProjectionMatrix,
                    m_shadowMapSRV.Get(), m_shadowSamplerState.Get());
            }

            // --- TIPO 2: RAMA (Modelo m_branch) ---
            else if (item.type == 2)
            {
                if (m_branch)
                {
                    itemWorld =
                        DirectX::SimpleMath::Matrix::CreateScale(10.0f) *
                        DirectX::SimpleMath::Matrix::CreateRotationY(time * 0.1f) * // Rotación lenta
                        DirectX::SimpleMath::Matrix::CreateTranslation(item.position);

                    m_branch->SetWorldMatrix(itemWorld);
                    m_branch->EvolvingDraw(context, viewMatrix, projectionMatrix,
                        m_lightPropertiesCB.Get(), m_samplerState.Get(),
                        m_lightViewMatrix, m_lightProjectionMatrix,
                        m_shadowMapSRV.Get(), m_shadowSamplerState.Get());
                }
                // Fallback a roca si no cargó la rama
                else if (m_rock1)
                {
                    itemWorld = DirectX::SimpleMath::Matrix::CreateScale(6.0f, 0.3f, 0.3f) *
                        DirectX::SimpleMath::Matrix::CreateTranslation(item.position);
                    m_rock1->SetWorldMatrix(itemWorld);
                    m_rock1->EvolvingDraw(context, viewMatrix, projectionMatrix, m_lightPropertiesCB.Get(), m_samplerState.Get(), m_lightViewMatrix, m_lightProjectionMatrix, m_shadowMapSRV.Get(), m_shadowSamplerState.Get());
                }
            }

            // --- TIPO 3: PIEDRA (Roca Pequeña) ---
            else if (item.type == 3 && m_rock1)
            {
                itemWorld =
                    DirectX::SimpleMath::Matrix::CreateScale(0.5f) *
                    DirectX::SimpleMath::Matrix::CreateTranslation(item.position);

                m_rock1->SetWorldMatrix(itemWorld);
                m_rock1->EvolvingDraw(context, viewMatrix, projectionMatrix,
                    m_lightPropertiesCB.Get(), m_samplerState.Get(),
                    m_lightViewMatrix, m_lightProjectionMatrix,
                    m_shadowMapSRV.Get(), m_shadowSamplerState.Get());
            }

            // --- TIPO 4: PIEZA MÍSTICA REFINADA (Salida del Horno) ---
            else if (item.type == 4 && m_crystal)
            {
                // Reusamos el cristal pero gira mucho más rápido para indicar "poder"
                float rotY = time * 5.0f;
                float bobOffset = sin(time * 5.0f) * 0.1f; // Vibración rápida

                itemWorld =
                    DirectX::SimpleMath::Matrix::CreateScale(15.0f) * 
                    DirectX::SimpleMath::Matrix::CreateRotationY(rotY) *
                    DirectX::SimpleMath::Matrix::CreateTranslation(item.position.x, item.position.y + bobOffset, item.position.z);

                if (m_mysticPS) {
                    m_crystal->SetWorldMatrix(itemWorld);
                    m_crystal->EvolvingDraw(context, viewMatrix, projectionMatrix,
                        m_lightPropertiesCB.Get(), m_samplerState.Get(),
                        m_lightViewMatrix, m_lightProjectionMatrix,
                        m_shadowMapSRV.Get(), m_shadowSamplerState.Get(),
                        m_mysticPS.Get());
                }
            }
        }
    }


    // 5. DEBUG COLISIONES (Opcional)
    if (m_drawDebugCollisions || m_inEditorMode)
    {
        m_deviceResources->PIXBeginEvent(L"Render Debug Collisions");

        context->RSSetState(m_states->Wireframe());

        if (m_cameraBoxToDraw.Extents.x > 0)
        {
            DirectX::SimpleMath::Matrix cameraBoxWorld =
                DirectX::SimpleMath::Matrix::CreateScale(m_cameraBoxToDraw.Extents.x * 2.0f,
                    m_cameraBoxToDraw.Extents.y * 2.0f,
                    m_cameraBoxToDraw.Extents.z * 2.0f) *
                DirectX::SimpleMath::Matrix::CreateTranslation(m_cameraBoxToDraw.Center);
            m_debugBoxDrawer->Draw(cameraBoxWorld, viewMatrix, projectionMatrix, DirectX::Colors::Yellow);
        }

        for (const auto& sphere : m_modelSpheresToDraw)
        {
            if (sphere.Radius > 0)
            {
                DirectX::SimpleMath::Matrix sphereWorld =
                    DirectX::SimpleMath::Matrix::CreateScale(sphere.Radius * 2.0f) * DirectX::SimpleMath::Matrix::CreateTranslation(sphere.Center);
                m_debugSphereDrawer->Draw(sphereWorld, viewMatrix, projectionMatrix, DirectX::Colors::Green);
            }
        }

        for (const auto& box : m_modelPartBoxesToDraw)
        {
            if (box.Extents.x > 0)
            {
                DirectX::SimpleMath::Matrix partBoxWorld =
                    DirectX::SimpleMath::Matrix::CreateScale(box.Extents.x * 2.0f,
                        box.Extents.y * 2.0f,
                        box.Extents.z * 2.0f) *
                    DirectX::SimpleMath::Matrix::CreateTranslation(box.Center);
                m_debugBoxDrawer->Draw(partBoxWorld, viewMatrix, projectionMatrix, DirectX::Colors::Red);
            }
        }

        for (const auto& box : m_customColliders)
        {
            DirectX::SimpleMath::Matrix world =
                DirectX::SimpleMath::Matrix::CreateScale(box.Extents * 2.0f) * DirectX::SimpleMath::Matrix::CreateTranslation(box.Center);

            m_debugBoxDrawer->Draw(world, viewMatrix, projectionMatrix, DirectX::Colors::Cyan);
        }

        DirectX::SimpleMath::Matrix triggerWorld =
            DirectX::SimpleMath::Matrix::CreateScale(m_anvilTriggerBox.Extents * 2.0f) * DirectX::SimpleMath::Matrix::CreateTranslation(m_anvilTriggerBox.Center);
        m_debugBoxDrawer->Draw(triggerWorld, viewMatrix, projectionMatrix, DirectX::Colors::Magenta);

        DirectX::SimpleMath::Matrix furnaceTriggerWorld =
            DirectX::SimpleMath::Matrix::CreateScale(m_furnaceTriggerBox.Extents * 2.0f) * DirectX::SimpleMath::Matrix::CreateTranslation(m_furnaceTriggerBox.Center);
        // Usamos Magenta también o Orange para diferenciarlo
        m_debugBoxDrawer->Draw(furnaceTriggerWorld, viewMatrix, projectionMatrix, DirectX::Colors::Orange);

        DirectX::SimpleMath::Matrix catacombTriggerWorld =
            DirectX::SimpleMath::Matrix::CreateScale(m_catacombsEntranceTrigger.Extents * 2.0f) * DirectX::SimpleMath::Matrix::CreateTranslation(m_catacombsEntranceTrigger.Center);
        m_debugBoxDrawer->Draw(catacombTriggerWorld, viewMatrix, projectionMatrix, DirectX::Colors::Red);

        // --- TRIGGER ALTAR FINAL (Azul) ---
        DirectX::SimpleMath::Matrix altarTriggerWorld =
            DirectX::SimpleMath::Matrix::CreateScale(m_dungeonAltarTrigger.Extents * 2.0f) * DirectX::SimpleMath::Matrix::CreateTranslation(m_dungeonAltarTrigger.Center);
        m_debugBoxDrawer->Draw(altarTriggerWorld, viewMatrix, projectionMatrix, DirectX::Colors::Blue);

        if (m_inEditorMode && !m_worldInstances.empty())
        {
            auto& inst = m_worldInstances[m_selectedInstanceIndex];
            Vector3 pos = inst.worldTransform.Translation();

            // 1. Obtener la escala actual del objeto (para saber si es gigante)
            Vector3 scale, t; Quaternion r;
            inst.worldTransform.Decompose(scale, r, t);
            float maxScale = std::max(scale.x, std::max(scale.y, scale.z));

            // 2. Obtener el radio original del modelo 3D
            float radius = 5.0f; // Valor por defecto si no tiene info
            if (inst.baseModel)
            {
                // Usamos el radio que calcula Assimp al cargar
                float modelRadius = inst.baseModel->GetOverallLocalBoundingSphere().Radius;
                if (modelRadius > 0.1f) radius = modelRadius;
            }

            // 3. Calcular tamaño final de la caja
            // (Radio * Escala del Objeto * 2.5 para que la caja quede holgada y visible)
            float finalBoxSize = radius * maxScale * 2.5f;

            // 4. DIBUJAR
            Matrix boxWorld = Matrix::CreateScale(finalBoxSize) * Matrix::CreateTranslation(pos);

            // Usamos CYAN (Azul Neón) que se ve muy bien sobre pasto y tierra
            m_debugBoxDrawer->Draw(boxWorld, viewMatrix, projectionMatrix, DirectX::Colors::Yellow);

            // Opcional: Dibujar una segunda caja más pequeña en AMARILLO en el centro exacto
            // para saber dónde está el punto de pivote
            Matrix pivotBox = Matrix::CreateScale(2.0f) * Matrix::CreateTranslation(pos);
            m_debugBoxDrawer->Draw(pivotBox, viewMatrix, projectionMatrix, DirectX::Colors::Yellow);
        }

        context->RSSetState(m_states->CullCounterClockwise());

        m_deviceResources->PIXEndEvent();
    }


    // 6. SKYBOX (Siempre al final de la escena opaca)
    if (m_skySphere && m_skyEffect)
    {
        Matrix skyWorld = Matrix::CreateScale(m_camera->GetFarPlane() * 0.9f) * Matrix::CreateTranslation(m_camera->GetPosition());
        m_skyEffect->SetWorld(skyWorld);
        m_skyEffect->SetView(viewMatrix);
        m_skyEffect->SetProjection(projectionMatrix);
        m_skyEffect->Apply(context);

        context->IASetInputLayout(m_skyInputLayout.Get());
        context->RSSetState(m_states->CullClockwise());
        context->OMSetDepthStencilState(m_skyDepthState.Get(), 0);
        m_skySphere->Draw(m_skyEffect.get(), m_skyInputLayout.Get());

        // Limpieza crítica del Skybox
        context->VSSetShader(nullptr, nullptr, 0);
        context->PSSetShader(nullptr, nullptr, 0);
        context->OMSetDepthStencilState(m_states->DepthDefault(), 0);
        context->RSSetState(m_states->CullCounterClockwise());
    }
}

void Game::RenderParticles()
{
    // 1. Si es de día o la textura no existe, no dibujamos nada.
    if (m_sunPower > 0.15f || !m_fireflyTexture) return;

    auto context = m_deviceResources->GetD3DDeviceContext();

    // 2. Configurar Shaders y Geometría
    context->VSSetShader(m_fireflyVS.Get(), nullptr, 0);
    context->PSSetShader(m_fireflyPS.Get(), nullptr, 0);
    context->IASetInputLayout(m_fireflyInputLayout.Get());

    UINT stride = sizeof(DirectX::VertexPositionTexture);
    UINT offset = 0;
    context->IASetVertexBuffers(0, 1, m_fireflyVertexBuffer.GetAddressOf(), &stride, &offset);
    context->IASetIndexBuffer(m_fireflyIndexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // 3. CONFIGURAR ESTADOS (Aditivo + Lectura de Profundidad)
    // ADITIVO: La luz se suma. Si esto se queda prendido, la pantalla se pone blanca después.
    context->OMSetBlendState(m_states->Additive(), nullptr, 0xFFFFFFFF);
    context->OMSetDepthStencilState(m_states->DepthRead(), 0);
    context->RSSetState(m_states->CullNone());

    // 4. Actualizar Datos Globales (Cámara)
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    if (SUCCEEDED(context->Map(m_cbFireflyPerFrame.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource)))
    {
        auto perFrameData = reinterpret_cast<CB_Firefly_PerFrame*>(mappedResource.pData);
        // Multiplicamos las matrices aquí para asegurar que el shader reciba la combinada
        perFrameData->ViewProjection = (m_camera->GetViewMatrix() * m_camera->GetProjectionMatrix()); // .Transpose() es vital si tus shaders usan column-major por defecto
        perFrameData->CameraRight_World = m_camera->GetRight();
        perFrameData->CameraUp_World = m_camera->GetUp();
        context->Unmap(m_cbFireflyPerFrame.Get(), 0);
    }
    context->VSSetConstantBuffers(0, 1, m_cbFireflyPerFrame.GetAddressOf());

    context->PSSetShaderResources(0, 1, m_fireflyTexture.GetAddressOf());
    context->PSSetSamplers(0, 1, m_samplerState.GetAddressOf());

    // 5. Dibujar cada luciérnaga
    for (const auto& firefly : m_fireflies)
    {
        if (SUCCEEDED(context->Map(m_cbFireflyPerParticle.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource)))
        {
            auto perParticleData = reinterpret_cast<CB_Firefly_PerParticle*>(mappedResource.pData);

            float blink = pow((sin(firefly.blinkTimer) + 1.0f) / 2.0f, 3.0f);

            perParticleData->ParticleCenter_World = firefly.position;
            perParticleData->ParticleColor = DirectX::SimpleMath::Vector4(1.5f, 2.0f, 1.0f, 1.0f) * blink;

            // --- TAMAÑO SEGURO ---
            // Usamos 0.05f (5 cm) para tu mundo escala 0.1f
            perParticleData->ParticleSize = DirectX::SimpleMath::Vector2(0.5f, 0.5f);

            context->Unmap(m_cbFireflyPerParticle.Get(), 0);
        }
        context->VSSetConstantBuffers(1, 1, m_cbFireflyPerParticle.GetAddressOf());

        context->DrawIndexed(6, 0, 0);
    }

    // 6. LIMPIEZA OBLIGATORIA (¡ESTO ES LO QUE FALTABA!)
    // Restauramos el modo de mezcla a OPACO para que no afecte a la UI ni al Bloom
    context->OMSetBlendState(m_states->Opaque(), nullptr, 0xFFFFFFFF);
    // Restauramos la escritura en profundidad
    context->OMSetDepthStencilState(m_states->DepthDefault(), 0);
    // Desvinculamos shaders para evitar conflictos
    context->VSSetShader(nullptr, nullptr, 0);
    context->PSSetShader(nullptr, nullptr, 0);
}

void Game::RenderInteractionBillboards()
{
    // --- 1. CONFIGURACIÓN DE PIPELINE ---
    // Usamos el shader de luciérnagas porque ya hace que el cuadro mire a la cámara.
    auto context = m_deviceResources->GetD3DDeviceContext();

    context->VSSetShader(m_fireflyVS.Get(), nullptr, 0);
    context->PSSetShader(m_fireflyPS.Get(), nullptr, 0);
    context->IASetInputLayout(m_fireflyInputLayout.Get());

    // Mezcla Alpha: Para que la parte transparente de los iconos (PNG) no se vea negra.
    context->OMSetBlendState(m_states->NonPremultiplied(), nullptr, 0xFFFFFFFF);

    // Profundidad: Read-Only. Se ocultan detrás de paredes, pero no tapan cosas detrás de ellos.
    context->OMSetDepthStencilState(m_states->DepthRead(), 0);

    // Configurar Geometría (Quad)
    UINT stride = sizeof(DirectX::VertexPositionTexture);
    UINT offset = 0;
    context->IASetVertexBuffers(0, 1, m_fireflyVertexBuffer.GetAddressOf(), &stride, &offset);
    context->IASetIndexBuffer(m_fireflyIndexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // Asegurar que la cámara esté actualizada (Buffer b0)
    // Asumimos que m_cbFireflyPerFrame ya tiene los datos de la cámara de este frame.
    context->VSSetConstantBuffers(0, 1, m_cbFireflyPerFrame.GetAddressOf());
    context->PSSetSamplers(0, 1, m_samplerState.GetAddressOf());

    D3D11_MAPPED_SUBRESOURCE mappedResource;

    // =================================================================================
    // PARTE A: ICONOS SOBRE LOS ITEMS TIRADOS
    // =================================================================================
    for (const auto& item : m_gameItems)
    {
        if (!item.isActive) continue; // Si ya lo recogiste, no dibujar

        // Distancia: Solo mostrar si estás a menos de 20 metros
        float dist = DirectX::SimpleMath::Vector3::Distance(m_playerPos, item.position);
        if (dist > 20.0f) continue;

        ID3D11ShaderResourceView* textureToDraw = nullptr;
        DirectX::SimpleMath::Vector4 colorTint = DirectX::SimpleMath::Vector4(1, 1, 1, 1);

        switch (item.type)
        {
        case 0: 
            textureToDraw = m_uiMysticTexture.Get();
            colorTint = DirectX::SimpleMath::Vector4(1.0f, 0.6f, 1.0f, 1.0f);
            break;

        case 1:
            textureToDraw = m_uiLogTexture.Get();
            colorTint = DirectX::SimpleMath::Vector4(1.0f, 1.0f, 1.0f, 1.0f);
            break;

        case 2: textureToDraw = m_uiAxeTexture.Get(); break; 
        case 3: textureToDraw = m_uiBranchTexture.Get(); break; 
        case 4: textureToDraw = m_uiRockTexture.Get(); break; 

        case 5: 
            textureToDraw = m_uiMysticCookedTexture.Get();
            colorTint = DirectX::SimpleMath::Vector4(1.0f, 0.9f, 0.5f, 1.0f);
            break;
        }

        // Si tenemos textura, dibujamos
        if (textureToDraw)
        {
            context->PSSetShaderResources(0, 1, &textureToDraw);

            // Efecto de flotación suave (Seno del tiempo)
            float bob = sin(static_cast<float>(m_timer.GetTotalSeconds()) * 2.5f) * 0.3f;

            if (SUCCEEDED(context->Map(m_cbFireflyPerParticle.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource)))
            {
                auto data = reinterpret_cast<CB_Firefly_PerParticle*>(mappedResource.pData);

                // --- TRUCO 1: ALTURA FIJA TIPO GENSHIN ---
                // En lugar de 2.0f, usamos 12.0f o 15.0f (ajústalo según la escala de tu caballero).
                // Queremos que el icono quede a la altura de los ojos del jugador, no en el suelo.
                float heightOffset = 12.0f;

                data->ParticleCenter_World = item.position + DirectX::SimpleMath::Vector3(0, heightOffset + bob, 0);

                // --- TRUCO 2: TAMAÑO CONSTANTE (Falso UI) ---
                // Hacemos que el icono crezca un poco si está lejos, para que no se haga diminuto.
                // dist es la distancia calculada arriba.
                float scaleFactor = 1.0f + (dist * 0.15f);
                float baseSize = 2.0f;

                data->ParticleSize = DirectX::SimpleMath::Vector2(baseSize * scaleFactor, baseSize * scaleFactor);

                // Color y Fade
                float alpha = std::min(1.0f, 1.5f - (dist / 20.0f));
                data->ParticleColor = colorTint * alpha;
                data->ParticleColor.w = alpha;

                context->Unmap(m_cbFireflyPerParticle.Get(), 0);
            }
            // Buffer b1 (Per Particle)
            context->VSSetConstantBuffers(1, 1, m_cbFireflyPerParticle.GetAddressOf());
            context->DrawIndexed(6, 0, 0);
        }
    }

    // =================================================================================
    // PARTE B: ICONO DEL YUNQUE (Usando el Trigger Box)
    // =================================================================================

    // 1. Obtenemos el centro exacto del Trigger del Yunque
    DirectX::SimpleMath::Vector3 anvilCenter = m_anvilTriggerBox.Center;

    // 2. Calculamos distancia al centro del trigger
    float distAnvil = DirectX::SimpleMath::Vector3::Distance(m_playerPos, anvilCenter);

    // Mostramos si estamos cerca (ej. 30 metros) y aún no tenemos el hacha
    if (distAnvil < 30.0f && !m_hasAxe)
    {
        context->PSSetShaderResources(0, 1, m_AnvilTexture.GetAddressOf());

        // Efecto de flotación para el yunque también
        float bob = sin(static_cast<float>(m_timer.GetTotalSeconds()) * 2.0f) * 0.5f;

        if (SUCCEEDED(context->Map(m_cbFireflyPerParticle.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource)))
        {
            auto data = reinterpret_cast<CB_Firefly_PerParticle*>(mappedResource.pData);

            // --- ALTURA ---
            // Usamos el centro del Trigger como base. 
            // Le sumamos una altura considerable (ej. 15.0f) para que flote sobre la maquinaria.
            float anvilHeightOffset = 19.0f;

            data->ParticleCenter_World = anvilCenter + DirectX::SimpleMath::Vector3(0, anvilHeightOffset + bob, 0);

            // --- TAMAÑO DINÁMICO (Genshin Style) ---
            // Hacemos que se vea grande a la distancia
            float scaleFactor = 1.0f + (distAnvil * 0.1f);
            float baseSize = 3.0f; // Icono base más grande que los items normales

            data->ParticleSize = DirectX::SimpleMath::Vector2(baseSize * scaleFactor, baseSize * scaleFactor);

            // --- FEEDBACK DE COLORES (Rojo/Verde) ---
            if (m_branchesCount >= 3 && m_stonesCount >= 3) // Usa tus constantes COST_
            {
                // Verde brillante si puedes craftear
                data->ParticleColor = DirectX::SimpleMath::Vector4(0.2f, 1.0f, 0.2f, 1.0f);
            }
            else
            {
                // Rojo si te faltan materiales
                data->ParticleColor = DirectX::SimpleMath::Vector4(1.0f, 0.0f, 0.0f, 0.8f);
            }

            context->Unmap(m_cbFireflyPerParticle.Get(), 0);
        }
        context->VSSetConstantBuffers(1, 1, m_cbFireflyPerParticle.GetAddressOf());
        context->DrawIndexed(6, 0, 0);
    }

    // --- LIMPIEZA DE ESTADOS ---
    context->OMSetBlendState(m_states->Opaque(), nullptr, 0xFFFFFFFF);
    context->OMSetDepthStencilState(m_states->DepthDefault(), 0);
}

void Game::RenderPostProcessing()
{
    auto context = m_deviceResources->GetD3DDeviceContext();
    auto finalRTV = m_deviceResources->GetRenderTargetView();
    const auto mainViewport = m_deviceResources->GetScreenViewport();

    ID3D11RenderTargetView* nullRTV = nullptr;
    context->OMSetRenderTargets(1, &nullRTV, nullptr);

    // 1. Extract Brightness
    context->OMSetRenderTargets(1, m_bloomExtractRTV.GetAddressOf(), nullptr);
    D3D11_VIEWPORT bloomViewport = { 0.0f, 0.0f, (float)mainViewport.Width / 2, (float)mainViewport.Height / 2, 0.0f, 1.0f };
    context->RSSetViewports(1, &bloomViewport);

    context->VSSetShader(m_fullscreenQuadVS.Get(), nullptr, 0);
    context->PSSetShader(m_bloomExtractPS.Get(), nullptr, 0);
    context->PSSetShaderResources(0, 1, m_sceneSRV.GetAddressOf());
    context->PSSetSamplers(0, 1, m_samplerState.GetAddressOf());

    // Update Params
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    context->Map(m_cbBloomParameters.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    memcpy(mappedResource.pData, &m_bloomParamsData, sizeof(CB_BloomParameters));
    context->Unmap(m_cbBloomParameters.Get(), 0);
    context->PSSetConstantBuffers(0, 1, m_cbBloomParameters.GetAddressOf());

    context->IASetInputLayout(nullptr);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->Draw(3, 0);

    // 2. Blur (Ping-Pong)
    m_blurParamsData.texelSize = { 1.0f / (mainViewport.Width / 2.0f), 1.0f / (mainViewport.Height / 2.0f) };
    context->Map(m_cbBlurParameters.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    memcpy(mappedResource.pData, &m_blurParamsData, sizeof(CB_BlurParameters));
    context->Unmap(m_cbBlurParameters.Get(), 0);
    context->PSSetConstantBuffers(0, 1, m_cbBlurParameters.GetAddressOf());

    for (int i = 0; i < 2; ++i)
    {
        // Horizontal
        context->OMSetRenderTargets(1, m_blurRTV.GetAddressOf(), nullptr);
        context->PSSetShader(m_gaussianBlurHorizontalPS.Get(), nullptr, 0);
        context->PSSetShaderResources(0, 1, m_bloomExtractSRV.GetAddressOf());
        context->Draw(3, 0);
        ID3D11ShaderResourceView* nullSRV[] = { nullptr };
        context->PSSetShaderResources(0, 1, nullSRV);

        // Vertical
        context->OMSetRenderTargets(1, m_bloomExtractRTV.GetAddressOf(), nullptr);
        context->PSSetShader(m_gaussianBlurVerticalPS.Get(), nullptr, 0);
        context->PSSetShaderResources(0, 1, m_blurSRV.GetAddressOf());
        context->Draw(3, 0);
        context->PSSetShaderResources(0, 1, nullSRV);
    }

    // 3. Composite to BackBuffer
    context->OMSetRenderTargets(1, &finalRTV, nullptr);
    context->RSSetViewports(1, &mainViewport);

    context->PSSetShader(m_bloomCompositePS.Get(), nullptr, 0);
    context->PSSetConstantBuffers(0, 1, m_cbBloomParameters.GetAddressOf());

    ID3D11ShaderResourceView* compositeSRVs[] = { m_sceneSRV.Get(), m_bloomExtractSRV.Get() };
    context->PSSetShaderResources(0, 2, compositeSRVs);
    context->Draw(3, 0);

    // Limpieza final
    ID3D11ShaderResourceView* nullSRVs[] = { nullptr, nullptr, nullptr, nullptr };
    context->PSSetShaderResources(0, 4, nullSRVs);
}

void Game::RenderUI()
{
    // 1. Configuración Inicial
    RECT outputSize = m_deviceResources->GetOutputSize();
    float screenW = float(outputSize.right - outputSize.left);
    float screenH = float(outputSize.bottom - outputSize.top);

    m_spriteBatchUI->Begin(SpriteSortMode_Deferred, m_states->NonPremultiplied());

    // ============================================================
    // 2. BARRA DE VIDA Y COORDENADAS
    // ============================================================
    if (m_blankTexture)
    {
        RECT bgRect = { 20, 20, 220, 45 };
        m_spriteBatchUI->Draw(m_blankTexture.Get(), bgRect, nullptr, Colors::Gray);
        RECT hpRect = { 22, 22, 218, 43 };
        m_spriteBatchUI->Draw(m_blankTexture.Get(), hpRect, nullptr, Colors::Crimson);
        m_font->DrawString(m_spriteBatchUI.get(), L"SALUD", Vector2(30, 25), Colors::White, 0, Vector2::Zero, 0.6f);
    }

    // Debug de coordenadas (útil para el Editor)
    if (m_camera)
    {
        wchar_t coordBuffer[256];
        auto pos = m_playerPos;
        swprintf_s(coordBuffer, L"POS: %.1f, %.1f, %.1f", pos.x, pos.y, pos.z);
        m_font->DrawString(m_spriteBatchUI.get(), coordBuffer, Vector2(22, 82), Colors::Black);
        m_font->DrawString(m_spriteBatchUI.get(), coordBuffer, Vector2(20, 80), Colors::Yellow);
    }

    // ============================================================
    // 3. BARRA DE INVENTARIO (ESTILO MINECRAFT)
    // ============================================================
    float slotSize = 60.0f;
    float padding = 10.0f;
    int totalSlots = 7;
    float totalWidth = (slotSize * totalSlots) + (padding * (totalSlots - 1));
    float startX = (screenW / 2.0f) - (totalWidth / 2.0f);
    float startY = screenH - 90.0f;

    auto DrawInventorySlot = [&](int index, ID3D11ShaderResourceView* icon, int count, bool isOwned)
        {
            float x = startX + index * (slotSize + padding);
            float y = startY;
            RECT slotRect = { (long)x, (long)y, (long)(x + slotSize), (long)(y + slotSize) };

            if (m_blankTexture)
                m_spriteBatchUI->Draw(m_blankTexture.Get(), slotRect, nullptr, DirectX::Colors::DarkSlateGray);

            if (icon)
            {
                Microsoft::WRL::ComPtr<ID3D11Resource> res; icon->GetResource(res.GetAddressOf());
                Microsoft::WRL::ComPtr<ID3D11Texture2D> tex; res.As(&tex);
                D3D11_TEXTURE2D_DESC desc; tex->GetDesc(&desc);
                Vector2 origin(desc.Width / 2.0f, desc.Height / 2.0f);
                float iconScale = (slotSize * 0.8f) / desc.Height;

                XMVECTOR iconColor = isOwned ? DirectX::Colors::White : DirectX::Colors::DimGray;
                if (!isOwned) iconColor = DirectX::SimpleMath::Color(0.3f, 0.3f, 0.3f, 0.5f);

                m_spriteBatchUI->Draw(icon, Vector2(x + slotSize / 2, y + slotSize / 2), nullptr, iconColor, 0.0f, origin, iconScale);
            }

            if (count > -1 && isOwned)
            {
                wchar_t numBuffer[16];
                swprintf_s(numBuffer, L"%d", count);
                Vector2 textSize = m_font->MeasureString(numBuffer) * 0.6f;
                Vector2 textPos = Vector2(x + slotSize - textSize.x - 5, y + slotSize - textSize.y - 2);
                m_font->DrawString(m_spriteBatchUI.get(), numBuffer, textPos + Vector2(2, 2), DirectX::Colors::Black, 0, Vector2::Zero, 0.6f);
                m_font->DrawString(m_spriteBatchUI.get(), numBuffer, textPos, DirectX::Colors::White, 0, Vector2::Zero, 0.6f);
            }
        };

    // Dibujar Slots
    DrawInventorySlot(0, m_uiMysticTexture.Get(), m_rawStonesCount, m_rawStonesCount > 0);
    DrawInventorySlot(1, m_uiBranchTexture.Get(), m_branchesCount, m_branchesCount > 0);
    DrawInventorySlot(2, m_uiRockTexture.Get(), m_stonesCount, m_stonesCount > 0);
    DrawInventorySlot(3, m_uiLogTexture.Get(), m_woodCount, m_woodCount > 0);
    DrawInventorySlot(4, m_uiAxeTexture.Get(), -1, m_hasAxe);
    DrawInventorySlot(5, m_uiMysticCookedTexture.Get(), m_refinedStonesCount, m_refinedStonesCount > 0);
    DrawInventorySlot(6, m_uiSwordTexture.Get(), -1, m_swordRepaired);

    // ============================================================
    // 4. HUD DE MISIONES
    // ============================================================
    wchar_t hudBuffer[512];
    if (m_gameWon) {
        swprintf_s(hudBuffer, L"VICTORIA! EL REINO ESTA A SALVO");
    }
    else if (m_gameLost) {
        swprintf_s(hudBuffer, L"DERROTA - TIEMPO AGOTADO");
    }
    else {
        swprintf_s(hudBuffer, L"TIEMPO: %.0f\n\nLena: %s\nEspada: %s",
            m_gameTimer, m_hasFirewood ? L"LISTO" : L"Falta", m_swordRepaired ? L"LISTO" : L"Rota");

        if (m_collectedPieces >= 5 && m_hasFirewood && !m_swordRepaired) wcscat_s(hudBuffer, L"\n\nVE A LA HERRERIA");
        else if (m_swordRepaired) wcscat_s(hudBuffer, L"\n\nVE A LAS CATACUMBAS");
    }

    Vector2 textSize = m_font->MeasureString(hudBuffer);
    Vector2 textPos = Vector2(screenW - textSize.x - 20, 20);
    m_font->DrawString(m_spriteBatchUI.get(), hudBuffer, textPos + Vector2(2, 2), Colors::Black);
    m_font->DrawString(m_spriteBatchUI.get(), hudBuffer, textPos, Colors::White);

    // ============================================================
    // 5. MINIMAPA
    // ============================================================
    long mapBottom = (long)screenH - 20;
    long mapTop = mapBottom - MINIMAP_SIZE;

    if (m_blankTexture) {
        RECT border = { 18, mapTop - 2, 22 + MINIMAP_SIZE, mapBottom + 2 };
        m_spriteBatchUI->Draw(m_blankTexture.Get(), border, nullptr, Colors::Gold);
    }
    RECT minimapRect = { 20, mapTop, 20 + MINIMAP_SIZE, mapBottom };
    m_spriteBatchUI->Draw(m_minimapSRV.Get(), minimapRect);

    if (m_playerIconTexture)
    {
        ComPtr<ID3D11Resource> res; m_playerIconTexture->GetResource(res.GetAddressOf());
        ComPtr<ID3D11Texture2D> tex; res.As(&tex);
        CD3D11_TEXTURE2D_DESC desc; tex->GetDesc(&desc);
        Vector2 origin(desc.Width / 2.f, desc.Height / 2.f);
        Vector2 pos(minimapRect.left + MINIMAP_SIZE / 2.f, minimapRect.top + MINIMAP_SIZE / 2.f);
        m_spriteBatchUI->Draw(m_playerIconTexture.Get(), pos, nullptr, Colors::White, -m_camera->GetYaw(), origin);
    }

    // ============================================================
    // 6. MENSAJES DE INTERACCIÓN (ITEMS, YUNQUE, HORNO, ÁRBOLES)
    // ============================================================

    const wchar_t* msg = nullptr;
    XMVECTOR msgColor = Colors::Yellow;

    // --- A. BUSCAR ITEMS EN EL SUELO (Prioridad Alta) ---
    for (const auto& item : m_gameItems)
    {
        if (item.isActive && Vector3::Distance(m_playerPos, item.position) < 10.0f)
        {
            switch (item.type)
            {
            case 0: msg = L"[E] RECOGER PIEZA MÍSTICA"; break;
            case 1: msg = L"[E] RECOGER LEÑA"; break;
            case 2: msg = L"[E] RECOGER HACHA"; break;
            case 3: msg = L"[E] RECOGER RAMA"; break;
            case 4: msg = L"[E] RECOGER PIEDRA"; break;
            case 5: msg = L"[E] RECOGER CRISTAL REFINADO"; break; // Agregado tipo 5
            default: msg = L"[E] RECOGER OBJETO"; break;
            }
            break;
        }
    }

    // --- B. YUNQUE (FORJA Y REPARACIÓN) ---
    if (msg == nullptr && m_anvilTriggerBox.Contains(m_playerPos))
    {
        if (!m_hasAxe)
        {
            if (m_branchesCount >= COST_BRANCHES && m_stonesCount >= COST_STONES)
            {
                msg = L"[E] FORJAR HACHA"; msgColor = Colors::Cyan;
            }
            else
            {
                msg = L"FALTAN MATERIALES (3 RAMAS, 3 PIEDRAS)"; msgColor = Colors::Gray;
            }
        }
        else if (!m_swordRepaired)
        {
            if (m_refinedStonesCount >= 5) // Corregido: Requiere cristales refinados, no collectedPieces
            {
                msg = L"[E] REPARAR ESPADA"; msgColor = Colors::Gold;
            }
            else
            {
                msg = L"NECESITAS 5 CRISTALES REFINADOS"; msgColor = Colors::Gray;
            }
        }
    }

    // --- C. HORNO (REFINAMIENTO) - ¡NUEVO! ---
    if (msg == nullptr && m_furnaceTriggerBox.Contains(m_playerPos))
    {
        if (m_isCooking)
        {
            msg = L"HORNO OPERANDO... ESPERA"; msgColor = Colors::Orange;
        }
        else
        {
            if (m_woodCount >= 2 && m_rawStonesCount >= 1)
            {
                msg = L"[E] REFINAR CRISTAL MÍSTICO"; msgColor = Colors::Gold;
            }
            else
            {
                msg = L"REQUIERE: 2 LEÑOS + 1 PIEZA CRUDA"; msgColor = Colors::Gray;
            }
        }
    }

    // --- D. ÁRBOLES (TALA) - ¡NUEVO! ---
    if (msg == nullptr && m_hasAxe)
    {
        for (const auto& instance : m_worldInstances)
        {
            // Filtro rápido: ¿Es un árbol?
            bool isTree = (instance.baseModel == m_green_tree1.get() ||
                instance.baseModel == m_forest_pine1.get() ||
                instance.baseModel == m_forest_pine2.get() ||
                instance.baseModel == m_forest_pine3.get());

            if (isTree)
            {
                // Distancia 2D
                float dx = m_playerPos.x - instance.worldTransform.Translation().x;
                float dz = m_playerPos.z - instance.worldTransform.Translation().z;
                float distTree = sqrt(dx * dx + dz * dz);

                if (distTree < 12.0f)
                {
                    msg = L"[CLICK IZQ] TALAR ÁRBOL";
                    msgColor = DirectX::Colors::LightGreen;
                    break;
                }
            }
        }
    }

    // --- DIBUJAR EL MENSAJE FINAL ---
    if (msg != nullptr)
    {
        Vector2 textDim = m_font->MeasureString(msg);
        Vector2 screenCenter(screenW / 2.0f, screenH / 2.0f);
        Vector2 pos = screenCenter + Vector2(0, 50.0f);

        m_font->DrawString(m_spriteBatchUI.get(), msg, pos + Vector2(2, 2), Colors::Black, 0, textDim / 2);
        m_font->DrawString(m_spriteBatchUI.get(), msg, pos, msgColor, 0, textDim / 2);
    }

    // ============================================================
    // 7. INDICADORES Y POPUPS
    // ============================================================

    // Indicador de Herramienta
    if (!m_hasAxe)
    {
        wchar_t matBuffer[128];
        swprintf_s(matBuffer, L"MATERIALES PARA HACHA:\nRamas: %d / %d\nPiedras: %d / %d",
            m_branchesCount, COST_BRANCHES, m_stonesCount, COST_STONES);

        Vector2 textSize = m_font->MeasureString(matBuffer);
        Vector2 textPos(screenW - textSize.x - 30, screenH - textSize.y - 30);

        m_font->DrawString(m_spriteBatchUI.get(), matBuffer, textPos + Vector2(2, 2), Colors::Black);
        m_font->DrawString(m_spriteBatchUI.get(), matBuffer, textPos, Colors::LightCyan);
    }
    else
    {
        const wchar_t* axeMsg = L"HERRAMIENTA: HACHA";
        Vector2 textSize = m_font->MeasureString(axeMsg);
        Vector2 textPos(screenW - textSize.x - 30, screenH - textSize.y - 30);
        m_font->DrawString(m_spriteBatchUI.get(), axeMsg, textPos + Vector2(2, 2), Colors::Black);
        m_font->DrawString(m_spriteBatchUI.get(), axeMsg, textPos, Colors::LightGreen);
    }

    // Debug
    if (!m_debugModelName.empty())
    {
        m_font->DrawString(m_spriteBatchUI.get(), m_debugModelName.c_str(),
            Vector2(50, 200), Colors::Red, 0.0f, Vector2::Zero, 1.5f);
    }

    // Popup de Obtención de Objeto
    if (m_itemPopupTimer > 0.0f)
    {
        const wchar_t* titleText = L"¡OBJETO OBTENIDO!";
        Vector2 titleSize = m_font->MeasureString(titleText);
        Vector2 centerScreen(screenW / 2.0f, screenH / 2.0f);
        float scalePulse = 1.0f + sin(m_timer.GetTotalSeconds() * 5.0f) * 0.05f;

        m_font->DrawString(m_spriteBatchUI.get(), titleText, centerScreen + Vector2(0, -180),
            Colors::Gold, 0.0f, titleSize / 2.0f, 1.2f * scalePulse);

        ID3D11ShaderResourceView* currentTexture = nullptr;
        const wchar_t* itemName = L"";

        if (m_popupItemType == 1) { currentTexture = m_uiAxeTexture.Get(); itemName = L"Hacha de Hierro"; }
        else if (m_popupItemType == 2) { currentTexture = m_uiSwordTexture.Get(); itemName = L"Espada Real"; }

        if (currentTexture)
        {
            ComPtr<ID3D11Resource> res; currentTexture->GetResource(res.GetAddressOf());
            ComPtr<ID3D11Texture2D> tex; res.As(&tex);
            CD3D11_TEXTURE2D_DESC desc; tex->GetDesc(&desc);
            Vector2 origin(desc.Width / 2.0f, desc.Height / 2.0f);
            float targetHeight = 300.0f;
            float scaleFactor = targetHeight / (float)desc.Height;

            m_spriteBatchUI->Draw(currentTexture, centerScreen, nullptr, Colors::White, 0.0f, origin, scaleFactor);

            Vector2 nameSize = m_font->MeasureString(itemName);
            float textOffsetY = (targetHeight / 2.0f) + 40.0f;
            m_font->DrawString(m_spriteBatchUI.get(), itemName, centerScreen + Vector2(0, textOffsetY), Colors::White, 0.0f, nameSize / 2.0f);
        }
    }

    // Pantalla de Victoria (Opcional: Pantalla Completa)
    if (m_gameWon)
    {
        const wchar_t* winText = L"¡VICTORIA!";
        Vector2 size = m_font->MeasureString(winText);
        m_font->DrawString(m_spriteBatchUI.get(), winText,
            Vector2(screenW / 2, screenH / 2), Colors::Gold, 0.f, size / 2, 2.0f);
    }

    // Info Editor
    if (m_inEditorMode && !m_worldInstances.empty())
    {
        auto& inst = m_worldInstances[m_selectedInstanceIndex];
        Vector3 pos = inst.worldTransform.Translation();
        wchar_t text[256];
        swprintf_s(text, L"[MODO EDITOR]\nObj: %d / %d\nPos: %.1f, %.1f, %.1f\n\nTAB: Cambiar Obj\nNumpad: Mover/Rotar\nENTER: Imprimir Código",
            m_selectedInstanceIndex, (int)m_worldInstances.size(), pos.x, pos.y, pos.z);
        m_font->DrawString(m_spriteBatchUI.get(), text, Vector2(20, 100), Colors::Yellow);
    }

    m_spriteBatchUI->End();
}

#pragma endregion

#pragma region Helpers

void Game::LoadAnimationSequence(
    std::vector<std::unique_ptr<Model>>& targetVector,
    std::string basePath,
    int count,
    ID3D11Device* device,
    ID3D11DeviceContext* context)
{
    const wchar_t* vsPath = L"C:EvolvingVS.cso";
    const wchar_t* psPath = L"C:EvolvingPS.cso";

    const wchar_t* tex1Path = L"GameAssets/models/knight/ID03_Base_color.png";
    const wchar_t* tex2Path = L"GameAssets/models/knight/ID02_Base_color.png";
    const wchar_t* tex3Path = L"GameAssets/models/knight/ID01_Base_color.png";
    const wchar_t* texAxePath = L"GameAssets/models/knight/Melee/Axe_BaseColor.png";

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv1, srv2, srv3, srvAxe;

    // Cargamos...
    DirectX::CreateWICTextureFromFile(device, tex1Path, nullptr, srv1.GetAddressOf());
    DirectX::CreateWICTextureFromFile(device, tex2Path, nullptr, srv2.GetAddressOf());
    DirectX::CreateWICTextureFromFile(device, tex3Path, nullptr, srv3.GetAddressOf());
    DirectX::CreateWICTextureFromFile(device, texAxePath, nullptr, srvAxe.GetAddressOf());

    // Creamos la lista para pasarla a los modelos
    // OJO: El orden importa. Asumimos que el material 0 es ID01, el 1 es ID02, etc.
    std::vector<ID3D11ShaderResourceView*> listNormal = {
        srv1.Get(),
        srv2.Get(),
        srv3.Get()
    };

    std::vector<ID3D11ShaderResourceView*> listMelee = {
        srv1.Get(),
        srv2.Get(),
        srv3.Get(),
    };

    bool isAttackAnim = (basePath.find("Melee") != std::string::npos) ||
        (basePath.find("Attack") != std::string::npos);

    // Log para confirmar
    wchar_t debugHeader[256];
    swprintf_s(debugHeader, L"--- CARGANDO: %S (%s) ---\n", basePath.c_str(), isAttackAnim ? L"CON HACHA" : L"NORMAL");
    OutputDebugString(debugHeader);

    for (int i = 0; i < count; ++i)
    {
        std::string filename = basePath + std::to_string(i) + ".obj";
        auto model = std::make_unique<Model>();

        if (model->Load(device, context, filename))
        {
            model->LoadEvolvingShaders(device, vsPath, psPath);
            model->SetScale(0.1f);

            if (isAttackAnim) {
                model->SetSharedTextures(listMelee);  
            }
            else {
                model->SetSharedTextures(listNormal); 
            }

            targetVector.push_back(std::move(model));
        }
        else
        {
            std::wstring wFilename(filename.begin(), filename.end());
            wchar_t errorMsg[256];
            swprintf_s(errorMsg, L"❌ FALLO: %s\n", wFilename.c_str());
            OutputDebugString(errorMsg);
        }
    }
}

#pragma endregion

#pragma region ParticleSystems

void Game::InitializeSmoke()
{
    m_smokeParticles.resize(NUM_SMOKE_PARTICLES);
    for (auto& p : m_smokeParticles)
    {
        p.lifetime = -1.0f * ((float)rand() / RAND_MAX) * 5.0f; 
        p.position = m_chimneyPos;
    }
}

void Game::UpdateSmoke(float elapsedTime)
{
    for (auto& p : m_smokeParticles)
    {
        p.lifetime -= elapsedTime;

        if (p.lifetime <= 0.0f)
        {
            // --- REINICIO (RESPAWN) ---

            float rX = ((float)rand() / RAND_MAX) * 2.0f - 1.0f;
            float rZ = ((float)rand() / RAND_MAX) * 2.0f - 1.0f;

            // 1. POSICIÓN: EXPANDIMOS EL AREA
            // -104.4f, 18.0f, 568.1f es tu centro.
            // Multiplicamos rX por 12.0f para hacerlo ANCHO.
            // Multiplicamos rZ por 6.0f para darle PROFUNDIDAD.
            p.position = DirectX::SimpleMath::Vector3(-104.4f, 18.0f, 568.1f) +
                DirectX::SimpleMath::Vector3(rX * 12.0f, 0.0f, rZ * 6.0f);

            // 2. VELOCIDAD
            // Le damos un poco más de movimiento lateral aleatorio (rX * 1.5f)
            // Y mantenemos que suba hacia la chimenea (Y = 4.0f)
            p.velocity = DirectX::SimpleMath::Vector3(rX * 1.5f, 4.0f, 2.0f);

            // 3. VIDA Y APARIENCIA
            p.maxLifetime = 2.0f + ((float)rand() / RAND_MAX);
            p.lifetime = p.maxLifetime;
            p.scale = 0.8f; // Empieza pequeño
            p.alpha = 1.0f;
        }
        else
        {
            // --- ACTUALIZACIÓN ---
            p.position += p.velocity * elapsedTime;

            // CAMBIO AQUÍ:
            // Aumentamos la velocidad de crecimiento a 4.0f (antes era 1.5f)
            // Esto hace que el humo se "abra" más rápido y se vea más suave.
            p.scale += 4.0f * elapsedTime;

            p.alpha = (p.lifetime / p.maxLifetime); // Se desvanece
        }
    }
}

void Game::RenderSmoke()
{
    if (!m_smokeTexture) return;

    auto context = m_deviceResources->GetD3DDeviceContext();

    // 1. Configuración Básica
    context->VSSetShader(m_fireflyVS.Get(), nullptr, 0);
    context->PSSetShader(m_fireflyPS.Get(), nullptr, 0);
    context->IASetInputLayout(m_fireflyInputLayout.Get());

    UINT stride = sizeof(DirectX::VertexPositionTexture);
    UINT offset = 0;
    context->IASetVertexBuffers(0, 1, m_fireflyVertexBuffer.GetAddressOf(), &stride, &offset);
    context->IASetIndexBuffer(m_fireflyIndexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // 2. ESTADOS: AlphaBlend para humo oscuro
    context->OMSetBlendState(m_states->AlphaBlend(), nullptr, 0xFFFFFFFF);
    context->OMSetDepthStencilState(m_states->DepthRead(), 0);
    context->RSSetState(m_states->CullNone());

    // 3. ACTUALIZAR CÁMARA (SIN TRANSPOSE - CORRECCIÓN IMPORTANTE)
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    if (SUCCEEDED(context->Map(m_cbFireflyPerFrame.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource)))
    {
        auto perFrameData = reinterpret_cast<CB_Firefly_PerFrame*>(mappedResource.pData);

        // IMPORTANTE: Quitamos el .Transpose() para que coincida con tu sistema
        perFrameData->ViewProjection = (m_camera->GetViewMatrix() * m_camera->GetProjectionMatrix());
        perFrameData->CameraRight_World = m_camera->GetRight();
        perFrameData->CameraUp_World = m_camera->GetUp();

        context->Unmap(m_cbFireflyPerFrame.Get(), 0);
    }
    context->VSSetConstantBuffers(0, 1, m_cbFireflyPerFrame.GetAddressOf());

    // 4. Textura
    context->PSSetShaderResources(0, 1, m_smokeTexture.GetAddressOf());
    context->PSSetSamplers(0, 1, m_samplerState.GetAddressOf());

    // 5. DIBUJAR (Color Oscuro)
    for (const auto& p : m_smokeParticles)
    {
        if (p.lifetime <= 0) continue;

        if (SUCCEEDED(context->Map(m_cbFireflyPerParticle.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource)))
        {
            auto data = reinterpret_cast<CB_Firefly_PerParticle*>(mappedResource.pData);
            data->ParticleCenter_World = p.position;

            // Color: Gris Oscuro (0.2) + Alpha
            data->ParticleColor = DirectX::SimpleMath::Vector4(0.4f, 0.4f, 0.4f, p.alpha * 0.2f);

            // Tamaño
            data->ParticleSize = DirectX::SimpleMath::Vector2(5.0f * p.scale, 5.0f * p.scale);

            context->Unmap(m_cbFireflyPerParticle.Get(), 0);
        }
        context->VSSetConstantBuffers(1, 1, m_cbFireflyPerParticle.GetAddressOf());
        context->DrawIndexed(6, 0, 0);
    }

    // 6. Restaurar
    context->OMSetBlendState(m_states->Opaque(), nullptr, 0xFFFFFFFF);
    context->OMSetDepthStencilState(m_states->DepthDefault(), 0);
}

#pragma endregion

// HotReload Helpers
void Game::BuildCustomColliders() {

    m_customColliders.clear();

    // Anvil position
    DirectX::SimpleMath::Vector3 commonCenter(-108.1f, 3.6f, 505.0f);

    // Anvil trigger box
    DirectX::SimpleMath::Vector3 triggerExtents(15.0f, 15.0f, 15.0f);
    m_anvilTriggerBox = DirectX::BoundingBox(commonCenter, triggerExtents);

    // Anvil collision box
    DirectX::SimpleMath::Vector3 colliderExtents(6.0f, 15.0f, 6.0f);
    DirectX::BoundingBox anvilColliderTemp(commonCenter, colliderExtents);

    m_customColliders.push_back(anvilColliderTemp);

    // Furnace 

    DirectX::SimpleMath::Vector3 furnaceCenter(-101.7f, 2.5f, 561.1f);
    DirectX::SimpleMath::Vector3 furnaceExtents(16.0f, 20.0f, 10.5f);

    m_customColliders.push_back(DirectX::BoundingBox(furnaceCenter, furnaceExtents));

    // Furnace trigger box

    DirectX::SimpleMath::Vector3 furnacetgCenter(-101.1f, 2.7f, 560.0f);
    DirectX::SimpleMath::Vector3 furnacetgExtents(16.0f, 25.0f, 15.0f);

    m_furnaceTriggerBox = DirectX::BoundingBox(furnacetgCenter, furnacetgExtents);

    // Furnace item

    DirectX::SimpleMath::Vector3 furnace02Center(-140.3f, 2.5f, 567.4f);
    DirectX::SimpleMath::Vector3 furnace02Extents(12.0f, 25.0f, 12.5f);

    m_customColliders.push_back(DirectX::BoundingBox(furnace02Center, furnace02Extents));

    // Worktable
    DirectX::SimpleMath::Vector3 tableCenter(-60.5f, 2.0f, 522.0f);
    DirectX::SimpleMath::Vector3 tableExtents(7.0f, 20.0f, 15.5f);

    m_customColliders.push_back(DirectX::BoundingBox(tableCenter, tableExtents));

    // Wall worktable

    DirectX::SimpleMath::Vector3 wall01Center(-44.8f, 1.6f, 518.6f);
    DirectX::SimpleMath::Vector3 wall01Extents(4.5f, 30.0f, 65.5f);

    m_customColliders.push_back(DirectX::BoundingBox(wall01Center, wall01Extents));

    // Chest

    DirectX::SimpleMath::Vector3 chestCenter(-55.8f, 6.0f, 476.5f);
    DirectX::SimpleMath::Vector3 chestExtents(8.0f, 12.0f, 15.0f);

    m_customColliders.push_back(DirectX::BoundingBox(chestCenter, chestExtents));

    // Wall Furnace

    DirectX::SimpleMath::Vector3 wall02Center(-87.8f, 2.2f, 586.2f);
    DirectX::SimpleMath::Vector3 wall02Extents(45.0f, 30.0f, 5.0f);

    m_customColliders.push_back(DirectX::BoundingBox(wall02Center, wall02Extents));

    // Column 01

    DirectX::SimpleMath::Vector3 column01Center(-134.0f, 2.2f, 532.0f);
    DirectX::SimpleMath::Vector3 column01Extents(12.0f, 30.0f, 5.0f);

    m_customColliders.push_back(DirectX::BoundingBox(column01Center, column01Extents));

    // Column 02

    DirectX::SimpleMath::Vector3 column02Center(-126.5f, 2.2f, 456.2f);
    DirectX::SimpleMath::Vector3 column02Extents(5.0f, 30.0f, 5.0f);

    m_customColliders.push_back(DirectX::BoundingBox(column02Center, column02Extents));

    // Charcoal boxes - aligned Column02

    DirectX::SimpleMath::Vector3 charcoal01Center(-142.0f, 2.2f, 454.9f);
    DirectX::SimpleMath::Vector3 charcoal01Extents(12.0f, 20.0f, 12.0f);

    m_customColliders.push_back(DirectX::BoundingBox(charcoal01Center, charcoal01Extents));

    // Charcoal boxes - aligned furnance 1

    DirectX::SimpleMath::Vector3 charcoal02Center(-64.0f, 2.2f, 549.8f);
    DirectX::SimpleMath::Vector3 charcoal02Extents(5.0f, 6.0f, 10.0f);

    m_customColliders.push_back(DirectX::BoundingBox(charcoal02Center, charcoal02Extents));

    DirectX::SimpleMath::Vector3 charcoal03Center(-69.6f, 2.2f, 571.8f);
    DirectX::SimpleMath::Vector3 charcoal03Extents(10.0f, 6.0f, 7.0f);

    m_customColliders.push_back(DirectX::BoundingBox(charcoal03Center, charcoal03Extents));

    // Blacksmith room

    DirectX::SimpleMath::Vector3 roomCenter(-226.0f, 2.2f, 560.0f);
    DirectX::SimpleMath::Vector3 roomExtents(35.0f, 30.0f, 35.0f);

    m_customColliders.push_back(DirectX::BoundingBox(roomCenter, roomExtents));

    // Blacksmith room boxes

    DirectX::SimpleMath::Vector3 boxes01Center(-186.7f, 2.2f, 530.9f);
    DirectX::SimpleMath::Vector3 boxes01Extents(10.0f, 15.0f, 10.0f);

    m_customColliders.push_back(DirectX::BoundingBox(boxes01Center, boxes01Extents));

    DirectX::SimpleMath::Vector3 boxes02Center(-179.2f, 2.2f, 553.8f);
    DirectX::SimpleMath::Vector3 boxes02Extents(8.0f, 15.0f, 8.0f);

    m_customColliders.push_back(DirectX::BoundingBox(boxes02Center, boxes02Extents));

    // DungeonEntranceGate trigger Box

    DirectX::SimpleMath::Vector3 catacombsentranceCenter(258.3f, 2.5f, -659.7f);
    DirectX::SimpleMath::Vector3 catacombsentranceExtents(30.0f, 60.0f, 30.0f);

    m_catacombsEntranceTrigger = DirectX::BoundingBox(catacombsentranceCenter, catacombsentranceExtents);

    // DungeonAltarTrigger

    DirectX::SimpleMath::Vector3 AltarCenter(165.0f, 0.0f, 3690.4f);
    DirectX::SimpleMath::Vector3 AltarExtents(15.0f, 20.0f, 15.0f);

    m_dungeonAltarTrigger = DirectX::BoundingBox(AltarCenter, AltarExtents);

    m_dungeonSpawnPos = DirectX::SimpleMath::Vector3(595.2f, 0.0f, 4005.5f);


    OutputDebugString(L"--- ¡Cajas y Triggers Actualizados! ---\n");

}


// EditorMode

void Game::UpdateEditor(float elapsedTime)
{
    // Toggle Editor con F1
    if (m_kbTracker.pressed.F1)
    {
        m_inEditorMode = !m_inEditorMode;

        // Si acabamos de APAGAR el editor (volvemos a jugar)
        if (!m_inEditorMode)
        {
            // 1. Traer al personaje a la posición de la cámara
            m_playerPos = m_camera->GetPosition();

            // 2. Ajustar altura para no caer al vacío o quedar enterrado
            // (Usamos tu función de terreno si estás sobre terreno, o un valor fijo si estás en la mazmorra)
            if (m_terrain)
            {
                float terrainH = 0.0f;
                // Si hay terreno abajo, nos pegamos al suelo
                if (m_terrain->GetWorldHeightAt(m_playerPos.x, m_playerPos.z, terrainH))
                {
                    m_playerPos.y = terrainH;
                }
                // Si NO hay terreno (ej. estás en la zona de la mazmorra que es piso plano), 
                // dejamos la Y como está o la ponemos en 0.0f
                else
                {
                    m_playerPos.y = 0.0f; // O la altura del piso de tu mazmorra
                }
            }
        }

        m_currentSpeed = m_inEditorMode ? 0.0f : m_normalSpeed;
    }

    if (!m_inEditorMode || m_worldInstances.empty()) return;

    // --- 1. SELECCIONAR OBJETO (TAB / Backspace) ---
    if (m_kbTracker.pressed.Tab) {
        m_selectedInstanceIndex = (m_selectedInstanceIndex + 1) % m_worldInstances.size();
    }
    if (m_kbTracker.pressed.Back) {
        m_selectedInstanceIndex--;
        if (m_selectedInstanceIndex < 0) m_selectedInstanceIndex = m_worldInstances.size() - 1;
    }

    // Obtenemos referencia a la matriz del objeto actual
    auto& transform = m_worldInstances[m_selectedInstanceIndex].worldTransform;

    // Descomponemos la matriz para modificarla fácilmente
    Vector3 scale, pos;
    Quaternion rot;
    transform.Decompose(scale, rot, pos);

    // --- 2. MOVER (Numpad 8,4,5,6 y 7,9 Altura) ---
    float speed = m_editorMoveSpeed * elapsedTime;
    if (m_kbState.LeftShift) speed *= 3.0f; // Shift para ir rápido

    if (m_kbState.NumPad8) pos.z += speed; // Norte
    if (m_kbState.NumPad2) pos.z -= speed; // Sur
    if (m_kbState.NumPad4) pos.x -= speed; // Oeste
    if (m_kbState.NumPad6) pos.x += speed; // Este
    if (m_kbState.NumPad9) pos.y += speed; // Arriba
    if (m_kbState.NumPad7) pos.y -= speed; // Abajo

    // --- 3. ROTAR (Numpad 1 y 3) ---
    float rotSpeed = m_editorRotSpeed * elapsedTime;
    if (m_kbState.NumPad1) rot *= Quaternion::CreateFromAxisAngle(Vector3::Up, -rotSpeed);
    if (m_kbState.NumPad3) rot *= Quaternion::CreateFromAxisAngle(Vector3::Up, rotSpeed);

    // --- 4. ESCALAR (Numpad + y -) ---
    if (m_kbState.Add)      scale += Vector3(elapsedTime);
    if (m_kbState.Subtract) scale -= Vector3(elapsedTime);

    // Reconstruir la matriz
    transform = Matrix::CreateScale(scale) * Matrix::CreateFromQuaternion(rot) * Matrix::CreateTranslation(pos);

    // --- 5. IMPRIMIR CÓDIGO (ENTER) ---
    if (m_kbTracker.pressed.Enter)
    {
        // Convertir rotación a Euler para que sea legible (aproximado)
        Vector3 euler = rot.ToEuler(); // Esto devuelve radianes

        wchar_t buffer[512];
        swprintf_s(buffer,
            L"\n\n// >>> COPIAR ESTO >>>\n"
            L"// Objeto Indice: %d\n"
            L"AddInstancedObject(MODELO_PTR, baseTransform, %.2ff, %.2ff, %.2ff, 0.0f);\n"
            L"// Escala: %.2f | Rotacion aprox (Y): %.2f\n\n",
            m_selectedInstanceIndex,
            pos.x, pos.z, pos.y, // X, Z, Y (Como tu funcion pide X, Z, Y)
            scale.x, euler.y
        );
        OutputDebugString(buffer);
    }
}