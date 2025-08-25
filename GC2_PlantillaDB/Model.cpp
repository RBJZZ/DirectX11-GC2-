#include "pch.h" 
#include "Model.h"

#include <Effects.h>          
#include <CommonStates.h>    
#include <WICTextureLoader.h> 
#include <d3dcompiler.h>


#pragma comment(lib, "d3dcompiler.lib")

// Usar los tipos de SimpleMath directamente para claridad
using namespace DirectX::SimpleMath;
using Microsoft::WRL::ComPtr;

// --- Implementación de la clase Model ---
// HELPERS
std::wstring StringToWString(const std::string& s)
{
    if (s.empty()) return std::wstring();
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &s[0], (int)s.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &s[0], (int)s.size(), &wstrTo[0], size_needed);
    return wstrTo;
}

inline Matrix ConvertAssimpMatrix(const aiMatrix4x4& aiMat)
{
    return Matrix(
        aiMat.a1, aiMat.a2, aiMat.a3, aiMat.a4,
        aiMat.b1, aiMat.b2, aiMat.b3, aiMat.b4,
        aiMat.c1, aiMat.c2, aiMat.c3, aiMat.c4,
        aiMat.d1, aiMat.d2, aiMat.d3, aiMat.d4
    );
}

Model::Model() :
    m_worldMatrix(Matrix::Identity),
    m_position(Vector3::Zero),              
    m_rotation(Quaternion::Identity),       
    m_scale(Vector3(1.0f, 1.0f, 1.0f))
    // m_effect y m_inputLayout se inicializarán en Load()
{
    UpdateWorldMatrix();
}

Model::~Model()
{
    // Los ComPtr y unique_ptr se encargarán de liberar sus recursos automáticamente.
}

void Model::SetWorldMatrix(const Matrix& worldMatrix)
{
    m_worldMatrix = worldMatrix;
}

const Matrix& Model::GetWorldMatrix() const
{
    return m_worldMatrix;
}


bool Model::Load(ID3D11Device* device, ID3D11DeviceContext* context, const std::string& filename)
{
    // HRESULT hr; // Puedes declarar hr localmente donde lo necesites, o mantenerlo si se usa más abajo.

    // --- ELIMINA ESTA SECCIÓN COMPLETA (1. CARGAR Y CREAR VERTEX SHADER E INPUT LAYOUT) ---
    /*
    ComPtr<ID3DBlob> vsBlob; // Blob para el bytecode del Vertex Shader
    // Las variables 'vsFilename' y 'psFilename' ya no son parámetros de esta función.
    // HRESULT hr = D3DReadFileToBlob(vsFilename, vsBlob.GetAddressOf());
    // if (FAILED(hr))
    // {
    //     wchar_t errorMsg[512];
    //     swprintf_s(errorMsg, L"ERROR::MODEL::Failed to load Vertex Shader file: %s. HRESULT: 0x%08X\n",
    //                vsFilename, static_cast<unsigned int>(hr));
    //     OutputDebugString(errorMsg);
    //     // ¡IMPORTANTE! Si decides retornar aquí en la versión original,
    //     // debes considerar si quieres que Load falle si los shaders no se cargan,
    //     // o si la carga de shaders es responsabilidad de otra función.
    //     // En nuestro nuevo enfoque, esta función NO carga shaders.
    // }
    // hr = device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, m_vertexShader.ReleaseAndGetAddressOf());
    // if (FAILED(hr))
    // {
    //     // vsBlob->Release(); // ComPtr lo hace
    //     OutputDebugString(L"ERROR::MODEL::Failed to create Vertex Shader.\n");
    //     return false;
    // }

    // const D3D11_INPUT_ELEMENT_DESC modelVertexLayoutDesc[] =
    // {
    //     { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    //     { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
    //     { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    //     // Si tu ModelVertex tiene TANGENT, necesitarías añadirlo aquí también para el layout original
    // };
    // hr = device->CreateInputLayout(
    //     modelVertexLayoutDesc, ARRAYSIZE(modelVertexLayoutDesc),
    //     vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
    //     m_inputLayout.ReleaseAndGetAddressOf()
    // );
    // if (FAILED(hr))
    // {
    //     OutputDebugString(L"ERROR::MODEL::Failed to create Input Layout.\n");
    //     return false;
    // }
    */

    // --- ELIMINA ESTA SECCIÓN COMPLETA (2. CARGAR Y CREAR PIXEL SHADER) ---
    /*
    ComPtr<ID3DBlob> psBlob; // Blob para el bytecode del Pixel Shader
    // hr = D3DReadFileToBlob(psFilename, psBlob.GetAddressOf());
    // if (FAILED(hr))
    // {
    //     OutputDebugString(L"ERROR::MODEL::Failed to load Pixel Shader file: ");
    //     // OutputDebugString(psFilename); OutputDebugString(L"\n"); // psFilename ya no es parámetro
    //     return false;
    // }
    // hr = device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, m_pixelShader.ReleaseAndGetAddressOf());
    // if (FAILED(hr))
    // {
    //     OutputDebugString(L"ERROR::MODEL::Failed to create Pixel Shader.\n");
    //     return false;
    // }
    */

    // --- ELIMINA ESTA SECCIÓN COMPLETA (3. CREAR CONSTANT BUFFERS PRINCIPALES) ---
    // (m_cbVS_PerObject y m_cbPS_MaterialProperties)
    // Nota: La creación de m_cbPS_MaterialProperties se moverá a LoadEvolvingShaders
    // si es que EvolvingPS.hlsl lo necesita (y sí lo necesita para la iluminación).
    /*
    D3D11_BUFFER_DESC cbd = {};
    cbd.Usage = D3D11_USAGE_DYNAMIC;
    cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    cbd.MiscFlags = 0;
    cbd.StructureByteStride = 0;

    // Constant Buffer para el Vertex Shader (PerObject: World, ViewProjection, WorldInverseTranspose)
    cbd.ByteWidth = sizeof(VSPerObjectData);
    hr = device->CreateBuffer(&cbd, nullptr, m_cbVS_PerObject.ReleaseAndGetAddressOf());
    if (FAILED(hr)) { OutputDebugString(L"Error creating VS PerObject CB\n"); return false; }

    // Constant Buffer para el Pixel Shader (MaterialProperties)
    // ¡ESTE CB (m_cbPS_MaterialProperties) SÍ LO NECESITA TU EvolvingPS.hlsl!
    // Su creación se moverá a Model::LoadEvolvingShaders() o una función similar.
    cbd.ByteWidth = sizeof(PSMaterialPropertiesData);
    hr = device->CreateBuffer(&cbd, nullptr, m_cbPS_MaterialProperties.ReleaseAndGetAddressOf());
    if (FAILED(hr)) { OutputDebugString(L"Error creating PS Material CB\n"); return false; }
    */

    // --- 4. CARGAR GEOMETRÍA Y MATERIALES DEL MODELO CON ASSIMP (ESTA SECCIÓN SE MANTIENE) ---

    D3D11_BUFFER_DESC cbd_shadow = {};
    cbd_shadow.Usage = D3D11_USAGE_DYNAMIC;
    cbd_shadow.ByteWidth = sizeof(CB_VS_Shadow_Data); // Solo contendrá una matriz WVP
    cbd_shadow.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbd_shadow.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    HRESULT hr = device->CreateBuffer(&cbd_shadow, nullptr, m_cbVS_Shadow.ReleaseAndGetAddressOf());
    if (FAILED(hr))
    {
        OutputDebugString(L"ERROR::MODEL::Failed to create Shadow VS Constant Buffer.\n");
        return false; 
    }

    Assimp::Importer importer;
    unsigned int postProcessSteps =
        aiProcess_Triangulate |
        aiProcess_ConvertToLeftHanded |     // Asegúrate de que esto sea lo que quieres para tu sistema de coordenadas
        aiProcess_GenSmoothNormals |        // Genera normales si no existen o para suavizar
        aiProcess_CalcTangentSpace |        // Importante si vas a usar Normal Mapping después
        aiProcess_JoinIdenticalVertices |   // Optimización
        aiProcess_SortByPType;              // Optimización

    const aiScene* scene = importer.ReadFile(filename, postProcessSteps);
    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode)
    {
        OutputDebugStringA("ERROR::ASSIMP::");
        OutputDebugStringA(importer.GetErrorString());
        OutputDebugStringA("\n");
        return false;
    }
    m_modelDirectory = filename.substr(0, filename.find_last_of("/\\"));

    // Estas funciones ahora solo cargan datos de material/textura y procesan la geometría
    // en tus m_meshParts y m_materials.
    LoadMaterials(device, context, scene);
    ProcessNode(device, context, scene->mRootNode, scene, Matrix::Identity);

    // El mensaje de éxito ya no mencionará "using custom shaders" porque esta función ya no los carga.
    CalculateOverallBoundingSphere();
    OutputDebugStringA("Model geometry and materials loaded successfully: ");
    OutputDebugStringA(filename.c_str()); OutputDebugStringA("\n");
    return true;
}

// --- Implementación de MeshPart::InitializeBuffers ---
bool Model::MeshPart::InitializeBuffers(ID3D11Device* device, const std::vector<ModelVertex>& vertices, const std::vector<uint32_t>& indices)
{
    if (vertices.empty() || indices.empty())
    {
        return false; // No hay nada que buferizar
    }

    this->indexCount = static_cast<UINT>(indices.size());
    this->vertexCount = static_cast<UINT>(vertices.size());

    D3D11_BUFFER_DESC vbd = {};
    vbd.Usage = D3D11_USAGE_DEFAULT;
    vbd.ByteWidth = sizeof(ModelVertex) * this->vertexCount;
    vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
    vbd.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA vinitData = {};
    vinitData.pSysMem = vertices.data();

    HRESULT hr = device->CreateBuffer(&vbd, &vinitData, vertexBuffer.ReleaseAndGetAddressOf());
    if (FAILED(hr))
    {
        OutputDebugString(L"ERROR::MODEL::MESH_PART::Failed to create vertex buffer.\n");
        return false;
    }

    D3D11_BUFFER_DESC ibd = {};
    ibd.Usage = D3D11_USAGE_DEFAULT;
    ibd.ByteWidth = sizeof(uint32_t) * this->indexCount;
    ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
    ibd.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA iinitData = {};
    iinitData.pSysMem = indices.data();

    hr = device->CreateBuffer(&ibd, &iinitData, indexBuffer.ReleaseAndGetAddressOf());
    if (FAILED(hr))
    {
        OutputDebugString(L"ERROR::MODEL::MESH_PART::Failed to create index buffer.\n");
        return false;
    }
    return true;
}

// --- Stubs para los métodos que implementaremos a continuación ---

void Model::ProcessNode(ID3D11Device* device, ID3D11DeviceContext* context, aiNode* node, const aiScene* scene, const Matrix& parentTransform)
{
    // Obtener la transformación local del nodo y combinarla con la de su padre
    Matrix localNodeTransform = ConvertAssimpMatrix(node->mTransformation);
    Matrix currentFullTransform = localNodeTransform * parentTransform;

    // Procesar todas las mallas asociadas con este nodo
    for (UINT i = 0; i < node->mNumMeshes; ++i)
    {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]]; // El nodo solo contiene índices a las mallas en la escena global
        ProcessMesh(device, context, mesh, scene, currentFullTransform);
    }

    // Procesar recursivamente todos los hijos de este nodo
    for (UINT i = 0; i < node->mNumChildren; ++i)
    {
        ProcessNode(device, context, node->mChildren[i], scene, currentFullTransform);
    }
}

void Model::ProcessMesh(ID3D11Device* device, ID3D11DeviceContext* context, aiMesh* mesh, const aiScene* scene, const Matrix& currentFullNodeTransform)
{
    std::vector<ModelVertex> verticesForRendering;
    std::vector<uint32_t> indices;
    std::vector<DirectX::SimpleMath::Vector3> positionsForAABB;

    // Extraer datos de los vértices
    for (UINT i = 0; i < mesh->mNumVertices; ++i)
    {
        ModelVertex vertex;

        // Posiciones
        vertex.position.x = mesh->mVertices[i].x;
        vertex.position.y = mesh->mVertices[i].y;
        vertex.position.z = mesh->mVertices[i].z;

        // Normales (si existen)
        if (mesh->HasNormals())
        {
            vertex.normal.x = mesh->mNormals[i].x;
            vertex.normal.y = mesh->mNormals[i].y;
            vertex.normal.z = mesh->mNormals[i].z;
        }
        else
        {
            vertex.normal = Vector3(0.0f, 1.0f, 0.0f); // Normal por defecto si no hay
        }

        // Coordenadas de Textura (usamos el primer canal, si existe)
        if (mesh->HasTextureCoords(0)) // mTextureCoords[0] significa el primer set de UVs
        {
            vertex.texCoord.x = mesh->mTextureCoords[0][i].x;
            vertex.texCoord.y = mesh->mTextureCoords[0][i].y;
        }
        else
        {
            vertex.texCoord = Vector2(0.0f, 0.0f); // UVs por defecto
        }

        // Aquí podrías extraer tangentes y bitangentes si mesh->HasTangentsAndBitangents() es true
        // y si tu ModelVertex tiene miembros para ellas.
        // vertex.tangent.x = mesh->mTangents[i].x; ...
        // vertex.bitangent.x = mesh->mBitangents[i].x; ...

        verticesForRendering.push_back(vertex);
    }

    // Extraer índices de las caras (asumimos que son triángulos debido a aiProcess_Triangulate)
    for (UINT i = 0; i < mesh->mNumFaces; ++i)
    {
        aiFace face = mesh->mFaces[i];
        for (UINT j = 0; j < face.mNumIndices; ++j) // Debería ser 3 por triángulo
        {
            indices.push_back(face.mIndices[j]);
        }
    }

    // Crear y almacenar el MeshPart
    MeshPart newMeshPart;
    newMeshPart.localNodeTransform = currentFullNodeTransform; // Esta es la transformación acumulada hasta este nodo/malla
    newMeshPart.materialIndex = mesh->mMaterialIndex;
    if (newMeshPart.InitializeBuffers(device, verticesForRendering, indices))
    {
        m_meshParts.push_back(std::move(newMeshPart)); // Usar std::move para eficiencia
    }
    else
    {
        OutputDebugString(L"ERROR::MODEL::PROCESS_MESH::Failed to initialize buffers for a mesh part.\n");
    }

    if (!positionsForAABB.empty()) {
        DirectX::BoundingBox::CreateFromPoints(newMeshPart.localAABB,
            positionsForAABB.size(),
            positionsForAABB.data(),
            sizeof(DirectX::SimpleMath::Vector3));
    }

    else {
        newMeshPart.localAABB.Center = DirectX::SimpleMath::Vector3::Zero;
        newMeshPart.localAABB.Extents = DirectX::SimpleMath::Vector3::Zero; // Caja inválida
    }

    if (newMeshPart.InitializeBuffers(device, verticesForRendering, indices)) {
        m_meshParts.push_back(std::move(newMeshPart));
    }
    else { 
    
        OutputDebugString(L"ERROR::MODEL::PROCESS_MESH::Failed to store vertices for collision.\n");
    }
}

void Model::LoadMaterials(ID3D11Device* device, ID3D11DeviceContext* context, const aiScene* scene)
{
    m_materials.resize(scene->mNumMaterials);
    for (UINT i = 0; i < scene->mNumMaterials; ++i)
    {
        aiMaterial* pMaterial = scene->mMaterials[i];
        Material currentMaterial; // Nuestra estructura de material

        aiString texturePathAi;
        // Cargar textura difusa (aiTextureType_DIFFUSE es el tipo más común)
        if (pMaterial->GetTexture(aiTextureType_DIFFUSE, 0, &texturePathAi) == AI_SUCCESS)
        {
            // texturePathAi.C_Str() da un const char*
            // Lo convertimos a wstring para usarlo con LoadTextureFromFile (que espera wstring)
            currentMaterial.diffuseTexturePath = StringToWString(std::string(texturePathAi.C_Str()));
            currentMaterial.diffuseTextureSRV = LoadTextureFromFile(device, context, std::string(texturePathAi.C_Str()));
        }
        else
        {
            currentMaterial.diffuseTextureSRV = nullptr; // No hay textura difusa
        }

        aiColor4D color;
        // Color Difuso del material
        if (aiGetMaterialColor(pMaterial, AI_MATKEY_COLOR_DIFFUSE, &color) == AI_SUCCESS)
        {
            currentMaterial.diffuseColor = Vector4(color.r, color.g, color.b, 1.0f);
        }
        // Color Especular del material
        if (aiGetMaterialColor(pMaterial, AI_MATKEY_COLOR_SPECULAR, &color) == AI_SUCCESS)
        {
            currentMaterial.specularColor = Vector4(color.r, color.g, color.b, color.a);
        }
        // Potencia especular (Shininess)
        float shininess;
        if (aiGetMaterialFloat(pMaterial, AI_MATKEY_SHININESS, &shininess) == AI_SUCCESS)
        {
            currentMaterial.specularPower = shininess;
            // Assimp shininess a veces necesita ajuste. Si es 0, BasicEffect puede no gustarle.
            if (currentMaterial.specularPower <= 0.0f) currentMaterial.specularPower = 1.0f;
        }

        if (aiGetMaterialColor(pMaterial, AI_MATKEY_COLOR_EMISSIVE, &color) == AI_SUCCESS)
        {
            currentMaterial.emissiveColor = Vector4(color.r, color.g, color.b, color.a);
        }

        m_materials[i] = std::move(currentMaterial);
    }
    OutputDebugString(L"Materials processed.\n");
}

ComPtr<ID3D11ShaderResourceView> Model::LoadTextureFromFile(ID3D11Device* device, ID3D11DeviceContext* context, const std::string& textureFilenameInModel)
{
    if (textureFilenameInModel.empty()) return nullptr;

    std::string fullPath = m_modelDirectory + "/" + textureFilenameInModel;

    // Reemplazar barras inclinadas si es necesario para consistencia de rutas
    for (char& c : fullPath) {
        if (c == '\\') c = '/';
    }
    // Assimp a veces da rutas con './' o '../', intentamos simplificar un poco, aunque esto no es un normalizador completo.
    // Si la ruta comienza con "./", quitarlo.
    if (fullPath.rfind("./", 0) == 0) {
        fullPath = fullPath.substr(2);
    }
    // (Una normalización de ruta más robusta podría ser necesaria para casos complejos)


    std::wstring wFullPath = StringToWString(fullPath);

    ComPtr<ID3D11ShaderResourceView> textureSRV = nullptr;
    ComPtr<ID3D11Resource> resource = nullptr; // Para que CreateWICTextureFromFile no necesite el contexto si no se generan mips

    // Intenta cargar la textura. CreateWICTextureFromFile necesita ID3D11Device, y opcionalmente ID3D11DeviceContext si quieres generar mipmaps.
    // Si no pasas el contexto, no se generan mipmaps automáticamente.
    HRESULT hr = DirectX::CreateWICTextureFromFile(device, /*context,*/ wFullPath.c_str(), resource.GetAddressOf(), textureSRV.ReleaseAndGetAddressOf());

    if (FAILED(hr))
    {
        std::wstring errMsg = L"ERROR::MODEL::LOAD_TEXTURE::Failed to load texture: " + wFullPath + L"\n";
        OutputDebugString(errMsg.c_str());
        // Podrías intentar cargar desde el directorio base de assets si la ruta relativa al modelo falla
        // std::wstring wFilenameOnly = StringToWString(textureFilenameInModel.substr(textureFilenameInModel.find_last_of("/\\") + 1));
        // hr = CreateWICTextureFromFile(device, context, wFilenameOnly.c_str(), nullptr, textureSRV.ReleaseAndGetAddressOf());
        // if(FAILED(hr)) OutputDebugString((L"Failed to load texture from base dir either: " + wFilenameOnly).c_str() );
        return nullptr;
    }
    OutputDebugString((L"Texture loaded: " + wFullPath + L"\n").c_str());
    return textureSRV;
}

// --- Implementación de Model::Draw y MeshPart::DrawPrim ---

void Model::MeshPart::DrawPrim(ID3D11DeviceContext* context)
{
    if (!vertexBuffer || !indexBuffer || indexCount == 0) return;

    UINT stride = sizeof(ModelVertex);
    UINT offset = 0;
    context->IASetVertexBuffers(0, 1, vertexBuffer.GetAddressOf(), &stride, &offset);
    context->IASetIndexBuffer(indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    wchar_t buffer[128];
    swprintf_s(buffer, L"Attempting to draw MeshPart with IndexCount: %u, MaterialIndex: %u\n", indexCount, materialIndex);
    OutputDebugString(buffer);
    context->DrawIndexed(indexCount, 0, 0);
}

void Model::Draw(ID3D11DeviceContext* context,
    const Matrix& viewMatrix,
    const Matrix& projectionMatrix,
    ID3D11Buffer* lightPropertiesCB, 
    ID3D11SamplerState* samplerState 
/*, bool wireframe = false */) // El wireframe requeriría cambiar el RasterizerState aquí o en Game
{


    // No dibujar si los shaders o el layout no están listos, o si no hay mallas
    if (!m_vertexShader || !m_pixelShader || !m_inputLayout || m_meshParts.empty())
    {
        return;
    }

    // --- 1. CONFIGURAR ESTADOS GLOBALES DE LA PIPELINE PARA ESTE MODELO ---
    context->IASetInputLayout(m_inputLayout.Get());
    context->VSSetShader(m_vertexShader.Get(), nullptr, 0);
    context->PSSetShader(m_pixelShader.Get(), nullptr, 0);

    // Configurar el Sampler State para el Pixel Shader (asume slot s0 en HLSL)
    if (samplerState)
    {
        context->PSSetSamplers(0, 1, &samplerState);
    }

    // Configurar el Constant Buffer de Luces para el Pixel Shader (asume slot b0 en HLSL para LightProperties)
    if (lightPropertiesCB)
    {
        context->PSSetConstantBuffers(0, 1, &lightPropertiesCB);
    }

    // Pre-calcular la matriz ViewProjection
    Matrix viewProjectionMatrix = viewMatrix * projectionMatrix;
    Matrix viewProjectionMatrix_local = viewMatrix * projectionMatrix;


    // --- 2. ITERAR Y DIBUJAR CADA PARTE DE LA MALLA (MeshPart) ---
    for (auto& meshPart : m_meshParts)
    {
        // --- 2a. Actualizar y Configurar Constant Buffer Per-Object para Vertex Shader ---
        // (Contiene World, ViewProjection, WorldInverseTranspose - slot b0 en LightingVS.hlsl)
        D3D11_MAPPED_SUBRESOURCE mappedResourceVS;
        VSPerObjectData* vsDataPtr;
        HRESULT hr = context->Map(m_cbVS_PerObject.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResourceVS);
        if (FAILED(hr))
        {
            // OutputDebugString(L"ERROR::MODEL::DRAW::Failed to map VS PerObject CB.\n"); // Línea original
            // --- MODIFICACIÓN AQUÍ ---
            wchar_t errorMsg[256];
            swprintf_s(errorMsg, L"ERROR::MODEL::DRAW::Failed to map VS PerObject CB. HRESULT: 0x%08X\n", static_cast<unsigned int>(hr));
            OutputDebugString(errorMsg);
            // --- FIN MODIFICACIÓN ---
            continue; // Saltar esta malla si falla el mapeo
        }
        vsDataPtr = (VSPerObjectData*)mappedResourceVS.pData;

        Matrix world = meshPart.localNodeTransform * m_worldMatrix;
        vsDataPtr->World = world; // Las matrices se pasan row-major como están en C++
        vsDataPtr->ViewProjection = viewProjectionMatrix; // ya que el shader usa transpose()
        vsDataPtr->WorldInverseTranspose = world.Invert().Transpose(); // Correcto para transformar normales

        context->Unmap(m_cbVS_PerObject.Get(), 0);
        context->VSSetConstantBuffers(0, 1, m_cbVS_PerObject.GetAddressOf()); // Vincula al slot b0 del VS

        // --- 2b. Actualizar y Configurar Constant Buffer de Material para Pixel Shader ---
        // (Contiene propiedades del material - slot b1 en LightingPS.hlsl)
        if (meshPart.materialIndex < m_materials.size() && m_cbPS_MaterialProperties)
        {
            const auto& material = m_materials[meshPart.materialIndex];
            D3D11_MAPPED_SUBRESOURCE mappedResourcePSMat;
            PSMaterialPropertiesData* psMatDataPtr;

            hr = context->Map(m_cbPS_MaterialProperties.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResourcePSMat);
            if (FAILED(hr))
            {
                OutputDebugString(L"ERROR::MODEL::DRAW::Failed to map PS Material CB.\n");
                // Podríamos continuar sin material o con uno por defecto, o saltar
            }
            else
            {
                psMatDataPtr = (PSMaterialPropertiesData*)mappedResourcePSMat.pData;
                psMatDataPtr->materialDiffuseColor = material.diffuseColor;
                psMatDataPtr->materialSpecularColor = material.specularColor;
                psMatDataPtr->specularPower = material.specularPower;
                // El padding se maneja por la estructura, no es necesario asignarlo aquí
                context->Unmap(m_cbPS_MaterialProperties.Get(), 0);
            }
            context->PSSetConstantBuffers(1, 1, m_cbPS_MaterialProperties.GetAddressOf()); // Vincula al slot b1 del PS

            // Configurar la textura difusa para el Pixel Shader (asume slot t0 en HLSL)
            if (material.diffuseTextureSRV)
            {
                context->PSSetShaderResources(0, 1, material.diffuseTextureSRV.GetAddressOf());
            }
            else
            {
                // Opcional: Desvincular textura o vincular una textura blanca/gris por defecto
                ID3D11ShaderResourceView* nullSRV[1] = { nullptr };
                context->PSSetShaderResources(0, 1, nullSRV);
            }
        }
        else
        {
            // Si no hay material o CB de material, podrías desvincular o usar uno por defecto
            // Por ahora, si no hay materialIndex válido, no se configura el CB de material ni textura
            ID3D11ShaderResourceView* nullSRV[1] = { nullptr };
            context->PSSetShaderResources(0, 1, nullSRV); // Desvincula textura
            // Podrías tener un CB de material por defecto y configurarlo aquí
        }

        // --- 2c. Dibujar la Primitiva de la Malla ---
        meshPart.DrawPrim(context);
    }
}

void Model::EvolvingDraw(ID3D11DeviceContext* context,
    const Matrix& viewMatrix,
    const Matrix& projectionMatrix,
    ID3D11Buffer* lightPropertiesCB,
    ID3D11SamplerState* samplerState,
    const Matrix& lightViewMatrix,
    const Matrix& lightProjectionMatrix,
    ID3D11ShaderResourceView* shadowMapSRV,
    ID3D11SamplerState* shadowSampler)
{
    if (!m_evolvingVertexShader || !m_evolvingPixelShader || !m_evolvingInputLayout || m_meshParts.empty() ||
        !m_cbVS_Evolving_WVP || !m_cbPS_MaterialProperties)
    {
        if (!m_cbVS_Evolving_WVP) OutputDebugString(L"EvolvingDraw: m_cbVS_Evolving_WVP es nullptr! No se dibujará nada.\n");
        else OutputDebugString(L"EvolvingDraw: Faltan otros recursos. No se dibujará nada.\n");
        return;
    }

    context->IASetInputLayout(m_evolvingInputLayout.Get());
    context->VSSetShader(m_evolvingVertexShader.Get(), nullptr, 0);
    context->PSSetShader(m_evolvingPixelShader.Get(), nullptr, 0);

    if (samplerState) context->PSSetSamplers(0, 1, &samplerState);
    if (lightPropertiesCB) context->PSSetConstantBuffers(1, 1, &lightPropertiesCB); 

    if (shadowMapSRV && shadowSampler)
    {
        context->PSSetShaderResources(1, 1, &shadowMapSRV); 
        context->PSSetSamplers(1, 1, &shadowSampler);    
    }

    Matrix vpMatrix = viewMatrix * projectionMatrix;
    Matrix lightVPMatrix = lightViewMatrix * lightProjectionMatrix;

    for (auto& meshPart : m_meshParts)
    {
        if (meshPart.indexCount == 0) continue;

        // 1. Actualizar Constant Buffer del Vertex Shader (m_cbVS_Evolving_WVP)
        D3D11_MAPPED_SUBRESOURCE mappedResourceVS;
        CB_VS_Evolving_Data* vsDataPtr; // Usa la struct que definiste (World, ViewProjection)

        HRESULT hr = context->Map(m_cbVS_Evolving_WVP.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResourceVS);
        if (FAILED(hr)) { /* Log y continue */ }

        vsDataPtr = (CB_VS_Evolving_Data*)mappedResourceVS.pData;
        Matrix world = meshPart.localNodeTransform * m_worldMatrix;
        vsDataPtr->World = meshPart.localNodeTransform * m_worldMatrix;
        vsDataPtr->ViewProjection = vpMatrix;
        vsDataPtr->LightViewProjection = lightVPMatrix;

        vsDataPtr->WorldInverseTranspose = world.Invert().Transpose();

        context->Unmap(m_cbVS_Evolving_WVP.Get(), 0);
        context->VSSetConstantBuffers(0, 1, m_cbVS_Evolving_WVP.GetAddressOf()); // slot b0

        // 2. Actualizar y Vincular Constant Buffer de Materiales (m_cbPS_MaterialProperties)
        if (meshPart.materialIndex < m_materials.size())
        {
            const auto& material = m_materials[meshPart.materialIndex];
            D3D11_MAPPED_SUBRESOURCE mappedResourcePSMat;
            PSMaterialPropertiesData* psMatDataPtr;

            hr = context->Map(m_cbPS_MaterialProperties.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResourcePSMat);
            if (FAILED(hr)) { /* Log y posible continue */ }
            else
            {
                psMatDataPtr = (PSMaterialPropertiesData*)mappedResourcePSMat.pData;
                psMatDataPtr->materialDiffuseColor = material.diffuseColor;
                psMatDataPtr->materialSpecularColor = material.specularColor;
                psMatDataPtr->specularPower = material.specularPower;
                psMatDataPtr->emissiveColor = material.emissiveColor;
                context->Unmap(m_cbPS_MaterialProperties.Get(), 0);
            }
            context->PSSetConstantBuffers(2, 1, m_cbPS_MaterialProperties.GetAddressOf()); // slot b2 (como en EvolvingPS.hlsl)

            // Vincular Textura (ya lo tienes)
            if (material.diffuseTextureSRV)
            {
                context->PSSetShaderResources(0, 1, material.diffuseTextureSRV.GetAddressOf()); // slot t0
            }
            else
            {
                ID3D11ShaderResourceView* nullSRV[1] = { nullptr };
                context->PSSetShaderResources(0, 1, nullSRV);
            }
        }
        

        meshPart.DrawPrim(context);
    }
}


void Model::UpdateWorldMatrix()
{
    Matrix scaleMatrix = Matrix::CreateScale(m_scale);
    Matrix rotationMatrix = Matrix::CreateFromQuaternion(m_rotation);
    Matrix translationMatrix = Matrix::CreateTranslation(m_position);

    // Orden SRT: m_worldMatrix = Escala * Rotación * Traslación
    m_worldMatrix = scaleMatrix * rotationMatrix * translationMatrix;
}

void Model::SetPosition(const Vector3& position)
{
    m_position = position;
    UpdateWorldMatrix();
}

void Model::SetPosition(float x, float y, float z)
{
    m_position = Vector3(x, y, z);
    UpdateWorldMatrix();
}

const Vector3& Model::GetPosition() const
{
    return m_position;
}

void Model::SetRotation(const Quaternion& rotation)
{
    m_rotation = rotation;
    m_rotation.Normalize(); 
    UpdateWorldMatrix();
}

void Model::Rotate(const Quaternion& deltaRotation)
{
    m_rotation *= deltaRotation; 
    m_rotation.Normalize();
    UpdateWorldMatrix();
}

const Quaternion& Model::GetRotation() const
{
    return m_rotation;
}

void Model::SetScale(const Vector3& scale)
{
    m_scale = scale;
    UpdateWorldMatrix();
}

void Model::SetScale(float uniformScale)
{
    m_scale = Vector3(uniformScale, uniformScale, uniformScale);
    UpdateWorldMatrix();
}

void Model::SetScale(float sx, float sy, float sz)
{
    m_scale = Vector3(sx, sy, sz);
    UpdateWorldMatrix();
}

const Vector3& Model::GetScale() const
{
    return m_scale;
}

void Model::SetRotationEuler(const Vector3& eulerAngles) // Parámetro: (pitch, yaw, roll) en radianes
{
    // Quaternion::CreateFromYawPitchRoll espera los ángulos en orden: Yaw (Y), Pitch (X), Roll (Z)
    m_rotation = Quaternion::CreateFromYawPitchRoll(eulerAngles.y, eulerAngles.x, eulerAngles.z);
    m_rotation.Normalize(); // Siempre es buena idea normalizar los quaternions después de operaciones
    UpdateWorldMatrix();
}

void Model::SetRotationEuler(float pitch, float yaw, float roll) // ángulos en radianes
{
    m_rotation = Quaternion::CreateFromYawPitchRoll(yaw, pitch, roll);
    m_rotation.Normalize();
    UpdateWorldMatrix();
}

bool Model::LoadDebugShaders(ID3D11Device* device, const wchar_t* vsFilename, const wchar_t* psFilename)
{
    // --- 1. CARGAR Y CREAR VERTEX SHADER DE DEPURACIÓN ---
    ComPtr<ID3DBlob> vsBlob;
    HRESULT hr = D3DReadFileToBlob(vsFilename, vsBlob.GetAddressOf());
    if (FAILED(hr))
    {
        OutputDebugString(L"ERROR::MODEL::Failed to load Debug Vertex Shader file.\n");
        return false;
    }
    hr = device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, m_debugVertexShader.ReleaseAndGetAddressOf());
    if (FAILED(hr))
    {
        OutputDebugString(L"ERROR::MODEL::Failed to create Debug Vertex Shader.\n");
        return false;
    }

    // --- 2. CREAR INPUT LAYOUT PARA EL VERTEX SHADER DE DEPURACIÓN ---
    // Este Input Layout DEBE describir la estructura de ModelVertex,
    // ya que los buffers de tus MeshParts contienen ModelVertex.
    // El DebugVS.hlsl solo usará el campo 'localPosition', pero el layout debe coincidir con el buffer.
    const D3D11_INPUT_ELEMENT_DESC modelVertexLayoutDesc[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }, // Incluso si el VS no lo usa explícitamente
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }  // Incluso si el VS no lo usa explícitamente
    };
    hr = device->CreateInputLayout(
        modelVertexLayoutDesc, ARRAYSIZE(modelVertexLayoutDesc),
        vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
        m_debugInputLayout.ReleaseAndGetAddressOf()
    );
    if (FAILED(hr))
    {
        OutputDebugString(L"ERROR::MODEL::Failed to create Debug Input Layout.\n");
        return false;
    }

    // --- 3. CARGAR Y CREAR PIXEL SHADER DE DEPURACIÓN ---
    ComPtr<ID3DBlob> psBlob;
    hr = D3DReadFileToBlob(psFilename, psBlob.GetAddressOf());
    if (FAILED(hr))
    {
        OutputDebugString(L"ERROR::MODEL::Failed to load Debug Pixel Shader file.\n");
        return false;
    }
    hr = device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, m_debugPixelShader.ReleaseAndGetAddressOf());
    if (FAILED(hr))
    {
        OutputDebugString(L"ERROR::MODEL::Failed to create Debug Pixel Shader.\n");
        return false;
    }

    // --- 4. CREAR CONSTANT BUFFER PARA EL VERTEX SHADER DE DEPURACIÓN (SOLO WVP) ---
    D3D11_BUFFER_DESC cbd_vs_debug = {};
    cbd_vs_debug.Usage = D3D11_USAGE_DYNAMIC;
    cbd_vs_debug.ByteWidth = sizeof(DirectX::SimpleMath::Matrix); // Solo una matriz WVP
    cbd_vs_debug.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbd_vs_debug.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    hr = device->CreateBuffer(&cbd_vs_debug, nullptr, m_cbVS_Debug_WVP.ReleaseAndGetAddressOf());
    if (FAILED(hr))
    {
        OutputDebugString(L"ERROR::MODEL::Failed to create Debug VS WVP Constant Buffer.\n");
        return false;
    }

    OutputDebugString(L"Debug shaders loaded successfully.\n");
    return true;
}

void Model::DebugDraw(ID3D11DeviceContext* context,
    const Matrix& viewMatrix,
    const Matrix& projectionMatrix)
{
    if (!m_debugVertexShader || !m_debugPixelShader || !m_debugInputLayout || m_meshParts.empty() || !m_cbVS_Debug_WVP)
    {
        // OutputDebugString(L"Model::DebugDraw - Debug shaders/resources not ready or no mesh parts.\n"); // Descomenta si necesitas este log
        return;
    }

    // 1. Configurar estados de la pipeline para depuración
    context->IASetInputLayout(m_debugInputLayout.Get());
    context->VSSetShader(m_debugVertexShader.Get(), nullptr, 0);
    context->PSSetShader(m_debugPixelShader.Get(), nullptr, 0);

    // No se necesitan texturas ni samplers para estos shaders de depuración.
    // Tampoco se necesitan los CB de material o luces.

    Matrix vpMatrix = viewMatrix * projectionMatrix;

    // 2. Iterar y dibujar cada parte de la malla
    for (auto& meshPart : m_meshParts)
    {
        if (meshPart.indexCount == 0) continue; // Saltar si no hay índices

        // --- Actualizar Constant Buffer del Vertex Shader (WVP) ---
        D3D11_MAPPED_SUBRESOURCE mappedResourceVS;
        struct CB_VS_WVP_Data { Matrix wvp; }; // Debe coincidir con el cbuffer en DebugVS.hlsl

        HRESULT hr = context->Map(m_cbVS_Debug_WVP.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResourceVS);
        if (FAILED(hr))
        {
            OutputDebugString(L"ERROR::MODEL::DEBUG_DRAW::Failed to map Debug VS WVP CB.\n");
            continue;
        }

        CB_VS_WVP_Data* vsDataPtr = (CB_VS_WVP_Data*)mappedResourceVS.pData;
        Matrix world = meshPart.localNodeTransform * m_worldMatrix; // m_worldMatrix es la transformación general del modelo
        vsDataPtr->wvp = world * vpMatrix; // Calculamos WVP: World * View * Projection
        // Las matrices se pasan como están (row-major) porque el shader usa transpose()

        context->Unmap(m_cbVS_Debug_WVP.Get(), 0);
        context->VSSetConstantBuffers(0, 1, m_cbVS_Debug_WVP.GetAddressOf()); // Vincula al slot b0 del VS

        // --- Dibujar la primitiva de la malla ---
        meshPart.DrawPrim(context); // Esta función debe simplemente enlazar los VBs/IBs y llamar a DrawIndexed
    }
}

bool Model::LoadEvolvingShaders(ID3D11Device * device, const wchar_t* vsFilename, const wchar_t* psFilename)
{
    // --- 1. CARGAR Y CREAR EVOLVING VERTEX SHADER ---
    Microsoft::WRL::ComPtr<ID3DBlob> vsBlob;
    HRESULT hr = D3DReadFileToBlob(vsFilename, vsBlob.GetAddressOf());
    if (FAILED(hr))
    {
        OutputDebugString(L"ERROR::MODEL::LOAD_EVOLVING_SHADERS::Failed to load Evolving Vertex Shader file: ");
        OutputDebugString(vsFilename);
        OutputDebugString(L"\n");
        return false;
    }

    hr = device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, m_evolvingVertexShader.ReleaseAndGetAddressOf());
    if (FAILED(hr))
    {
        OutputDebugString(L"ERROR::MODEL::LOAD_EVOLVING_SHADERS::Failed to create Evolving Vertex Shader.\n");
        return false;
    }

    // --- 2. CREAR INPUT LAYOUT PARA EL EVOLVING VERTEX SHADER ---
    // Este Input Layout debe describir la estructura de ModelVertex,
    // ya que los buffers de tus MeshParts contienen ModelVertex, y
    // VertexInputType_Evolving en EvolvingVS.hlsl (Paso 1) coincide con ModelVertex.
    const D3D11_INPUT_ELEMENT_DESC modelVertexLayoutDesc[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };

    hr = device->CreateInputLayout(
        modelVertexLayoutDesc, ARRAYSIZE(modelVertexLayoutDesc),
        vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), // Usar el bytecode del EvolvingVS
        m_evolvingInputLayout.ReleaseAndGetAddressOf()
    );
    if (FAILED(hr))
    {
        OutputDebugString(L"ERROR::MODEL::LOAD_EVOLVING_SHADERS::Failed to create Evolving Input Layout.\n");
        return false;
    }

    // El blob del VS ya no es necesario después de crear el shader y el layout
    // vsBlob se liberará automáticamente aquí por ComPtr

    // --- 3. CARGAR Y CREAR EVOLVING PIXEL SHADER ---
    Microsoft::WRL::ComPtr<ID3DBlob> psBlob;
    hr = D3DReadFileToBlob(psFilename, psBlob.GetAddressOf());
    if (FAILED(hr))
    {
        OutputDebugString(L"ERROR::MODEL::LOAD_EVOLVING_SHADERS::Failed to load Evolving Pixel Shader file: ");
        OutputDebugString(psFilename);
        OutputDebugString(L"\n");
        return false;
    }

    hr = device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, m_evolvingPixelShader.ReleaseAndGetAddressOf());
    if (FAILED(hr))
    {
        OutputDebugString(L"ERROR::MODEL::LOAD_EVOLVING_SHADERS::Failed to create Evolving Pixel Shader.\n");
        return false;
    }

    D3D11_BUFFER_DESC cbd_ps_material = {};
    cbd_ps_material.Usage = D3D11_USAGE_DYNAMIC;
    cbd_ps_material.ByteWidth = sizeof(PSMaterialPropertiesData); // Tu struct de C++
    cbd_ps_material.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbd_ps_material.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    HRESULT hr_mat_cb = device->CreateBuffer(&cbd_ps_material, nullptr, m_cbPS_MaterialProperties.ReleaseAndGetAddressOf());
    if (FAILED(hr_mat_cb)) {
        OutputDebugString(L"ERROR::MODEL::LOAD_EVOLVING_SHADERS::Failed to create PS Material Properties CB.\n");
        return false;
    }

    D3D11_BUFFER_DESC cbd_vs_evolving = {};
    cbd_vs_evolving.Usage = D3D11_USAGE_DYNAMIC;
    // Asegúrate que el tamaño coincida con lo que espera tu EvolvingVS.hlsl en b0
    // Si es la struct CB_VS_Evolving_Data (World y ViewProjection):
    cbd_vs_evolving.ByteWidth = sizeof(CB_VS_Evolving_Data);
    // Si solo era una matriz WorldViewProjection:
    // cbd_vs_evolving.ByteWidth = sizeof(DirectX::SimpleMath::Matrix); 
    cbd_vs_evolving.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    cbd_vs_evolving.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    // Usa el miembro m_cbVS_Evolving_WVP que deberías tener declarado en Model.h
    HRESULT hr_vs_cb = device->CreateBuffer(&cbd_vs_evolving, nullptr, m_cbVS_Evolving_WVP.ReleaseAndGetAddressOf());
    if (FAILED(hr_vs_cb)) {
        OutputDebugString(L"ERROR::MODEL::LOAD_EVOLVING_SHADERS::Failed to create Evolving VS PerObject CB.\n");
        return false;
    }

    OutputDebugString(L"Evolving shaders and input layout loaded successfully.\n");
    return true;
}

const DirectX::BoundingSphere& Model::GetOverallLocalBoundingSphere() const { return m_overallLocalBoundingSphere; }
DirectX::BoundingSphere Model::GetOverallWorldBoundingSphere() const {
    DirectX::BoundingSphere worldSphere;
    m_overallLocalBoundingSphere.Transform(worldSphere, m_worldMatrix);
    return worldSphere;
}


bool Model::CheckCollisionAgainstParts(
    const DirectX::BoundingBox& worldSpaceQueryBox,    // Es la cameraFutureBox desde Game::Update
    const DirectX::SimpleMath::Matrix& instanceWorldMatrix, // Es la instance.worldTransform desde Game::Update
    std::vector<DirectX::BoundingBox>& boxesToDrawDebug,
    bool shouldAddDebugBox) const
{
    for (const auto& meshPart : m_meshParts)
    {
        // Opcional: Saltar si la AABB local de la parte de la malla es inválida o no tiene extensión.
        // Esto asume que una AABB con Extents.x == 0 es considerada vacía/inválida para colisión.
        if (meshPart.localAABB.Extents.x == 0.0f &&
            meshPart.localAABB.Extents.y == 0.0f &&
            meshPart.localAABB.Extents.z == 0.0f) {
            continue;
        }

        DirectX::BoundingBox worldMeshPartBox;

        // Paso 1: Transformar la AABB de la parte de la malla (que ya está en el espacio del nodo local del modelo)
        // usando la matriz de mundo completa de la INSTANCIA.
        // La 'meshPart.localNodeTransform' lleva los vértices de la malla a las coordenadas relativas al origen del modelo.
        // Luego, 'instanceWorldMatrix' lleva esas coordenadas al espacio del mundo.
        // Si 'meshPart.localAABB' ya fue calculada después de aplicar 'meshPart.localNodeTransform' (es decir, está en el espacio local del modelo completo):
        //      meshPart.localAABB.Transform(worldMeshPartBox, instanceWorldMatrix);
        // Si 'meshPart.localAABB' está en el espacio de la malla ANTES de 'meshPart.localNodeTransform':
        DirectX::SimpleMath::Matrix finalMeshPartTransform = meshPart.localNodeTransform * instanceWorldMatrix;
        meshPart.localAABB.Transform(worldMeshPartBox, finalMeshPartTransform);


        if (shouldAddDebugBox)
        {
            boxesToDrawDebug.push_back(worldMeshPartBox);
        }

        if (worldSpaceQueryBox.Intersects(worldMeshPartBox))
        {
            // Opcional: Añadir un OutputDebugString aquí si quieres saber con qué parte específica hubo colisión
            // wchar_t msg[256];
            // swprintf_s(msg, L"    Colisión de Narrow Phase con MeshPart (Centro AABB: %.2f, %.2f, %.2f)\n", 
            //    worldMeshPartBox.Center.x, worldMeshPartBox.Center.y, worldMeshPartBox.Center.z);
            // OutputDebugString(msg);
            return true; // Se detectó colisión con esta parte
        }
    }
    return false; // No se detectó colisión con ninguna parte de esta instancia
}

void Model::CalculateOverallBoundingSphere() {
    if (m_meshParts.empty()) {
        m_overallLocalBoundingSphere.Center = DirectX::SimpleMath::Vector3::Zero;
        m_overallLocalBoundingSphere.Radius = 0.f;
        return;
    }

    std::vector<DirectX::SimpleMath::Vector3> allCornerPoints; // Vector para almacenar todas las esquinas

    for (const auto& part : m_meshParts) {
        // Solo procesar si la AABB local de la parte es válida (tiene extensión)
        if (part.localAABB.Extents.x > 0 || part.localAABB.Extents.y > 0 || part.localAABB.Extents.z > 0) {
            DirectX::BoundingBox modelSpaceBox;
            // La localAABB está en el espacio del nodo de la malla. 
            // localNodeTransform la lleva al espacio del modelo (raíz del modelo).
            part.localAABB.Transform(modelSpaceBox, part.localNodeTransform);

            DirectX::SimpleMath::Vector3 corners[DirectX::BoundingBox::CORNER_COUNT]; // CORNER_COUNT es 8
            modelSpaceBox.GetCorners(corners); // Obtiene las 8 esquinas de esta caja

            for (int i = 0; i < DirectX::BoundingBox::CORNER_COUNT; ++i) {
                allCornerPoints.push_back(corners[i]); // Añade cada esquina a nuestra lista global
            }
        }
    }

    if (!allCornerPoints.empty()) {
        DirectX::BoundingSphere::CreateFromPoints(
            m_overallLocalBoundingSphere,       // Esfera de salida
            allCornerPoints.size(),             // Número de puntos
            allCornerPoints.data(),             // Puntero a los datos de los puntos
            sizeof(DirectX::SimpleMath::Vector3) // Tamaño de cada elemento punto
        );
        // OutputDebugString(L"Overall BoundingSphere calculada a partir de las esquinas de las AABBs de MeshParts.\n");
    }
    else {
        // Fallback si no se encontraron puntos válidos (ej. todas las AABBs de las partes eran inválidas)
        m_overallLocalBoundingSphere.Center = DirectX::SimpleMath::Vector3::Zero;
        m_overallLocalBoundingSphere.Radius = 1.0f;
        // OutputDebugString(L"ADVERTENCIA: No se encontraron puntos válidos para la BoundingSphere general, usando valores por defecto.\n");
    }
}

void Model::ShadowDraw(
    ID3D11DeviceContext* context,
    const DirectX::SimpleMath::Matrix& worldMatrix,
    const DirectX::SimpleMath::Matrix& lightViewMatrix,
    const DirectX::SimpleMath::Matrix& lightProjectionMatrix)
{
    // No dibujar si no tenemos el buffer o no hay mallas
    if (!m_cbVS_Shadow || m_meshParts.empty())
    {
        return;
    }

    OutputDebugStringA("Drawing model shadow...\n");

    // Combinamos las matrices para obtener la World-View-Projection desde la luz
    Matrix lightWorldViewProj = worldMatrix * lightViewMatrix * lightProjectionMatrix;

    // --- Actualizar el Constant Buffer del Vertex Shader ---
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    HRESULT hr = context->Map(m_cbVS_Shadow.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    if (FAILED(hr)) return;

    CB_VS_Shadow_Data* dataPtr = (CB_VS_Shadow_Data*)mappedResource.pData;
    dataPtr->World = worldMatrix; // Pasamos la matriz World de la instancia
    dataPtr->LightViewProjection = lightViewMatrix * lightProjectionMatrix; // Pasamos la VP de la luz

    context->Unmap(m_cbVS_Shadow.Get(), 0);

    // Vinculamos el buffer al slot b0 del Vertex Shader (como espera nuestro ShadowVS.hlsl)
    context->VSSetConstantBuffers(0, 1, m_cbVS_Shadow.GetAddressOf());

    // --- Dibujar cada parte de la malla ---
    // No necesitamos configurar materiales, texturas, etc. Solo la geometría.
    for (auto& meshPart : m_meshParts)
    {
        meshPart.DrawPrim(context); // Dibuja la geometría usando los VBs/IBs
    }
}

void Model::ShadowDrawAlphaClip(
    ID3D11DeviceContext* context,
    const DirectX::SimpleMath::Matrix& worldMatrix,
    const DirectX::SimpleMath::Matrix& lightViewMatrix,
    const DirectX::SimpleMath::Matrix& lightProjectionMatrix,
    ID3D11SamplerState* sampler)
{
    if (!m_cbVS_Shadow || m_meshParts.empty()) return;

    Matrix lightWVP = worldMatrix * lightViewMatrix * lightProjectionMatrix;

    D3D11_MAPPED_SUBRESOURCE mappedResource;
    context->Map(m_cbVS_Shadow.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    CB_VS_Shadow_Data* dataPtr = (CB_VS_Shadow_Data*)mappedResource.pData;
    dataPtr->World = worldMatrix; // El nuevo VS necesita la World matrix por separado
    dataPtr->LightViewProjection = lightViewMatrix * lightProjectionMatrix;
    context->Unmap(m_cbVS_Shadow.Get(), 0);

    context->VSSetConstantBuffers(0, 1, m_cbVS_Shadow.GetAddressOf());

    // Vinculamos el sampler que usarn todas las partes
    context->PSSetSamplers(0, 1, &sampler);

    for (auto& meshPart : m_meshParts)
    {
        if (meshPart.materialIndex < m_materials.size())
        {
            const auto& material = m_materials[meshPart.materialIndex];
            if (material.diffuseTextureSRV)
            {
                // Vinculamos la textura difusa de este material al slot t0
                context->PSSetShaderResources(0, 1, material.diffuseTextureSRV.GetAddressOf());
            }
        }
        meshPart.DrawPrim(context);
    }
}