#include "pch.h" // O tu encabezado precompilado
#include "Terrain.h"
#include <WICTextureLoader.h> // Para CreateWICTextureFromFile y operaciones con texturas WIC
#include <Effects.h>          // Para BasicEffect
#include <VertexTypes.h>      // Para VertexPositionNormalTexture
#include <d3dcompiler.h>

#pragma comment(lib, "d3dcompiler.lib")

using namespace DirectX;
using namespace DirectX::SimpleMath;
using Microsoft::WRL::ComPtr;

float Lerp(float a, float b, float t) {
    return a + t * (b - a);
}

Terrain::Terrain() :
    m_terrainWidth(0),
    m_terrainHeight(0),
    m_heightScale(30.0f),
    m_textureTilingFactor(8.0f),
    m_vertexCount(0),
    m_indexCount(0),
    m_worldMatrix(Matrix::Identity)
{
}

Terrain::~Terrain()
{
}

bool Terrain::LoadHeightmap(ID3D11Device* device, ID3D11DeviceContext* context, const wchar_t* filename)
{
    ComPtr<ID3D11Resource> sourceTextureResource;
    HRESULT hr = CreateWICTextureFromFile(device, context, filename,
        sourceTextureResource.GetAddressOf(),
        nullptr); // No necesitamos el SRV aquí
    if (FAILED(hr))
    {
        OutputDebugString(L"Failed to load heightmap image using WIC.\n");
        return false;
    }

    ComPtr<ID3D11Texture2D> sourceTexture;
    hr = sourceTextureResource.As(&sourceTexture);
    if (FAILED(hr))
    {
        OutputDebugString(L"Failed to cast heightmap resource to Texture2D.\n");
        return false;
    }

    D3D11_TEXTURE2D_DESC texDesc;
    sourceTexture->GetDesc(&texDesc);

    m_terrainWidth = texDesc.Width;
    m_terrainHeight = texDesc.Height;

    // Crear una textura de "staging" para copiar los datos de la GPU a la CPU
    D3D11_TEXTURE2D_DESC stagingDesc = texDesc;
    stagingDesc.Usage = D3D11_USAGE_STAGING;
    stagingDesc.BindFlags = 0;
    stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    stagingDesc.MiscFlags = 0;

    ComPtr<ID3D11Texture2D> stagingTexture;
    hr = device->CreateTexture2D(&stagingDesc, nullptr, stagingTexture.GetAddressOf());
    if (FAILED(hr))
    {
        OutputDebugString(L"Failed to create staging texture for heightmap.\n");
        return false;
    }

    // Copiar la textura cargada a la textura de staging
    context->CopyResource(stagingTexture.Get(), sourceTexture.Get());

    // Mapear la textura de staging para leer sus datos
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    hr = context->Map(stagingTexture.Get(), 0, D3D11_MAP_READ, 0, &mappedResource);
    if (FAILED(hr))
    {
        OutputDebugString(L"Failed to map staging texture for heightmap.\n");
        return false;
    }

    m_heightData.resize(m_terrainWidth * m_terrainHeight);
    const BYTE* pPixels = reinterpret_cast<const BYTE*>(mappedResource.pData);

    for (int y = 0; y < m_terrainHeight; ++y)
    {
        for (int x = 0; x < m_terrainWidth; ++x)
        {
            // Asumimos formato R8G8B8A8_UNORM o similar (4 bytes por píxel)
            // Puedes necesitar ajustar esto basado en el formato real de tu imagen
            // El RowPitch es el número de bytes por fila
            UINT pixelOffset = (y * mappedResource.RowPitch) + (x * 4); // Asume 4 bytes/píxel (RGBA)

            // Usamos el canal Rojo como valor de altura (0-255)
            // Si es una imagen en escala de grises, R, G y B deberían ser iguales.
            BYTE heightValue = pPixels[pixelOffset];
            m_heightData[y * m_terrainWidth + x] = static_cast<float>(heightValue) / 255.0f; // Normalizado a 0.0 - 1.0
        }
    }

    context->Unmap(stagingTexture.Get(), 0);

    OutputDebugString(L"Heightmap loaded successfully.\n");
    return true;
}

void Terrain::CalculateNormals()
{
    if (m_vertices.empty() || m_indices.empty()) return;

    // Primero, inicializa todas las normales a (0,1,0) o (0,0,0)
    for (size_t i = 0; i < m_vertices.size(); ++i)
    {
        m_vertices[i].normal = Vector3::Zero;
    }

    // Calcula la normal para cada triángulo y la añade a los vértices que lo componen
    for (size_t i = 0; i < m_indexCount / 3; ++i)
    {
        unsigned long i0 = m_indices[i * 3 + 0];
        unsigned long i1 = m_indices[i * 3 + 1];
        unsigned long i2 = m_indices[i * 3 + 2];

        Vector3 v0 = m_vertices[i0].position;
        Vector3 v1 = m_vertices[i1].position;
        Vector3 v2 = m_vertices[i2].position;

        Vector3 edge1 = v1 - v0;
        Vector3 edge2 = v2 - v0;
        Vector3 faceNormal = edge1.Cross(edge2); // El orden importa para la dirección de la normal

        Vector3 tempNormal;

        tempNormal = XMLoadFloat3(&m_vertices[i0].normal);
        tempNormal += faceNormal;
        XMStoreFloat3(&m_vertices[i0].normal, tempNormal);

        tempNormal = XMLoadFloat3(&m_vertices[i1].normal);
        tempNormal += faceNormal;
        XMStoreFloat3(&m_vertices[i1].normal, tempNormal);

        tempNormal = XMLoadFloat3(&m_vertices[i2].normal);
        tempNormal += faceNormal;
        XMStoreFloat3(&m_vertices[i2].normal, tempNormal);
    }

    // Normaliza todas las normales de los vértices
    for (size_t i = 0; i < m_vertices.size(); ++i)
    {
        Vector3 tempNormal = XMLoadFloat3(&m_vertices[i].normal);
        tempNormal.Normalize();
        XMStoreFloat3(&m_vertices[i].normal, tempNormal);
    }
    OutputDebugString(L"Normals calculated.\n");
}


bool Terrain::InitializeBuffers(ID3D11Device* device)
{
    // Generar la malla de vértices e índices si m_heightData está cargada
    if (m_heightData.empty() || m_terrainWidth == 0 || m_terrainHeight == 0)
    {
        OutputDebugString(L"Height data not loaded, cannot initialize terrain mesh.\n");
        return false;
    }

    m_vertexCount = m_terrainWidth * m_terrainHeight;
    m_indexCount = (m_terrainWidth - 1) * (m_terrainHeight - 1) * 6; // 2 triángulos por quad, 3 índices por triángulo

    m_vertices.resize(m_vertexCount);
    m_indices.resize(m_indexCount);

    // Crear vértices
    for (int j = 0; j < m_terrainHeight; ++j)
    {
        for (int i = 0; i < m_terrainWidth; ++i)
        {
            int index = j * m_terrainWidth + i;
            float height = m_heightData[index] * m_heightScale;

            m_vertices[index].position = Vector3(static_cast<float>(i), height, static_cast<float>(j));
            m_vertices[index].textureCoordinate = Vector2(
                (static_cast<float>(i) / (m_terrainWidth - 1)) * m_textureTilingFactor,
                (static_cast<float>(j) / (m_terrainHeight - 1)) * m_textureTilingFactor
            );
            // La normal se calculará después
        }
    }

    // Crear índice
    int k = 0;
    for (int j = 0; j < m_terrainHeight - 1; ++j)
    {
        for (int i = 0; i < m_terrainWidth - 1; ++i)
        {
            int topLeft = j * m_terrainWidth + i;
            int topRight = topLeft + 1;
            int bottomLeft = (j + 1) * m_terrainWidth + i;
            int bottomRight = bottomLeft + 1;

            m_indices[k++] = topLeft;
            m_indices[k++] = bottomLeft; // Para CCW en LH
            m_indices[k++] = topRight;

            m_indices[k++] = topRight;
            m_indices[k++] = bottomLeft; // Para CCW en LH
            m_indices[k++] = bottomRight;
        }
    }

    CalculateNormals(); // Calcular normales después de tener todas las posiciones de los vértices

    // Crear Vertex Buffer
    D3D11_BUFFER_DESC vertexBufferDesc = {};
    vertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    vertexBufferDesc.ByteWidth = sizeof(TerrainVertex) * m_vertexCount;
    vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA vertexData = {};
    vertexData.pSysMem = m_vertices.data();

    HRESULT hr = device->CreateBuffer(&vertexBufferDesc, &vertexData, m_vertexBuffer.ReleaseAndGetAddressOf());
    if (FAILED(hr)) { OutputDebugString(L"Failed to create terrain vertex buffer.\n"); return false; }

    // Crear Index Buffer
    D3D11_BUFFER_DESC indexBufferDesc = {};
    indexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
    indexBufferDesc.ByteWidth = sizeof(unsigned long) * m_indexCount;
    indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

    D3D11_SUBRESOURCE_DATA indexData = {};
    indexData.pSysMem = m_indices.data();

    hr = device->CreateBuffer(&indexBufferDesc, &indexData, m_indexBuffer.ReleaseAndGetAddressOf());
    if (FAILED(hr)) { OutputDebugString(L"Failed to create terrain index buffer.\n"); return false; }

    OutputDebugString(L"Terrain buffers initialized.\n");
    return true;
}

bool Terrain::LoadTexture(ID3D11Device* device, const wchar_t* filename, ComPtr<ID3D11ShaderResourceView>& textureSRV)
{
    HRESULT hr = CreateWICTextureFromFile(device, filename, nullptr, textureSRV.ReleaseAndGetAddressOf());
    if (FAILED(hr))
    {
        OutputDebugString(L"Failed to load terrain texture.\n");
        return false;
    }
    OutputDebugString(L"Terrain texture loaded.\n");
    return true;
}


bool Terrain::Initialize(ID3D11Device* device, ID3D11DeviceContext* contextForHeightmapLoad,
    const wchar_t* heightmapFilename,
    const wchar_t* textureFilename1, const wchar_t* textureFilename2, const wchar_t* textureFilename3,
    const wchar_t* terrainVS_path, const wchar_t* terrainPS_path)
{
    if (!LoadHeightmap(device, contextForHeightmapLoad, heightmapFilename)) return false;
    if (!InitializeBuffers(device)) return false; // Crea vértices e índices, calcula normales

    if (!LoadTexture(device, textureFilename1, m_textureSRV1)) return false; // Textura base
    if (!LoadTexture(device, textureFilename2, m_textureSRV2)) return false; // Textura baja altitud
    if (!LoadTexture(device, textureFilename3, m_textureSRV3)) return false; // Textura alta altitud

    // --- Cargar Shaders del Terreno ---
    ComPtr<ID3DBlob> vsBlob;
    HRESULT hr = D3DReadFileToBlob(terrainVS_path, vsBlob.GetAddressOf());
    if (FAILED(hr)) { OutputDebugString(L"ERROR: Failed to load Terrain VS file.\n"); return false; }
    hr = device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, m_terrainVS.ReleaseAndGetAddressOf());
    if (FAILED(hr)) { OutputDebugString(L"ERROR: Failed to create Terrain VS.\n"); return false; }

    const D3D11_INPUT_ELEMENT_DESC terrainVertexLayoutDesc[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };
    UINT numElements = ARRAYSIZE(terrainVertexLayoutDesc);

    hr = device->CreateInputLayout(
        terrainVertexLayoutDesc,    // Usar nuestra descripción explícita
        numElements,                // Número de elementos en nuestra descripción
        vsBlob->GetBufferPointer(), // Bytecode del shader
        vsBlob->GetBufferSize(),    // Tamaño del bytecode
        m_inputLayout.ReleaseAndGetAddressOf() // Puntero al Input Layout
    );
    if (FAILED(hr)) {
        OutputDebugString(L"ERROR: Failed to create Terrain Input Layout with explicit descriptor.\n");
        // vsBlob->Release(); // ComPtr lo maneja si la función retorna
        return false;
    }
    // vsBlob ya no se necesita

    ComPtr<ID3DBlob> psBlob;
    hr = D3DReadFileToBlob(terrainPS_path, psBlob.GetAddressOf());
    if (FAILED(hr)) { OutputDebugString(L"ERROR: Failed to load Terrain PS file.\n"); return false; }
    hr = device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, m_terrainPS.ReleaseAndGetAddressOf());
    if (FAILED(hr)) { OutputDebugString(L"ERROR: Failed to create Terrain PS.\n"); return false; }
    // psBlob ya no se necesita

    // --- Crear Constant Buffer para el Vertex Shader del Terreno ---
    D3D11_BUFFER_DESC cbd_vs_terrain = {};
    cbd_vs_terrain.Usage = D3D11_USAGE_DYNAMIC;
    cbd_vs_terrain.ByteWidth = sizeof(CBTerrainVSData);
    cbd_vs_terrain.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbd_vs_terrain.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    hr = device->CreateBuffer(&cbd_vs_terrain, nullptr, m_cbVSTerrainData.ReleaseAndGetAddressOf());
    if (FAILED(hr)) { OutputDebugString(L"ERROR: Failed to create Terrain VS CB.\n"); return false; }

    OutputDebugString(L"Terrain with custom shaders initialized successfully.\n");
    return true;
}

void Terrain::SetWorldMatrix(const Matrix& world) { m_worldMatrix = world; }
void Terrain::SetViewMatrix(const Matrix& view) { m_viewMatrix = view; }
void Terrain::SetProjectionMatrix(const Matrix& projection) { m_projectionMatrix = projection; }

void Terrain::Render(ID3D11DeviceContext* context,
    ID3D11Buffer* lightPropertiesCB,
    ID3D11SamplerState* samplerState,
    const DirectX::SimpleMath::Vector3& cameraPositionWorld // Ya la tienes en LightPropertiesCB, pero la pasamos por si el CB de luces no la tuviera
)
{
    if (!m_vertexBuffer || !m_indexBuffer || !m_terrainVS || !m_terrainPS || !m_inputLayout ||
        !m_textureSRV1 || !m_textureSRV2 || !m_textureSRV3 || !m_cbVSTerrainData ||
        !lightPropertiesCB || !samplerState)
    {
        OutputDebugString(L"Terrain::Render - Missing resources for custom shader rendering.\n");
        return;
    }

    context->IASetInputLayout(m_inputLayout.Get());
    context->VSSetShader(m_terrainVS.Get(), nullptr, 0);
    context->PSSetShader(m_terrainPS.Get(), nullptr, 0);

    // Actualizar Constant Buffer del Vertex Shader
    D3D11_MAPPED_SUBRESOURCE mappedResourceVS;
    CBTerrainVSData* vsDataPtr;
    HRESULT hr = context->Map(m_cbVSTerrainData.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResourceVS);
    if (FAILED(hr)) { /* Log y return */ }
    vsDataPtr = (CBTerrainVSData*)mappedResourceVS.pData;
    vsDataPtr->World = m_worldMatrix; // Asume que m_worldMatrix ya está como el shader la espera (o transponla aquí/shader)
    vsDataPtr->ViewProjection = m_viewMatrix * m_projectionMatrix; // Asume que m_viewMatrix y m_projectionMatrix son correctas
    vsDataPtr->maxTerrainHeightLocal = m_heightScale; // m_heightScale es la Y máxima local si el heightmap va de 0-1
    context->Unmap(m_cbVSTerrainData.Get(), 0);
    context->VSSetConstantBuffers(0, 1, m_cbVSTerrainData.GetAddressOf()); // slot b0

    // Vincular Constant Buffer de Luces (al Pixel Shader)
    context->PSSetConstantBuffers(1, 1, &lightPropertiesCB); // slot b1

    // Vincular Texturas al Pixel Shader
    ID3D11ShaderResourceView* terrainTextures[] = { m_textureSRV1.Get(), m_textureSRV2.Get(), m_textureSRV3.Get() };
    context->PSSetShaderResources(0, 3, terrainTextures); // slots t0, t1, t2

    // Vincular Sampler State al Pixel Shader
    context->PSSetSamplers(0, 1, &samplerState); // slot s0

    // Configurar Buffers y Dibujar (como antes)
    UINT stride = sizeof(TerrainVertex);
    UINT offset = 0;
    context->IASetVertexBuffers(0, 1, m_vertexBuffer.GetAddressOf(), &stride, &offset);
    context->IASetIndexBuffer(m_indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    context->DrawIndexed(m_indexCount, 0, 0);

    // (Opcional) Desvincular texturas para no afectar otros dibujados
    ID3D11ShaderResourceView* nullSRVs[3] = { nullptr, nullptr, nullptr };
    context->PSSetShaderResources(0, 3, nullSRVs);
}

int Terrain::GetTerrainWidth() const { return m_terrainWidth; }
int Terrain::GetTerrainHeight() const { return m_terrainHeight; }

void Terrain::UpdateGlobalLighting(
    const Vector3& dirLightDirection,
    const Vector4& dirLightColor,
    const Vector4& ambientLightColor,
    const Vector4& dirLightSpecularColor)
{
    if (m_effect) {
        m_effect->SetLightingEnabled(true); // Asegura que la iluminación esté activa
        m_effect->SetAmbientLightColor(ambientLightColor);

        // BasicEffect puede manejar hasta 3 luces direccionales. Usaremos la primera (índice 0).
        m_effect->SetLightEnabled(0, true);
        m_effect->SetLightDirection(0, dirLightDirection); // Dirección HACIA la luz
        m_effect->SetLightDiffuseColor(0, dirLightColor); // Color difuso de la luz
        m_effect->SetLightSpecularColor(0, dirLightSpecularColor); // Color especular de la luz
    }
}


bool Terrain::GetWorldHeightAt(float worldX, float worldZ, float& outHeight) const
{
    if (m_heightData.empty() || m_terrainWidth <= 1 || m_terrainHeight <= 1) {
        return false; // No hay datos de terreno o es demasiado pequeño
    }

    // Para transformar de coordenadas del mundo (worldX, worldZ) a coordenadas de la cuadrícula del heightmap (gridI, gridJ)
    // necesitamos resolver el sistema:
    // worldX = gridI * m_worldMatrix._11 + gridJ * m_worldMatrix._31 + m_worldMatrix._41 
    // worldZ = gridI * m_worldMatrix._13 + gridJ * m_worldMatrix._33 + m_worldMatrix._43
    // (Asumiendo que la componente Y de la posición local del grid es 0 para este mapeo XZ)

    // Despejamos las componentes de traslación
    float tx = worldX - m_worldMatrix._41;
    float tz = worldZ - m_worldMatrix._43;

    // Coeficientes de la matriz para XZ
    float m11 = m_worldMatrix._11;
    float m13 = m_worldMatrix._13; // Corresponde a cómo la J de la cuadrícula afecta a worldX
    float m31 = m_worldMatrix._31; // Corresponde a cómo la I de la cuadrícula afecta a worldZ
    float m33 = m_worldMatrix._33;

    // Calculamos el determinante de la submatriz 2x2 de XZ
    float determinant = m11 * m33 - m31 * m13;

    if (abs(determinant) < 0.0001f) { // El determinante es cero o muy pequeño, la matriz no es invertible para XZ
        return false;
    }

    float invDeterminant = 1.0f / determinant;
    float gridI = (tx * m33 - tz * m31) * invDeterminant;
    float gridJ = (m11 * tz - m13 * tx) * invDeterminant;

    // --- Comprobación de Límites ---
    if (gridI < 0 || gridI >= m_terrainWidth - 1 || gridJ < 0 || gridJ >= m_terrainHeight - 1) {
        // El punto está fuera de los límites del heightmap definidos
        // Puedes devolver una altura por defecto o la altura del borde más cercano
        // Por simplicidad, retornamos false indicando que está fuera.
        return false;
    }

    // --- Interpolación Bilineal ---
    int x0 = static_cast<int>(floor(gridI));
    int z0 = static_cast<int>(floor(gridJ)); // Usamos z0 para la dimensión J de la cuadrícula

    // Asegurarnos de no salirnos de los límites al acceder a x0+1, z0+1
    int x1 = std::min(x0 + 1, m_terrainWidth - 1);
    int z1 = std::min(z0 + 1, m_terrainHeight - 1);

    // Fracciones para la interpolación
    float fracI = gridI - x0;
    float fracJ = gridJ - z0;

    // Obtener las alturas normalizadas (0-1) de los 4 puntos de la cuadrícula circundantes
    float h00 = m_heightData[z0 * m_terrainWidth + x0];
    float h10 = m_heightData[z0 * m_terrainWidth + x1]; // Altura en (x0+1, z0)
    float h01 = m_heightData[z1 * m_terrainWidth + x0]; // Altura en (x0, z0+1)
    float h11 = m_heightData[z1 * m_terrainWidth + x1]; // Altura en (x0+1, z0+1)

    // Interpolar en I (o X de la cuadrícula)
    float interpBottom = Lerp(h00, h10, fracI);
    float interpTop = Lerp(h01, h11, fracI);

    // Interpolar en J (o Z de la cuadrícula)
    float interpolatedNormalizedHeight = Lerp(interpBottom, interpTop, fracJ); // Altura normalizada (0-1)

    // --- Aplicar m_heightScale y la transformación Y de la matriz del mundo ---
    // Esta es la altura Y en el espacio local del mesh del terreno, después de m_heightScale
    float localScaledHeightY = interpolatedNormalizedHeight * m_heightScale;

    // Ahora, transformamos esta altura Y al espacio del mundo usando la matriz del mundo.
    // La altura final Y = gridI_original * M._12 + localScaledHeightY * M._22 + gridJ_original * M._32 + M._42
    // Donde gridI y gridJ son las coordenadas locales que ya calculamos.
    outHeight = gridI * m_worldMatrix._12 +
        localScaledHeightY * m_worldMatrix._22 +
        gridJ * m_worldMatrix._32 +
        m_worldMatrix._42;

    return true;
}