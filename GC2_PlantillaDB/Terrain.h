#pragma once

#include <d3d11.h>
#include <wrl.h> 
#include <SimpleMath.h>
#include <vector>
#include <string>
#include <VertexTypes.h>
#include <Effects.h>
#include "Model.h"

// Estructura de vértice para el terreno (puedes expandirla después)
using TerrainVertex = DirectX::VertexPositionNormalTexture;

struct CBTerrainPSMaterialData
{
    // Control de Altura
    float dirtMaxHeight = 0.05f;      
    float grassMaxHeight = 0.40f;     
    float blendRange = 0.05f;         

    float rockSlopeThreshold = 0.45f; 

    DirectX::SimpleMath::Vector4 padding;
};

class Terrain
{
public:
    Terrain();
    ~Terrain();

    bool Initialize(ID3D11Device* device, ID3D11DeviceContext* contextForHeightmapLoad,
        const wchar_t* heightmapFilename,
        const wchar_t* textureFilename1, 
        const wchar_t* textureFilename2, 
        const wchar_t* textureFilename3, 
        const wchar_t* terrainVS_path,   
        const wchar_t* terrainPS_path);

    void Render(ID3D11DeviceContext* context,
        ID3D11Buffer* lightPropertiesCB,
        ID3D11SamplerState* samplerState,
        const DirectX::SimpleMath::Vector3& cameraPositionWorld,
        const DirectX::SimpleMath::Matrix& lightViewProjMatrix,
        ID3D11ShaderResourceView* shadowMapSRV,
        ID3D11SamplerState* shadowSampler
    );

    struct CBTerrainVSData
    {
        DirectX::SimpleMath::Matrix World;
        DirectX::SimpleMath::Matrix ViewProjection;
        DirectX::SimpleMath::Matrix LightViewProjection;
        DirectX::SimpleMath::Matrix WorldInverseTranspose;
        float maxTerrainHeightLocal;
        DirectX::SimpleMath::Vector3 padding;
    };


    void SetWorldMatrix(const DirectX::SimpleMath::Matrix& world);
    void SetViewMatrix(const DirectX::SimpleMath::Matrix& view);
    void SetProjectionMatrix(const DirectX::SimpleMath::Matrix& projection);
    int GetTerrainWidth() const;
    int GetTerrainHeight() const;

    void UpdateGlobalLighting(
        const DirectX::SimpleMath::Vector3& dirLightDirection,
        const DirectX::SimpleMath::Vector4& dirLightColor, 
        const DirectX::SimpleMath::Vector4& ambientLightColor,
        const DirectX::SimpleMath::Vector4& dirLightSpecularColor 
    );

    bool GetWorldHeightAt(float worldX, float worldZ, float& outHeight) const;
    void ShadowDraw(
        ID3D11DeviceContext* context,
        const DirectX::SimpleMath::Matrix& lightViewMatrix,
        const DirectX::SimpleMath::Matrix& lightProjectionMatrix
    );

private:
    bool LoadHeightmap(ID3D11Device* device, ID3D11DeviceContext* context, const wchar_t* filename);
    void CalculateNormals();
    bool InitializeBuffers(ID3D11Device* device);
    bool LoadTexture(ID3D11Device* device, const wchar_t* filename, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& textureSRV);
    float m_textureTilingFactor;

    int m_terrainWidth;
    int m_terrainHeight; // Dimensiones de la malla del terreno (basadas en el heightmap)
    float m_heightScale;  // Escala para la altura del terreno

    Microsoft::WRL::ComPtr<ID3D11Buffer> m_vertexBuffer;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_indexBuffer;
    int m_vertexCount;
    int m_indexCount;

    std::vector<float> m_heightData; // Almacenará los valores de altura
    std::vector<TerrainVertex> m_vertices;
    std::vector<unsigned long> m_indices;

    // Recursos para el renderizado (empezaremos con BasicEffect)
    std::unique_ptr<DirectX::BasicEffect> m_effect;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_textureSRV1;
    // Más texturas para multitextura después:
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_textureSRV2; 
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_textureSRV3;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_textureSRV_Rock;
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_cbVSTerrainData;

    Microsoft::WRL::ComPtr<ID3D11InputLayout> m_inputLayout;

    DirectX::SimpleMath::Matrix m_worldMatrix;
    DirectX::SimpleMath::Matrix m_viewMatrix;       
    DirectX::SimpleMath::Matrix m_projectionMatrix; 

    Microsoft::WRL::ComPtr<ID3D11VertexShader> m_terrainVS;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>  m_terrainPS;

    Microsoft::WRL::ComPtr<ID3D11Buffer> m_cbVS_ShadowPass;

    Microsoft::WRL::ComPtr<ID3D11Buffer> m_cbPSTerrainMaterial; 
    CBTerrainPSMaterialData m_terrainMaterialData;
};