/*
* Copyright (c) 2012-2018, NVIDIA CORPORATION. All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto. Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#pragma once

#include "GFSDK_VXGI.h"
#include "Scene.h"
#include <functional>

#pragma warning(disable : 4324)

__declspec(align(16)) struct GlobalConstants
{
    VXGI::float4x4 worldMatrix;
    VXGI::float4x4 viewProjMatrix;
    VXGI::float4x4 viewProjMatrixInv;
    VXGI::float4x4 lightMatrix;
    VXGI::float4 cameraPos;
    VXGI::float4 lightDirection;
    VXGI::float4 diffuseColor;
    VXGI::float4 lightColor;
    VXGI::float4 ambientColor;
    float rShadowMapSize;
    uint32_t enableIndirectDiffuse;
    uint32_t enableIndirectSpecular;
    float transparentRoughness;
    float transparentReflectance;
};


struct MeshMaterialInfo : public VXGI::MaterialInfo
{
    NVRHI::TextureHandle    diffuseTexture;
    NVRHI::TextureHandle    specularTexture;
    NVRHI::TextureHandle    normalsTexture;
    VXGI::float3          diffuseColor;

    MeshMaterialInfo() :
        diffuseTexture(NULL),
        specularTexture(NULL),
        normalsTexture(NULL),
        diffuseColor(0.f)
    {}
};

typedef std::function<void(const MeshMaterialInfo&)> MaterialCallback;

class SceneRenderer
{
private:

    Scene*                  m_pScene;
    Scene*                  m_pTransparentScene;
    NVRHI::IRendererInterface* m_RendererInterface;

    NVRHI::InputLayoutRef    m_pInputLayout;
    NVRHI::ShaderRef         m_pDefaultVS;
    NVRHI::ShaderRef         m_pFullScreenQuadVS;

    NVRHI::ShaderRef         m_pAttributesPS;
    NVRHI::ShaderRef         m_pBlitPS;
    NVRHI::ShaderRef         m_pBlitLdrPS;
    NVRHI::ShaderRef         m_pCompositingPS;

    NVRHI::ConstantBufferRef m_pGlobalCBuffer;

    NVRHI::SamplerRef        m_pDefaultSamplerState;
    NVRHI::SamplerRef        m_pComparisonSamplerState;

    UINT                    m_Width;
    UINT                    m_Height;
    UINT                    m_SampleCount;

    VXGI::float3            m_LightDirection;
    VXGI::float4x4          m_LightViewMatrix;
    VXGI::float4x4          m_LightProjMatrix;
    VXGI::float4x4          m_LightViewProjMatrix;

    NVRHI::TextureRef       m_TargetAlbedo;
    NVRHI::TextureRef       m_TargetNormal;
    NVRHI::TextureRef       m_TargetDepth;
    NVRHI::TextureRef       m_TargetNormalPrev;
    NVRHI::TextureRef       m_TargetDepthPrev;

    NVRHI::TextureRef       m_ShadowMap;

    NVRHI::TextureRef       m_NullTexture;

    VXGI::IUserDefinedShaderSet* m_pVoxelizationGS;
    VXGI::IUserDefinedShaderSet* m_pVoxelizationPS;
    VXGI::IUserDefinedShaderSet* m_pTransparentGeometryPS;

public:
    SceneRenderer(NVRHI::IRendererInterface* pRenderer);
    
    HRESULT LoadMesh(const char* strFileName);
    HRESULT LoadTransparentMesh(const char* strFileName);

    HRESULT AllocateResources(VXGI::IGlobalIllumination* pGI, VXGI::IShaderCompiler* pCompiler);

    HRESULT CreateVoxelizationGS(VXGI::IShaderCompiler* pCompiler, VXGI::IGlobalIllumination* pGI);
    HRESULT CreateVoxelizationPS(VXGI::IShaderCompiler* pCompiler, VXGI::IGlobalIllumination* pGI);
    HRESULT CreateTransparentGeometryPS(VXGI::IShaderCompiler* pCompiler, VXGI::IGlobalIllumination* pGI);

    void AllocateViewDependentResources(UINT width, UINT height, UINT sampleCount = 1);
    void ReleaseResources(VXGI::IGlobalIllumination* pGI);
    void ReleaseViewDependentResources();

    void RenderToGBuffer(const VXGI::float4x4& viewProjMatrix, VXGI::float3 cameraPos, bool drawTransparent);
    void RenderTransparentScene(VXGI::IGlobalIllumination* pGI, NVRHI::TextureHandle pDest, const VXGI::float4x4& viewProjMatrix, VXGI::float3 cameraPos, float transparentRoughness, float transparentReflectance);

    void SetLightDirection(VXGI::float3 direction);
    void RenderShadowMap(const VXGI::float3 cameraPosition, float lightSize, bool drawTransparent);

    void GetMaterialInfo(Scene* pScene, UINT meshID, OUT MeshMaterialInfo& materialInfo);

    void FillTracingInputBuffers(VXGI::IBasicViewTracer::InputBuffers& inputBuffers);
    NVRHI::TextureHandle GetAlbedoBufferHandle();

    void Blit(NVRHI::TextureHandle pSource, NVRHI::TextureHandle pDest, bool convertToLdr);
    void Shade(
        NVRHI::TextureHandle indirectDiffuse, 
        NVRHI::TextureHandle indirectSpecular,
        NVRHI::TextureHandle indirectConfidence,
        NVRHI::TextureHandle pDest, 
        const VXGI::float4x4& viewProjMatrix, 
        VXGI::float3 ambientColor);

    void RenderSceneCommon(
        Scene* pScene,
        NVRHI::DrawCallState& state,
        VXGI::IGlobalIllumination* pGI,
        const VXGI::Box3f *clippingBoxes,
        uint32_t numBoxes,
        const GlobalConstants& constants,
        MaterialCallback* onChangeMaterial,
        bool voxelization);

    void RenderForVoxelization(
        NVRHI::DrawCallState& state,
        VXGI::IGlobalIllumination* pGI,
        const VXGI::Box3f *clippingBoxes,
        uint32_t numBoxes,
        const VXGI::float4x4& viewProjMatrix,
        MaterialCallback* onChangeMaterial);

    VXGI::Frustum GetLightFrustum();
};