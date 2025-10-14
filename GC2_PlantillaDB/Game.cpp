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
    m_dayNightCycleSpeed(0.003f),
    m_sunPower(0.0f)
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
    auto context = m_deviceResources->GetD3DDeviceContext();

    // TODO: Add your game logic here.

    UpdateDayNightCycle(elapsedTime);
    UpdateFireflies(elapsedTime);

    if (m_keyboard)
    {
        m_kbState = m_keyboard->GetState();
        m_kbTracker.Update(m_kbState);
    }

    if (m_mouse)
    {
        m_mouseState = m_mouse->GetState();
        m_mouseTracker.Update(m_mouseState);
    }


    if (m_kbState.Escape)
    {
        ExitGame();
    }

    bool wKeyIsCurrentlyPressed = m_kbState.W;


    if (m_wTapTimer > 0.0f)
    {
        m_wTapTimer -= elapsedTime;
        if (m_wTapTimer <= 0.0f)
        {
            m_wTapCount = 0;
            m_wTapTimer = 0.0f;
        }
    }

    
    if (m_kbTracker.pressed.W)
    {
        if (m_wTapCount == 1 && m_wTapTimer > 0.0f) 
        {
            m_isSprinting = true;
            m_wTapCount = 0;       
            m_wTapTimer = 0.0f;
        }
        else 
        {
            m_wTapCount = 1;
            m_wTapTimer = m_doubleTapTimeLimit; 
           
        }
    }

    
    if (!wKeyIsCurrentlyPressed) 
    {
        m_isSprinting = false;

    }

    if (m_isSprinting && (m_kbState.A || m_kbState.S || m_kbState.D)) {
        m_isSprinting = false;
        m_wTapCount = 0;
        m_wTapTimer = 0.0f;
    }

    if (m_isSprinting && wKeyIsCurrentlyPressed)
    {
        m_currentSpeed = m_sprintSpeed;
    }
    else
    {
        m_isSprinting = false;
        m_currentSpeed = m_normalSpeed;
    }

    if (m_camera)
    {
        float rotateSpeed = 2.0f;
        float yawDelta = 0.0f;
        float pitchDelta = 0.0f;

        // Procesar entrada del ratón para rotación
        if (m_mouse) {
            if (m_mouseState.leftButton) {
                m_mouse->SetMode(DirectX::Mouse::MODE_RELATIVE);
            }
            if (m_mouse->GetState().positionMode == DirectX::Mouse::MODE_RELATIVE) {
                yawDelta = -static_cast<float>(m_mouseState.x) * 0.001f;
                pitchDelta = -static_cast<float>(m_mouseState.y) * 0.001f;
            }
        }

        // Aplicar rotación a la cámara INMEDIATAMENTE
        if (yawDelta != 0.0f || pitchDelta != 0.0f) {
            m_camera->Rotate(yawDelta * rotateSpeed, pitchDelta * rotateSpeed);
        }

        // Obtener la posición actual ANTES de calcular el movimiento
        DirectX::SimpleMath::Vector3 currentCamPos = m_camera->GetPosition();
        DirectX::SimpleMath::Vector3 intendedMovementVector = DirectX::SimpleMath::Vector3::Zero;

        DirectX::SimpleMath::Vector3 relativeMovement = DirectX::SimpleMath::Vector3::Zero;
        if (wKeyIsCurrentlyPressed || m_kbState.Up) relativeMovement.z -= 1.f; 
        if (m_kbState.S || m_kbState.Down) relativeMovement.z += 1.f;     
        if (m_kbState.A || m_kbState.Left) relativeMovement.x -= 1.f;
        if (m_kbState.D || m_kbState.Right) relativeMovement.x += 1.f;

        if (m_kbState.Space) relativeMovement.y += 1.f;
        if (m_kbState.X || m_kbState.LeftControl || m_kbState.RightControl) relativeMovement.y -= 1.f;

        // Calcular el desplazamiento deseado para este frame
        if (relativeMovement.LengthSquared() > 0.0f) {
            // Transformar el movimiento relativo a coordenadas del mundo basado en la orientación actual de la cámara
            DirectX::SimpleMath::Matrix camRotationMatrix = DirectX::SimpleMath:: Matrix::CreateFromQuaternion(m_camera->GetRotation());
            intendedMovementVector = DirectX::SimpleMath::Vector3::TransformNormal(relativeMovement, camRotationMatrix);
            intendedMovementVector.Normalize(); // Asegurar que sea una dirección
            intendedMovementVector *= m_currentSpeed * elapsedTime; // Aplicar velocidad y tiempo
        }

        DirectX::SimpleMath::Vector3 nextCamPos = currentCamPos + intendedMovementVector;

        // --- Detección y Respuesta a la Colisión con Modelos ---
        bool collisionHappened = false; // Inicializar antes de las comprobaciones

        if (intendedMovementVector.LengthSquared() > 0.0f) // Solo checar colisión si hay intento de movimiento
        {
            float cameraHeight = 1.8f;
            float cameraRadius = 0.4f;
            DirectX::BoundingBox cameraFutureBox(nextCamPos, DirectX::SimpleMath::Vector3(cameraRadius, cameraHeight / 2.0f, cameraRadius));

            if (m_drawDebugCollisions) {
                m_cameraBoxToDraw = cameraFutureBox;
                m_modelPartBoxesToDraw.clear();
            }

            for (const auto& instance : m_worldInstances)
            {
                if (!instance.baseModel) continue;

                DirectX::BoundingSphere localSphere = instance.baseModel->GetOverallLocalBoundingSphere();
                DirectX::BoundingSphere instanceWorldSphere;

                localSphere.Transform(instanceWorldSphere, instance.worldTransform);

                // Opcional: si quieres seguir viendo las esferas de debug que se calculan aquí en Update
                // if (m_drawDebugCollisions) {
                //     m_modelSpheresToDraw.push_back(instanceWorldSphere); // PERO RECUERDA que m_modelSpheresToDraw se limpia y llena en Render
                // }                                                     // para el dibujado final. Esto sería para un debug más inmediato en Update.

                if (cameraFutureBox.Intersects(instanceWorldSphere))
                {

                    if (instance.baseModel->CheckCollisionAgainstParts(cameraFutureBox,
                        instance.worldTransform, 
                        m_modelPartBoxesToDraw,
                        m_drawDebugCollisions))
                    {
                        collisionHappened = true;
                        wchar_t msg[128];
                        swprintf_s(msg, L"COLISIÓN DETECTADA (Update) con instancia de modelo %p!\n", instance.baseModel);
                        OutputDebugString(msg);
                        break;
                    }
                }
            }
            // --- FIN DEL NUEVO BUCLE DE COLISIÓN ---

            // El antiguo bucle "for (Model* model : collidableModels)" DEBE SER ELIMINADO
            // o modificado para que solo maneje objetos que NO estén en m_worldInstances.
            // Si todos tus objetos colisionables están ahora en m_worldInstances, puedes eliminar ese viejo bucle.
        }

        // Aplicar movimiento final o revertir
        if (collisionHappened) {
            // No mover la cámara, se queda en currentCamPos
        }
        else {
            m_camera->SetPosition(nextCamPos);
        }

        // --- Ajuste de Altura del Terreno (después de colisiones con modelos) ---
        DirectX::SimpleMath::Vector3 finalAttemptCamPos = m_camera->GetPosition();
        float terrainHeight;
        if (m_terrain && m_terrain->GetWorldHeightAt(finalAttemptCamPos.x, finalAttemptCamPos.z, terrainHeight)) {

            // Esta es la altura de los "ojos" sobre el suelo. Un valor como 2.0f es ms realista.
            float cameraHeightAboveTerrain = 15.0f;

            // LA LÓGICA CORRECTA: Para estar SOBRE el terreno, SUMAMOS la altura.
            m_camera->SetPosition(DirectX::SimpleMath::Vector3(finalAttemptCamPos.x, terrainHeight + cameraHeightAboveTerrain, finalAttemptCamPos.z));
        }
        else {
            // Fuera del terreno, podras mantener la Y o aplicar lgica de cada/lmite
        }

        m_camera->UpdateViewMatrix(); // Actualizar la matriz de vista UNA VEZ después de todos los ajustes de posición y rotación
    }



    if (m_camera && m_lightPropertiesCB)
    {
        m_lightData.cameraPositionWorld = m_camera->GetPosition();
        // Actualizar el constant buffer de luces
        D3D11_MAPPED_SUBRESOURCE mappedResource;
        HRESULT hr = context->Map(m_lightPropertiesCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
        if (SUCCEEDED(hr))
        {
            memcpy(mappedResource.pData, &m_lightData, sizeof(PSLightPropertiesData));
            context->Unmap(m_lightPropertiesCB.Get(), 0);
        }
        else
        {
            OutputDebugString(L"Failed to map light properties constant buffer.\n");
        }
    }

    if (m_terrain) {
        // El vector de luz en m_lightData (directionalLightVector) es HACIA la luz.
        // BasicEffect::SetLightDirection también espera un vector HACIA la luz.
        m_terrain->UpdateGlobalLighting(
            m_lightData.directionalLightVector,
            m_lightData.directionalLightColor,
            m_lightData.ambientLightColor,
            m_lightData.directionalLightColor // O un color blanco/gris si quieres que el brillo especular de la luz sea incoloro
            // por ejemplo: DirectX::Colors::White * 0.7f
        );
    }

    elapsedTime;
}
#pragma endregion

#pragma region Frame Render
// Draws the scene.
void Game::Render()
{
    // No intentar renderizar antes de la primera actualización.
    if (m_timer.GetFrameCount() == 0)
    {
        return;
    }

    auto context = m_deviceResources->GetD3DDeviceContext();
    auto finalRTV = m_deviceResources->GetRenderTargetView();
    auto depthStencil = m_deviceResources->GetDepthStencilView();
    const auto mainViewport = m_deviceResources->GetScreenViewport();

    // --- Pases de pre-renderizado (sombras, minimapa) ---
    RenderShadowPass();
    RenderMinimapPass();

    // ====================================================================
    // PASO 1: RENDERIZAR TODA LA ESCENA 3D A UNA TEXTURA FUERA DE PANTALLA
    // ====================================================================
    m_deviceResources->PIXBeginEvent(L"1. Render 3D Scene to Texture");

    // Establecer nuestro RTV de escena como el objetivo de renderizado
    context->OMSetRenderTargets(1, m_sceneRTV.GetAddressOf(), depthStencil);
    context->ClearRenderTargetView(m_sceneRTV.Get(), DirectX::Colors::CornflowerBlue);
    context->ClearDepthStencilView(depthStencil, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
    context->RSSetViewports(1, &mainViewport);

    // --- Renderizado principal de la escena ---
    DirectX::SimpleMath::Matrix viewMatrix = m_camera->GetViewMatrix();
    DirectX::SimpleMath::Matrix projectionMatrix = m_camera->GetProjectionMatrix();

    // Configurar estados comunes para los objetos opacos
    if (m_states)
    {
        context->OMSetBlendState(m_states->Opaque(), nullptr, 0xFFFFFFFF);
        context->OMSetDepthStencilState(m_states->DepthDefault(), 0);
        context->RSSetState(m_states->CullCounterClockwise());
    }

    // Dibujar Terreno
    if (m_terrain)
    {
        m_terrain->SetViewMatrix(viewMatrix);
        m_terrain->SetProjectionMatrix(projectionMatrix);
        m_terrain->Render(context, m_lightPropertiesCB.Get(), m_samplerState.Get(), m_camera->GetPosition(),
            m_lightViewMatrix * m_lightProjectionMatrix, m_shadowMapSRV.Get(), m_shadowSamplerState.Get());
    }

    // Dibujar Modelos
    context->RSSetState(m_states->CullNone());

    // Dibujar Modelos
    for (const auto& instance : m_worldInstances)
    {
        if (instance.baseModel)
        {
            // 1. Establecer la matriz de mundo
            instance.baseModel->SetWorldMatrix(instance.worldTransform);

            // 2. Ya no cambiamos el estado aquí dentro, porque es el mismo para todos

            // 3. Dibujar
            instance.baseModel->EvolvingDraw(
                context,
                viewMatrix,
                projectionMatrix,
                m_lightPropertiesCB.Get(),
                m_samplerState.Get(),
                m_lightViewMatrix,
                m_lightProjectionMatrix,
                m_shadowMapSRV.Get(),
                m_shadowSamplerState.Get()
            );
        }
    }
    // Restaurar el estado por defecto después del bucle
    context->RSSetState(m_states->CullCounterClockwise());

    if (m_drawDebugCollisions)
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


        context->RSSetState(m_states->CullCounterClockwise());

        m_deviceResources->PIXEndEvent();
    }


    if (m_skySphere && m_skyEffect)
    {
        DirectX::SimpleMath::Matrix skyWorldMatrix = DirectX::SimpleMath::Matrix::CreateScale(m_camera->GetFarPlane() * 0.9f);
        skyWorldMatrix *= DirectX::SimpleMath::Matrix::CreateTranslation(m_camera->GetPosition());

        m_skyEffect->SetWorld(skyWorldMatrix);
        m_skyEffect->SetView(viewMatrix);
        m_skyEffect->SetProjection(projectionMatrix);

        m_skyEffect->Apply(context);

        context->IASetInputLayout(m_skyInputLayout.Get());

        // Guardar el estado de profundidad y stencil actual para restaurarlo después
        ID3D11DepthStencilState* oldDepthState = nullptr;
        UINT stencilRef = 0;
        context->OMGetDepthStencilState(&oldDepthState, &stencilRef);

        context->RSSetState(m_states->CullClockwise());
        context->OMSetDepthStencilState(m_skyDepthState.Get(), 0);

        m_skySphere->Draw(m_skyEffect.get(), m_skyInputLayout.Get());

        // Restaurar el estado de profundidad y stencil anterior
        if (oldDepthState)
        {
            context->OMSetDepthStencilState(oldDepthState, stencilRef);
            oldDepthState->Release();
        }

        // ESTE BLOQUE DE LIMPIEZA DENTRO DEL SKYDOME ES CRUCIAL
        context->VSSetShader(nullptr, nullptr, 0);
        context->PSSetShader(nullptr, nullptr, 0);
        context->GSSetShader(nullptr, nullptr, 0);
        context->HSSetShader(nullptr, nullptr, 0);
        context->DSSetShader(nullptr, nullptr, 0);

        ID3D11Buffer* nullBuffer[] = { nullptr };
        UINT stride = 0;
        UINT offset = 0;
        context->IASetVertexBuffers(0, 1, nullBuffer, &stride, &offset);
        context->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
        context->IASetInputLayout(nullptr);

        context->OMSetBlendState(m_states->Opaque(), nullptr, 0xFFFFFFFF);
        context->OMSetDepthStencilState(m_states->DepthDefault(), 0);
        context->RSSetState(m_states->CullCounterClockwise());
    }


    m_deviceResources->PIXEndEvent(); // Fin del renderizado de la escena 3D


    // ====================================================================
    // PASO 2: POST-PROCESAMIENTO - BLOOM
    // ====================================================================
    m_deviceResources->PIXBeginEvent(L"2. Post-Processing: Bloom");

    // Ya no necesitamos el buffer de profundidad para los pases de post-procesamiento
    ID3D11RenderTargetView* nullRTV = nullptr;
    context->OMSetRenderTargets(1, &nullRTV, nullptr); // Desvincular todo temporalmente

    // --- 2a. Extraer Píxeles Brillantes ---
    m_deviceResources->PIXBeginEvent(L"Bloom - Extract Brightness");
    context->OMSetRenderTargets(1, m_bloomExtractRTV.GetAddressOf(), nullptr);
    D3D11_VIEWPORT bloomViewport = { 0.0f, 0.0f, static_cast<float>(mainViewport.Width / 2), static_cast<float>(mainViewport.Height / 2), 0.0f, 1.0f };
    context->RSSetViewports(1, &bloomViewport);

    context->VSSetShader(m_fullscreenQuadVS.Get(), nullptr, 0);
    context->PSSetShader(m_bloomExtractPS.Get(), nullptr, 0);
    context->PSSetShaderResources(0, 1, m_sceneSRV.GetAddressOf());
    context->PSSetSamplers(0, 1, m_samplerState.GetAddressOf());

    D3D11_MAPPED_SUBRESOURCE mappedResource;
    context->Map(m_cbBloomParameters.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    memcpy(mappedResource.pData, &m_bloomParamsData, sizeof(CB_BloomParameters));
    context->Unmap(m_cbBloomParameters.Get(), 0);
    context->PSSetConstantBuffers(0, 1, m_cbBloomParameters.GetAddressOf());

    context->IASetInputLayout(nullptr);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    context->Draw(3, 0);
    m_deviceResources->PIXEndEvent();

    // --- 2b. Desenfoque Gaussiano (Ping-Pong) ---
    m_deviceResources->PIXBeginEvent(L"Bloom - Gaussian Blur");

    // Actualizar el Constant Buffer de Blur (solo se necesita una vez)
    m_blurParamsData.texelSize = { 1.0f / (mainViewport.Width / 2.0f), 1.0f / (mainViewport.Height / 2.0f) };
    context->Map(m_cbBlurParameters.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    memcpy(mappedResource.pData, &m_blurParamsData, sizeof(CB_BlurParameters));
    context->Unmap(m_cbBlurParameters.Get(), 0);
    context->PSSetConstantBuffers(0, 1, m_cbBlurParameters.GetAddressOf());

    // Hacemos varios pases para un desenfoque más suave y amplio
    for (int i = 0; i < 2; ++i)
    {
        // --- PASE HORIZONTAL ---
        // El resultado se dibuja en m_blurRTV
        context->OMSetRenderTargets(1, m_blurRTV.GetAddressOf(), nullptr);
        context->PSSetShader(m_gaussianBlurHorizontalPS.Get(), nullptr, 0);
        // La entrada es la textura de los puntos brillantes
        context->PSSetShaderResources(0, 1, m_bloomExtractSRV.GetAddressOf());
        context->Draw(3, 0);

        // Desvinculamos la textura de extracción para poder usarla como objetivo en el siguiente paso
        ID3D11ShaderResourceView* nullSRV[] = { nullptr };
        context->PSSetShaderResources(0, 1, nullSRV);

        // --- PASE VERTICAL ---
        // El resultado se dibuja de vuelta en m_bloomExtractRTV
        context->OMSetRenderTargets(1, m_bloomExtractRTV.GetAddressOf(), nullptr);
        context->PSSetShader(m_gaussianBlurVerticalPS.Get(), nullptr, 0);
        // La entrada es la textura que acabamos de desenfocar horizontalmente
        context->PSSetShaderResources(0, 1, m_blurSRV.GetAddressOf());
        context->Draw(3, 0);

        // Desvinculamos la textura de blur para el siguiente ciclo
        context->PSSetShaderResources(0, 1, nullSRV);
    }
    m_deviceResources->PIXEndEvent();

    // --- 2c. Composición Final ---
    m_deviceResources->PIXBeginEvent(L"Bloom - Composite to Back Buffer");
    context->OMSetRenderTargets(1, &finalRTV, nullptr); // ¡Ahora sí, al back buffer de la ventana!
    context->RSSetViewports(1, &mainViewport);

    context->PSSetShader(m_bloomCompositePS.Get(), nullptr, 0);
    context->PSSetConstantBuffers(0, 1, m_cbBloomParameters.GetAddressOf());

    ID3D11ShaderResourceView* compositeSRVs[] = { m_sceneSRV.Get(), m_bloomExtractSRV.Get() }; // Usar la textura de extracción ya desenfocada
    context->PSSetShaderResources(0, 2, compositeSRVs);
    context->Draw(3, 0);
    m_deviceResources->PIXEndEvent();

    m_deviceResources->PIXEndEvent(); // Fin del post-procesamiento

    context->OMSetRenderTargets(1, &finalRTV, depthStencil);

    ID3D11ShaderResourceView* nullSRVs[] = { nullptr, nullptr, nullptr, nullptr };
    context->PSSetShaderResources(0, 4, nullSRVs);
    context->VSSetShader(nullptr, nullptr, 0);
    context->PSSetShader(nullptr, nullptr, 0);
    context->GSSetShader(nullptr, nullptr, 0);
    context->IASetInputLayout(nullptr);
    if (m_states)
    {
        context->OMSetBlendState(m_states->Opaque(), nullptr, 0xFFFFFFFF);
        context->OMSetDepthStencilState(m_states->DepthDefault(), 0);
        context->RSSetState(m_states->CullCounterClockwise());
    }

    m_deviceResources->PIXBeginEvent(L"Render Firefly Particles");
    if (m_sunPower < 0.15f)
    {
        // --- REEMPLAZA TODO EL BLOQUE DE m_spriteBatch3D POR ESTO ---

        // 1. Configurar el pipeline para el renderizado de partculas
        context->VSSetShader(m_fireflyVS.Get(), nullptr, 0);
        context->PSSetShader(m_fireflyPS.Get(), nullptr, 0);
        context->IASetInputLayout(m_fireflyInputLayout.Get());

        // Geometra del quad (la misma para todas las partculas)
        UINT stride = sizeof(DirectX::VertexPositionTexture);
        UINT offset = 0;
        context->IASetVertexBuffers(0, 1, m_fireflyVertexBuffer.GetAddressOf(), &stride, &offset);
        context->IASetIndexBuffer(m_fireflyIndexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);
        context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        // Estados de renderizado (mezcla aditiva, sin escritura de profundidad)
        context->OMSetBlendState(m_states->Additive(), nullptr, 0xFFFFFFFF);
        context->OMSetDepthStencilState(m_states->DepthRead(), 0); // Leemos la profundidad para que se oculten detrs de objetos, pero no escribimos en ella
        context->RSSetState(m_states->CullNone());

        // 2. Configurar el Constant Buffer "PerFrame" (una sola vez)
        D3D11_MAPPED_SUBRESOURCE mappedResource;
        context->Map(m_cbFireflyPerFrame.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
        auto perFrameData = reinterpret_cast<CB_Firefly_PerFrame*>(mappedResource.pData);
        perFrameData->ViewProjection = m_camera->GetViewMatrix() * m_camera->GetProjectionMatrix();
        perFrameData->CameraRight_World = m_camera->GetRight();
        perFrameData->CameraUp_World = m_camera->GetUp();
        context->Unmap(m_cbFireflyPerFrame.Get(), 0);
        context->VSSetConstantBuffers(0, 1, m_cbFireflyPerFrame.GetAddressOf());

        // 3. Vincular recursos del Pixel Shader (textura y sampler)
        context->PSSetShaderResources(0, 1, m_fireflyTexture.GetAddressOf());
        context->PSSetSamplers(0, 1, m_samplerState.GetAddressOf());

        // 4. Bucle para dibujar cada partcula
        for (const auto& firefly : m_fireflies)
        {
            // Actualizar el Constant Buffer "PerParticle" para esta lucirnaga
            context->Map(m_cbFireflyPerParticle.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
            auto perParticleData = reinterpret_cast<CB_Firefly_PerParticle*>(mappedResource.pData);

            float blink = (sin(firefly.blinkTimer) + 1.0f) / 2.0f;
            blink = pow(blink, 3.0f);

            perParticleData->ParticleCenter_World = firefly.position;
            perParticleData->ParticleColor = DirectX::SimpleMath::Vector4(1.5f, 2.0f, 1.0f, 1.0f) * blink;
            perParticleData->ParticleSize = { 0.5f, 0.5f }; // <--- JUEGA CON ESTE TAMAO!

            context->Unmap(m_cbFireflyPerParticle.Get(), 0);
            context->VSSetConstantBuffers(1, 1, m_cbFireflyPerParticle.GetAddressOf());

            // Dibujar el quad
            context->DrawIndexed(6, 0, 0);
        }

        // MUY IMPORTANTE: Restaurar el estado de profundidad por defecto despus de terminar
        context->OMSetDepthStencilState(m_states->DepthDefault(), 0);
    }
    m_deviceResources->PIXEndEvent();


    m_deviceResources->PIXBeginEvent(L"3. Render UI");
    if (m_spriteBatchUI)
    {
        m_spriteBatchUI->Begin(SpriteSortMode_Deferred, m_states->AlphaBlend());
        wchar_t buffer[256];
        // Mostramos Posición X, Y, Z y Rotación Yaw, Pitch en grados para facilitar la lectura
        swprintf_s(buffer, L"Pos: (%.2f, %.2f, %.2f)\nYaw: %.1f deg\nPitch: %.1f deg",
            m_camera->GetPosition().x, m_camera->GetPosition().y, m_camera->GetPosition().z,
            DirectX::XMConvertToDegrees(m_camera->GetYaw()),
            DirectX::XMConvertToDegrees(m_camera->GetPitch()));

        DirectX::SimpleMath::Vector2 textPosition(10.0f, 10.0f);
        DirectX::SimpleMath::Vector4 textColor(1.0f, 1.0f, 0.0f, 1.0f);

        m_font->DrawString(m_spriteBatchUI.get(), buffer, textPosition, textColor);

        RECT minimapRect = { 10, 150, 10 + MINIMAP_SIZE, 150 + MINIMAP_SIZE };
        m_spriteBatchUI->Draw(m_minimapSRV.Get(), minimapRect);

        // 2. Dibuja el icono del jugador en el centro del minimapa
        ComPtr<ID3D11Resource> playerIconResource;
        m_playerIconTexture->GetResource(playerIconResource.GetAddressOf());
        ComPtr<ID3D11Texture2D> playerIconTex2D;
        playerIconResource.As(&playerIconTex2D);
        CD3D11_TEXTURE2D_DESC iconDesc;
        playerIconTex2D->GetDesc(&iconDesc);

        Vector2 playerIconOrigin = Vector2(iconDesc.Width / 2.f, iconDesc.Height / 2.f);
        Vector2 playerIconPos = Vector2(minimapRect.left + MINIMAP_SIZE / 2.f, minimapRect.top + MINIMAP_SIZE / 2.f);

        // Rotamos el icono segn el Yaw de la cmara
        float playerRotation = -m_camera->GetYaw();

        m_spriteBatchUI->Draw(m_playerIconTexture.Get(), playerIconPos, nullptr, Colors::White, playerRotation, playerIconOrigin);

        RECT outputSize = m_deviceResources->GetOutputSize();
        long width = outputSize.right - outputSize.left;

        RECT shadowMapRect = { width - 266, 10, width - 10, 266 };
        m_spriteBatchUI->Draw(m_shadowMapSRV.Get(), shadowMapRect);

        m_spriteBatchUI->End();
    }
    m_deviceResources->PIXEndEvent();

    // ====================================================================
    // PASO 4: PRESENTAR EL CUADRO FINAL
    // ====================================================================
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
    // Una luz direccional suave desde arriba solo para dar un poco de definicin
    m_minimapLightData.directionalLightVector = DirectX::SimpleMath::Vector3(0.0f, -1.0f, 0.0f);
    m_minimapLightData.directionalLightColor = DirectX::SimpleMath::Vector4(0.5f, 0.5f, 0.5f, 1.0f);
    // La posicin de la cmara no es crtica para esta iluminacin
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
    m_camera->SetPosition(DirectX::SimpleMath::Vector3(11.2f, 5.0f, -72.0f));
    // m_camera->SetRotation(yaw, pitch); // Opcional si quieres una rotación inicial específica
    m_camera->UpdateViewMatrix(); // Asegura que la matriz de vista se calcule inicialmente
    

    m_spriteBatch3D = std::make_unique<DirectX::SpriteBatch>(context);
    m_spriteBatchUI = std::make_unique<DirectX::SpriteBatch>(context);
    m_font = std::make_unique<DirectX::SpriteFont>(device, L"GameAssets\\Fonts\\GameFont.spritefont");

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
        L"C:\\Users\\rebeq\\source\\repos\\GC2_PlantillaDB\\x64\\Debug\\TerrainVS.cso",
        L"C:\\Users\\rebeq\\source\\repos\\GC2_PlantillaDB\\x64\\Debug\\TerrainPS.cso"))
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

        float deseadoAnchoDelTerrenoEnElMundo = terrainGridActualWidth * 5.0f; // Ejemplo: Hacerlo el doble de ancho
        float deseadoProfundidadDelTerrenoEnElMundo = terrainGridActualDepth * 5.0f; // Ejemplo: Hacerlo el doble de profundo
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
        "GameAssets/models/blacksmith/blacksmith.obj"))
    {
        throw std::runtime_error("Failed to load m_blacksmith!");
    }

    if (m_blacksmith) // Asegúrate de que el modelo principal se cargó
    {
        // Asegúrate de que DebugVS.cso y DebugPS.cso estén en tu directorio de salida
        if (!m_blacksmith->LoadEvolvingShaders(device, L"C:\\Users\\rebeq\\source\\repos\\GC2_PlantillaDB\\x64\\Debug\\EvolvingVS.cso", L"C:\\Users\\rebeq\\source\\repos\\GC2_PlantillaDB\\x64\\Debug\\EvolvingPS.cso"))
        {
            throw std::runtime_error("Failed to load debug shaders for m_blacksmith!");
        }
    }

    // Ajusta escala, rotación, posición para m_miPrimerModelo como lo tenías
    m_blacksmith->SetScale(0.2f); // Ejemplo
    m_blacksmith->SetRotationEuler(DirectX::XM_PI, DirectX::XM_PIDIV2, 0.0f); // Ejemplo
    

    m_green_tree1 = std::make_unique<Model>();
    if (!m_green_tree1->Load(device, context,
        "GameAssets/models/green_tree/green_tree.obj"))
    {
        throw std::runtime_error("Failed to load m_green_tree1!");
    }

    if (m_green_tree1)
    {
        if (!m_green_tree1->LoadEvolvingShaders(device, L"C:\\Users\\rebeq\\source\\repos\\GC2_PlantillaDB\\x64\\Debug\\EvolvingVS.cso", L"C:\\Users\\rebeq\\source\\repos\\GC2_PlantillaDB\\x64\\Debug\\EvolvingPS.cso"))
        {
            throw std::runtime_error("Failed to load debug shaders for m_green_tree1!");
        }
    }

    m_green_tree1->SetScale(5.0f);
    m_green_tree1->SetRotationEuler(DirectX::XM_PI, DirectX::XM_PI, 0.0f); 

   
    m_forest_pine1 = std::make_unique<Model>();
    if (!m_forest_pine1->Load(device, context,
        "GameAssets/models/trees/pine1.obj"))
    {
        throw std::runtime_error("Failed to load m_forest_pine1!");
    }

    if (m_forest_pine1)
    {
        if (!m_forest_pine1->LoadEvolvingShaders(device, L"C:\\Users\\rebeq\\source\\repos\\GC2_PlantillaDB\\x64\\Debug\\EvolvingVS.cso", L"C:\\Users\\rebeq\\source\\repos\\GC2_PlantillaDB\\x64\\Debug\\EvolvingPS.cso"))
        {
            throw std::runtime_error("Failed to load debug shaders for m_forest_pine1!");
        }
    }

    m_forest_pine1->SetScale(5.0f);
    m_forest_pine1->SetRotationEuler(DirectX::XM_PI, DirectX::XM_PI, 0.0f);


    m_forest_pine2 = std::make_unique<Model>();
    if (!m_forest_pine2->Load(device, context,
        "GameAssets/models/trees/pine2.obj"))
    {
        throw std::runtime_error("Failed to load m_forest_pine2!");
    }

    if (m_forest_pine2)
    {
        if (!m_forest_pine2->LoadEvolvingShaders(device, L"C:\\Users\\rebeq\\source\\repos\\GC2_PlantillaDB\\x64\\Debug\\EvolvingVS.cso", L"C:\\Users\\rebeq\\source\\repos\\GC2_PlantillaDB\\x64\\Debug\\EvolvingPS.cso"))
        {
            throw std::runtime_error("Failed to load debug shaders for m_forest_pine2!");
        }
    }

    m_forest_pine2->SetScale(2.0f);
    m_forest_pine2->SetRotationEuler(DirectX::XM_PI, DirectX::XM_PI, 0.0f);


    m_forest_pine3 = std::make_unique<Model>();
    if (!m_forest_pine3->Load(device, context,
        "GameAssets/models/trees/pine3.obj"))
    {
        throw std::runtime_error("Failed to load m_forest_pine3!");
    }

    if (m_forest_pine3)
    {
        if (!m_forest_pine3->LoadEvolvingShaders(device, L"C:\\Users\\rebeq\\source\\repos\\GC2_PlantillaDB\\x64\\Debug\\EvolvingVS.cso", L"C:\\Users\\rebeq\\source\\repos\\GC2_PlantillaDB\\x64\\Debug\\EvolvingPS.cso"))
        {
            throw std::runtime_error("Failed to load debug shaders for m_forest_pine3!");
        }
    }

    m_forest_pine3->SetScale(2.0f);
    m_forest_pine3->SetRotationEuler(DirectX::XM_PI, DirectX::XM_PI, 0.0f);




    m_cart = std::make_unique<Model>();
    if (!m_cart->Load(device, context,
        "GameAssets/models/cart/Cart.obj"))
    {
        throw std::runtime_error("Failed to load m_cart!");
    }

    if (m_cart)
    {
        if (!m_cart->LoadEvolvingShaders(device, L"C:\\Users\\rebeq\\source\\repos\\GC2_PlantillaDB\\x64\\Debug\\EvolvingVS.cso", L"C:\\Users\\rebeq\\source\\repos\\GC2_PlantillaDB\\x64\\Debug\\EvolvingPS.cso"))
        {
            throw std::runtime_error("Failed to load debug shaders for m_cart!");
        }
    }

    m_cart->SetScale(0.1f);
    m_cart->SetRotationEuler(DirectX::XM_PI, DirectX::XM_PIDIV2, 0.0f);

    m_windmill = std::make_unique<Model>();
    if (!m_windmill->Load(device, context,
        "GameAssets/models/windmill/windmill.obj"))
    {
        throw std::runtime_error("Failed to load m_windmill!");
    }

    if (m_windmill)
    {
        if (!m_windmill->LoadEvolvingShaders(device, L"C:\\Users\\rebeq\\source\\repos\\GC2_PlantillaDB\\x64\\Debug\\EvolvingVS.cso", L"C:\\Users\\rebeq\\source\\repos\\GC2_PlantillaDB\\x64\\Debug\\EvolvingPS.cso"))
        {
            throw std::runtime_error("Failed to load debug shaders for m_windmill!");
        }
    }

    m_windmill->SetScale(1.0f);
    m_windmill->SetRotationEuler(DirectX::XM_PI, 0.0f, 0.0f);

    m_rock1 = std::make_unique<Model>();
    if (!m_rock1->Load(device, context,
        "GameAssets/models/rocks/rock1.obj"))
    {
        throw std::runtime_error("Failed to load m_rock1!");
    }

    if (m_rock1)
    {
        if (!m_rock1->LoadEvolvingShaders(device, L"C:\\Users\\rebeq\\source\\repos\\GC2_PlantillaDB\\x64\\Debug\\EvolvingVS.cso", L"C:\\Users\\rebeq\\source\\repos\\GC2_PlantillaDB\\x64\\Debug\\EvolvingPS.cso"))
        {
            throw std::runtime_error("Failed to load debug shaders for m_rock1!");
        }
    }

    m_rock1->SetScale(3.0f);
    m_rock1->SetRotationEuler(DirectX::XM_PI, 0.0f, 0.0f);

    m_rock2 = std::make_unique<Model>();
    if (!m_rock2->Load(device, context,
        "GameAssets/models/rocks/rock2.obj"))
    {
        throw std::runtime_error("Failed to load m_rock2!");
    }

    if (m_rock2)
    {
        if (!m_rock2->LoadEvolvingShaders(device, L"C:\\Users\\rebeq\\source\\repos\\GC2_PlantillaDB\\x64\\Debug\\EvolvingVS.cso", L"C:\\Users\\rebeq\\source\\repos\\GC2_PlantillaDB\\x64\\Debug\\EvolvingPS.cso"))
        {
            throw std::runtime_error("Failed to load debug shaders for m_rock2!");
        }
    }

    m_rock2->SetScale(1.0f);
    m_rock2->SetRotationEuler(DirectX::XM_PI, 0.0f, 0.0f);

    m_rock3 = std::make_unique<Model>();
    if (!m_rock3->Load(device, context,
        "GameAssets/models/rocks/rock3.obj"))
    {
        throw std::runtime_error("Failed to load m_rock3!");
    }

    if (m_rock3)
    {
        if (!m_rock3->LoadEvolvingShaders(device, L"C:\\Users\\rebeq\\source\\repos\\GC2_PlantillaDB\\x64\\Debug\\EvolvingVS.cso", L"C:\\Users\\rebeq\\source\\repos\\GC2_PlantillaDB\\x64\\Debug\\EvolvingPS.cso"))
        {
            throw std::runtime_error("Failed to load debug shaders for m_rock3!");
        }
    }

    m_rock3->SetScale(1.0f);
    m_rock3->SetRotationEuler(DirectX::XM_PI, 0.0f, 0.0f);

    m_rock4 = std::make_unique<Model>();
    if (!m_rock4->Load(device, context,
        "GameAssets/models/rocks/rock4.obj"))
    {
        throw std::runtime_error("Failed to load m_rock4!");
    }

    if (m_rock4)
    {
        if (!m_rock1->LoadEvolvingShaders(device, L"C:\\Users\\rebeq\\source\\repos\\GC2_PlantillaDB\\x64\\Debug\\EvolvingVS.cso", L"C:\\Users\\rebeq\\source\\repos\\GC2_PlantillaDB\\x64\\Debug\\EvolvingPS.cso"))
        {
            throw std::runtime_error("Failed to load debug shaders for m_rock4!");
        }
    }

    m_rock4->SetScale(1.0f);
    m_rock4->SetRotationEuler(DirectX::XM_PI, 0.0f, 0.0f);

    m_rock5 = std::make_unique<Model>();
    if (!m_rock5->Load(device, context,
        "GameAssets/models/rocks/rock5.obj"))
    {
        throw std::runtime_error("Failed to load m_rock5!");
    }

    if (m_rock5)
    {
        if (!m_rock5->LoadEvolvingShaders(device, L"C:\\Users\\rebeq\\source\\repos\\GC2_PlantillaDB\\x64\\Debug\\EvolvingVS.cso", L"C:\\Users\\rebeq\\source\\repos\\GC2_PlantillaDB\\x64\\Debug\\EvolvingPS.cso"))
        {
            throw std::runtime_error("Failed to load debug shaders for m_rock5!");
        }
    }

    m_rock5->SetScale(1.0f);
    m_rock5->SetRotationEuler(DirectX::XM_PI, 0.0f, 0.0f);

    m_rock6 = std::make_unique<Model>();
    if (!m_rock6->Load(device, context,
        "GameAssets/models/rocks/rock6.obj"))
    {
        throw std::runtime_error("Failed to load m_rock6!");
    }

    if (m_rock6)
    {
        if (!m_rock6->LoadEvolvingShaders(device, L"C:\\Users\\rebeq\\source\\repos\\GC2_PlantillaDB\\x64\\Debug\\EvolvingVS.cso", L"C:\\Users\\rebeq\\source\\repos\\GC2_PlantillaDB\\x64\\Debug\\EvolvingPS.cso"))
        {
            throw std::runtime_error("Failed to load debug shaders for m_rock6!");
        }
    }

    m_rock6->SetScale(1.0f);
    m_rock6->SetRotationEuler(DirectX::XM_PI, 0.0f, 0.0f);

    // Modelo de casa 1
    m_house1 = std::make_unique<Model>();
    if (!m_house1->Load(device, context,
        "GameAssets/models/houses/CASA01.obj"))
    {
        throw std::runtime_error("Failed to load m_house1!");
    }

    if (m_house1)
    {
        if (!m_house1->LoadEvolvingShaders(device, L"C:\\Users\\rebeq\\source\\repos\\GC2_PlantillaDB\\x64\\Debug\\EvolvingVS.cso", L"C:\\Users\\rebeq\\source\\repos\\GC2_PlantillaDB\\x64\\Debug\\EvolvingPS.cso"))
        {
            throw std::runtime_error("Failed to load debug shaders for m_house1!");
        }
    }

    m_house1->SetScale(5.0f);
    m_house1->SetRotationEuler(DirectX::XM_PI, DirectX::XM_PI, 0.0f);

    //Modelo de casa 2

    m_house2 = std::make_unique<Model>();
    if (!m_house2->Load(device, context,
        "GameAssets/models/houses/CASA02.obj"))
    {
        throw std::runtime_error("Failed to load m_house2!");
    }

    if (m_house2)
    {
        if (!m_house2->LoadEvolvingShaders(device, L"C:\\Users\\rebeq\\source\\repos\\GC2_PlantillaDB\\x64\\Debug\\EvolvingVS.cso", L"C:\\Users\\rebeq\\source\\repos\\GC2_PlantillaDB\\x64\\Debug\\EvolvingPS.cso"))
        {
            throw std::runtime_error("Failed to load debug shaders for m_house2!");
        }
    }

    m_house2->SetScale(5.0f);
    m_house2->SetRotationEuler(DirectX::XM_PI, DirectX::XM_PI, 0.0f);

    //Modelo de casa 3
	m_house3 = std::make_unique<Model>();
    if (!m_house3->Load(device, context,
        "GameAssets/models/houses/CASA03.obj"))
    {
        throw std::runtime_error("Failed to load m_house3!");
    }

    if (m_house3)
    {
        if (!m_house3->LoadEvolvingShaders(device, L"C:\\Users\\rebeq\\source\\repos\\GC2_PlantillaDB\\x64\\Debug\\EvolvingVS.cso", L"C:\\Users\\rebeq\\source\\repos\\GC2_PlantillaDB\\x64\\Debug\\EvolvingPS.cso"))
        {
            throw std::runtime_error("Failed to load debug shaders for m_house3!");
        }
    }

    m_house3->SetScale(5.0f);
    m_house3->SetRotationEuler(DirectX::XM_PI, -DirectX::XM_PIDIV2, 0.0f);

    //Modelo de casa 4
    m_house4 = std::make_unique<Model>();
    if (!m_house4->Load(device, context,
        "GameAssets/models/houses/CASA04.obj"))
    {
        throw std::runtime_error("Failed to load m_house4!");
    }

    if (m_house4)
    {
        if (!m_house4->LoadEvolvingShaders(device, L"C:\\Users\\rebeq\\source\\repos\\GC2_PlantillaDB\\x64\\Debug\\EvolvingVS.cso", L"C:\\Users\\rebeq\\source\\repos\\GC2_PlantillaDB\\x64\\Debug\\EvolvingPS.cso"))
        {
            throw std::runtime_error("Failed to load debug shaders for m_house4!");
        }
    }

    m_house4->SetScale(5.0f);
    m_house4->SetRotationEuler(DirectX::XM_PI, -DirectX::XM_PIDIV2, 0.0f);
    
    m_knight = std::make_unique<Model>();
    if (!m_knight->Load(device, context,
        "GameAssets/models/knight/knight.obj"))
    {
        throw std::runtime_error("Failed to load m_knight!");
    }
    if (m_knight)
    {
        if (!m_knight->LoadEvolvingShaders(device, L"C:\\Users\\rebeq\\source\\repos\\GC2_PlantillaDB\\x64\\Debug\\EvolvingVS.cso", L"C:\\Users\\rebeq\\source\\repos\\GC2_PlantillaDB\\x64\\Debug\\EvolvingPS.cso"))
        {
            throw std::runtime_error("Failed to load debug shaders for m_knight!");
        }
    }
    m_knight->SetScale(0.1f);
    m_knight->SetRotationEuler(DirectX::XM_PI, DirectX::XM_PI, 0.0f);
    

    hr = CreateWICTextureFromFile(device, L"GameAssets\\textures\\firefly.png", nullptr, m_fireflyTexture.ReleaseAndGetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("Fallo al cargar la textura de la luciernaga.");

    InitializeFireflies();

    m_worldInstances.clear();

    const float offsetY_pine1 = -7.0f;
    const float offsetY_pine2 = -1.0f;
    const float offsetY_pine3 = -1.2f;
    const float offsetY_green_tree1 = 0.0f;
    const float offsetY_rock = -0.5f; 

    DirectX::SimpleMath::Matrix baseTransform;

    if (m_forest_pine1 && m_terrain) {
        baseTransform = m_forest_pine1->GetWorldMatrix(); 
        const std::vector<std::tuple<float, float, float>> pine1_data = {
            {222.93f, -102.9f,  -7.0f}, {196.91f, -241.45f, -2.0f}, {81.28f,  -103.99f, -2.0f},
            {83.29f,  -216.94f, -7.0f}, {114.82f, -83.19f,  -3.0f}, {146.82f, -244.59f, -9.90f},
            {-151.1f, -96.5f,   -7.0f}, {-238.7f, -49.85f,  -7.0f}, {-215.8f, 42.1f,    -7.0f},
            {-1.93f,  34.34f,    8.0f}, {-24.93f, -135.34f, -7.0f}
        };
        for (const auto& data : pine1_data) {
            AddInstancedObject(m_forest_pine1.get(), baseTransform, std::get<0>(data), std::get<1>(data), std::get<2>(data), offsetY_pine1);
        }
    }

    // === Instancias de m_forest_pine2 ===
    if (m_forest_pine2 && m_terrain) {
        baseTransform = m_forest_pine2->GetWorldMatrix();
        const std::vector<std::tuple<float, float, float>> pine2_data = {
            {-78.17f, -9.94f,    5.0f}, {-86.17f, -117.03f,  5.0f}, {-178.17f, -133.03f, 1.0f},
            {132.84f, -106.03f, 1.0f}, {48.45f,  -185.03f,  1.0f}, {151.50f, 13.02f,    1.0f}
        };
        for (const auto& data : pine2_data) {
            AddInstancedObject(m_forest_pine2.get(), baseTransform, std::get<0>(data), std::get<1>(data), std::get<2>(data), offsetY_pine2);
        }
    }
    
    // === Instancias de m_forest_pine3 ===
    if (m_forest_pine3 && m_terrain) {
        baseTransform = m_forest_pine3->GetWorldMatrix();
        const std::vector<std::tuple<float, float, float>> pine3_data = {
            {96.50f, 125.02f,  1.0f}, {-221.11f, -75.25f,1.0f}, {-0.57f, -28.85f,  3.0f}
        };
        for (const auto& data : pine3_data) {
            AddInstancedObject(m_forest_pine3.get(), baseTransform, std::get<0>(data), std::get<1>(data), std::get<2>(data), offsetY_pine3);
        }
    }
    
    // === Instancias de m_green_tree1 ===
    if (m_green_tree1 && m_terrain) {
        baseTransform = m_green_tree1->GetWorldMatrix();
        const std::vector<std::tuple<float, float, float>> green_tree1_data = {
            {62.2f, 25.8f,  12.0f}, {-24.2f, -70.8f, 13.0f}, {-103.2f, -61.01f,6.0f}
        };
        for (const auto& data : green_tree1_data) {
            AddInstancedObject(m_green_tree1.get(), baseTransform, std::get<0>(data), std::get<1>(data), std::get<2>(data), offsetY_green_tree1);
        }
    }

    if (m_blacksmith && m_terrain) { // El herrero también podría necesitar ajuste al terreno
        baseTransform = m_blacksmith->GetWorldMatrix();
        // Decide una posición X, Z, Y-fallback y offsetY para el herrero
        AddInstancedObject(m_blacksmith.get(), baseTransform, 215.7f, -177.07f, -9.0f, 0.0f); // offsetY=0 si su origen está bien
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

    if (m_knight && m_terrain) {
        baseTransform = m_knight->GetWorldMatrix();
        AddInstancedObject(m_knight.get(), baseTransform, 6.15f, -256.0f, -4.0f, 0.0f);
    }

    if (m_cart && m_terrain) {
        baseTransform = m_cart->GetWorldMatrix();
        AddInstancedObject(m_cart.get(), baseTransform, 154.8f, -137.55f, -4.7f, 0.0f); // offsetY=0 ejemplo
    }

    if (m_windmill && m_terrain) {
        baseTransform = m_windmill->GetWorldMatrix();
        AddInstancedObject(m_windmill.get(), baseTransform, -203.6f, -23.9f, 1.0f, 0.0f); // offsetY=0 ejemplo
    }

    // Rocas:
    // Para m_rock1 (y las demás rocas si las vas a instanciar o colocar individualmente)
    if (m_rock1 && m_terrain) {
        baseTransform = m_rock1->GetWorldMatrix();
        AddInstancedObject(m_rock1.get(), baseTransform, -79.10f, -118.56f, 5.0f, offsetY_rock);
    }

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
    hr = D3DReadFileToBlob(L"C:\\Users\\rebeq\\source\\repos\\GC2_PlantillaDB\\x64\\Debug\\FireflyVS.cso", vsBlob.GetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("Fallo al cargar FireflyVS.cso.");
    hr = device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, m_fireflyVS.ReleaseAndGetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("Fallo al crear el Firefly Vertex Shader.");

    hr = D3DReadFileToBlob(L"C:\\Users\\rebeq\\source\\repos\\GC2_PlantillaDB\\x64\\Debug\\FireflyPS.cso", psBlob.GetAddressOf());
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

    D3D11_RASTERIZER_DESC rasterDesc = {};
    rasterDesc.FillMode = D3D11_FILL_SOLID;
    rasterDesc.DepthClipEnable = true;
    rasterDesc.CullMode = D3D11_CULL_BACK;

    rasterDesc.DepthBias = 100;  
    rasterDesc.DepthBiasClamp = 0.0f;
    rasterDesc.SlopeScaledDepthBias = 1.0f;// Aumentamos de 2.0 a 4.0
    // --- FIN DE LA NUEVA CONFIGURACIN ---

    hr = device->CreateRasterizerState(&rasterDesc, m_shadowRasterizerState.ReleaseAndGetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("Fallo al crear el estado de rasterizador para sombras.");


    hr = D3DReadFileToBlob(L"C:\\Users\\rebeq\\source\\repos\\GC2_PlantillaDB\\x64\\Debug\\ShadowVS.cso", vsBlob.GetAddressOf());
    if (FAILED(hr))
    {
        throw std::runtime_error("Fallo al cargar ShadowVS.cso. Revisa la ruta y asegúrate de que compila.");
    }

    hr = device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, m_shadowVertexShader.ReleaseAndGetAddressOf());
    if (FAILED(hr))
    {
        throw std::runtime_error("Fallo al crear el vertex shader de sombras.");
    }

    hr = D3DReadFileToBlob(L"C:\\Users\\rebeq\\source\\repos\\GC2_PlantillaDB\\x64\\Debug\\ShadowPS.cso", psBlob.GetAddressOf()); // O la ruta completa si es necesario
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
    hr = D3DReadFileToBlob(L"C:\\Users\\rebeq\\source\\repos\\GC2_PlantillaDB\\x64\\Debug\\ShadowVS_AlphaClip.cso", vsAlphaBlob.GetAddressOf());
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
    hr = D3DReadFileToBlob(L"\\Users\\rebeq\\source\\repos\\GC2_PlantillaDB\\x64\\Debug\\ShadowPS_AlphaClip.cso", psAlphaBlob.GetAddressOf());
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

    hr = D3DReadFileToBlob(L"\\Users\\rebeq\\source\\repos\\GC2_PlantillaDB\\x64\\Debug\\FullscreenQuadVS.cso", vsBlob.GetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("Fallo al cargar FullscreenQuadVS.cso");
    hr = device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, m_fullscreenQuadVS.ReleaseAndGetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("Fallo al crear FullscreenQuadVS");

    hr = D3DReadFileToBlob(L"\\Users\\rebeq\\source\\repos\\GC2_PlantillaDB\\x64\\Debug\\BloomExtractPS.cso", psBlob.GetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("Fallo al cargar BloomExtractPS.cso");
    hr = device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, m_bloomExtractPS.ReleaseAndGetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("Fallo al crear BloomExtractPS");
    
    hr = D3DReadFileToBlob(L"\\Users\\rebeq\\source\\repos\\GC2_PlantillaDB\\x64\\Debug\\GaussianBlurHorizontalPS.cso", psBlob.GetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("Fallo al cargar GaussianBlurHorizontalPS.cso");
    hr = device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, m_gaussianBlurHorizontalPS.ReleaseAndGetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("Fallo al crear GaussianBlurHorizontalPS");

    hr = D3DReadFileToBlob(L"\\Users\\rebeq\\source\\repos\\GC2_PlantillaDB\\x64\\Debug\\GaussianBlurVerticalPS.cso", psBlob.GetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("Fallo al cargar GaussianBlurVerticalPS.cso");
    hr = device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, m_gaussianBlurVerticalPS.ReleaseAndGetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("Fallo al crear GaussianBlurVerticalPS");
    
    hr = D3DReadFileToBlob(L"\\Users\\rebeq\\source\\repos\\GC2_PlantillaDB\\x64\\Debug\\BloomCompositePS.cso", psBlob.GetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("Fallo al cargar BloomCompositePS.cso");
    hr = device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, m_bloomCompositePS.ReleaseAndGetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("Fallo al crear BloomCompositePS");

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
    float modelSpecificOffsetY)
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

    m_worldInstances.emplace_back(modelPtr, instanceWorldMatrix);
}

#pragma endregion

#pragma region Shadow Mapping

void Game::RenderShadowPass()
{
    auto context = m_deviceResources->GetD3DDeviceContext();
    if (!m_shadowDepthState) return;

    // 1. Configurar la pipeline una sola vez para TODOS los objetos
    context->OMSetRenderTargets(0, nullptr, m_shadowMapDSV.Get());
    context->OMSetDepthStencilState(m_shadowDepthState.Get(), 0);
    context->ClearDepthStencilView(m_shadowMapDSV.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);

    D3D11_VIEWPORT shadowViewport = { 0.0f, 0.0f, SHADOW_MAP_SIZE, SHADOW_MAP_SIZE, 0.0f, 1.0f };
    context->RSSetViewports(1, &shadowViewport);

    // Usamos el VS potente y el Input Layout correcto para todo
    context->VSSetShader(m_shadowVertexShader_AlphaClip.Get(), nullptr, 0);
    context->IASetInputLayout(m_shadowInputLayout.Get());

    // 2. Calcular matrices de la luz
    Vector3 shadowFocusPoint = m_camera->GetPosition();
    Vector3 lightPosition = shadowFocusPoint - (m_lightData.directionalLightVector * 400.0f); 

    Vector3 lightUp = Vector3::Up;

    m_lightViewMatrix = Matrix::CreateLookAt(lightPosition, shadowFocusPoint, lightUp);

    m_lightProjectionMatrix = Matrix::CreateOrthographic(500.f, 500.f, 1.0f, 800.0f);

    // 3. Dibujar los modelos
    for (const auto& instance : m_worldInstances)
    {
        if (instance.baseModel)
        {
            context->RSSetState(m_shadowRasterizerState.Get());

            bool usesAlphaClip = (instance.baseModel == m_green_tree1.get() ||
                instance.baseModel == m_forest_pine1.get() ||
                instance.baseModel == m_forest_pine2.get() ||
                instance.baseModel == m_forest_pine3.get());

            if (usesAlphaClip)
            {
                context->PSSetShader(m_shadowPixelShader_AlphaClip.Get(), nullptr, 0);
            }
            else
            {
                context->PSSetShader(nullptr, nullptr, 0);
            }

            // Usamos siempre la funcin de dibujado ms completa
            instance.baseModel->ShadowDrawAlphaClip(context, instance.worldTransform, m_lightViewMatrix, m_lightProjectionMatrix, m_samplerState.Get());
        }
    }

    // 4. Dibujar el terreno (slido, no necesita alfa)
    if (m_terrain)
    {
        context->RSSetState(m_shadowRasterizerState.Get());
        context->PSSetShader(nullptr, nullptr, 0);
        m_terrain->ShadowDraw(context, m_lightViewMatrix, m_lightProjectionMatrix);
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
    Matrix minimapView = Matrix::CreateLookAt(mapCamPos, mapCamTarget, Vector3::Forward);
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

#pragma DayNight Cycle

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
    Vector3 finalLightDirection = Vector3(sin(cycleAngle) * 0.4f, sin(cycleAngle), cos(cycleAngle));
    finalLightDirection.Normalize();

    // Actualizamos la dirección de la luz en los datos que se enviarán al shader.
    m_lightData.directionalLightVector = finalLightDirection;

    // 3. DETERMINAR LA INFLUENCIA DEL SOL BASADO EN SU ALTURA
    // m_sunPower será 1.0 en el punto más alto del sol y 0.0 cuando esté en el horizonte o por debajo.
    m_sunPower = std::clamp(finalLightDirection.y, 0.0f, 1.0f);

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