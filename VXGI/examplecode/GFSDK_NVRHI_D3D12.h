/*
* Copyright (c) 2012-2016, NVIDIA CORPORATION. All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto. Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#pragma once

#include <GFSDK_NVRHI.h>

struct ID3D12Device;
struct ID3D12CommandQueue;
struct ID3D12Resource;
struct ID3D12GraphicsCommandList;
struct ID3D12CommandAllocator;

namespace NVRHI
{
    class PipelineState;
    class RootSignature;
    class CommandList;
    class ManagedResource;
    typedef PipelineState* PipelineStateHandle;
    typedef RootSignature* RootSignatureHandle;
    typedef CommandList* CommandListHandle;

    typedef uint32_t ArrayIndex;
    typedef uint32_t MipLevel;
    typedef uint32_t DescriptorIndex;

    struct BackendResources;

    class RendererInterfaceD3D12 : public IRendererInterface
    {
    public:
        RendererInterfaceD3D12(IErrorCallback* errorCB, ID3D12Device* pDevice, ID3D12CommandQueue* pCommandQueue);
        virtual ~RendererInterfaceD3D12();

        TextureHandle getHandleForTexture(ID3D12Resource* pResource);
        void setNonManagedTextureResourceState(TextureHandle texture, uint32_t state);
        void releaseNonManagedTextures();
        void flushCommandList();
        void loadBalanceCommandList();

    private:
        friend class DescriptorHeap;
        friend class StaticDescriptorHeap;
        friend class UploadManager;
        friend class Texture;
        friend class Buffer;
        friend class ConstantBuffer;
        friend class Sampler;
        friend struct BackendResources;

        BackendResources* m_pResources;
        IErrorCallback* m_pErrorCallback;
        ID3D12Device* m_pDevice;
        ID3D12CommandQueue* m_pCommandQueue;
        CommandListHandle m_ActiveCommandList;

        RendererInterfaceD3D12& operator=(const RendererInterfaceD3D12& other); //undefined
        void signalError(const char* file, int line, const char* errorDesc);
        CommandListHandle createCommandList();
        uint32_t getStateHashForRS(const DrawCallState& state);
        uint32_t getStateHashForPSO(const DrawCallState& state);
        uint32_t getComputeStateHash(const DispatchState& state);
        RootSignatureHandle buildRootSignature(uint32_t numShaders, const ShaderHandle* shaders, bool allowInputLayout);
        RootSignatureHandle getRootSignature(const DrawCallState& state);
        RootSignatureHandle getRootSignature(const DispatchState& state, uint32_t hash);
        PipelineStateHandle getPipelineState(const DrawCallState& state, RootSignatureHandle pRS);
        PipelineStateHandle getPipelineState(const DispatchState& state, RootSignatureHandle pRS, uint32_t hash);
        DescriptorIndex getCBV(ConstantBufferHandle cbuffer);
        DescriptorIndex getTextureSRV(const TextureBinding& binding);
        DescriptorIndex getTextureUAV(const TextureBinding& binding);
        DescriptorIndex getBufferSRV(const BufferBinding& binding);
        DescriptorIndex getBufferUAV(const BufferBinding& binding);
        DescriptorIndex getSamplerView(SamplerHandle sampler);
        DescriptorIndex getRTV(TextureHandle texture, ArrayIndex arrayIndex, MipLevel mipLevel);
        DescriptorIndex getDSV(TextureHandle texture, ArrayIndex arrayIndex, MipLevel mipLevel);
        void releaseTextureViews(TextureHandle texture);
        void releaseBufferViews(BufferHandle buffer);
        void releaseConstantBufferViews(ConstantBufferHandle cbuffer);
        void releaseSamplerViews(SamplerHandle sampler);
        uint64_t getFenceCounter();
        void deferredDestroyResource(ManagedResource* resource);
        void requireTextureState(TextureHandle texture, uint32_t arrayIndex, uint32_t mipLevel, uint32_t state);
        void requireBufferState(BufferHandle buffer, uint32_t state);
        void commitBarriers();

        void bindShaderResources(uint32_t& rootIndex, void* rootDescriptorTableHandles, const PipelineStageBindings& stage);

        void syncWithGPU(const char* reason);
        void waitForFence(unsigned long long fenceValue, const char* reason);

    public:
        virtual TextureHandle createTexture(const TextureDesc& d, const void* data);
        virtual TextureDesc describeTexture(TextureHandle t);
        virtual void clearTextureFloat(TextureHandle t, const Color& clearColor);
        virtual void clearTextureUInt(TextureHandle t, uint32_t clearColor);
        virtual void writeTexture(TextureHandle t, uint32_t subresource, const void* data, uint32_t rowPitch, uint32_t depthPitch);
        virtual void destroyTexture(TextureHandle t);

        virtual BufferHandle createBuffer(const BufferDesc& d, const void* data);
        virtual void writeBuffer(BufferHandle b, const void* data, size_t dataSize);
        virtual void clearBufferUInt(BufferHandle b, uint32_t clearValue);
        virtual void copyToBuffer(BufferHandle dest, uint32_t destOffsetBytes, BufferHandle src, uint32_t srcOffsetBytes, size_t dataSizeBytes);
        virtual void readBuffer(BufferHandle b, void* data, size_t* dataSize);
        virtual void destroyBuffer(BufferHandle b);

        virtual ConstantBufferHandle createConstantBuffer(const ConstantBufferDesc& d, const void* data);
        virtual void writeConstantBuffer(ConstantBufferHandle b, const void* data, size_t dataSize);
        virtual void destroyConstantBuffer(ConstantBufferHandle b);

        virtual ShaderHandle createShader(const ShaderDesc& d, const void* binary, const size_t binarySize);
        virtual ShaderHandle createShaderFromAPIInterface(ShaderType::Enum shaderType, const void* apiInterface);
        virtual void destroyShader(ShaderHandle s);

        virtual SamplerHandle createSampler(const SamplerDesc& d);
        virtual void destroySampler(SamplerHandle s);

        virtual InputLayoutHandle createInputLayout(const VertexAttributeDesc* d, uint32_t attributeCount, const void* vertexShaderBinary, const size_t binarySize);
        virtual void destroyInputLayout(InputLayoutHandle i);

        virtual PerformanceQueryHandle createPerformanceQuery(const char* name);
        virtual void destroyPerformanceQuery(PerformanceQueryHandle query);
        virtual void beginPerformanceQuery(PerformanceQueryHandle query, bool onlyAnnotation);
        virtual void endPerformanceQuery(PerformanceQueryHandle query);
        virtual float getPerformanceQueryTimeMS(PerformanceQueryHandle query);

        virtual GraphicsAPI::Enum getGraphicsAPI();
        virtual void* getAPISpecificInterface(APISpecificInterface::Enum interfaceType);
        virtual bool isOpenGLExtensionSupported(const char* name);
        virtual void* getOpenGLProcAddress(const char* procname);

        virtual void draw(const DrawCallState& state, const DrawArguments* args, uint32_t numDrawCalls);
        virtual void drawIndexed(const DrawCallState& state, const DrawArguments* args, uint32_t numDrawCalls);
        virtual void drawIndirect(const DrawCallState& state, BufferHandle indirectParams, uint32_t offsetBytes);
        virtual void dispatch(const DispatchState& state, uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ);
        virtual void dispatchIndirect(const DispatchState& state, BufferHandle indirectParams, uint32_t offsetBytes);

        virtual void executeRenderThreadCommand(IRenderThreadCommand* onCommand);

        virtual uint32_t getNumberOfAFRGroups();
        virtual uint32_t getAFRGroupOfCurrentFrame(uint32_t numAFRGroups);

		virtual void setEnableUavBarriersForTexture(TextureHandle texture, bool enableBarriers);
		virtual void setEnableUavBarriersForBuffer(BufferHandle buffer, bool enableBarriers);

        void applyState(const DrawCallState& state);
        void applyState(const DispatchState& state);
    };
}
