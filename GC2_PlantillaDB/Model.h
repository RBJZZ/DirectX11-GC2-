#pragma once

#include <string>
#include <vector>
#include <memory> 

#include <d3d11.h>
#include <wrl.h>      
#include <SimpleMath.h> 

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <Effects.h>
#include <DirectXCollision.h>
#include <vector>


// Estructura de vértice para nuestros modelos.
// Puedes usar DirectX::VertexPositionNormalTexture directamente si prefieres.
struct ModelVertex
{
    DirectX::SimpleMath::Vector3 position; // POSITION
    DirectX::SimpleMath::Vector2 texCoord; // TEXCOORD0
    DirectX::SimpleMath::Vector3 normal;   // NORMAL
};

struct VSPerObjectData
{
    DirectX::SimpleMath::Matrix World;
    DirectX::SimpleMath::Matrix ViewProjection;
    DirectX::SimpleMath::Matrix WorldInverseTranspose;
};

struct PSLightPropertiesData // Esta estructura la llenará y actualizará la clase Game
{
    DirectX::SimpleMath::Vector3 cameraPositionWorld;
    float _pad0; // Padding
    DirectX::SimpleMath::Vector3 directionalLightVector; // Hacia la fuente de luz
    float _pad1; // Padding
    DirectX::SimpleMath::Vector4 directionalLightColor;
    DirectX::SimpleMath::Vector4 ambientLightColor;
};

struct PSMaterialPropertiesData
{
    DirectX::SimpleMath::Vector4 materialDiffuseColor;
    DirectX::SimpleMath::Vector4 materialSpecularColor;
    float specularPower;
    DirectX::SimpleMath::Vector3 _padding_material; // Padding para alinear a 16 bytes
};

struct CB_VS_Evolving_Data {
    DirectX::SimpleMath::Matrix World;
    DirectX::SimpleMath::Matrix ViewProjection;
    DirectX::SimpleMath::Matrix LightViewProjection;
    DirectX::SimpleMath::Matrix WorldInverseTranspose;
};

struct CB_VS_Shadow_Data {
    DirectX::SimpleMath::Matrix World;
    DirectX::SimpleMath::Matrix LightViewProjection;
};


class Model
{
public:
    Model();
    ~Model(); // Importante para liberar recursos si es necesario

    // Carga un modelo desde un archivo.
    // 'device' es para crear buffers y texturas.
    // 'context' se puede usar para carga inmediata de texturas con CreateWICTextureFromFile.
    bool Load(ID3D11Device* device, ID3D11DeviceContext* context, const std::string& filename);

    // Dibuja todas las mallas del modelo.
    // Necesitará las matrices de vista y proyección de la cámara.
    // 'effect' podría ser un efecto global o cada malla podría manejar el suyo.
    // Empezaremos usando un BasicEffect interno para simplificar.
    void Draw(ID3D11DeviceContext* context,
        const DirectX::SimpleMath::Matrix& viewMatrix,
        const DirectX::SimpleMath::Matrix& projectionMatrix,
        ID3D11Buffer* lightPropertiesCB, // CB de luces globales
        ID3D11SamplerState* samplerState // Sampler global
    /*, bool wireframe = false */);

    void EvolvingDraw(ID3D11DeviceContext* context,
        const DirectX::SimpleMath::Matrix& viewMatrix,
        const DirectX::SimpleMath::Matrix& projectionMatrix,
        ID3D11Buffer* lightPropertiesCB,
        ID3D11SamplerState* samplerState,
        const DirectX::SimpleMath::Matrix& lightViewMatrix,
        const DirectX::SimpleMath::Matrix& lightProjectionMatrix,
        ID3D11ShaderResourceView* shadowMapSRV,
        ID3D11SamplerState* shadowSampler
    );

    // --- MÉTODOS PARA GESTIONAR TRANSFORMACIONES INDIVIDUALES ---
    void SetPosition(const DirectX::SimpleMath::Vector3& position);
    void SetPosition(float x, float y, float z);
    const DirectX::SimpleMath::Vector3& GetPosition() const;

    // Usaremos Quaternions para la rotación
    void SetRotation(const DirectX::SimpleMath::Quaternion& rotation);
    void SetRotationEuler(const DirectX::SimpleMath::Vector3& eulerAngles); // ej. eulerAngles.x = pitch, .y = yaw, .z = roll
    void SetRotationEuler(float pitch, float yaw, float roll); // Sobrecarga para conveniencia
    void Rotate(const DirectX::SimpleMath::Quaternion& deltaRotation); // Acumula rotación
    const DirectX::SimpleMath::Quaternion& GetRotation() const;

    void SetScale(const DirectX::SimpleMath::Vector3& scale);
    void SetScale(float uniformScale); // Escala uniforme
    void SetScale(float sx, float sy, float sz);

    const DirectX::SimpleMath::Vector3& GetScale() const;


    void SetWorldMatrix(const DirectX::SimpleMath::Matrix& worldMatrix);
    const DirectX::SimpleMath::Matrix& GetWorldMatrix() const;

    bool LoadDebugShaders(ID3D11Device* device, const wchar_t* vsFilename, const wchar_t* psFilename);
    void DebugDraw(ID3D11DeviceContext* context,
        const DirectX::SimpleMath::Matrix& viewMatrix,
        const DirectX::SimpleMath::Matrix& projectionMatrix);

    bool LoadEvolvingShaders(ID3D11Device* device, const wchar_t* vsFilename, const wchar_t* psFilename);
    void CalculateOverallBoundingSphere();
    bool CheckCollisionAgainstParts(
        const DirectX::BoundingBox& worldSpaceQueryBox,
        const DirectX::SimpleMath::Matrix& instanceWorldMatrix, // <--- ¡Este parámetro es crucial!
        std::vector<DirectX::BoundingBox>& boxesToDrawDebug,
        bool shouldAddDebugBox) const;
    const DirectX::BoundingSphere& GetOverallLocalBoundingSphere() const;
    DirectX::BoundingSphere GetOverallWorldBoundingSphere() const;

    void ShadowDraw(
        ID3D11DeviceContext* context,
        const DirectX::SimpleMath::Matrix& worldMatrix,
        const DirectX::SimpleMath::Matrix& lightViewMatrix,
        const DirectX::SimpleMath::Matrix& lightProjectionMatrix
    );

    void ShadowDrawAlphaClip(
        ID3D11DeviceContext* context,
        const DirectX::SimpleMath::Matrix& worldMatrix,
        const DirectX::SimpleMath::Matrix& lightViewMatrix,
        const DirectX::SimpleMath::Matrix& lightProjectionMatrix,
        ID3D11SamplerState* sampler
    );
private:
    // Estructura para representar una parte de la malla (sub-malla) de un modelo
    struct MeshPart
    {
        Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer;
        Microsoft::WRL::ComPtr<ID3D11Buffer> indexBuffer;
        UINT indexCount = 0;
        UINT vertexCount = 0;
        UINT materialIndex = 0;
        DirectX::SimpleMath::Matrix localNodeTransform;
        DirectX::BoundingBox localAABB;

        bool InitializeBuffers(ID3D11Device* device, const std::vector<ModelVertex>& vertices, const std::vector<uint32_t>& indices);
        void DrawPrim(ID3D11DeviceContext* context);
    };

    // Estructura simplificada para el material
    struct Material
    {
        std::wstring diffuseTexturePath; // Ruta a la textura difusa leída del modelo
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> diffuseTextureSRV;
        DirectX::SimpleMath::Vector4 diffuseColor = DirectX::SimpleMath::Vector4(0.8f, 0.8f, 0.8f, 1.0f); // Color por defecto
        DirectX::SimpleMath::Vector4 specularColor = DirectX::SimpleMath::Vector4(0.2f, 0.2f, 0.2f, 1.0f);
        float specularPower = 32.0f;
        // Podríamos añadir más: ambientColor, emissiveColor, normalMapSRV, etc.
    };

    // Funciones de ayuda para procesar la escena de Assimp (serán privadas)
    void ProcessNode(ID3D11Device* device, ID3D11DeviceContext* context, aiNode* node, const aiScene* scene, const DirectX::SimpleMath::Matrix& parentTransform);
    void ProcessMesh(ID3D11Device* device, ID3D11DeviceContext* context, aiMesh* mesh, const aiScene* scene, const DirectX::SimpleMath::Matrix& transform);
    void LoadMaterials(ID3D11Device* device, ID3D11DeviceContext* context, const aiScene* scene);
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> LoadTextureFromFile(ID3D11Device* device, ID3D11DeviceContext* context, const std::string& textureFilenameInModel);


    std::vector<MeshPart> m_meshParts;   // Todas las mallas que componen este modelo
    std::vector<Material> m_materials; // Todos los materiales usados por este modelo
    std::string m_modelDirectory;      // Directorio base del archivo del modelo, para resolver rutas relativas de texturas

    // Para renderizado simple inicial, usaremos un BasicEffect para todas las mallas.
    // Más adelante, cada material podría tener su propia configuración de shader.
    std::unique_ptr<DirectX::BasicEffect> m_effect;
    // Si usamos nuestro ModelVertex personalizado, necesitaremos un InputLayout para él.

    // Model transformations
    DirectX::SimpleMath::Matrix m_worldMatrix;
    DirectX::SimpleMath::Vector3 m_position;
    DirectX::SimpleMath::Quaternion m_rotation; // Usamos Quaternion para rotación
    DirectX::SimpleMath::Vector3 m_scale;

    Microsoft::WRL::ComPtr<ID3D11VertexShader> m_vertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>  m_pixelShader;
    Microsoft::WRL::ComPtr<ID3D11InputLayout>  m_inputLayout; // Input layout para ModelVertex y nuestro VS

    Microsoft::WRL::ComPtr<ID3D11Buffer> m_cbVS_PerObject;       // Para World, ViewProjection, WorldInvTranspose
    Microsoft::WRL::ComPtr<ID3D11Buffer> m_cbPS_MaterialProperties;

    Microsoft::WRL::ComPtr<ID3D11VertexShader> m_debugVertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>  m_debugPixelShader;
    Microsoft::WRL::ComPtr<ID3D11InputLayout>  m_debugInputLayout;
    Microsoft::WRL::ComPtr<ID3D11Buffer>       m_cbVS_Debug_WVP;

    Microsoft::WRL::ComPtr<ID3D11VertexShader> m_evolvingVertexShader;
    Microsoft::WRL::ComPtr<ID3D11PixelShader>  m_evolvingPixelShader;
    Microsoft::WRL::ComPtr<ID3D11InputLayout>  m_evolvingInputLayout;

    Microsoft::WRL::ComPtr<ID3D11Buffer> m_cbVS_Evolving_WVP;

    DirectX::BoundingSphere m_localBoundingSphere;
    DirectX::BoundingSphere m_overallLocalBoundingSphere;

    Microsoft::WRL::ComPtr<ID3D11Buffer> m_cbVS_Shadow;

    void UpdateWorldMatrix();

};