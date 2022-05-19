/*
* Copyright (c) 2012-2018, NVIDIA CORPORATION. All rights reserved.
*
* NVIDIA CORPORATION and its licensors retain all intellectual property
* and proprietary rights in and to this software, related documentation
* and any modifications thereto. Any use, reproduction, disclosure or
* distribution of this software and related documentation without an express
* license agreement from NVIDIA CORPORATION is strictly prohibited.
*/

#include "GFSDK_NVRHI_D3D12.h"
#include <d3d12.h>
#include <vector>
#include <set>
#include <bitset>
#include <map>
#include <algorithm>
#include <assert.h>
#include <list>
#include <pix.h>

#ifndef NVRHI_D3D12_WITH_NVAPI
#define NVRHI_D3D12_WITH_NVAPI 1
#endif

#if NVRHI_D3D12_WITH_NVAPI
#include <nvapi.h>
#ifdef _WIN64
#pragma comment(lib, "nvapi64.lib")
#else
#pragma comment(lib, "nvapi.lib")
#endif
#endif

#pragma comment(lib, "dxguid.lib")

#define ENABLE_D3D_REFLECTION 1

#if ENABLE_D3D_REFLECTION
#include <d3dcompiler.h>
#pragma comment(lib, "d3dcompiler.lib")
#endif

#define SIGNAL_ERROR(msg) this->signalError(__FILE__, __LINE__, msg)
#define CHECK_ERROR(expr, msg) if (!(expr)) this->signalError(__FILE__, __LINE__, msg)
#define SAFE_RELEASE(p) { if (p) { (p)->Release(); (p)=nullptr; } }
#define HR_RETURN(hr) if(FAILED(hr)) return hr;

#ifdef _DEBUG
#define DEBUG_PRINT(cat, x) if(int(g_MessageCategories) & int(MessageCategories::cat)) OutputDebugStringA(x)
#define DEBUG_PRINTF(cat, format, ...) if(int(g_MessageCategories) & int(MessageCategories::cat)) { char buf[256]; sprintf_s(buf, format, ## __VA_ARGS__); OutputDebugStringA(buf); }
#else
#define DEBUG_PRINT(cat, x) (void)(x)
#define DEBUG_PRINTF(cat, format, ...) (void)(format)
#endif

#define START_CPU_PERF LARGE_INTEGER timeBegin, timeEnd, timeFreq; QueryPerformanceCounter(&timeBegin);
#define END_CPU_PERF(Target) QueryPerformanceCounter(&timeEnd); QueryPerformanceFrequency(&timeFreq); double Target = double(timeEnd.QuadPart - timeBegin.QuadPart) / double(timeFreq.QuadPart);

#define INVALID_DESCRIPTOR_INDEX (~0u)
#define MAX_COMMANDS_IN_LIST 128

namespace NVRHI
{
	MessageCategories g_MessageCategories = MessageCategories::DEFAULT;

    class ManagedResource
    {
    public:
        UINT64 fenceCounterAtLastUse;

        ManagedResource()
            : fenceCounterAtLastUse(0)
        { }

        virtual ~ManagedResource()
        { }
    };

    class Shader : public IShader
    {
    public:
        RendererInterfaceD3D12* parent;
        ULONG refCount;
        UINT64 fenceCounterAtLastUse;
        std::vector<char> bytecode;
        ShaderType::Enum type;
        uint32_t minSRV, numSRV;
        uint32_t minUAV, numUAV;
        uint32_t minSampler, numSamplers;
        uint32_t minCB, numCB;
        uint32_t numBindings;
        uint32_t hlslExtensionsUAV;
        std::bitset<128> slotsSRV;
        std::bitset<16> slotsUAV;
        std::bitset<128> slotsSampler;
        std::bitset<16> slotsCB;
#if NVRHI_D3D12_WITH_NVAPI
		std::vector<NVAPI_D3D12_PSO_EXTENSION_DESC*> extensions;
        std::vector<NV_CUSTOM_SEMANTIC> customSemantics;
		std::vector<uint32_t> coordinateSwizzling;
#endif

        Shader(RendererInterfaceD3D12* _parent)
            : parent(_parent)
            , refCount(1)
            , fenceCounterAtLastUse(0)
            , type(ShaderType::SHADER_VERTEX)
            , minSRV(~0u), numSRV(0)
            , minUAV(~0u), numUAV(0)
            , minSampler(~0u), numSamplers(0)
            , minCB(~0u), numCB(0)
            , numBindings(0)
            , hlslExtensionsUAV(~0u)
        { }

        ULONG AddRef() override { return ++refCount; }
        ULONG Release() override { ULONG result = --refCount; if (result == 0) parent->destroyShader(this); return result; }
    };


    class Texture : public ITexture
    {
    public:
        RendererInterfaceD3D12* parent;
        ULONG refCount;
        UINT64 fenceCounterAtLastUse;
        TextureDesc desc;
        ID3D12Resource* resource;
        bool isManaged;
		bool enableUavBarriers;
        bool firstUavBarrierPlaced;
        uint32_t numPlanes;
        std::vector<D3D12_RESOURCE_STATES> subresourceStates;
        std::map<std::pair<ArrayIndex, MipLevel>, DescriptorIndex> renderTargetViews;
        std::map<std::pair<ArrayIndex, MipLevel>, DescriptorIndex> depthStencilViews;
        std::map<std::pair<Format::Enum, MipLevel>, DescriptorIndex> shaderResourceViews;
        std::map<std::pair<Format::Enum, MipLevel>, DescriptorIndex> unorderedAccessViews;

        Texture(RendererInterfaceD3D12* _parent)
            : parent(_parent)
            , refCount(1)
            , resource(nullptr)
            , isManaged(false) 
			, enableUavBarriers(true)
			, firstUavBarrierPlaced(false)
        { }

        virtual ~Texture()
        {
            parent->releaseTextureViews(this);
            SAFE_RELEASE(resource);
        }

        ULONG AddRef() override { return ++refCount; }
        ULONG Release() override { ULONG result = --refCount; if (result == 0) parent->destroyTexture(this); return result; }
        const TextureDesc& GetDesc() const override { return desc; }
    };

    class Buffer : public IBuffer
    {
    public:
        RendererInterfaceD3D12* parent;
        ULONG refCount;
        UINT64 fenceCounterAtLastUse;
        BufferDesc desc;
        ID3D12Resource* resource;
		bool enableUavBarriers;
		bool firstUavBarrierPlaced;
        D3D12_RESOURCE_STATES state;
        DescriptorIndex shaderResourceView;
        DescriptorIndex unorderedAccessView;
		D3D12_GPU_VIRTUAL_ADDRESS gpuVA;

        Buffer(RendererInterfaceD3D12* _parent)
            : parent(_parent)
            , refCount(1)
            , resource(nullptr)
			, enableUavBarriers(true)
			, firstUavBarrierPlaced(false)
            , state(D3D12_RESOURCE_STATE_COMMON) 
            , shaderResourceView(INVALID_DESCRIPTOR_INDEX)
            , unorderedAccessView(INVALID_DESCRIPTOR_INDEX)
        { }

        virtual ~Buffer()
        {
            parent->releaseBufferViews(this);
            SAFE_RELEASE(resource);
        }

        ULONG AddRef() override { return ++refCount; }
        ULONG Release() override { ULONG result = --refCount; if (result == 0) parent->destroyBuffer(this); return result; }
        const BufferDesc& GetDesc() const override { return desc; }
    };

    class ConstantBuffer : public IConstantBuffer
    {
    public:
        RendererInterfaceD3D12* parent;
        ULONG refCount;
        UINT64 fenceCounterAtLastUse;
        ConstantBufferDesc desc;
        DescriptorIndex constantBufferView;
        UINT64 currentVersionOffset;
        bool uploadedDataValid;
        std::vector<BYTE> data;
        uint32_t alignedSize;

        uint32_t numEvictions;
        uint32_t numWrites;
        uint32_t numIdenticalWrites;
        uint32_t numRefreshes;
        uint32_t numCachedRefs;

        ConstantBuffer(RendererInterfaceD3D12* _parent)
            : parent(_parent)
            , refCount(1)
            , constantBufferView(INVALID_DESCRIPTOR_INDEX)
            , currentVersionOffset(0)
            , uploadedDataValid(false)
            , numEvictions(0)
            , numWrites(0)
            , numIdenticalWrites(0)
            , numRefreshes(0)
            , numCachedRefs(0)
        { }

        virtual ~ConstantBuffer()
        {
            parent->releaseConstantBufferViews(this);
        }

        ULONG AddRef() override { return ++refCount; }
        ULONG Release() override { ULONG result = --refCount; if (result == 0) parent->destroyConstantBuffer(this); return result; }
    };

    class Sampler : public ISampler
    {
    public:
        RendererInterfaceD3D12* parent;
        ULONG refCount;
        UINT64 fenceCounterAtLastUse;
        SamplerDesc desc;
        DescriptorIndex view;

        Sampler(RendererInterfaceD3D12* _parent)
            : parent(_parent)
            , refCount(1)
            , view(INVALID_DESCRIPTOR_INDEX)
        { }

        virtual ~Sampler()
        {
            parent->releaseSamplerViews(this);
        }

        ULONG AddRef() override { return ++refCount; }
        ULONG Release() override { ULONG result = --refCount; if (result == 0) parent->destroySampler(this); return result; }
    };

    class InputLayout : public IInputLayout
    {
    public:
        RendererInterfaceD3D12* parent;
        ULONG refCount;
        std::vector<VertexAttributeDesc> attributes;
        std::vector<D3D12_INPUT_ELEMENT_DESC> inputElements;

        InputLayout(RendererInterfaceD3D12* _parent)
            : parent(_parent)
            , refCount(1)
        { }

        ULONG AddRef() override { return ++refCount; }
        ULONG Release() override { ULONG result = --refCount; if (result == 0) parent->destroyInputLayout(this); return result; }
    };

    class PerformanceQuery : public IPerformanceQuery
    {
    public:  
        enum State { NEW, STARTED, ANNOTATION, FINISHED, RESOLVED };

        RendererInterfaceD3D12* parent;
        ULONG refCount;
        UINT64 fenceCounterAtLastUse;
        std::string name;
        uint32_t beginIndex;
        uint32_t endIndex;
        State state;
        float time;

        PerformanceQuery(RendererInterfaceD3D12* _parent)
            : parent(_parent)
            , refCount(1)
            , beginIndex(INVALID_DESCRIPTOR_INDEX)
            , endIndex(INVALID_DESCRIPTOR_INDEX)
            , state(NEW)
            , time(0.f)
        { }

        ULONG AddRef() override { return ++refCount; }
        ULONG Release() override { ULONG result = --refCount; if (result == 0) parent->destroyPerformanceQuery(this); return result; }
    };

    class RootSignature : public ManagedResource
    {
    public:
        std::set<Shader*> shaders;
        ID3D12RootSignature* handle;

        RootSignature()
            : handle(nullptr)
        { }

        virtual ~RootSignature() 
        {
            SAFE_RELEASE(handle);
        }
    };


    class PipelineState : public ManagedResource
    {
    public:
        RootSignature* rootSignature;  // weak reference
        ID3D12PipelineState* handle;

        PipelineState()
            : handle(nullptr)
            , rootSignature(nullptr)
        { }

        virtual ~PipelineState() 
        {
            SAFE_RELEASE(handle);
        }
    };

    class CommandList : public ManagedResource
    {
    public:
        ID3D12CommandAllocator* allocator;
        ID3D12GraphicsCommandList* commandList;
        uint32_t size;

        CommandList()
            : allocator(nullptr)
            , commandList(nullptr)
            , size(0)
        { }

        virtual ~CommandList()
        {
            SAFE_RELEASE(commandList);
            SAFE_RELEASE(allocator);
        }
    };
    
    class DescriptorHeapWrapper : public ManagedResource
    {
    public:
        ID3D12DescriptorHeap* heap;

        DescriptorHeapWrapper()
            : heap(nullptr)
        { }

        virtual ~DescriptorHeapWrapper()
        {
            SAFE_RELEASE(heap);
        }
    };


    struct FormatMapping
    {
        Format::Enum abstractFormat;
        DXGI_FORMAT resourceFormat;
        DXGI_FORMAT srvFormat;
        DXGI_FORMAT rtvFormat;
        UINT bytesPerPixel;
        bool isDepthStencil;
    };
        
    // Format mapping table. The rows must be in the exactly same order as Format enum members are defined.
    const FormatMapping FormatMappings[] = {
        { Format::UNKNOWN,              DXGI_FORMAT_UNKNOWN,                DXGI_FORMAT_UNKNOWN,                DXGI_FORMAT_UNKNOWN,                0, false },
        { Format::R8_UINT,              DXGI_FORMAT_R8_TYPELESS,            DXGI_FORMAT_R8_UINT,                DXGI_FORMAT_R8_UINT,                1, false },
        { Format::R8_UNORM,             DXGI_FORMAT_R8_TYPELESS,            DXGI_FORMAT_R8_UNORM,               DXGI_FORMAT_R8_UNORM,               1, false },
        { Format::RG8_UINT,             DXGI_FORMAT_R8G8_TYPELESS,          DXGI_FORMAT_R8G8_UINT,              DXGI_FORMAT_R8G8_UINT,              2, false },
        { Format::RG8_UNORM,            DXGI_FORMAT_R8G8_TYPELESS,          DXGI_FORMAT_R8G8_UNORM,             DXGI_FORMAT_R8G8_UNORM,             2, false },
        { Format::R16_UINT,             DXGI_FORMAT_R16_TYPELESS,           DXGI_FORMAT_R16_UINT,               DXGI_FORMAT_R16_UINT,               2, false },
        { Format::R16_UNORM,            DXGI_FORMAT_R16_TYPELESS,           DXGI_FORMAT_R16_UNORM,              DXGI_FORMAT_R16_UNORM,              2, false },
        { Format::R16_FLOAT,            DXGI_FORMAT_R16_TYPELESS,           DXGI_FORMAT_R16_FLOAT,              DXGI_FORMAT_R16_FLOAT,              2, false },
        { Format::RGBA8_UNORM,          DXGI_FORMAT_R8G8B8A8_TYPELESS,      DXGI_FORMAT_R8G8B8A8_UNORM,         DXGI_FORMAT_R8G8B8A8_UNORM,         4, false },
        { Format::RGBA8_SNORM,          DXGI_FORMAT_R8G8B8A8_TYPELESS,      DXGI_FORMAT_R8G8B8A8_SNORM,         DXGI_FORMAT_R8G8B8A8_SNORM,         4, false },
        { Format::BGRA8_UNORM,          DXGI_FORMAT_B8G8R8A8_TYPELESS,      DXGI_FORMAT_B8G8R8A8_UNORM,         DXGI_FORMAT_B8G8R8A8_UNORM,         4, false },
        { Format::SRGBA8_UNORM,         DXGI_FORMAT_R8G8B8A8_TYPELESS,      DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,    DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,    4, false },
		{ Format::SBGRA8_UNORM,         DXGI_FORMAT_B8G8R8A8_TYPELESS,      DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,    DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,    4, false },
		{ Format::R10G10B10A2_UNORM,    DXGI_FORMAT_R10G10B10A2_TYPELESS,   DXGI_FORMAT_R10G10B10A2_UNORM,      DXGI_FORMAT_R10G10B10A2_UNORM,      4, false },
		{ Format::R11G11B10_FLOAT,      DXGI_FORMAT_R11G11B10_FLOAT,        DXGI_FORMAT_R11G11B10_FLOAT,        DXGI_FORMAT_R11G11B10_FLOAT,        4, false },
        { Format::RG16_UINT,            DXGI_FORMAT_R16G16_TYPELESS,        DXGI_FORMAT_R16G16_UINT,            DXGI_FORMAT_R16G16_UINT,            4, false },
        { Format::RG16_FLOAT,           DXGI_FORMAT_R16G16_TYPELESS,        DXGI_FORMAT_R16G16_FLOAT,           DXGI_FORMAT_R16G16_FLOAT,           4, false },
        { Format::R32_UINT,             DXGI_FORMAT_R32_TYPELESS,           DXGI_FORMAT_R32_UINT,               DXGI_FORMAT_R32_UINT,               4, false },
        { Format::R32_FLOAT,            DXGI_FORMAT_R32_TYPELESS,           DXGI_FORMAT_R32_FLOAT,              DXGI_FORMAT_R32_FLOAT,              4, false },
		{ Format::RGBA16_FLOAT,         DXGI_FORMAT_R16G16B16A16_TYPELESS,  DXGI_FORMAT_R16G16B16A16_FLOAT,     DXGI_FORMAT_R16G16B16A16_FLOAT,     8, false },
		{ Format::RGBA16_UNORM,         DXGI_FORMAT_R16G16B16A16_TYPELESS,  DXGI_FORMAT_R16G16B16A16_UNORM,     DXGI_FORMAT_R16G16B16A16_UNORM,     8, false },
		{ Format::RGBA16_SNORM,         DXGI_FORMAT_R16G16B16A16_TYPELESS,  DXGI_FORMAT_R16G16B16A16_SNORM,     DXGI_FORMAT_R16G16B16A16_SNORM,     8, false },
        { Format::RG32_UINT,            DXGI_FORMAT_R32G32_TYPELESS,        DXGI_FORMAT_R32G32_UINT,            DXGI_FORMAT_R32G32_UINT,            8, false },
        { Format::RG32_FLOAT,           DXGI_FORMAT_R32G32_TYPELESS,        DXGI_FORMAT_R32G32_FLOAT,           DXGI_FORMAT_R32G32_FLOAT,           8, false },
        { Format::RGB32_UINT,           DXGI_FORMAT_R32G32B32_TYPELESS,     DXGI_FORMAT_R32G32B32_UINT,         DXGI_FORMAT_R32G32B32_UINT,         12, false },
        { Format::RGB32_FLOAT,          DXGI_FORMAT_R32G32B32_TYPELESS,     DXGI_FORMAT_R32G32B32_FLOAT,        DXGI_FORMAT_R32G32B32_FLOAT,        12, false },
        { Format::RGBA32_UINT,          DXGI_FORMAT_R32G32B32A32_TYPELESS,  DXGI_FORMAT_R32G32B32A32_UINT,      DXGI_FORMAT_R32G32B32A32_UINT,      16, false },
        { Format::RGBA32_FLOAT,         DXGI_FORMAT_R32G32B32A32_TYPELESS,  DXGI_FORMAT_R32G32B32A32_FLOAT,     DXGI_FORMAT_R32G32B32A32_FLOAT,     16, false },
        { Format::D16,                  DXGI_FORMAT_R16_TYPELESS,           DXGI_FORMAT_R16_UNORM,              DXGI_FORMAT_D16_UNORM,              2, true },
        { Format::D24S8,                DXGI_FORMAT_R24G8_TYPELESS,         DXGI_FORMAT_R24_UNORM_X8_TYPELESS,  DXGI_FORMAT_D24_UNORM_S8_UINT,      4, true },
        { Format::X24G8_UINT,           DXGI_FORMAT_R24G8_TYPELESS,         DXGI_FORMAT_X24_TYPELESS_G8_UINT,   DXGI_FORMAT_D24_UNORM_S8_UINT,      4, true },
        { Format::D32,                  DXGI_FORMAT_R32_TYPELESS,           DXGI_FORMAT_R32_FLOAT,              DXGI_FORMAT_D32_FLOAT,              4, true },
        { Format::BC1,                  DXGI_FORMAT_BC1_TYPELESS,           DXGI_FORMAT_BC1_UNORM,              DXGI_FORMAT_BC1_UNORM,              0, false },
        { Format::BC2,                  DXGI_FORMAT_BC2_TYPELESS,           DXGI_FORMAT_BC2_UNORM,              DXGI_FORMAT_BC2_UNORM,              0, false },
        { Format::BC3,                  DXGI_FORMAT_BC3_TYPELESS,           DXGI_FORMAT_BC3_UNORM,              DXGI_FORMAT_BC3_UNORM,              0, false },
        { Format::BC4,                  DXGI_FORMAT_BC4_TYPELESS,           DXGI_FORMAT_BC4_UNORM,              DXGI_FORMAT_BC4_UNORM,              0, false },
        { Format::BC5,                  DXGI_FORMAT_BC5_TYPELESS,           DXGI_FORMAT_BC5_UNORM,              DXGI_FORMAT_BC5_UNORM,              0, false },
        { Format::BC6H,                 DXGI_FORMAT_BC6H_UF16,              DXGI_FORMAT_BC6H_UF16,              DXGI_FORMAT_BC6H_UF16,              0, false },
        { Format::BC7,                  DXGI_FORMAT_BC7_TYPELESS,           DXGI_FORMAT_BC7_UNORM,              DXGI_FORMAT_BC7_UNORM,              0, false },
    };

    const FormatMapping& GetFormatMapping(Format::Enum abstractFormat)
    {
        const FormatMapping& mapping = FormatMappings[abstractFormat];
        assert(mapping.abstractFormat == abstractFormat);
        return mapping;
    }

    bool GetSSE42Support()
    {
        int cpui[4];
        __cpuidex(cpui, 1, 0);
        return !!(cpui[2] & 0x100000);
    }

    static const bool CpuSupportsSSE42 = GetSSE42Support();

    static const uint32_t CrcTable[] = {
        0x00000000, 0x77073096, 0xee0e612c, 0x990951ba, 0x076dc419, 0x706af48f,
        0xe963a535, 0x9e6495a3, 0x0edb8832, 0x79dcb8a4, 0xe0d5e91e, 0x97d2d988,
        0x09b64c2b, 0x7eb17cbd, 0xe7b82d07, 0x90bf1d91, 0x1db71064, 0x6ab020f2,
        0xf3b97148, 0x84be41de, 0x1adad47d, 0x6ddde4eb, 0xf4d4b551, 0x83d385c7,
        0x136c9856, 0x646ba8c0, 0xfd62f97a, 0x8a65c9ec, 0x14015c4f, 0x63066cd9,
        0xfa0f3d63, 0x8d080df5, 0x3b6e20c8, 0x4c69105e, 0xd56041e4, 0xa2677172,
        0x3c03e4d1, 0x4b04d447, 0xd20d85fd, 0xa50ab56b, 0x35b5a8fa, 0x42b2986c,
        0xdbbbc9d6, 0xacbcf940, 0x32d86ce3, 0x45df5c75, 0xdcd60dcf, 0xabd13d59,
        0x26d930ac, 0x51de003a, 0xc8d75180, 0xbfd06116, 0x21b4f4b5, 0x56b3c423,
        0xcfba9599, 0xb8bda50f, 0x2802b89e, 0x5f058808, 0xc60cd9b2, 0xb10be924,
        0x2f6f7c87, 0x58684c11, 0xc1611dab, 0xb6662d3d, 0x76dc4190, 0x01db7106,
        0x98d220bc, 0xefd5102a, 0x71b18589, 0x06b6b51f, 0x9fbfe4a5, 0xe8b8d433,
        0x7807c9a2, 0x0f00f934, 0x9609a88e, 0xe10e9818, 0x7f6a0dbb, 0x086d3d2d,
        0x91646c97, 0xe6635c01, 0x6b6b51f4, 0x1c6c6162, 0x856530d8, 0xf262004e,
        0x6c0695ed, 0x1b01a57b, 0x8208f4c1, 0xf50fc457, 0x65b0d9c6, 0x12b7e950,
        0x8bbeb8ea, 0xfcb9887c, 0x62dd1ddf, 0x15da2d49, 0x8cd37cf3, 0xfbd44c65,
        0x4db26158, 0x3ab551ce, 0xa3bc0074, 0xd4bb30e2, 0x4adfa541, 0x3dd895d7,
        0xa4d1c46d, 0xd3d6f4fb, 0x4369e96a, 0x346ed9fc, 0xad678846, 0xda60b8d0,
        0x44042d73, 0x33031de5, 0xaa0a4c5f, 0xdd0d7cc9, 0x5005713c, 0x270241aa,
        0xbe0b1010, 0xc90c2086, 0x5768b525, 0x206f85b3, 0xb966d409, 0xce61e49f,
        0x5edef90e, 0x29d9c998, 0xb0d09822, 0xc7d7a8b4, 0x59b33d17, 0x2eb40d81,
        0xb7bd5c3b, 0xc0ba6cad, 0xedb88320, 0x9abfb3b6, 0x03b6e20c, 0x74b1d29a,
        0xead54739, 0x9dd277af, 0x04db2615, 0x73dc1683, 0xe3630b12, 0x94643b84,
        0x0d6d6a3e, 0x7a6a5aa8, 0xe40ecf0b, 0x9309ff9d, 0x0a00ae27, 0x7d079eb1,
        0xf00f9344, 0x8708a3d2, 0x1e01f268, 0x6906c2fe, 0xf762575d, 0x806567cb,
        0x196c3671, 0x6e6b06e7, 0xfed41b76, 0x89d32be0, 0x10da7a5a, 0x67dd4acc,
        0xf9b9df6f, 0x8ebeeff9, 0x17b7be43, 0x60b08ed5, 0xd6d6a3e8, 0xa1d1937e,
        0x38d8c2c4, 0x4fdff252, 0xd1bb67f1, 0xa6bc5767, 0x3fb506dd, 0x48b2364b,
        0xd80d2bda, 0xaf0a1b4c, 0x36034af6, 0x41047a60, 0xdf60efc3, 0xa867df55,
        0x316e8eef, 0x4669be79, 0xcb61b38c, 0xbc66831a, 0x256fd2a0, 0x5268e236,
        0xcc0c7795, 0xbb0b4703, 0x220216b9, 0x5505262f, 0xc5ba3bbe, 0xb2bd0b28,
        0x2bb45a92, 0x5cb36a04, 0xc2d7ffa7, 0xb5d0cf31, 0x2cd99e8b, 0x5bdeae1d,
        0x9b64c2b0, 0xec63f226, 0x756aa39c, 0x026d930a, 0x9c0906a9, 0xeb0e363f,
        0x72076785, 0x05005713, 0x95bf4a82, 0xe2b87a14, 0x7bb12bae, 0x0cb61b38,
        0x92d28e9b, 0xe5d5be0d, 0x7cdcefb7, 0x0bdbdf21, 0x86d3d2d4, 0xf1d4e242,
        0x68ddb3f8, 0x1fda836e, 0x81be16cd, 0xf6b9265b, 0x6fb077e1, 0x18b74777,
        0x88085ae6, 0xff0f6a70, 0x66063bca, 0x11010b5c, 0x8f659eff, 0xf862ae69,
        0x616bffd3, 0x166ccf45, 0xa00ae278, 0xd70dd2ee, 0x4e048354, 0x3903b3c2,
        0xa7672661, 0xd06016f7, 0x4969474d, 0x3e6e77db, 0xaed16a4a, 0xd9d65adc,
        0x40df0b66, 0x37d83bf0, 0xa9bcae53, 0xdebb9ec5, 0x47b2cf7f, 0x30b5ffe9,
        0xbdbdf21c, 0xcabac28a, 0x53b39330, 0x24b4a3a6, 0xbad03605, 0xcdd70693,
        0x54de5729, 0x23d967bf, 0xb3667a2e, 0xc4614ab8, 0x5d681b02, 0x2a6f2b94,
        0xb40bbe37, 0xc30c8ea1, 0x5a05df1b, 0x2d02ef8d
    };

    class CrcHash
    {
    private:
        uint32_t crc;
    public:
        CrcHash() 
            : crc(0) 
        { 
        }

        uint32_t Get() 
        {
            return crc;
        }

        template<size_t size> __forceinline void AddBytesSSE42(void* p)
        {
            static_assert(size % 4 == 0, "Size of hashable types must be multiple of 4");

            uint32_t* data = (uint32_t*)p;

            const size_t numIterations = size / sizeof(uint32_t);
            for (size_t i = 0; i < numIterations; i++)
            {
                crc = _mm_crc32_u32(crc, data[i]);
            }
        }

        __forceinline void AddBytes(char* p, uint32_t size)
        {
            for (uint32_t idx = 0; idx < size; idx++)
                crc = CrcTable[(crc ^ *p++) & 0xFF] ^ (crc >> 8);
        }

        template<typename T> void Add(const T& value)
        {
            if (CpuSupportsSSE42)
                AddBytesSSE42<sizeof(value)>((void*)&value);
            else
                AddBytes((char*)&value, sizeof(value));
        }
    };

    class DescriptorHeap
    {
    private:
        RendererInterfaceD3D12* m_pParent;
        ID3D12DescriptorHeap* m_Heap;
        D3D12_CPU_DESCRIPTOR_HANDLE m_StartCpuHandle;
        D3D12_GPU_DESCRIPTOR_HANDLE m_StartGpuHandle;
        uint32_t m_Stride;
        uint32_t m_NumDescriptors;
        std::list<std::pair<UINT64, uint32_t>> m_FencePointers;
        uint32_t m_WritePointer;
        bool m_Monitored;
        const char* m_TypeString;

    public:
        DescriptorHeap(RendererInterfaceD3D12* pParent)
            : m_pParent(pParent)
            , m_Heap(NULL)
            , m_Stride(0)
            , m_NumDescriptors(0)
            , m_WritePointer(0)
            , m_Monitored(false)
        {
        }

        ~DescriptorHeap()
        {
            SAFE_RELEASE(m_Heap);
            m_WritePointer = 0;
            m_NumDescriptors = 0;
        }

        HRESULT AllocateResources(D3D12_DESCRIPTOR_HEAP_DESC& heapDesc)
        {
            HRESULT hr;

            hr = m_pParent->m_pDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_Heap));

            HR_RETURN(hr);

            m_NumDescriptors = heapDesc.NumDescriptors;
            m_StartCpuHandle = m_Heap->GetCPUDescriptorHandleForHeapStart();
            m_StartGpuHandle = m_Heap->GetGPUDescriptorHandleForHeapStart();
            m_Stride = m_pParent->m_pDevice->GetDescriptorHandleIncrementSize(heapDesc.Type);
            //m_Monitored = heapDesc.Type == D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV && (heapDesc.Flags & D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE) != 0;

            switch (heapDesc.Type)
            {
            case D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV:    m_TypeString = "SRV"; break;
            case D3D12_DESCRIPTOR_HEAP_TYPE_DSV:            m_TypeString = "DSV"; break;
            case D3D12_DESCRIPTOR_HEAP_TYPE_RTV:            m_TypeString = "RTV"; break;
            case D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER:        m_TypeString = "SAMPLER"; break;
            default:                                        m_TypeString = "UNKNOWN"; break;
            }

            return S_OK;
        }

        bool AllocateDescriptors(uint32_t numDescriptors, uint32_t & firstIndex)
        {
            uint32_t available = m_NumDescriptors - m_WritePointer;
            if (!m_FencePointers.empty() && m_FencePointers.begin()->second > m_WritePointer)
                available = m_FencePointers.begin()->second - m_WritePointer - 1;

            if (numDescriptors > available)
            {
                UINT64 fenceToSync = 0;

                if (m_WritePointer + numDescriptors > m_NumDescriptors)
                {
                    for (auto pair : m_FencePointers)
                        if (pair.second <= m_WritePointer && pair.second > numDescriptors)
                        {
                            fenceToSync = pair.first;
                            break;
                        }

                    m_WritePointer = 0;

                    // DEBUG_PRINTF(DESCRIPTOR_HEAP, "INFO: %s descriptor heap wrap-around!\n", m_TypeString);
                }
                else
                {
                    for (auto pair : m_FencePointers)
                        if (pair.second > m_WritePointer + numDescriptors || pair.second < m_WritePointer)
                        {
                            fenceToSync = pair.first;
                            break;
                        }
                }

                if (m_Monitored)
                    DEBUG_PRINTF(DESCRIPTOR_HEAP, "SRV: Wait for fence %llu\n", fenceToSync);

                if (fenceToSync > 0)
                    m_pParent->waitForFence(fenceToSync, m_TypeString);
                else
                    m_pParent->syncWithGPU(m_TypeString);
            }

            if (m_Monitored)
                DEBUG_PRINTF(DESCRIPTOR_HEAP, "SRV: Allocated range %d - %d\n", m_WritePointer, m_WritePointer + numDescriptors - 1);

            firstIndex = m_WritePointer;
            m_WritePointer += numDescriptors;
            return true;
        }

        D3D12_CPU_DESCRIPTOR_HANDLE GetCpuHandle(uint32_t index)
        {
            D3D12_CPU_DESCRIPTOR_HANDLE handle = m_StartCpuHandle;
            handle.ptr += index * m_Stride;
            return handle;
        }

        D3D12_GPU_DESCRIPTOR_HANDLE GetGpuHandle(uint32_t index)
        {
            D3D12_GPU_DESCRIPTOR_HANDLE handle = m_StartGpuHandle;
            handle.ptr += index * m_Stride;
            return handle;
        }

        ID3D12DescriptorHeap * GetHeap()
        {
            return m_Heap;
        }

        void AddFencePointer(UINT64 fenceValue)
        {
            m_FencePointers.push_back(std::make_pair(fenceValue, m_WritePointer));

            if (m_Monitored)
                DEBUG_PRINTF(DESCRIPTOR_HEAP, "SRV: Fence %llu at %d\n", fenceValue, m_WritePointer);
        }

        void ReleaseFences(UINT64 lastCompletedValue)
        {
            if (m_FencePointers.empty())
            {
                DebugBreak();
                return; // suspicious
            }

            auto it = m_FencePointers.begin();
            while (it != m_FencePointers.end() && it->first < lastCompletedValue)
                it++;

            if (it == m_FencePointers.end())
                DebugBreak();

            m_FencePointers.erase(m_FencePointers.begin(), it);

            if (m_Monitored)
                DEBUG_PRINTF(DESCRIPTOR_HEAP, "SRV: Released until %llu, new head at %d, write pointer %d\n", lastCompletedValue, it->second, m_WritePointer);
        }
    };

    class StaticDescriptorHeap
    {

    private:
        RendererInterfaceD3D12* m_pParent;
        ID3D12DescriptorHeap* m_Heap;
        D3D12_CPU_DESCRIPTOR_HANDLE m_StartCpuHandle;
        uint32_t m_Stride;
        uint32_t m_NumDescriptors;
        std::vector<bool> m_AllocatedDescriptors;
        DescriptorIndex m_SearchStart;

    public:
        StaticDescriptorHeap(RendererInterfaceD3D12* pParent)
            : m_pParent(pParent)
            , m_Heap(NULL)
            , m_Stride(0)
            , m_NumDescriptors(0)
            , m_SearchStart(0)
        {
        }

        ~StaticDescriptorHeap()
        {
            SAFE_RELEASE(m_Heap);
        }

        HRESULT AllocateResources(D3D12_DESCRIPTOR_HEAP_DESC& heapDesc)
        {
            HRESULT hr;

            hr = m_pParent->m_pDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_Heap));

            HR_RETURN(hr);

            m_NumDescriptors = heapDesc.NumDescriptors;
            m_StartCpuHandle = m_Heap->GetCPUDescriptorHandleForHeapStart();
            m_Stride = m_pParent->m_pDevice->GetDescriptorHandleIncrementSize(heapDesc.Type);
            m_AllocatedDescriptors.resize(m_NumDescriptors);

            return S_OK;
        }

        HRESULT Grow()
        {
            D3D12_DESCRIPTOR_HEAP_DESC desc = m_Heap->GetDesc();
            desc.NumDescriptors *= 2;
                
            uint32_t oldSize = m_NumDescriptors;
            D3D12_CPU_DESCRIPTOR_HANDLE oldStart = m_StartCpuHandle;
            ID3D12DescriptorHeap* oldHeap = m_Heap;

            HR_RETURN(AllocateResources(desc));

            m_pParent->m_pDevice->CopyDescriptorsSimple(oldSize, m_StartCpuHandle, oldStart, desc.Type);

            // Put the old heap into the deletion pool
            auto wrapper = new DescriptorHeapWrapper();
            wrapper->heap = oldHeap;
            wrapper->fenceCounterAtLastUse = m_pParent->getFenceCounter();
            m_pParent->deferredDestroyResource(wrapper);

            return S_OK;
        }

        DescriptorIndex AllocateDescriptor()
        {
            DescriptorIndex index = m_SearchStart;
            while (m_AllocatedDescriptors[index])
            {
                index++;

                if (index == m_NumDescriptors)
                {
                    Grow();
                    break;
                }
            }

            m_AllocatedDescriptors[index] = true;
            m_SearchStart = std::min(index + 1, m_NumDescriptors - 1);
            return index;
        }

        void ReleaseDescriptor(DescriptorIndex index)
        {
            m_AllocatedDescriptors[index] = false;
            if (m_SearchStart > index)
                m_SearchStart = index;
        }

        D3D12_CPU_DESCRIPTOR_HANDLE GetCpuHandle(DescriptorIndex index)
        {
            D3D12_CPU_DESCRIPTOR_HANDLE handle = m_StartCpuHandle;
            handle.ptr += index * m_Stride;
            return handle;
        }
    };

    template<typename T> T Align(T size, uint32_t alignment)
    {
        return (size + alignment - 1) & ~(T(alignment) - 1);
    }

    class UploadManager
    {
    private:
        RendererInterfaceD3D12* m_pParent;
        ID3D12Resource* m_UploadBuffer;
        void* m_pUploadBufferHostData;
		D3D12_GPU_VIRTUAL_ADDRESS m_UploadBufferGPUVA;
        UINT64 m_BufferSize;
        UINT64 m_WritePointer;
        std::list<std::pair<UINT64, UINT64>> m_FencePointers;
        const std::list<ConstantBuffer*>& m_ConstantBuffers;
    public:
        UploadManager(RendererInterfaceD3D12* pParent, const std::list<ConstantBuffer*>& constantBuffers)
            : m_pParent(pParent)
            , m_ConstantBuffers(constantBuffers)
            , m_UploadBuffer(NULL)
            , m_pUploadBufferHostData(NULL)
            , m_BufferSize(0)
            , m_WritePointer(0)
        {
        }

        ~UploadManager()
        {
            SAFE_RELEASE(m_UploadBuffer);
        }


        HRESULT AllocateResources(size_t bufferSize)
        {
            m_BufferSize = bufferSize;

            HRESULT hr;

            D3D12_HEAP_PROPERTIES heapProps = {};
            heapProps.Type = D3D12_HEAP_TYPE_UPLOAD;

            D3D12_RESOURCE_DESC bufferDesc = {};
            bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
            bufferDesc.Width = m_BufferSize;
            bufferDesc.Height = 1;
            bufferDesc.DepthOrArraySize = 1;
            bufferDesc.MipLevels = 1;
            bufferDesc.SampleDesc.Count = 1;
            bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

            hr = m_pParent->m_pDevice->CreateCommittedResource(
                &heapProps,
                D3D12_HEAP_FLAG_NONE,
                &bufferDesc,
                D3D12_RESOURCE_STATE_GENERIC_READ,
                NULL,
                IID_PPV_ARGS(&m_UploadBuffer));

            HR_RETURN(hr);

			m_UploadBufferGPUVA = m_UploadBuffer->GetGPUVirtualAddress();

            hr = m_UploadBuffer->Map(0, NULL, &m_pUploadBufferHostData);

            HR_RETURN(hr);

            return S_OK;
        }

        UINT64 UploadManager::SuballocateBuffer(UINT64 size, UINT alignment = 256)
        {
            if (size > m_BufferSize)
            {
                // There is not enough room in the upload buffer to allocate this.
                DebugBreak();
                return 0;
            }

            UINT64 available = m_BufferSize - m_WritePointer;
            if (!m_FencePointers.empty() && m_FencePointers.front().second > m_WritePointer)
                available = m_FencePointers.begin()->second - m_WritePointer - 1;

            UINT64 alignedWritePointer = Align(m_WritePointer, alignment);
            UINT64 extendedSize = size + alignedWritePointer - m_WritePointer;

            if (extendedSize > available)
            {
                UINT64 fenceToSync = 0;

                if (alignedWritePointer + size > m_BufferSize)
                {
                    for (auto pair : m_FencePointers)
                        if (pair.second <= m_WritePointer && pair.second > size)
                        {
                            fenceToSync = pair.first;
                            break;
                        }

                    m_WritePointer = 0;

                    DEBUG_PRINT(UPLOAD_BUFFER, "INFO: upload buffer wrap-around!\n");
                }
                else
                {
                    for (auto pair : m_FencePointers)
                        if (pair.second > alignedWritePointer + size || pair.second < m_WritePointer)
                        {
                            fenceToSync = pair.first;
                            break;
                        }

                    m_WritePointer = alignedWritePointer;
                }

                if (fenceToSync > 0)
                    m_pParent->waitForFence(fenceToSync, "UploadBuffer");
                else
                    m_pParent->syncWithGPU("UploadBuffer");
            }
            else
            {
                m_WritePointer = alignedWritePointer;
            }

            UINT64 bufferOffset = m_WritePointer;
            m_WritePointer += size;
            return bufferOffset;
        }

        void* GetCpuVA(UINT64 bufferOffset)
        {
            return (char*)m_pUploadBufferHostData + bufferOffset;
        }

        D3D12_GPU_VIRTUAL_ADDRESS GetGpuVA(UINT64 bufferOffset)
        {
            return m_UploadBufferGPUVA + bufferOffset;
        }

        ID3D12Resource* GetBuffer()
        {
            return m_UploadBuffer;
        }

        void AddFencePointer(UINT64 fenceValue)
        {
            m_FencePointers.push_back(std::make_pair(fenceValue, m_WritePointer));
        }

        void ReleaseFences(UINT64 lastCompletedValue)
        {
            if (m_FencePointers.empty())
            {
                DebugBreak();
                return; // suspicious
            }

            auto it = m_FencePointers.begin();
            while (it != m_FencePointers.end() && it->first < lastCompletedValue)
                it++;

            if (it == m_FencePointers.end())
                DebugBreak();

            // mark the cbuffers in the released region as invalid
            UINT64 invalidUntil = it->second;
            bool disjointRegion = invalidUntil > m_WritePointer;

            for (auto cbuffer : m_ConstantBuffers)
            {
                if (!cbuffer->uploadedDataValid)
                    continue;

                // continuous:  [------IU******WP---]   ( * are valid, - are invalid )
                // disjoint:    [***WP--------IU****]
                bool a = cbuffer->currentVersionOffset >= invalidUntil;
                bool b = cbuffer->currentVersionOffset + cbuffer->alignedSize < m_WritePointer;

                if (disjointRegion && (a || b) || !disjointRegion && (a && b))
                    continue;
                    
                cbuffer->uploadedDataValid = false;
                cbuffer->numEvictions++;
            }

            m_FencePointers.erase(m_FencePointers.begin(), it);
        }
    };
        
    struct BackendResources
    {
        RendererInterfaceD3D12* parent;

        std::set<Shader*> shaders;
        std::set<Texture*> textures;
        std::set<Buffer*> buffers;
        std::list<ConstantBuffer*> constantBuffers;
        std::set<Sampler*> samplers;
        std::set<InputLayout*> inputLayouts;
        std::set<PerformanceQuery*> perfQueries;
        std::set<ManagedResource*> deletedResources;
        std::set<Texture*> deletedTextures;
        std::set<Sampler*> deletedSamplers;
        std::set<Buffer*> deletedBuffers;
        std::set<PerformanceQuery*> deletedPerfQueries;
        std::list<CommandListHandle> commandLists;
        StaticDescriptorHeap dhRTV;
        StaticDescriptorHeap dhDSV;
        StaticDescriptorHeap dhSRVstatic;
        StaticDescriptorHeap dhSamplerStatic;
        DescriptorHeap dhSRVetc;
        DescriptorHeap dhSamplers;
        UploadManager upload;

        std::map<uint32_t, PipelineStateHandle> psoCache;
        std::map<uint32_t, RootSignatureHandle> rootsigCache;
        std::vector<D3D12_RESOURCE_BARRIER> barrier;

        ID3D12Fence* fence;
        HANDLE fenceEvent;
        UINT64 fenceCounter;

        ID3D12CommandSignature* drawIndirectSignature;
        ID3D12CommandSignature* dispatchIndirectSignature;

        DescriptorIndex nullCBV;
        DescriptorIndex nullSRV;
        DescriptorIndex nullUAV;
        DescriptorIndex nullSampler;

        ID3D12QueryHeap* perfQueryHeap;
        uint32_t nextQueryIndex;
        Buffer* perfQueryResolveBuffer;

		ID3D12RootSignature* currentRS;
		ID3D12PipelineState* currentPSO;
		D3D12_CPU_DESCRIPTOR_HANDLE currentRTVs[8];
		D3D12_CPU_DESCRIPTOR_HANDLE currentDSV;
		D3D12_VERTEX_BUFFER_VIEW currentVBVs[16];
		D3D12_INDEX_BUFFER_VIEW currentIBV;

#if NVRHI_D3D12_WITH_NVAPI
		NV_MODIFIED_W_PARAMS currentModifiedWParams;
		bool spsEnabled;
		uint32_t spsRenderTargetIndexOffset; 
		bool spsIndependentViewportMask;
#endif

        BackendResources(RendererInterfaceD3D12* pParent)
            : parent(pParent)
            , dhRTV(pParent)
            , dhDSV(pParent)
            , dhSRVetc(pParent)
            , dhSRVstatic(pParent)
            , dhSamplerStatic(pParent)
            , dhSamplers(pParent)
            , upload(pParent, constantBuffers)
            , fence(nullptr)
            , fenceEvent(0)
            , fenceCounter(0)
            , drawIndirectSignature(nullptr)
            , dispatchIndirectSignature(nullptr)
            , nullCBV(INVALID_DESCRIPTOR_INDEX)
            , nullSRV(INVALID_DESCRIPTOR_INDEX)
            , nullUAV(INVALID_DESCRIPTOR_INDEX)
            , nullSampler(INVALID_DESCRIPTOR_INDEX)
            , perfQueryHeap(nullptr)
            , nextQueryIndex(0)
            , perfQueryResolveBuffer(nullptr)
			, currentRS(nullptr)
			, currentPSO(nullptr)
#if NVRHI_D3D12_WITH_NVAPI
			, spsEnabled(false)
			, spsRenderTargetIndexOffset(0)
			, spsIndependentViewportMask(false)
#endif
        {
			memset(currentRTVs, 0, sizeof(currentRTVs));
			memset(currentVBVs, 0, sizeof(currentVBVs));
			memset(&currentIBV, 0, sizeof(currentIBV));
			currentDSV.ptr = 0;
#if NVRHI_D3D12_WITH_NVAPI
			memset(&currentModifiedWParams, 0, sizeof(currentModifiedWParams));
#endif
        }

        ~BackendResources()
        {
            for (auto shader : shaders)
                delete shader;

            for (auto texture : textures)
                delete texture;

            for (auto buffer : buffers)
                delete buffer;

            for (auto cbuffer : constantBuffers)
                delete cbuffer;

            for (auto sampler : samplers)
                delete sampler;

            for (auto layout : inputLayouts)
                delete layout;

            for (auto query : perfQueries)
                delete query;

            for (auto& pair : psoCache)
                delete pair.second;

            for (auto& pair : rootsigCache)
                delete pair.second;

            for (auto list : commandLists)
                delete list;

            SAFE_RELEASE(fence);

            if (fenceEvent)
            {
                CloseHandle(fenceEvent);
                fenceEvent = 0;
            }

            SAFE_RELEASE(drawIndirectSignature);
            SAFE_RELEASE(dispatchIndirectSignature);
            SAFE_RELEASE(perfQueryHeap);
        }

        void SetFence()
        {
            fenceCounter++;
            parent->m_pCommandQueue->Signal(fence, fenceCounter);
            
            dhSRVetc.AddFencePointer(fenceCounter);
            dhSamplers.AddFencePointer(fenceCounter);

            upload.AddFencePointer(fenceCounter);
        }

        template<typename T> void cleanupDeletedResources(std::set<T*>& pool, UINT64 completedFence)
        {
            std::set<T*> unreferenced;
            for (auto resource : pool)
            {
                if (resource->fenceCounterAtLastUse < completedFence)
                    unreferenced.insert(resource);
            }

            for (auto resource : unreferenced)
            {
                delete resource;
                pool.erase(resource);
            }
        }

        void WaitForFence(UINT64 fenceValue, const char* reason)
        {
            (void)reason; // unused in non-debug builds

            if (fenceValue > fenceCounter)
                DebugBreak();

            fenceValue = std::min(fenceValue, fenceCounter);

            UINT64 completed = fence->GetCompletedValue();
            if (completed < fenceValue)
            {
                fence->SetEventOnCompletion(fenceValue, fenceEvent);
#ifdef _DEBUG
                START_CPU_PERF
#endif

                WaitForSingleObject(fenceEvent, INFINITE);

#ifdef _DEBUG
                END_CPU_PERF(time)
                DEBUG_PRINTF(FENCE_TIMING, "D3D12 RHI: WaitForFence(%llu, %s) took %.3f ms\n", fenceValue, reason, time * 1000.0);
#endif

                completed = fenceValue;
                //completed = fence->GetCompletedValue();
            }

            dhSRVetc.ReleaseFences(completed);
            dhSamplers.ReleaseFences(completed);

            upload.ReleaseFences(completed);

            cleanupDeletedResources<ManagedResource>(deletedResources, completed);
            cleanupDeletedResources<Texture>(deletedTextures, completed);
            cleanupDeletedResources<Sampler>(deletedSamplers, completed);
            cleanupDeletedResources<Buffer>(deletedBuffers, completed);
            cleanupDeletedResources<PerformanceQuery>(deletedPerfQueries, completed);
        }
    };

    RendererInterfaceD3D12::RendererInterfaceD3D12(IErrorCallback * errorCB, ID3D12Device * pDevice, ID3D12CommandQueue * pCommandQueue)
        : m_pErrorCallback(errorCB)
        , m_pDevice(pDevice)
        , m_pCommandQueue(pCommandQueue)
        , m_pResources(new BackendResources(this))
        , m_ActiveCommandList(nullptr)
    {
        m_pDevice->AddRef();
        m_pCommandQueue->AddRef();

        m_ActiveCommandList = createCommandList();

        D3D12_DESCRIPTOR_HEAP_DESC descriptorHeapDesc = {};
        descriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        descriptorHeapDesc.NumDescriptors = 1024;

        m_pResources->dhDSV.AllocateResources(descriptorHeapDesc);

        descriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        descriptorHeapDesc.NumDescriptors = 1024;

        m_pResources->dhRTV.AllocateResources(descriptorHeapDesc);

        descriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        descriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        descriptorHeapDesc.NumDescriptors = 16384;

        m_pResources->dhSRVetc.AllocateResources(descriptorHeapDesc);

        descriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        descriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        descriptorHeapDesc.NumDescriptors = 1024;

        m_pResources->dhSRVstatic.AllocateResources(descriptorHeapDesc);

        descriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
        descriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        descriptorHeapDesc.NumDescriptors = 128;

        m_pResources->dhSamplerStatic.AllocateResources(descriptorHeapDesc);

        descriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
        descriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        descriptorHeapDesc.NumDescriptors = 2048;

        m_pResources->dhSamplers.AllocateResources(descriptorHeapDesc);

        m_pResources->upload.AllocateResources(128 * 1024 * 1024);

        m_pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_pResources->fence));
        m_pResources->fenceEvent = CreateEvent(nullptr, false, false, nullptr);
        m_pResources->fenceCounter = 0;

        {
            D3D12_INDIRECT_ARGUMENT_DESC argDesc = {};
            D3D12_COMMAND_SIGNATURE_DESC csDesc = {};
            csDesc.NumArgumentDescs = 1;
            csDesc.pArgumentDescs = &argDesc;

            csDesc.ByteStride = 16;
            argDesc.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW;
            m_pDevice->CreateCommandSignature(&csDesc, nullptr, IID_PPV_ARGS(&m_pResources->drawIndirectSignature));

            csDesc.ByteStride = 12;
            argDesc.Type = D3D12_INDIRECT_ARGUMENT_TYPE_DISPATCH;
            m_pDevice->CreateCommandSignature(&csDesc, nullptr, IID_PPV_ARGS(&m_pResources->dispatchIndirectSignature));
        }

        {
            m_pResources->nullCBV = m_pResources->dhSRVstatic.AllocateDescriptor();
            m_pResources->nullSRV = m_pResources->dhSRVstatic.AllocateDescriptor();
            m_pResources->nullUAV = m_pResources->dhSRVstatic.AllocateDescriptor();
            m_pResources->nullSampler = m_pResources->dhSamplerStatic.AllocateDescriptor();

            m_pDevice->CreateConstantBufferView(nullptr, m_pResources->dhSRVstatic.GetCpuHandle(m_pResources->nullCBV));

            D3D12_SHADER_RESOURCE_VIEW_DESC descSRV = {};
            descSRV.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
            descSRV.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
            descSRV.Format = DXGI_FORMAT_R32_UINT;

            m_pDevice->CreateShaderResourceView(nullptr, &descSRV, m_pResources->dhSRVstatic.GetCpuHandle(m_pResources->nullSRV));

            D3D12_UNORDERED_ACCESS_VIEW_DESC descUAV = {};
            descUAV.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;
            descUAV.Format = DXGI_FORMAT_R32_UINT;

            m_pDevice->CreateUnorderedAccessView(nullptr, nullptr, &descUAV, m_pResources->dhSRVstatic.GetCpuHandle(m_pResources->nullUAV));

            D3D12_SAMPLER_DESC descSampler = {};
            descSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
            descSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
            descSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;

            m_pDevice->CreateSampler(&descSampler, m_pResources->dhSamplerStatic.GetCpuHandle(m_pResources->nullSampler));
        }

        D3D12_QUERY_HEAP_DESC queryHeapDesc = {};
        queryHeapDesc.Type = D3D12_QUERY_HEAP_TYPE_TIMESTAMP;
        queryHeapDesc.Count = 256;
        m_pDevice->CreateQueryHeap(&queryHeapDesc, IID_PPV_ARGS(&m_pResources->perfQueryHeap));

        BufferDesc qbDesc;
        qbDesc.byteSize = queryHeapDesc.Count * 8;
        m_pResources->perfQueryResolveBuffer = static_cast<Buffer*>(createBuffer(qbDesc, nullptr));


		ID3D12DescriptorHeap* heaps[2] = { m_pResources->dhSRVetc.GetHeap(), m_pResources->dhSamplers.GetHeap() };
		m_ActiveCommandList->commandList->SetDescriptorHeaps(2, heaps);
    }

    RendererInterfaceD3D12::~RendererInterfaceD3D12()
    {
        syncWithGPU("Shutdown");

        delete m_pResources;
        delete m_ActiveCommandList;
        SAFE_RELEASE(m_pDevice);
        SAFE_RELEASE(m_pCommandQueue);
    }

	void RendererInterfaceD3D12::enableDebugMessages(MessageCategories categories)
	{
		g_MessageCategories = MessageCategories( int(g_MessageCategories) | int(categories) );
	}

	void RendererInterfaceD3D12::disableDebugMessages(MessageCategories categories)
	{
		g_MessageCategories = MessageCategories( int(g_MessageCategories) & ~int(categories) );
	}

    TextureHandle RendererInterfaceD3D12::getHandleForTexture(void* resource, Format::Enum formatOverride)
    {
        ID3D12Resource* pResource = static_cast<ID3D12Resource*>(resource);

        if (pResource == nullptr)
            return nullptr;

        for (auto texture : m_pResources->textures)
            if (texture->resource == pResource)
                return texture;

        D3D12_RESOURCE_DESC desc = pResource->GetDesc();
        Texture* texture = new Texture(this);
        texture->isManaged = false;
        texture->resource = pResource;
        texture->desc.width = uint32_t(desc.Width);
        texture->desc.height = desc.Height;
        texture->desc.depthOrArraySize = desc.DepthOrArraySize;
        texture->desc.mipLevels = desc.MipLevels;
        texture->desc.sampleCount = desc.SampleDesc.Count;
        texture->desc.sampleQuality = desc.SampleDesc.Quality;
        texture->desc.isArray = desc.DepthOrArraySize > 1 && desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        texture->desc.isRenderTarget = (desc.Flags & (D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET | D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL)) != 0;
        texture->desc.isUAV = (desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) != 0;

		if (formatOverride == Format::UNKNOWN)
		{
			for (auto& mapping : FormatMappings)
				if (mapping.resourceFormat == desc.Format || mapping.srvFormat == desc.Format || mapping.rtvFormat == desc.Format)
				{
					texture->desc.format = mapping.abstractFormat;
					break;
				}
		}
		else
		{
			texture->desc.format = formatOverride;
        }
        texture->numPlanes = (texture->desc.format == Format::D24S8) ? 2u : 1u;

        CHECK_ERROR(desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE2D || desc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D, "Unsupported unmanaged texture dimension");
        CHECK_ERROR(texture->desc.format != Format::UNKNOWN, "Unknown unmanaged texture format");

        uint32_t numSubresources = texture->desc.mipLevels * texture->numPlanes;
        if (texture->desc.isArray)
            numSubresources *= texture->desc.depthOrArraySize;
        texture->subresourceStates.resize(numSubresources, D3D12_RESOURCE_STATE_COMMON);

        pResource->AddRef();
        m_pResources->textures.insert(texture);
        return texture;
    }

    void RendererInterfaceD3D12::setNonManagedTextureResourceState(TextureHandle texture, uint32_t state)
    {
        for (auto& storedState : static_cast<Texture*>(texture)->subresourceStates)
            storedState = D3D12_RESOURCE_STATES(state);
    }

    void RendererInterfaceD3D12::releaseNonManagedTextures()
    {
        std::set<Texture*> nonManaged;

        for (auto texture : m_pResources->textures)
            if (!texture->isManaged)
                nonManaged.insert(texture);

        for (auto texture : nonManaged)
        {
            m_pResources->textures.erase(texture);
            delete texture;
        }
    }

    void RendererInterfaceD3D12::flushCommandList()
    {
        if (m_ActiveCommandList->size > 0)
        {
            m_ActiveCommandList->commandList->Close();
            m_ActiveCommandList->fenceCounterAtLastUse = m_pResources->fenceCounter;
            m_pCommandQueue->ExecuteCommandLists(1, (ID3D12CommandList**)&m_ActiveCommandList->commandList);
            m_pResources->commandLists.push_back(m_ActiveCommandList);

            m_pResources->SetFence();

            UINT64 completedFence = m_pResources->fence->GetCompletedValue();
            m_ActiveCommandList = m_pResources->commandLists.front();

            if (m_ActiveCommandList->fenceCounterAtLastUse < completedFence)
            {
                m_ActiveCommandList->allocator->Reset();
                m_ActiveCommandList->commandList->Reset(m_ActiveCommandList->allocator, nullptr);
                m_ActiveCommandList->size = 0;
                m_pResources->commandLists.pop_front();
            }
            else
            {
                m_ActiveCommandList = createCommandList();
            }

			m_pResources->currentRS = nullptr;
			m_pResources->currentPSO = nullptr;

			memset(m_pResources->currentRTVs, 0, sizeof(m_pResources->currentRTVs));
			memset(m_pResources->currentVBVs, 0, sizeof(m_pResources->currentVBVs));
			memset(&m_pResources->currentIBV, 0, sizeof(m_pResources->currentIBV));
			m_pResources->currentDSV.ptr = 0;

			ID3D12DescriptorHeap* heaps[2] = { m_pResources->dhSRVetc.GetHeap(), m_pResources->dhSamplers.GetHeap() };
			m_ActiveCommandList->commandList->SetDescriptorHeaps(2, heaps);

#if NVRHI_D3D12_WITH_NVAPI
			if (m_pResources->currentModifiedWParams.numEntries > 0)
			{
				if (NvAPI_D3D12_SetModifiedWMode(m_ActiveCommandList->commandList, &m_pResources->currentModifiedWParams) != NVAPI_OK)
					m_pResources->currentModifiedWParams.numEntries = 0;
			}

			if (m_pResources->spsEnabled)
			{
				if (NvAPI_D3D12_SetSinglePassStereoMode(m_ActiveCommandList->commandList, 2, m_pResources->spsRenderTargetIndexOffset, m_pResources->spsIndependentViewportMask) != NVAPI_OK)
					m_pResources->spsEnabled = false;
			}
#endif

            HRESULT hr = m_pDevice->GetDeviceRemovedReason();
            if (FAILED(hr))
            {
                DEBUG_PRINT(D3D_MESSAGES, "FATAL ERROR: Device Removed!\n");
                DebugBreak();
            }
        }
    }

    void RendererInterfaceD3D12::loadBalanceCommandList()
    {
        if (m_ActiveCommandList->size > MAX_COMMANDS_IN_LIST)
            flushCommandList();
    }

    void RendererInterfaceD3D12::signalError(const char * file, int line, const char * errorDesc)
    {
        m_pErrorCallback->signalError(file, line, errorDesc);
    }

    CommandListHandle RendererInterfaceD3D12::createCommandList()
    {
        CommandListHandle commandList = new CommandList();

        m_pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandList->allocator));
        m_pDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandList->allocator, nullptr, IID_PPV_ARGS(&commandList->commandList));

        return commandList;
    }

    DXGI_SAMPLE_DESC getStateSampleDesc(const DrawCallState& state)
    {
        DXGI_SAMPLE_DESC sampleDesc;
        if (state.renderState.depthTarget)
        {
            Texture* depthTarget = static_cast<Texture*>(state.renderState.depthTarget);
            sampleDesc.Count = depthTarget->desc.sampleCount;
            sampleDesc.Quality = depthTarget->desc.sampleQuality;
        }
        else if (state.renderState.targetCount > 0)
        {
            Texture* target0 = static_cast<Texture*>(state.renderState.targets[0]);
            sampleDesc.Count = target0->desc.sampleCount;
            sampleDesc.Quality = target0->desc.sampleQuality;
        }
        else
        {
            sampleDesc.Count = 1;
            sampleDesc.Quality = 0;
        }

        return sampleDesc;
    }

    uint32_t RendererInterfaceD3D12::getStateHashForRS(const DrawCallState & state)
    {
        CrcHash hash;

        hash.Add(state.VS.shader);
        hash.Add(state.HS.shader);
        hash.Add(state.DS.shader);
        hash.Add(state.GS.shader);
        hash.Add(state.PS.shader);

        return hash.Get();
    }

    uint32_t RendererInterfaceD3D12::getComputeStateHash(const DispatchState & state)
    {
        CrcHash hash;

        hash.Add(state.shader);

        return hash.Get();
    }

    uint32_t RendererInterfaceD3D12::getStateHashForPSO(const DrawCallState & state)
    {
        CrcHash hash;

        hash.Add(state.VS.shader);
        hash.Add(state.HS.shader);
        hash.Add(state.DS.shader);
        hash.Add(state.GS.shader);
        hash.Add(state.PS.shader);
        hash.Add(state.renderState.blendState);
        hash.Add(state.renderState.depthStencilState);
        hash.Add(state.renderState.rasterState);
        hash.Add(state.primType);
        hash.Add(state.inputLayout);
        hash.Add(state.renderState.depthTarget ? static_cast<Texture*>(state.renderState.depthTarget)->desc.format : Format::UNKNOWN);
        for (uint32_t target = 0; target < 8; target++)
            hash.Add(state.renderState.targets[target] ? static_cast<Texture*>(state.renderState.targets[target])->desc.format : Format::UNKNOWN);
        hash.Add(getStateSampleDesc(state));

        return hash.Get();
    }

    D3D12_SHADER_VISIBILITY convertShaderStage(ShaderType::Enum s)
    {
        switch (s)
        {
        case ShaderType::SHADER_VERTEX:
            return D3D12_SHADER_VISIBILITY_VERTEX;
        case ShaderType::SHADER_HULL:
            return D3D12_SHADER_VISIBILITY_HULL;
        case ShaderType::SHADER_DOMAIN:
            return D3D12_SHADER_VISIBILITY_DOMAIN;
        case ShaderType::SHADER_GEOMETRY:
            return D3D12_SHADER_VISIBILITY_GEOMETRY;
        case ShaderType::SHADER_PIXEL:
            return D3D12_SHADER_VISIBILITY_PIXEL;

        case ShaderType::SHADER_COMPUTE:
        default:
            return D3D12_SHADER_VISIBILITY_ALL;
        }
    }

    RootSignatureHandle RendererInterfaceD3D12::buildRootSignature(uint32_t numShaders, const ShaderHandle* shaders, bool allowInputLayout)
    {
        HRESULT hr;
        D3D12_ROOT_SIGNATURE_DESC rsDesc = {};
        if (allowInputLayout) rsDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

        D3D12_ROOT_PARAMETER rsParameters[ShaderType::GRAPHIC_SHADERS_NUM * 2];
        D3D12_DESCRIPTOR_RANGE rsdtRanges[ShaderType::GRAPHIC_SHADERS_NUM][4];
        rsDesc.pParameters = rsParameters;

        RootSignatureHandle rootsig = new RootSignature();
            
        for (uint32_t i = 0; i < numShaders; i++)
        {
            Shader* shader = static_cast<Shader*>(shaders[i]);

            if (shader == nullptr)
                continue;

            rootsig->shaders.insert(shader);

            D3D12_ROOT_PARAMETER* param = nullptr;
            uint32_t descriptorOffset = 0;

            auto addRange = [&param, &descriptorOffset](uint32_t first, uint32_t count, D3D12_DESCRIPTOR_RANGE_TYPE type)
            {
                if (count == 0)
                    return;

                D3D12_DESCRIPTOR_RANGE& range = const_cast<D3D12_DESCRIPTOR_RANGE&>(param->DescriptorTable.pDescriptorRanges[param->DescriptorTable.NumDescriptorRanges]);
                range.BaseShaderRegister = first;
                range.NumDescriptors = count;
                range.OffsetInDescriptorsFromTableStart = descriptorOffset;
                range.RangeType = type;
                range.RegisterSpace = 0;

                param->DescriptorTable.NumDescriptorRanges++;
                descriptorOffset += count;
            };

            if (shader->numBindings > 0)
            {
                param = &rsParameters[rsDesc.NumParameters];
                descriptorOffset = 0;

                param->ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
                param->DescriptorTable.NumDescriptorRanges = 0;
                param->DescriptorTable.pDescriptorRanges = rsdtRanges[rsDesc.NumParameters];
                param->ShaderVisibility = convertShaderStage(shader->type);

                addRange(shader->minCB, shader->numCB, D3D12_DESCRIPTOR_RANGE_TYPE_CBV);
                addRange(shader->minSRV, shader->numSRV, D3D12_DESCRIPTOR_RANGE_TYPE_SRV);
                addRange(shader->minUAV, shader->numUAV, D3D12_DESCRIPTOR_RANGE_TYPE_UAV);

                rsDesc.NumParameters++;
            }

            if (shader->numSamplers > 0)
            {
                param = &rsParameters[rsDesc.NumParameters];
                descriptorOffset = 0;

                param->ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
                param->DescriptorTable.NumDescriptorRanges = 0;
                param->DescriptorTable.pDescriptorRanges = rsdtRanges[rsDesc.NumParameters] + 3;
                param->ShaderVisibility = convertShaderStage(shader->type);

                addRange(shader->minSampler, shader->numSamplers, D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER);

                rsDesc.NumParameters++;
            }
        }

        ID3DBlob* rsBlob = NULL;
        ID3DBlob* errorBlob = NULL;
        hr = D3D12SerializeRootSignature(&rsDesc, D3D_ROOT_SIGNATURE_VERSION_1, &rsBlob, &errorBlob);

        if (FAILED(hr))
        {
            SIGNAL_ERROR("Failed to serialize a root signature");
            if (errorBlob)
                DEBUG_PRINT(D3D_MESSAGES, (char*)errorBlob->GetBufferPointer());
            SAFE_RELEASE(errorBlob);
            return nullptr;
        }

        hr = m_pDevice->CreateRootSignature(0, rsBlob->GetBufferPointer(), rsBlob->GetBufferSize(), IID_PPV_ARGS(&rootsig->handle));
        SAFE_RELEASE(rsBlob);

        CHECK_ERROR(SUCCEEDED(hr), "Failed to create a root signature object");

        return rootsig;
    }

    RootSignatureHandle RendererInterfaceD3D12::getRootSignature(const DrawCallState & state)
    {
        uint32_t hash = getStateHashForRS(state);

        RootSignatureHandle rootsig = m_pResources->rootsigCache[hash];
            
        if (rootsig)
            return rootsig;

        ShaderHandle shaders[5] = { 
            state.VS.shader, 
            state.HS.shader, 
            state.DS.shader, 
            state.GS.shader, 
            state.PS.shader
        };
        rootsig = buildRootSignature(5, shaders, state.inputLayout != nullptr);

        m_pResources->rootsigCache[hash] = rootsig;
        return rootsig;
    }

    RootSignatureHandle RendererInterfaceD3D12::getRootSignature(const DispatchState & state, uint32_t hash)
    {
        RootSignatureHandle rootsig = m_pResources->rootsigCache[hash];

        if (rootsig)
            return rootsig;

        rootsig = buildRootSignature(1, &state.shader, false);

        m_pResources->rootsigCache[hash] = rootsig;
        return rootsig;
    }

    D3D12_BLEND convertBlendValue(BlendState::BlendValue value)
    {
        switch (value)
        {
        case BlendState::BLEND_ZERO:
            return D3D12_BLEND_ZERO;
        case BlendState::BLEND_ONE:
            return D3D12_BLEND_ONE;
        case BlendState::BLEND_SRC_COLOR:
            return D3D12_BLEND_SRC_COLOR;
        case BlendState::BLEND_INV_SRC_COLOR:
            return D3D12_BLEND_INV_SRC_COLOR;
        case BlendState::BLEND_SRC_ALPHA:
            return D3D12_BLEND_SRC_ALPHA;
        case BlendState::BLEND_INV_SRC_ALPHA:
            return D3D12_BLEND_INV_SRC_ALPHA;
        case BlendState::BLEND_DEST_ALPHA:
            return D3D12_BLEND_DEST_ALPHA;
        case BlendState::BLEND_INV_DEST_ALPHA:
            return D3D12_BLEND_INV_DEST_ALPHA;
        case BlendState::BLEND_DEST_COLOR:
            return D3D12_BLEND_DEST_COLOR;
        case BlendState::BLEND_INV_DEST_COLOR:
            return D3D12_BLEND_INV_DEST_COLOR;
        case BlendState::BLEND_SRC_ALPHA_SAT:
            return D3D12_BLEND_SRC_ALPHA_SAT;
        case BlendState::BLEND_BLEND_FACTOR:
            return D3D12_BLEND_BLEND_FACTOR;
        case BlendState::BLEND_INV_BLEND_FACTOR:
            return D3D12_BLEND_INV_BLEND_FACTOR;
        case BlendState::BLEND_SRC1_COLOR:
            return D3D12_BLEND_SRC1_COLOR;
        case BlendState::BLEND_INV_SRC1_COLOR:
            return D3D12_BLEND_INV_SRC1_COLOR;
        case BlendState::BLEND_SRC1_ALPHA:
            return D3D12_BLEND_SRC1_ALPHA;
        case BlendState::BLEND_INV_SRC1_ALPHA:
            return D3D12_BLEND_INV_SRC1_ALPHA;
        default:
            return D3D12_BLEND_ZERO;
        }
    }

    D3D12_BLEND_OP convertBlendOp(BlendState::BlendOp value)
    {
        switch (value)
        {
        case BlendState::BLEND_OP_ADD:
            return D3D12_BLEND_OP_ADD;
        case BlendState::BLEND_OP_SUBTRACT:
            return D3D12_BLEND_OP_SUBTRACT;
        case BlendState::BLEND_OP_REV_SUBTRACT:
            return D3D12_BLEND_OP_REV_SUBTRACT;
        case BlendState::BLEND_OP_MIN:
            return D3D12_BLEND_OP_MIN;
        case BlendState::BLEND_OP_MAX:
            return D3D12_BLEND_OP_MAX;
        default:
            return D3D12_BLEND_OP_ADD;
        }
    }

    D3D12_STENCIL_OP convertStencilOp(DepthStencilState::StencilOp value)
    {
        switch (value)
        {
        case DepthStencilState::STENCIL_OP_KEEP:
            return D3D12_STENCIL_OP_KEEP;
        case DepthStencilState::STENCIL_OP_ZERO:
            return D3D12_STENCIL_OP_ZERO;
        case DepthStencilState::STENCIL_OP_REPLACE:
            return D3D12_STENCIL_OP_REPLACE;
        case DepthStencilState::STENCIL_OP_INCR_SAT:
            return D3D12_STENCIL_OP_INCR_SAT;
        case DepthStencilState::STENCIL_OP_DECR_SAT:
            return D3D12_STENCIL_OP_DECR_SAT;
        case DepthStencilState::STENCIL_OP_INVERT:
            return D3D12_STENCIL_OP_INVERT;
        case DepthStencilState::STENCIL_OP_INCR:
            return D3D12_STENCIL_OP_INCR;
        case DepthStencilState::STENCIL_OP_DECR:
            return D3D12_STENCIL_OP_DECR;
        default:
            return D3D12_STENCIL_OP_KEEP;
        }
    }

    D3D12_COMPARISON_FUNC convertComparisonFunc(DepthStencilState::ComparisonFunc value)
    {
        switch (value)
        {
        case DepthStencilState::COMPARISON_NEVER:
            return D3D12_COMPARISON_FUNC_NEVER;
        case DepthStencilState::COMPARISON_LESS:
            return D3D12_COMPARISON_FUNC_LESS;
        case DepthStencilState::COMPARISON_EQUAL:
            return D3D12_COMPARISON_FUNC_EQUAL;
        case DepthStencilState::COMPARISON_LESS_EQUAL:
            return D3D12_COMPARISON_FUNC_LESS_EQUAL;
        case DepthStencilState::COMPARISON_GREATER:
            return D3D12_COMPARISON_FUNC_GREATER;
        case DepthStencilState::COMPARISON_NOT_EQUAL:
            return D3D12_COMPARISON_FUNC_NOT_EQUAL;
        case DepthStencilState::COMPARISON_GREATER_EQUAL:
            return D3D12_COMPARISON_FUNC_GREATER_EQUAL;
        case DepthStencilState::COMPARISON_ALWAYS:
            return D3D12_COMPARISON_FUNC_ALWAYS;
        default:
            return D3D12_COMPARISON_FUNC_NEVER;
        }
    }


    D3D_PRIMITIVE_TOPOLOGY convertPrimitiveType(PrimitiveType::Enum pt)
    {
        switch (pt)
        {
        case PrimitiveType::POINT_LIST:
            return D3D_PRIMITIVE_TOPOLOGY_POINTLIST;
            break;
		case PrimitiveType::LINE_LIST:
			return D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
			break;
        case PrimitiveType::TRIANGLE_LIST:
            return D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
            break;
        case PrimitiveType::TRIANGLE_STRIP:
            return D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
            break;
        case PrimitiveType::PATCH_1_CONTROL_POINT:
            return D3D_PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST;
            break;
        case PrimitiveType::PATCH_3_CONTROL_POINT:
            return D3D_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST;
            break;
        default:
            return D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;
            break;
        }
    }

    PipelineStateHandle RendererInterfaceD3D12::getPipelineState(const DrawCallState & state, RootSignatureHandle pRS)
    {
        uint32_t hash = getStateHashForPSO(state);

        PipelineStateHandle pipelineState = m_pResources->psoCache[hash];

        if (pipelineState)
            return pipelineState;

        pipelineState = new PipelineState();
        pipelineState->rootSignature = pRS;

        D3D12_GRAPHICS_PIPELINE_STATE_DESC desc = {};
        desc.pRootSignature = pRS->handle;

        Shader* shader;
        shader = static_cast<Shader*>(state.VS.shader);
        if (shader) desc.VS = { &shader->bytecode[0], shader->bytecode.size() };

        shader = static_cast<Shader*>(state.HS.shader);
        if (shader) desc.HS = { &shader->bytecode[0], shader->bytecode.size() };

        shader = static_cast<Shader*>(state.DS.shader);
        if (shader) desc.DS = { &shader->bytecode[0], shader->bytecode.size() };

        shader = static_cast<Shader*>(state.GS.shader);
        if (shader) desc.GS = { &shader->bytecode[0], shader->bytecode.size() };

        shader = static_cast<Shader*>(state.PS.shader);
        if (shader) desc.PS = { &shader->bytecode[0], shader->bytecode.size() };
            

        const BlendState& blendState = state.renderState.blendState;

        desc.BlendState.AlphaToCoverageEnable = blendState.alphaToCoverage;
        desc.BlendState.IndependentBlendEnable = true;

        for (uint32_t i = 0; i < state.renderState.targetCount; i++)
        {
            desc.BlendState.RenderTarget[i].BlendEnable = blendState.blendEnable[i] ? TRUE : FALSE;
            desc.BlendState.RenderTarget[i].SrcBlend = convertBlendValue(blendState.srcBlend[i]);
            desc.BlendState.RenderTarget[i].DestBlend = convertBlendValue(blendState.destBlend[i]);
            desc.BlendState.RenderTarget[i].BlendOp = convertBlendOp(blendState.blendOp[i]);
            desc.BlendState.RenderTarget[i].SrcBlendAlpha = convertBlendValue(blendState.srcBlendAlpha[i]);
            desc.BlendState.RenderTarget[i].DestBlendAlpha = convertBlendValue(blendState.destBlendAlpha[i]);
            desc.BlendState.RenderTarget[i].BlendOpAlpha = convertBlendOp(blendState.blendOpAlpha[i]);
            desc.BlendState.RenderTarget[i].RenderTargetWriteMask = 
                (blendState.colorWriteEnable[i] & BlendState::COLOR_MASK_RED   ? D3D12_COLOR_WRITE_ENABLE_RED   : 0) |
                (blendState.colorWriteEnable[i] & BlendState::COLOR_MASK_GREEN ? D3D12_COLOR_WRITE_ENABLE_GREEN : 0) |
                (blendState.colorWriteEnable[i] & BlendState::COLOR_MASK_BLUE  ? D3D12_COLOR_WRITE_ENABLE_BLUE  : 0) |
                (blendState.colorWriteEnable[i] & BlendState::COLOR_MASK_ALPHA ? D3D12_COLOR_WRITE_ENABLE_ALPHA : 0);
        }

            
        const DepthStencilState& depthState = state.renderState.depthStencilState;

        desc.DepthStencilState.DepthEnable = depthState.depthEnable ? TRUE : FALSE;
        desc.DepthStencilState.DepthWriteMask = depthState.depthWriteMask == DepthStencilState::DEPTH_WRITE_MASK_ALL ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
        desc.DepthStencilState.DepthFunc = convertComparisonFunc(depthState.depthFunc);
        desc.DepthStencilState.StencilEnable = depthState.stencilEnable ? TRUE : FALSE;
        desc.DepthStencilState.StencilReadMask = (UINT8)depthState.stencilReadMask;
        desc.DepthStencilState.StencilWriteMask = (UINT8)depthState.stencilWriteMask;
        desc.DepthStencilState.FrontFace.StencilFailOp = convertStencilOp(depthState.frontFace.stencilFailOp);
        desc.DepthStencilState.FrontFace.StencilDepthFailOp = convertStencilOp(depthState.frontFace.stencilDepthFailOp);
        desc.DepthStencilState.FrontFace.StencilPassOp = convertStencilOp(depthState.frontFace.stencilPassOp);
        desc.DepthStencilState.FrontFace.StencilFunc = convertComparisonFunc(depthState.frontFace.stencilFunc);
        desc.DepthStencilState.BackFace.StencilFailOp = convertStencilOp(depthState.backFace.stencilFailOp);
        desc.DepthStencilState.BackFace.StencilDepthFailOp = convertStencilOp(depthState.backFace.stencilDepthFailOp);
        desc.DepthStencilState.BackFace.StencilPassOp = convertStencilOp(depthState.backFace.stencilPassOp);
        desc.DepthStencilState.BackFace.StencilFunc = convertComparisonFunc(depthState.backFace.stencilFunc);

        if ((depthState.depthEnable || depthState.stencilEnable) && state.renderState.depthTarget == nullptr)
        {
            desc.DepthStencilState.DepthEnable = FALSE;
            desc.DepthStencilState.StencilEnable = FALSE;
            DEBUG_PRINT(RENDER_TARGETS, "WARNING: depthEnable or stencilEnable is true, but no depth target is bound\n");
        }

        const RasterState& rasterState = state.renderState.rasterState;

        switch (rasterState.fillMode)
        {
        case RasterState::FILL_SOLID:
            desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
            break;
        case RasterState::FILL_LINE:
            desc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
            break;
        default:
            SIGNAL_ERROR("Unknown fillMode");
        }

        switch (rasterState.cullMode)
        {
        case RasterState::CULL_BACK:
            desc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
            break;
        case RasterState::CULL_FRONT:
            desc.RasterizerState.CullMode = D3D12_CULL_MODE_FRONT;
            break;
        case RasterState::CULL_NONE:
            desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
            break;
        default:
            SIGNAL_ERROR("Unknown cullMode");
        }

        desc.RasterizerState.FrontCounterClockwise = rasterState.frontCounterClockwise ? TRUE : FALSE;
        desc.RasterizerState.DepthBias = rasterState.depthBias;
        desc.RasterizerState.DepthBiasClamp = rasterState.depthBiasClamp;
        desc.RasterizerState.SlopeScaledDepthBias = rasterState.slopeScaledDepthBias;
        desc.RasterizerState.DepthClipEnable = rasterState.depthClipEnable ? TRUE : FALSE;
        desc.RasterizerState.MultisampleEnable = rasterState.multisampleEnable ? TRUE : FALSE;
        desc.RasterizerState.AntialiasedLineEnable = rasterState.antialiasedLineEnable ? TRUE : FALSE;
        desc.RasterizerState.ConservativeRaster = rasterState.conservativeRasterEnable ? D3D12_CONSERVATIVE_RASTERIZATION_MODE_ON : D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
        desc.RasterizerState.ForcedSampleCount = rasterState.forcedSampleCount;

        switch (state.primType)
        {
        case PrimitiveType::POINT_LIST:
            desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
            break;
		case PrimitiveType::LINE_LIST:
			desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_LINE;
			break;
        case PrimitiveType::TRIANGLE_LIST:
        case PrimitiveType::TRIANGLE_STRIP:
            desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
            break;
        case PrimitiveType::PATCH_1_CONTROL_POINT:
        case PrimitiveType::PATCH_3_CONTROL_POINT:
            desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH;
            break;
        }

        if (state.renderState.depthTarget)
            desc.DSVFormat = GetFormatMapping(static_cast<Texture*>(state.renderState.depthTarget)->desc.format).rtvFormat;

        desc.SampleDesc = getStateSampleDesc(state);
            
        for (uint32_t i = 0; i < state.renderState.targetCount; i++)
        {
            desc.RTVFormats[i] = GetFormatMapping(static_cast<Texture*>(state.renderState.targets[i])->desc.format).rtvFormat;
        }

        InputLayout* inputLayout = static_cast<InputLayout*>(state.inputLayout);
        if (inputLayout && !inputLayout->inputElements.empty())
        {
            desc.InputLayout.NumElements = uint32_t(inputLayout->inputElements.size());
            desc.InputLayout.pInputElementDescs = &(inputLayout->inputElements[0]);
        }

        desc.NumRenderTargets = state.renderState.targetCount;
        desc.SampleMask = ~0u;
            
#if NVRHI_D3D12_WITH_NVAPI
        std::vector<const NVAPI_D3D12_PSO_EXTENSION_DESC*> extensions;

        shader = static_cast<Shader*>(state.VS.shader); if (shader) extensions.insert(extensions.end(), shader->extensions.begin(), shader->extensions.end());
        shader = static_cast<Shader*>(state.HS.shader); if (shader) extensions.insert(extensions.end(), shader->extensions.begin(), shader->extensions.end());
        shader = static_cast<Shader*>(state.DS.shader); if (shader) extensions.insert(extensions.end(), shader->extensions.begin(), shader->extensions.end());
        shader = static_cast<Shader*>(state.GS.shader); if (shader) extensions.insert(extensions.end(), shader->extensions.begin(), shader->extensions.end());
        shader = static_cast<Shader*>(state.PS.shader); if (shader) extensions.insert(extensions.end(), shader->extensions.begin(), shader->extensions.end());

        if (rasterState.programmableSamplePositionsEnable || rasterState.quadFillEnable)
        {
            NVAPI_D3D12_PSO_RASTERIZER_STATE_DESC rasterizerDesc = {};
            rasterizerDesc.baseVersion = NV_PSO_EXTENSION_DESC_VER;
            rasterizerDesc.psoExtension = NV_PSO_RASTER_EXTENSION;
            rasterizerDesc.version = NV_RASTERIZER_PSO_EXTENSION_DESC_VER;

            rasterizerDesc.ProgrammableSamplePositionsEnable = rasterState.programmableSamplePositionsEnable;
            rasterizerDesc.SampleCount = rasterState.forcedSampleCount;
            memcpy(rasterizerDesc.SamplePositionsX, rasterState.samplePositionsX, sizeof(rasterState.samplePositionsX));
            memcpy(rasterizerDesc.SamplePositionsY, rasterState.samplePositionsY, sizeof(rasterState.samplePositionsY));
            rasterizerDesc.QuadFillMode = rasterState.quadFillEnable ? NVAPI_QUAD_FILLMODE_BBOX : NVAPI_QUAD_FILLMODE_DISABLED;

            extensions.push_back(&rasterizerDesc);
        }

        if (extensions.size() > 0)
        {
            NvAPI_Status status = NvAPI_D3D12_CreateGraphicsPipelineState(m_pDevice, &desc, NvU32(extensions.size()), &extensions[0], &pipelineState->handle);

            if (status != NVAPI_OK || pipelineState->handle == nullptr)
            {
                SIGNAL_ERROR("Failed to create a graphics pipeline state object with NVAPI extensions");
                return nullptr;
            }

            m_pResources->psoCache[hash] = pipelineState;
            return pipelineState;
        }
#endif

        HRESULT hr;
        hr = m_pDevice->CreateGraphicsPipelineState(&desc, IID_PPV_ARGS(&pipelineState->handle));

        if (FAILED(hr))
        {
            SIGNAL_ERROR("Failed to create a graphics pipeline state object");
            return nullptr;
        }

        m_pResources->psoCache[hash] = pipelineState;
        return pipelineState;
    }

    PipelineStateHandle RendererInterfaceD3D12::getPipelineState(const DispatchState & state, RootSignatureHandle pRS, uint32_t hash)
    {
        PipelineStateHandle pipelineState = m_pResources->psoCache[hash];

        if (pipelineState)
            return pipelineState;

        pipelineState = new PipelineState();
        pipelineState->rootSignature = pRS;

        D3D12_COMPUTE_PIPELINE_STATE_DESC desc = {};

        desc.pRootSignature = pRS->handle;
        Shader* shader = static_cast<Shader*>(state.shader);
        desc.CS = { &shader->bytecode[0], shader->bytecode.size() };

#if NVRHI_D3D12_WITH_NVAPI
        if (shader->extensions.size() > 0)
        {
            NvAPI_Status status = NVAPI_NOT_SUPPORTED;// NvAPI_D3D12_CreateComputePipelineState(m_pDevice, &desc, NvU32(state.shader->extensions.size()), &state.shader->extensions[0], &pipelineState->handle);

            if (status != NVAPI_OK || pipelineState->handle == nullptr)
            {
                SIGNAL_ERROR("Failed to create a compute pipeline state object with NVAPI extensions");
                return nullptr;
            }

            m_pResources->psoCache[hash] = pipelineState;
            return pipelineState;
        }
#endif

        HRESULT hr;
        hr = m_pDevice->CreateComputePipelineState(&desc, IID_PPV_ARGS(&pipelineState->handle));

        if (FAILED(hr))
        {
            SIGNAL_ERROR("Failed to create a compute pipeline state object");
            return nullptr;
        }

        m_pResources->psoCache[hash] = pipelineState;
        return pipelineState;
    }

    DescriptorIndex RendererInterfaceD3D12::getCBV(ConstantBufferHandle _cbuffer)
    {
        ConstantBuffer* cbuffer= static_cast<ConstantBuffer*>(_cbuffer);

        if (!cbuffer->uploadedDataValid)
        {
            UINT64 offset = m_pResources->upload.SuballocateBuffer(cbuffer->alignedSize);
            memcpy(m_pResources->upload.GetCpuVA(offset), &cbuffer->data[0], cbuffer->data.size());
            cbuffer->currentVersionOffset = offset;
            cbuffer->uploadedDataValid = true;

            if (cbuffer->constantBufferView == INVALID_DESCRIPTOR_INDEX)
                cbuffer->constantBufferView = m_pResources->dhSRVstatic.AllocateDescriptor();

            D3D12_CONSTANT_BUFFER_VIEW_DESC desc = {};
            desc.BufferLocation = m_pResources->upload.GetGpuVA(cbuffer->currentVersionOffset);
            desc.SizeInBytes = cbuffer->alignedSize;
            m_pDevice->CreateConstantBufferView(&desc, m_pResources->dhSRVstatic.GetCpuHandle(cbuffer->constantBufferView));

            cbuffer->numRefreshes++;
        }
        else
        {
            cbuffer->numCachedRefs++;
        }

        return cbuffer->constantBufferView;
    }

    DescriptorIndex RendererInterfaceD3D12::getTextureSRV(const TextureBinding& binding)
    {
        Texture* texture = static_cast<Texture*>(binding.texture);

        auto key = std::make_pair(binding.format, binding.mipLevel);
        auto found = texture->shaderResourceViews.find(key);
        if (found != texture->shaderResourceViews.end())
            return found->second;

        D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};

        desc.Format = GetFormatMapping(binding.format == Format::UNKNOWN ? texture->desc.format : binding.format).srvFormat;
        desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

        UINT planeSlice = 0;
        if (desc.Format == DXGI_FORMAT_X24_TYPELESS_G8_UINT)
            planeSlice = 1;

        uint32_t firstMip = binding.mipLevel >= texture->desc.mipLevels ? 0 : binding.mipLevel;
        uint32_t mipLevels = binding.mipLevel >= texture->desc.mipLevels ? texture->desc.mipLevels : 1;

        if (texture->desc.isArray || texture->desc.isCubeMap)
        {
            if (texture->desc.sampleCount > 1)
            {
                desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMSARRAY;
                desc.Texture2DMSArray.ArraySize = texture->desc.depthOrArraySize;
            }
            else if (texture->desc.isCubeMap)
            {
                if (texture->desc.depthOrArraySize > 6)
                {
                    desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBEARRAY;
                    desc.TextureCubeArray.NumCubes = texture->desc.depthOrArraySize / 6;
                    desc.TextureCubeArray.MostDetailedMip = firstMip;
                    desc.TextureCubeArray.MipLevels = mipLevels;
                }
                else
                {
                    desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
                    desc.TextureCube.MostDetailedMip = firstMip;
                    desc.TextureCube.MipLevels = mipLevels;
                }
            }
            else
            {
                desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DARRAY;
                desc.Texture2DArray.ArraySize = texture->desc.depthOrArraySize;
                desc.Texture2DArray.MostDetailedMip = firstMip;
                desc.Texture2DArray.MipLevels = mipLevels;
                desc.Texture2DArray.PlaneSlice = planeSlice;
            }
        }
        else if (texture->desc.depthOrArraySize > 1)
        {
            desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE3D;
            desc.Texture3D.MostDetailedMip = firstMip;
            desc.Texture3D.MipLevels = mipLevels;
        }
        else
        {
            if (texture->desc.sampleCount > 1)
            {
                desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2DMS;
            }
            else
            {
                desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
                desc.Texture2D.MostDetailedMip = firstMip;
                desc.Texture2D.MipLevels = mipLevels;
                desc.Texture2D.PlaneSlice = planeSlice;
            }
        }

        DescriptorIndex index = m_pResources->dhSRVstatic.AllocateDescriptor();
        m_pDevice->CreateShaderResourceView(texture->resource, &desc, m_pResources->dhSRVstatic.GetCpuHandle(index));

        texture->shaderResourceViews[key] = index;
        return index;
    }

    DescriptorIndex RendererInterfaceD3D12::getTextureUAV(const TextureBinding& binding)
    {
        Texture* texture = static_cast<Texture*>(binding.texture);

        auto key = std::make_pair(binding.format, binding.mipLevel);
        auto found = texture->unorderedAccessViews.find(key);
        if (found != texture->unorderedAccessViews.end())
            return found->second;

        D3D12_UNORDERED_ACCESS_VIEW_DESC desc = {};

        desc.Format = GetFormatMapping(binding.format == Format::UNKNOWN ? texture->desc.format : binding.format).srvFormat;

        if (texture->desc.isArray || texture->desc.isCubeMap)
        {
            desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2DARRAY;
            desc.Texture2DArray.ArraySize = texture->desc.depthOrArraySize;
            desc.Texture2DArray.MipSlice = binding.mipLevel;
        }
        else if (texture->desc.depthOrArraySize > 1)
        {
            desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE3D;
            desc.Texture3D.WSize = texture->desc.depthOrArraySize;
            desc.Texture3D.MipSlice = binding.mipLevel;
        }
        else
        {
            desc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
            desc.Texture2D.MipSlice = binding.mipLevel;
        }

        DescriptorIndex index = m_pResources->dhSRVstatic.AllocateDescriptor();
        m_pDevice->CreateUnorderedAccessView(texture->resource, nullptr, &desc, m_pResources->dhSRVstatic.GetCpuHandle(index));

        texture->unorderedAccessViews[key] = index;
        return index;
    }

    DescriptorIndex RendererInterfaceD3D12::getBufferSRV(const BufferBinding& binding)
    {
        Buffer* buffer = static_cast<Buffer*>(binding.buffer);

        if (buffer->shaderResourceView != INVALID_DESCRIPTOR_INDEX)
            return buffer->shaderResourceView;

        D3D12_SHADER_RESOURCE_VIEW_DESC desc = {};

        desc.ViewDimension = D3D12_SRV_DIMENSION_BUFFER;
        desc.Buffer.FirstElement = 0;
        desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

        if (buffer->desc.structStride != 0)
        {
            desc.Format = DXGI_FORMAT_UNKNOWN;
            desc.Buffer.NumElements = buffer->desc.byteSize / buffer->desc.structStride;
            desc.Buffer.StructureByteStride = buffer->desc.structStride;
        }
        else
        {
            const FormatMapping& mapping = GetFormatMapping(binding.format == Format::UNKNOWN ? Format::R32_UINT : binding.format);
                
            desc.Format = mapping.srvFormat;
            desc.Buffer.NumElements = buffer->desc.byteSize / mapping.bytesPerPixel;
        }

        DescriptorIndex index = m_pResources->dhSRVstatic.AllocateDescriptor();
        m_pDevice->CreateShaderResourceView(buffer->resource, &desc, m_pResources->dhSRVstatic.GetCpuHandle(index));

        buffer->shaderResourceView = index;
        return index;
    }

    DescriptorIndex RendererInterfaceD3D12::getBufferUAV(const BufferBinding& binding)
    {
        Buffer* buffer = static_cast<Buffer*>(binding.buffer);

        if (buffer->unorderedAccessView != INVALID_DESCRIPTOR_INDEX)
            return buffer->unorderedAccessView;

        D3D12_UNORDERED_ACCESS_VIEW_DESC desc = {};
            
        desc.ViewDimension = D3D12_UAV_DIMENSION_BUFFER;

        if (buffer->desc.structStride != 0)
        {
            desc.Format = DXGI_FORMAT_UNKNOWN;
            desc.Buffer.NumElements = buffer->desc.byteSize / buffer->desc.structStride;
            desc.Buffer.StructureByteStride = buffer->desc.structStride;
        }
        else
        {
            const FormatMapping& mapping = GetFormatMapping(binding.format == Format::UNKNOWN ? Format::R32_UINT : binding.format);

            desc.Format = mapping.srvFormat;
            desc.Buffer.NumElements = buffer->desc.byteSize / mapping.bytesPerPixel;
        }

        DescriptorIndex index = m_pResources->dhSRVstatic.AllocateDescriptor();
        m_pDevice->CreateUnorderedAccessView(buffer->resource, nullptr, &desc, m_pResources->dhSRVstatic.GetCpuHandle(index));

        buffer->unorderedAccessView = index;
        return index;
    }

    DescriptorIndex RendererInterfaceD3D12::getSamplerView(SamplerHandle _sampler)
    {
        Sampler* sampler = static_cast<Sampler*>(_sampler);

        if (sampler->view != INVALID_DESCRIPTOR_INDEX)
            return sampler->view;

        D3D12_SAMPLER_DESC desc;
        const SamplerDesc& samplerDesc = sampler->desc;

        if (samplerDesc.anisotropy > 1.0f)
        {
            desc.Filter = D3D12_ENCODE_ANISOTROPIC_FILTER(
                samplerDesc.shadowCompare ? D3D12_FILTER_REDUCTION_TYPE_COMPARISON : D3D12_FILTER_REDUCTION_TYPE_STANDARD);
        }
        else
        {
            desc.Filter = D3D12_ENCODE_BASIC_FILTER(
                samplerDesc.minFilter ? D3D12_FILTER_TYPE_LINEAR : D3D12_FILTER_TYPE_POINT, 
                samplerDesc.magFilter ? D3D12_FILTER_TYPE_LINEAR : D3D12_FILTER_TYPE_POINT, 
                samplerDesc.mipFilter ? D3D12_FILTER_TYPE_LINEAR : D3D12_FILTER_TYPE_POINT, 
                samplerDesc.shadowCompare ? D3D12_FILTER_REDUCTION_TYPE_COMPARISON : D3D12_FILTER_REDUCTION_TYPE_STANDARD);
        }

        D3D12_TEXTURE_ADDRESS_MODE* dest[] = { &desc.AddressU, &desc.AddressV, &desc.AddressW };
        for (int i = 0; i < 3; i++)
        {
            switch (samplerDesc.wrapMode[i])
            {
            case SamplerDesc::WRAP_MODE_BORDER:
                *dest[i] = D3D12_TEXTURE_ADDRESS_MODE_BORDER;
                break;
            case SamplerDesc::WRAP_MODE_CLAMP:
                *dest[i] = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
                break;
            case SamplerDesc::WRAP_MODE_WRAP:
                *dest[i] = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
                break;
            }
        }

        desc.MipLODBias = samplerDesc.mipBias;
        desc.MaxAnisotropy = std::max((UINT)samplerDesc.anisotropy, 1U);
        desc.ComparisonFunc = D3D12_COMPARISON_FUNC_LESS;
        desc.BorderColor[0] = samplerDesc.borderColor.r;
        desc.BorderColor[1] = samplerDesc.borderColor.g;
        desc.BorderColor[2] = samplerDesc.borderColor.b;
        desc.BorderColor[3] = samplerDesc.borderColor.a;
        desc.MinLOD = 0;
        desc.MaxLOD = D3D12_FLOAT32_MAX;

        DescriptorIndex index = m_pResources->dhSamplerStatic.AllocateDescriptor();
        m_pDevice->CreateSampler(&desc, m_pResources->dhSamplerStatic.GetCpuHandle(index));

        sampler->view = index;
        return index;
    }

    DescriptorIndex RendererInterfaceD3D12::getRTV(TextureHandle _texture, ArrayIndex arrayIndex, MipLevel mipLevel)
    {
        Texture* texture = static_cast<Texture*>(_texture);

        auto key = std::make_pair(arrayIndex, mipLevel);
        auto found = texture->renderTargetViews.find(key);
        if (found != texture->renderTargetViews.end())
            return found->second;

        D3D12_RENDER_TARGET_VIEW_DESC desc = {};

        desc.Format = GetFormatMapping(texture->desc.format).rtvFormat;

        if (texture->desc.isArray || texture->desc.isCubeMap)
        {
            if (texture->desc.sampleCount > 1)
            {
                desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMSARRAY;
                if (arrayIndex >= texture->desc.depthOrArraySize)
                {
                    desc.Texture2DMSArray.FirstArraySlice = 0;
                    desc.Texture2DMSArray.ArraySize = texture->desc.depthOrArraySize;
                }
                else
                {
                    desc.Texture2DMSArray.FirstArraySlice = arrayIndex;
                    desc.Texture2DMSArray.ArraySize = 1;
                }
            }
            else
            {
                desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
                if (arrayIndex >= texture->desc.depthOrArraySize)
                {
                    desc.Texture2DArray.FirstArraySlice = 0;
                    desc.Texture2DArray.ArraySize = texture->desc.depthOrArraySize;
                }
                else
                {
                    desc.Texture2DArray.FirstArraySlice = arrayIndex;
                    desc.Texture2DArray.ArraySize = 1;
                }
                desc.Texture2DArray.MipSlice = mipLevel;
            }
        }
        else
        {
            if (texture->desc.sampleCount > 1)
            {
                desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2DMS;
            }
            else
            {
                desc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
                desc.Texture2D.MipSlice = mipLevel;
            }
        }

        DescriptorIndex index = m_pResources->dhRTV.AllocateDescriptor();
        m_pDevice->CreateRenderTargetView(texture->resource, &desc, m_pResources->dhRTV.GetCpuHandle(index));

        texture->renderTargetViews[key] = index;
        return index;
    }

    DescriptorIndex RendererInterfaceD3D12::getDSV(TextureHandle _texture, ArrayIndex arrayIndex, MipLevel mipLevel)
    {
        Texture* texture = static_cast<Texture*>(_texture);

        auto key = std::make_pair(arrayIndex, mipLevel);
        auto found = texture->depthStencilViews.find(key);
        if (found != texture->depthStencilViews.end())
            return found->second;

        D3D12_DEPTH_STENCIL_VIEW_DESC desc = {};

        desc.Format = GetFormatMapping(texture->desc.format).rtvFormat;

        if (texture->desc.isArray)
        {
            if (texture->desc.sampleCount > 1)
            {
                desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMSARRAY;
                if (arrayIndex >= texture->desc.depthOrArraySize)
                {
                    desc.Texture2DMSArray.FirstArraySlice = 0;
                    desc.Texture2DMSArray.ArraySize = texture->desc.depthOrArraySize;
                }
                else
                {
                    desc.Texture2DMSArray.FirstArraySlice = arrayIndex;
                    desc.Texture2DMSArray.ArraySize = 1;
                }
            }
            else
            {
                desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DARRAY;
                if (arrayIndex >= texture->desc.depthOrArraySize)
                {
                    desc.Texture2DArray.FirstArraySlice = 0;
                    desc.Texture2DArray.ArraySize = texture->desc.depthOrArraySize;
                }
                else
                {
                    desc.Texture2DArray.FirstArraySlice = arrayIndex;
                    desc.Texture2DArray.ArraySize = 1;
                }
                desc.Texture2DArray.MipSlice = mipLevel;
            }
        }
        else
        {
            if (texture->desc.sampleCount > 1)
            {
                desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2DMS;
            }
            else
            {
                desc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
                desc.Texture2D.MipSlice = mipLevel;
            }
        }

        DescriptorIndex index = m_pResources->dhDSV.AllocateDescriptor();
        m_pDevice->CreateDepthStencilView(texture->resource, &desc, m_pResources->dhDSV.GetCpuHandle(index));

        texture->depthStencilViews[key] = index;
        return index;
    }

    void RendererInterfaceD3D12::releaseTextureViews(TextureHandle _texture)
    {
        Texture* texture = static_cast<Texture*>(_texture);

        for (auto pair : texture->renderTargetViews)
            m_pResources->dhRTV.ReleaseDescriptor(pair.second);

        for (auto pair : texture->depthStencilViews)
            m_pResources->dhDSV.ReleaseDescriptor(pair.second);

        for (auto pair : texture->shaderResourceViews)
            m_pResources->dhSRVstatic.ReleaseDescriptor(pair.second);

        for (auto pair : texture->unorderedAccessViews)
            m_pResources->dhSRVstatic.ReleaseDescriptor(pair.second);
    }

    void RendererInterfaceD3D12::releaseBufferViews(BufferHandle _buffer)
    {
        Buffer* buffer = static_cast<Buffer*>(_buffer);

        if (buffer->shaderResourceView != INVALID_DESCRIPTOR_INDEX)
            m_pResources->dhSRVstatic.ReleaseDescriptor(buffer->shaderResourceView);

        if (buffer->unorderedAccessView != INVALID_DESCRIPTOR_INDEX)
            m_pResources->dhSRVstatic.ReleaseDescriptor(buffer->unorderedAccessView);
    }

    void RendererInterfaceD3D12::releaseConstantBufferViews(ConstantBufferHandle _cbuffer)
    {
        ConstantBuffer* cbuffer = static_cast<ConstantBuffer*>(_cbuffer);

        if (cbuffer->constantBufferView != INVALID_DESCRIPTOR_INDEX)
        {
            m_pResources->dhSRVstatic.ReleaseDescriptor(cbuffer->constantBufferView);
            cbuffer->constantBufferView = INVALID_DESCRIPTOR_INDEX;
        }
    }

    void RendererInterfaceD3D12::releaseSamplerViews(SamplerHandle _sampler)
    {
        Sampler* sampler = static_cast<Sampler*>(_sampler);

        if (sampler->view != INVALID_DESCRIPTOR_INDEX)
            m_pResources->dhSamplerStatic.ReleaseDescriptor(sampler->view);
    }

    uint64_t RendererInterfaceD3D12::getFenceCounter()
    {
        return m_pResources->fenceCounter;
    }

    void RendererInterfaceD3D12::deferredDestroyResource(ManagedResource * resource)
    {
        m_pResources->deletedResources.insert(resource);
    }

    void RendererInterfaceD3D12::requireTextureState(TextureHandle _texture, uint32_t arrayIndex, uint32_t mipLevel, uint32_t state)
    {
        Texture* texture = static_cast<Texture*>(_texture);

        bool isArray = texture->desc.isArray || texture->desc.isCubeMap;
        bool wholeArray = arrayIndex >= texture->desc.depthOrArraySize;
        bool wholeMipChain = mipLevel >= texture->desc.mipLevels;

        uint32_t minArrayIndex = wholeArray || !isArray ? 0 : arrayIndex;
        uint32_t maxArrayIndex = wholeArray && isArray ? texture->desc.depthOrArraySize - 1 : isArray ? arrayIndex : 0;
        uint32_t minMipLevel = wholeMipChain ? 0 : mipLevel;
        uint32_t maxMipLevel = wholeMipChain ? texture->desc.mipLevels - 1 : mipLevel;

        bool anyUavBarrier = false;

        for (arrayIndex = minArrayIndex; arrayIndex <= maxArrayIndex; arrayIndex++)
        {
            for (mipLevel = minMipLevel; mipLevel <= maxMipLevel; mipLevel++)
            {
                for (uint32_t planeSlice = 0; planeSlice < texture->numPlanes; planeSlice++)
                {
                    uint32_t subresource = mipLevel + (arrayIndex + planeSlice * texture->desc.depthOrArraySize) * texture->desc.mipLevels;
                    requireTextureState(texture, subresource, state, anyUavBarrier);
                }
            }
        }
    }

	void RendererInterfaceD3D12::requireTextureState(TextureHandle _texture, uint32_t subresource, uint32_t state, bool& anyUavBarrier)
	{
        Texture* texture = static_cast<Texture*>(_texture);

        texture->fenceCounterAtLastUse = m_pResources->fenceCounter;

		D3D12_RESOURCE_STATES d3dstate = D3D12_RESOURCE_STATES(state);

		if (texture->subresourceStates[subresource] != d3dstate)
		{
			D3D12_RESOURCE_BARRIER barrier = {};
			barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			barrier.Transition.pResource = texture->resource;
			barrier.Transition.StateBefore = texture->subresourceStates[subresource];
			barrier.Transition.StateAfter = d3dstate;
			barrier.Transition.Subresource = subresource;
			m_pResources->barrier.push_back(barrier);
		}
		else if (d3dstate == D3D12_RESOURCE_STATE_UNORDERED_ACCESS && !anyUavBarrier && (texture->enableUavBarriers || !texture->firstUavBarrierPlaced))
		{
			D3D12_RESOURCE_BARRIER barrier = {};
			barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
			barrier.UAV.pResource = texture->resource;
			m_pResources->barrier.push_back(barrier);
			anyUavBarrier = true;
			texture->firstUavBarrierPlaced = true;
		}

		texture->subresourceStates[subresource] = d3dstate;
	}

    void RendererInterfaceD3D12::requireBufferState(BufferHandle _buffer, uint32_t state)
    {
        Buffer* buffer = static_cast<Buffer*>(_buffer);

        buffer->fenceCounterAtLastUse = m_pResources->fenceCounter;

        D3D12_RESOURCE_STATES d3dstate = D3D12_RESOURCE_STATES(state);

        if (buffer->state != d3dstate)
        {
            D3D12_RESOURCE_BARRIER barrier = {};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier.Transition.pResource = buffer->resource;
            barrier.Transition.StateBefore = buffer->state;
            barrier.Transition.StateAfter = d3dstate;
            barrier.Transition.Subresource = 0;
            m_pResources->barrier.push_back(barrier);
        }
        else if (d3dstate == D3D12_RESOURCE_STATE_UNORDERED_ACCESS && (buffer->enableUavBarriers || !buffer->firstUavBarrierPlaced))
        {
            D3D12_RESOURCE_BARRIER barrier = {};
            barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
            barrier.UAV.pResource = buffer->resource;
            m_pResources->barrier.push_back(barrier);
			buffer->firstUavBarrierPlaced = true;
        }

        buffer->state = d3dstate;
    }

    void RendererInterfaceD3D12::commitBarriers()
    {
        if (m_pResources->barrier.empty())
            return;

#if 0
        // Debug code to detect duplicate barriers on the same resource
        for (size_t i = 0; i < m_pResources->barrier.size(); i++)
            for (size_t j = i + 1; j < m_pResources->barrier.size(); j++)
            {
                const D3D12_RESOURCE_BARRIER& a = m_pResources->barrier[i];
                const D3D12_RESOURCE_BARRIER& b = m_pResources->barrier[j];
                if (a.Type == b.Type && a.Type == D3D12_RESOURCE_BARRIER_TYPE_TRANSITION && a.Transition.pResource == b.Transition.pResource && a.Transition.Subresource == b.Transition.Subresource)
                    __debugbreak();
    }
#endif

#if 1
        m_ActiveCommandList->commandList->ResourceBarrier(uint32_t(m_pResources->barrier.size()), &m_pResources->barrier[0]);
#else
        for (size_t i = 0; i < m_pResources->barrier.size(); i++)
        {
            m_ActiveCommandList->commandList->ResourceBarrier(1, &m_pResources->barrier[i]);
            i = i;
        }
#endif
        m_ActiveCommandList->size++;
        m_pResources->barrier.clear();
    }

    void RendererInterfaceD3D12::bindShaderResources(uint32_t & rootIndex, void* rootDescriptorTableHandles, const PipelineStageBindings& stage)
    {
        D3D12_CPU_DESCRIPTOR_HANDLE nullDescriptor;
        D3D12_CPU_DESCRIPTOR_HANDLE copySources[256];

        Shader* shader = static_cast<Shader*>(stage.shader);

        if (shader->numBindings > 0)
        {
            std::bitset<16> slotsCB = shader->slotsCB;
            std::bitset<128> slotsSRV = shader->slotsSRV;
            std::bitset<16> slotsUAV = shader->slotsUAV;

            uint32_t currentTableOffset = 0;

            nullDescriptor = m_pResources->dhSRVstatic.GetCpuHandle(m_pResources->nullCBV);
            for (uint32_t i = 0; i < shader->numCB; i++)
            {
                copySources[currentTableOffset + i] = nullDescriptor;
            }

            for (uint32_t i = 0; i < stage.constantBufferBindingCount; i++)
            {
                const ConstantBufferBinding& binding = stage.constantBuffers[i];
                if (binding.buffer)
                {
                    if (slotsCB[binding.slot])
                    {
                        DescriptorIndex index = getCBV(binding.buffer);
                        copySources[currentTableOffset + binding.slot - shader->minCB] = m_pResources->dhSRVstatic.GetCpuHandle(index);

                        slotsCB.reset(binding.slot);
                    }
                    else
                        DEBUG_PRINT(UNUSED_CB, "WARNING: attempted CB binding to a slot unused by shader\n");
                }
            }
            currentTableOffset += shader->numCB;

            nullDescriptor = m_pResources->dhSRVstatic.GetCpuHandle(m_pResources->nullSRV);
            for (uint32_t i = 0; i < shader->numSRV; i++)
            {
                copySources[currentTableOffset + i] = nullDescriptor;
            }

            for (uint32_t i = 0; i < stage.textureBindingCount; i++)
            {
                const TextureBinding& binding = stage.textures[i];
                if (binding.texture && !binding.isWritable)
                {
                    if (slotsSRV[binding.slot])
                    {
                        DescriptorIndex index = getTextureSRV(binding);
                        copySources[currentTableOffset + binding.slot - shader->minSRV] = m_pResources->dhSRVstatic.GetCpuHandle(index);

                        D3D12_RESOURCE_STATES newState = shader->type == ShaderType::SHADER_PIXEL
                            ? D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE
                            : D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

                        requireTextureState(binding.texture, ~0u, binding.mipLevel, newState);

                        slotsSRV.reset(binding.slot);
                    }
                    else
                        DEBUG_PRINT(UNUSED_SRV, "WARNING: attempted texture SRV binding to a slot unused by shader\n");
                }
            }
            for (uint32_t i = 0; i < stage.bufferBindingCount; i++)
            {
                const BufferBinding& binding = stage.buffers[i];
                if (binding.buffer && !binding.isWritable)
                {
                    if (slotsSRV[binding.slot])
                    {
                        DescriptorIndex index = getBufferSRV(binding);
                        copySources[currentTableOffset + binding.slot - shader->minSRV] = m_pResources->dhSRVstatic.GetCpuHandle(index);

                        requireBufferState(binding.buffer, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
                        slotsSRV.reset(binding.slot);
                    }
                    else
                        DEBUG_PRINT(UNUSED_SRV, "WARNING: attempted buffer SRV binding to a slot unused by shader\n");
                }
            }
            currentTableOffset += shader->numSRV;

            nullDescriptor = m_pResources->dhSRVstatic.GetCpuHandle(m_pResources->nullUAV);
            for (uint32_t i = 0; i < shader->numUAV; i++)
            {
                copySources[currentTableOffset + i] = nullDescriptor;
            }

            for (uint32_t i = 0; i < stage.textureBindingCount; i++)
            {
                const TextureBinding& binding = stage.textures[i];
                if (binding.texture && binding.isWritable)
                {
                    if (slotsUAV[binding.slot])
                    {
                        DescriptorIndex index = getTextureUAV(binding);
                        copySources[currentTableOffset + binding.slot - shader->minUAV] = m_pResources->dhSRVstatic.GetCpuHandle(index);

                        requireTextureState(binding.texture, ~0u, binding.mipLevel, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
                        slotsUAV.reset(binding.slot);
                    }
                    else
                        DEBUG_PRINT(UNUSED_UAV, "WARNING: attempted texture UAV binding to a slot unused by shader\n");
                }
            }
            for (uint32_t i = 0; i < stage.bufferBindingCount; i++)
            {
                const BufferBinding& binding = stage.buffers[i];
                if (binding.buffer && binding.isWritable)
                {
                    if (slotsUAV[binding.slot])
                    {
                        DescriptorIndex index = getBufferUAV(binding);
                        copySources[currentTableOffset + binding.slot - shader->minUAV] = m_pResources->dhSRVstatic.GetCpuHandle(index);

                        requireBufferState(binding.buffer, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
                        slotsUAV.reset(binding.slot);
                    }
                    else
                        DEBUG_PRINT(UNUSED_UAV, "WARNING: attempted buffer UAV binding to a slot unused by shader\n");
                }
            }

            if (shader->hlslExtensionsUAV != ~0u)
                slotsUAV.reset(shader->hlslExtensionsUAV);

            if (slotsCB.any())
                DEBUG_PRINT(UNBOUND_CB, "WARNING: some CB slots are not bound\n");

            if (slotsSRV.any())
                DEBUG_PRINT(UNBOUND_SRV, "WARNING: some SRV slots are not bound\n");

            if (slotsUAV.any())
                DEBUG_PRINT(UNBOUND_UAV, "WARNING: some UAV slots are not bound\n");

            DescriptorIndex baseDescriptorIndex;
            m_pResources->dhSRVetc.AllocateDescriptors(shader->numBindings, baseDescriptorIndex);
            D3D12_CPU_DESCRIPTOR_HANDLE baseDescriptor = m_pResources->dhSRVetc.GetCpuHandle(baseDescriptorIndex);

            m_pDevice->CopyDescriptors(1, &baseDescriptor, &shader->numBindings, shader->numBindings, copySources, nullptr, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

            ((D3D12_GPU_DESCRIPTOR_HANDLE*)rootDescriptorTableHandles)[rootIndex] = m_pResources->dhSRVetc.GetGpuHandle(baseDescriptorIndex);
            rootIndex++;
        }

        if (shader->numSamplers > 0)
        {
            std::bitset<128> slotsSampler = shader->slotsSampler;

            nullDescriptor = m_pResources->dhSamplerStatic.GetCpuHandle(m_pResources->nullSampler);
            for (uint32_t i = 0; i < shader->numSRV; i++)
            {
                copySources[i] = nullDescriptor;
            }

            for (uint32_t i = 0; i < stage.textureSamplerBindingCount; i++)
            {
                const SamplerBinding& binding = stage.textureSamplers[i];
                if (binding.sampler)
                {
                    if (slotsSampler[binding.slot])
                    {
                        DescriptorIndex index = getSamplerView(binding.sampler);
                        copySources[binding.slot - shader->minSampler] = m_pResources->dhSamplerStatic.GetCpuHandle(index);

                        slotsSampler.reset(binding.slot);
                    }
                    else
                        DEBUG_PRINT(UNUSED_SAMPLER, "WARNING: attempted sampler binding to a slot unused by shader\n");
                }
            }

            if(slotsSampler.any())
                DEBUG_PRINT(UNBOUND_SAMPLER, "WARNING: some sampler slots are not bound\n");

            DescriptorIndex baseDescriptorIndex;
            m_pResources->dhSamplers.AllocateDescriptors(shader->numSamplers, baseDescriptorIndex);
            D3D12_CPU_DESCRIPTOR_HANDLE baseDescriptor = m_pResources->dhSamplers.GetCpuHandle(baseDescriptorIndex);

            m_pDevice->CopyDescriptors(1, &baseDescriptor, &shader->numSamplers, shader->numSamplers, copySources, nullptr, D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER);
                
            ((D3D12_GPU_DESCRIPTOR_HANDLE*)rootDescriptorTableHandles)[rootIndex] = m_pResources->dhSamplers.GetGpuHandle(baseDescriptorIndex);
            rootIndex++;
        }
    }

    void RendererInterfaceD3D12::syncWithGPU(const char* reason)
    {
        flushCommandList();
        m_pResources->WaitForFence(m_pResources->fenceCounter, reason);
    }

    void RendererInterfaceD3D12::waitForFence(unsigned long long fenceValue, const char* reason)
    {
        flushCommandList();
        m_pResources->WaitForFence(fenceValue, reason);
    }

    TextureHandle RendererInterfaceD3D12::createTexture(const TextureDesc & d, const void * data)
    {
        Texture* texture = new Texture(this);
        texture->desc = d;
        texture->isManaged = true;
        texture->desc.depthOrArraySize = std::max(d.depthOrArraySize, 1u);
        texture->numPlanes = (d.format == Format::D24S8) ? 2u : 1u;

        const auto& formatMapping = GetFormatMapping(d.format);

        D3D12_RESOURCE_DESC desc = {};
        desc.Width = d.width;
        desc.Height = d.height;
        desc.DepthOrArraySize = UINT16(texture->desc.depthOrArraySize);
        desc.MipLevels = UINT16(d.mipLevels);
        desc.Format = formatMapping.resourceFormat;
        desc.SampleDesc.Count = d.sampleCount;
        desc.SampleDesc.Quality = d.sampleQuality;

        if (d.depthOrArraySize > 1 && !d.isArray && !d.isCubeMap)
            desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE3D;
        else
            desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

        if (d.isRenderTarget)
        {
            if (formatMapping.isDepthStencil)
                desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
            else
                desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        }

        if (d.isUAV)
            desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

        D3D12_HEAP_PROPERTIES heapProps = {};
        heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

        D3D12_CLEAR_VALUE clearValue = {};
        clearValue.Format = formatMapping.rtvFormat;
        if (formatMapping.isDepthStencil)
        {
            clearValue.DepthStencil.Depth = d.clearValue.r;
            clearValue.DepthStencil.Stencil = UINT8(d.clearValue.g);
        }
        else
        {
            clearValue.Color[0] = d.clearValue.r;
            clearValue.Color[1] = d.clearValue.g;
            clearValue.Color[2] = d.clearValue.b;
            clearValue.Color[3] = d.clearValue.a;
        }

        HRESULT hr = m_pDevice->CreateCommittedResource(
            &heapProps, 
            D3D12_HEAP_FLAG_NONE, 
            &desc, 
            D3D12_RESOURCE_STATE_COMMON, 
            d.useClearValue ? &clearValue : nullptr, 
            IID_PPV_ARGS(&texture->resource));

        CHECK_ERROR(SUCCEEDED(hr), "Failed to create a texture");

        if (FAILED(hr))
        {
            delete texture;
            return nullptr;
        }

        if (d.debugName)
            D3D_SET_OBJECT_NAME_N_A(texture->resource, uint32_t(strlen(d.debugName)), d.debugName);

        uint32_t numSubresources = d.mipLevels * texture->numPlanes;
        if (d.isArray || d.isCubeMap)
            numSubresources *= texture->desc.depthOrArraySize;
        texture->subresourceStates.resize(numSubresources, D3D12_RESOURCE_STATE_COMMON);

        m_pResources->textures.insert(texture);

		if (data && d.mipLevels == 1)
		{
			uint32_t rowPitch = formatMapping.bytesPerPixel * d.width;
			uint32_t slicePitch = rowPitch * d.height;
			uint32_t subresourcePitch = slicePitch * (d.isArray ? 1 : d.depthOrArraySize);

			for (uint32_t subresource = 0; subresource < numSubresources; subresource++)
			{ 
				writeTexture(texture, subresource, (char*)data + subresourcePitch * subresource, rowPitch, slicePitch);
			}
        }

        return texture;
    }

    const TextureDesc& RendererInterfaceD3D12::describeTexture(TextureHandle t)
    {
        return t->GetDesc();
    }

    void RendererInterfaceD3D12::clearTextureFloat(TextureHandle _t, const Color & clearColor)
    {
        Texture* t = static_cast<Texture*>(_t);

        if (!t->desc.useClearValue)
        { 
            DEBUG_PRINT(CLEAR_VALUE, "WARNING: No clear value passed to createTexture. D3D will issue a warning here.\n");
        }
        else if (t->desc.clearValue != clearColor)
        {
			DEBUG_PRINT(CLEAR_VALUE, "WARNING: Clear value differs from one passed to createTexture. D3D will issue a warning here.\n");
        }

        const auto& formatMapping = GetFormatMapping(t->desc.format);

        if (t->desc.isRenderTarget)
        {
            if (formatMapping.isDepthStencil)
            {
				requireTextureState(t, ~0u, ~0u, D3D12_RESOURCE_STATE_DEPTH_WRITE);
				commitBarriers();

                for (UINT mipLevel = 0; mipLevel < t->desc.mipLevels; mipLevel++)
                {
                    DescriptorIndex index = getDSV(t, ~0u, mipLevel);

                    m_ActiveCommandList->commandList->ClearDepthStencilView(
                        m_pResources->dhDSV.GetCpuHandle(index),
                        D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
                        clearColor.r, UINT8(clearColor.g),
                        0, nullptr);
                }
            }
            else
            {
				requireTextureState(t, ~0u, ~0u, D3D12_RESOURCE_STATE_RENDER_TARGET);
				commitBarriers();

				for (UINT mipLevel = 0; mipLevel < t->desc.mipLevels; mipLevel++)
                {
                    DescriptorIndex index = getRTV(t, ~0u, mipLevel);

                    m_ActiveCommandList->commandList->ClearRenderTargetView(
                        m_pResources->dhRTV.GetCpuHandle(index),
                        &clearColor.r,
                        0, nullptr);
                }
            }
        }
        else
        {
			requireTextureState(t, ~0u, ~0u, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
			commitBarriers();

			for (UINT mipLevel = 0; mipLevel < t->desc.mipLevels; mipLevel++)
            {
                TextureBinding binding;
                binding.texture = t;
                binding.format = Format::UNKNOWN;
                binding.mipLevel = mipLevel;
                DescriptorIndex indexCpu = getTextureUAV(binding);

                D3D12_CPU_DESCRIPTOR_HANDLE descriptorCpu = m_pResources->dhSRVstatic.GetCpuHandle(indexCpu);

                DescriptorIndex indexGpu;
                m_pResources->dhSRVetc.AllocateDescriptors(1, indexGpu);

                m_pDevice->CopyDescriptorsSimple(1, m_pResources->dhSRVetc.GetCpuHandle(indexGpu), descriptorCpu, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

                m_ActiveCommandList->commandList->ClearUnorderedAccessViewFloat(m_pResources->dhSRVetc.GetGpuHandle(indexGpu), descriptorCpu, t->resource, &clearColor.r, 0, nullptr);
            }
        }

        m_ActiveCommandList->size++;
        loadBalanceCommandList();
    }

    void RendererInterfaceD3D12::clearTextureUInt(TextureHandle _t, uint32_t clearColor)
    {
        Texture* t = static_cast<Texture*>(_t);

		CHECK_ERROR(t->desc.isUAV, "cannot clear a non-UAV texture as uint");

		for (UINT mipLevel = 0; mipLevel < t->desc.mipLevels; mipLevel++)
		{
			bool dummy = false;
			requireTextureState(_t, mipLevel, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, dummy);
		}

		commitBarriers();

        const auto& formatMapping = GetFormatMapping(t->desc.format);

        for (UINT mipLevel = 0; mipLevel < t->desc.mipLevels; mipLevel++)
        {
            TextureBinding binding;
            binding.texture = t;
            binding.format = formatMapping.bytesPerPixel == 4 ? Format::R32_UINT : Format::UNKNOWN;
            binding.mipLevel = mipLevel;
            DescriptorIndex indexCpu = getTextureUAV(binding);

            D3D12_CPU_DESCRIPTOR_HANDLE descriptorCpu = m_pResources->dhSRVstatic.GetCpuHandle(indexCpu);

            DescriptorIndex indexGpu;
            m_pResources->dhSRVetc.AllocateDescriptors(1, indexGpu);

            m_pDevice->CopyDescriptorsSimple(1, m_pResources->dhSRVetc.GetCpuHandle(indexGpu), descriptorCpu, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

            uint32_t clearValues[4] = { clearColor, clearColor, clearColor, clearColor };

            m_ActiveCommandList->commandList->ClearUnorderedAccessViewUint(m_pResources->dhSRVetc.GetGpuHandle(indexGpu), descriptorCpu, t->resource, clearValues, 0, nullptr);
        }

        m_ActiveCommandList->size++;
        loadBalanceCommandList();
    }

    void RendererInterfaceD3D12::writeTexture(TextureHandle _t, uint32_t subresource, const void * data, uint32_t rowPitch, uint32_t depthPitch)
    {
        Texture* t = static_cast<Texture*>(_t);

		bool dummy = false;
		requireTextureState(t, subresource, D3D12_RESOURCE_STATE_COPY_DEST, dummy);
		commitBarriers();

        D3D12_RESOURCE_DESC desc = t->resource->GetDesc();
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint;
            
        UINT64 footprintBytes;
        m_pDevice->GetCopyableFootprints(&desc, subresource, 1, 0, &footprint, nullptr, nullptr, &footprintBytes);
        footprint.Offset = m_pResources->upload.SuballocateBuffer(footprintBytes, D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT);
            
        for (uint32_t plane = 0; plane < footprint.Footprint.Depth; plane++)
        {
            for (uint32_t row = 0; row < footprint.Footprint.Height; row++)
            {
                void* destAddress = m_pResources->upload.GetCpuVA(footprint.Offset + footprint.Footprint.RowPitch * (row + plane * footprint.Footprint.Height));
                void* srcAddress = (char*)data + rowPitch * row + depthPitch * plane;
                memcpy(destAddress, srcAddress, std::min(rowPitch, footprint.Footprint.RowPitch));
            }
        }

        D3D12_TEXTURE_COPY_LOCATION dest = {};
        dest.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dest.SubresourceIndex = subresource;
        dest.pResource = t->resource;

        D3D12_TEXTURE_COPY_LOCATION src = {};
        src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        src.PlacedFootprint = footprint;
        src.pResource = m_pResources->upload.GetBuffer();

        m_ActiveCommandList->commandList->CopyTextureRegion(&dest, 0, 0, 0, &src, nullptr);
        m_ActiveCommandList->size++;
        loadBalanceCommandList();
    }

    bool RendererInterfaceD3D12::readTexture(TextureHandle _t, void* data, size_t rowPitch)
    {
        Texture* t = static_cast<Texture*>(_t);

        D3D12_RESOURCE_DESC desc = t->resource->GetDesc();

        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint;
        UINT numRows;
        UINT64 rowSizeInBytes, footprintBytes;
        m_pDevice->GetCopyableFootprints(&desc, 0, 1, 0, &footprint, &numRows, &rowSizeInBytes, &footprintBytes);

        if (desc.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D)
            return false;

        D3D12_RESOURCE_DESC readbackDesc = {};
        readbackDesc.Width = footprintBytes;
        readbackDesc.Height = 1;
        readbackDesc.DepthOrArraySize = 1;
        readbackDesc.MipLevels = 1;
        readbackDesc.Format = DXGI_FORMAT_UNKNOWN;
        readbackDesc.SampleDesc.Count = 1;
        readbackDesc.SampleDesc.Quality = 0;
        readbackDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        readbackDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        D3D12_HEAP_PROPERTIES heapProps = {};
        heapProps.Type = D3D12_HEAP_TYPE_READBACK;

        ID3D12Resource* pReadbackBuffer = nullptr;

        HRESULT hr = m_pDevice->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &readbackDesc,
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(&pReadbackBuffer));

        CHECK_ERROR(SUCCEEDED(hr), "Failed to create a readback buffer");

        if (FAILED(hr))
            return false;

        bool anyUavBarrier = false;
        requireTextureState(t, 0, D3D12_RESOURCE_STATE_COPY_SOURCE, anyUavBarrier);
        commitBarriers();

        D3D12_TEXTURE_COPY_LOCATION locationDst = {};
        locationDst.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        locationDst.pResource = pReadbackBuffer;
        locationDst.PlacedFootprint = footprint;

        D3D12_TEXTURE_COPY_LOCATION locationSrc = {};
        locationSrc.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        locationSrc.pResource = t->resource;
        locationSrc.SubresourceIndex = 0;

        m_ActiveCommandList->commandList->CopyTextureRegion(&locationDst, 0, 0, 0, &locationSrc, nullptr);
        m_ActiveCommandList->size++;

        syncWithGPU("ReadTexture");

        D3D12_RANGE range;
        range.Begin = 0;
        range.End = size_t(footprintBytes);
        void* pData = nullptr;
        hr = pReadbackBuffer->Map(0, &range, &pData);

        CHECK_ERROR(SUCCEEDED(hr), "Failed to Map a readback buffer");

        const FormatMapping& formatMapping = GetFormatMapping(t->desc.format);
        for (int row = 0; row < int(desc.Height); row++)
        {
            memcpy((char*)data + row * rowPitch, (char*)pData + row * footprint.Footprint.RowPitch, size_t(desc.Width) * formatMapping.bytesPerPixel);
        }

        pReadbackBuffer->Unmap(0, nullptr);
        pReadbackBuffer->Release();

        return true;
    }

    void RendererInterfaceD3D12::destroyTexture(TextureHandle _t)
    {
        Texture* t = static_cast<Texture*>(_t);

        if (t == nullptr)
            return;

        m_pResources->textures.erase(t);

		if (t->isManaged)
			m_pResources->deletedTextures.insert(t);
		else
			delete t;
    }

	void RendererInterfaceD3D12::resolveTexture(TextureHandle _dst, TextureHandle _src, Format::Enum format, uint32_t dstSubres, uint32_t srcSubres)
    {
        Texture* dst = static_cast<Texture*>(_dst);
        Texture* src = static_cast<Texture*>(_src);

		DXGI_FORMAT dxgiFormat = GetFormatMapping(format != Format::UNKNOWN ? format : dst->desc.format).srvFormat;

		// TODO: transitions?
		m_ActiveCommandList->commandList->ResolveSubresource(dst->resource, dstSubres, src->resource, srcSubres, dxgiFormat);
	}

	ID3D12Resource* RendererInterfaceD3D12::getResourceForTexture(TextureHandle handle)
	{
		if(!handle)
			return nullptr;

		return static_cast<Texture*>(handle)->resource;
	}

    void* RendererInterfaceD3D12::handoffTexture(TextureHandle t)
    {
        requireTextureState(t, 0, 0, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
        commitBarriers();
        flushCommandList();
        return static_cast<Texture*>(t)->resource;
    }

    BufferHandle RendererInterfaceD3D12::createBuffer(const BufferDesc & d, const void * data)
    {
        Buffer* buffer = new Buffer(this);
        buffer->desc = d;

        D3D12_RESOURCE_DESC desc = {};
        desc.Width = d.byteSize;
        desc.Height = 1;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.Format = DXGI_FORMAT_UNKNOWN;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        if (d.canHaveUAVs)
            desc.Flags |= D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
            
        D3D12_HEAP_PROPERTIES heapProps = {};
        heapProps.Type = D3D12_HEAP_TYPE_DEFAULT;

        HRESULT hr = m_pDevice->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_COMMON,
            nullptr,
            IID_PPV_ARGS(&buffer->resource));

        CHECK_ERROR(SUCCEEDED(hr), "Failed to create a buffer");

        if (FAILED(hr))
        {
            delete buffer;
            return nullptr;
        }

		buffer->gpuVA = buffer->resource->GetGPUVirtualAddress();

        if (d.debugName)
            D3D_SET_OBJECT_NAME_N_A(buffer->resource, uint32_t(strlen(d.debugName)), d.debugName);

        m_pResources->buffers.insert(buffer);

        if (data)
            writeBuffer(buffer, data, d.byteSize);

        return buffer;
    }

    void RendererInterfaceD3D12::writeBuffer(BufferHandle _b, const void * data, size_t dataSize)
    {
        Buffer* b = static_cast<Buffer*>(_b);

        UINT64 uploadOffset = m_pResources->upload.SuballocateBuffer(dataSize);
        memcpy(m_pResources->upload.GetCpuVA(uploadOffset), data, dataSize);
        requireBufferState(b, D3D12_RESOURCE_STATE_COPY_DEST);
        commitBarriers();
        m_ActiveCommandList->commandList->CopyBufferRegion(b->resource, 0, m_pResources->upload.GetBuffer(), uploadOffset, dataSize);
        m_ActiveCommandList->size++;
        loadBalanceCommandList();
    }

    void RendererInterfaceD3D12::clearBufferUInt(BufferHandle b, uint32_t clearValue)
    {
        requireBufferState(b, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        commitBarriers();

        BufferBinding binding;
        binding.buffer = b;
        binding.format = Format::UNKNOWN;
        DescriptorIndex indexCpu = getBufferUAV(binding);
        D3D12_CPU_DESCRIPTOR_HANDLE descriptorCpu = m_pResources->dhSRVstatic.GetCpuHandle(indexCpu);

        DescriptorIndex indexGpu;
        m_pResources->dhSRVetc.AllocateDescriptors(1, indexGpu);

        m_pDevice->CopyDescriptorsSimple(1, m_pResources->dhSRVetc.GetCpuHandle(indexGpu), descriptorCpu, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

        const uint32_t values[4] = { clearValue, clearValue, clearValue, clearValue };
        m_ActiveCommandList->commandList->ClearUnorderedAccessViewUint(m_pResources->dhSRVetc.GetGpuHandle(indexGpu), descriptorCpu, static_cast<Buffer*>(b)->resource, values, 0, nullptr);
        m_ActiveCommandList->size++;
        loadBalanceCommandList();
    }

    void RendererInterfaceD3D12::copyToBuffer(BufferHandle _dst, uint32_t destOffsetBytes, BufferHandle _src, uint32_t srcOffsetBytes, size_t dataSizeBytes)
    {
        Buffer* dst = static_cast<Buffer*>(_dst);
        Buffer* src = static_cast<Buffer*>(_src);

        requireBufferState(dst, D3D12_RESOURCE_STATE_COPY_DEST);
        requireBufferState(src, D3D12_RESOURCE_STATE_COPY_SOURCE);
        commitBarriers();

        m_ActiveCommandList->commandList->CopyBufferRegion(dst->resource, destOffsetBytes, src->resource, srcOffsetBytes, dataSizeBytes);
        m_ActiveCommandList->size++;
        loadBalanceCommandList();
    }

    void RendererInterfaceD3D12::readBuffer(BufferHandle _b, void * data, size_t * dataSize)
    {
        Buffer* b = static_cast<Buffer*>(_b);

        D3D12_RESOURCE_DESC desc = {};
        desc.Width = b->desc.byteSize;
        desc.Height = 1;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.Format = DXGI_FORMAT_UNKNOWN;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        D3D12_HEAP_PROPERTIES heapProps = {};
        heapProps.Type = D3D12_HEAP_TYPE_READBACK;

        ID3D12Resource* pReadbackBuffer = nullptr;

        HRESULT hr = m_pDevice->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(&pReadbackBuffer));

        CHECK_ERROR(SUCCEEDED(hr), "Failed to create a readback buffer");

        if (FAILED(hr))
        {
            *dataSize = 0;
            return;
        }

        requireBufferState(b, D3D12_RESOURCE_STATE_COPY_SOURCE);
        commitBarriers();
        m_ActiveCommandList->commandList->CopyBufferRegion(pReadbackBuffer, 0, b->resource, 0, b->desc.byteSize);
        m_ActiveCommandList->size++;

        syncWithGPU("ReadBuffer");

        D3D12_RANGE range;
        range.Begin = 0;
        range.End = b->desc.byteSize;
        void* pData = nullptr;
        hr = pReadbackBuffer->Map(0, &range, &pData);

        CHECK_ERROR(SUCCEEDED(hr), "Failed to Map a readback buffer");

        memcpy(data, pData, *dataSize);

        pReadbackBuffer->Unmap(0, nullptr);
        pReadbackBuffer->Release();
    }

    void RendererInterfaceD3D12::destroyBuffer(BufferHandle _b)
    {
        Buffer* b = static_cast<Buffer*>(_b);

        if (b == nullptr)
            return;

        m_pResources->buffers.erase(b);
        m_pResources->deletedBuffers.insert(b);
    }

    ConstantBufferHandle RendererInterfaceD3D12::createConstantBuffer(const ConstantBufferDesc & d, const void * data)
    {
        ConstantBuffer* cbuffer = new ConstantBuffer(this);
        cbuffer->desc = d;
        cbuffer->data.resize(d.byteSize);
        cbuffer->alignedSize = Align(d.byteSize, 256);

        if (data)
            memcpy(&cbuffer->data[0], data, d.byteSize);
            
        m_pResources->constantBuffers.push_back(cbuffer);
        return cbuffer;
    }

    void RendererInterfaceD3D12::writeConstantBuffer(ConstantBufferHandle _b, const void * data, size_t dataSize)
    {
        ConstantBuffer* b = static_cast<ConstantBuffer*>(_b);

        size_t size = std::min(uint32_t(dataSize), b->desc.byteSize);
        if (memcmp(&b->data[0], data, size) == 0)
        {
            b->numIdenticalWrites++;
            return;
        }

        memcpy(&b->data[0], data, size);
        b->uploadedDataValid = false;
        b->numWrites++;
    }

    void RendererInterfaceD3D12::destroyConstantBuffer(ConstantBufferHandle b)
    {
        if (b == nullptr)
            return;

        for (auto it = m_pResources->constantBuffers.begin(); it != m_pResources->constantBuffers.end(); it++)
            if (*it == b)
            {
                m_pResources->constantBuffers.erase(it);
                break;
            }

        // no need to put CBs into the deleted resources pool: they do not have actual D3D resource associated
        delete b;
    }

    ShaderHandle RendererInterfaceD3D12::createShader(const ShaderDesc & d, const void * binary, const size_t binarySize)
    {
        if (binarySize == 0)
            return nullptr;


        Shader* shader = new Shader(this);
        shader->type = d.shaderType;
        shader->bytecode.resize(binarySize);
        shader->hlslExtensionsUAV = d.hlslExtensionsUAV;
        memcpy(&shader->bytecode[0], binary, binarySize);

#if NVRHI_D3D12_WITH_NVAPI
		// Save the custom semantics structure because it may be on the stack or otherwise dynamic.
		// Note that this has to be a deep copy; currently NV_CUSTOM_SEMANTIC has no pointers, but that might change.
		if (d.numCustomSemantics && d.pCustomSemantics)
		{
			shader->customSemantics.resize(d.numCustomSemantics);
			memcpy(&shader->customSemantics[0], d.pCustomSemantics, sizeof(NV_CUSTOM_SEMANTIC) * d.numCustomSemantics);
		}

		// Save the coordinate swizzling patterns for the same reason
		if (d.pCoordinateSwizzling)
		{
			const uint32_t numSwizzles = 16;
			shader->coordinateSwizzling.resize(numSwizzles);
			memcpy(&shader->coordinateSwizzling[0], d.pCoordinateSwizzling, sizeof(uint32_t) * numSwizzles);
		}

		if (d.hlslExtensionsUAV >= 0)
		{
			NVAPI_D3D12_PSO_SET_SHADER_EXTENSION_SLOT_DESC* pExtn = new NVAPI_D3D12_PSO_SET_SHADER_EXTENSION_SLOT_DESC();
			memset(pExtn, 0, sizeof(*pExtn));
			pExtn->baseVersion = NV_PSO_EXTENSION_DESC_VER;
			pExtn->psoExtension = NV_PSO_SET_SHADER_EXTNENSION_SLOT_AND_SPACE;
			pExtn->version = NV_SET_SHADER_EXTENSION_SLOT_DESC_VER;

			pExtn->uavSlot = d.hlslExtensionsUAV;
			pExtn->registerSpace = 0;

			shader->extensions.push_back(pExtn);
		}

		switch (d.shaderType)
		{
		case ShaderType::SHADER_VERTEX:
			if (d.numCustomSemantics)
			{
				NVAPI_D3D12_PSO_VERTEX_SHADER_DESC* pExtn = new NVAPI_D3D12_PSO_VERTEX_SHADER_DESC();
				memset(pExtn, 0, sizeof(*pExtn));
				pExtn->baseVersion = NV_PSO_EXTENSION_DESC_VER;
				pExtn->psoExtension = NV_PSO_VERTEX_SHADER_EXTENSION;
				pExtn->version = NV_VERTEX_SHADER_PSO_EXTENSION_DESC_VER;

				pExtn->NumCustomSemantics = d.numCustomSemantics;
				pExtn->pCustomSemantics = &shader->customSemantics[0];
				pExtn->UseSpecificShaderExt = d.useSpecificShaderExt;

				shader->extensions.push_back(pExtn);
			}
			break;
		case ShaderType::SHADER_HULL:
			if (d.numCustomSemantics)
			{
				NVAPI_D3D12_PSO_HULL_SHADER_DESC* pExtn = new NVAPI_D3D12_PSO_HULL_SHADER_DESC();
				memset(pExtn, 0, sizeof(*pExtn));
				pExtn->baseVersion = NV_PSO_EXTENSION_DESC_VER;
				pExtn->psoExtension = NV_PSO_VERTEX_SHADER_EXTENSION;
				pExtn->version = NV_HULL_SHADER_PSO_EXTENSION_DESC_VER;

				pExtn->NumCustomSemantics = d.numCustomSemantics;
				pExtn->pCustomSemantics = &shader->customSemantics[0];
				pExtn->UseSpecificShaderExt = d.useSpecificShaderExt;

				shader->extensions.push_back(pExtn);
			}
			break;
		case ShaderType::SHADER_DOMAIN:
			if (d.numCustomSemantics)
			{
				NVAPI_D3D12_PSO_DOMAIN_SHADER_DESC* pExtn = new NVAPI_D3D12_PSO_DOMAIN_SHADER_DESC();
				memset(pExtn, 0, sizeof(*pExtn));
				pExtn->baseVersion = NV_PSO_EXTENSION_DESC_VER;
				pExtn->psoExtension = NV_PSO_VERTEX_SHADER_EXTENSION;
				pExtn->version = NV_DOMAIN_SHADER_PSO_EXTENSION_DESC_VER;

				pExtn->NumCustomSemantics = d.numCustomSemantics;
				pExtn->pCustomSemantics = &shader->customSemantics[0];
				pExtn->UseSpecificShaderExt = d.useSpecificShaderExt;

				shader->extensions.push_back(pExtn);
			}
			break;
		case ShaderType::SHADER_GEOMETRY:
			if ((d.fastGSFlags & FastGeometryShaderFlags::COMPATIBILITY_MODE) && (d.fastGSFlags & FastGeometryShaderFlags::FORCE_FAST_GS))
			{
				CHECK_ERROR(d.numCustomSemantics == 0, "Compatibility mode FastGS does not support custom semantics");

				NVAPI_D3D12_PSO_CREATE_FASTGS_EXPLICIT_DESC* pExtn = new NVAPI_D3D12_PSO_CREATE_FASTGS_EXPLICIT_DESC();
				memset(pExtn, 0, sizeof(*pExtn));
				pExtn->baseVersion = NV_PSO_EXTENSION_DESC_VER;
				pExtn->psoExtension = NV_PSO_EXPLICIT_FASTGS_EXTENSION;
				pExtn->version = NV_FASTGS_EXPLICIT_PSO_EXTENSION_VER;

				if (d.pCoordinateSwizzling)
					pExtn->pCoordinateSwizzling = &shader->coordinateSwizzling[0];

				pExtn->flags = 0;

				if (d.fastGSFlags & FastGeometryShaderFlags::USE_VIEWPORT_MASK) 
					pExtn->flags |= NV_FASTGS_USE_VIEWPORT_MASK;

				if (d.fastGSFlags & FastGeometryShaderFlags::OFFSET_RT_INDEX_BY_VP_INDEX) 
					pExtn->flags |= NV_FASTGS_OFFSET_RT_INDEX_BY_VP_INDEX;

				if (d.fastGSFlags & FastGeometryShaderFlags::STRICT_API_ORDER) 
					pExtn->flags |= NV_FASTGS_STRICT_API_ORDER;

				shader->extensions.push_back(pExtn);
			}
			else if ((d.fastGSFlags & FastGeometryShaderFlags::FORCE_FAST_GS) || d.numCustomSemantics || d.pCoordinateSwizzling)
			{
				NVAPI_D3D12_PSO_GEOMETRY_SHADER_DESC* pExtn = new NVAPI_D3D12_PSO_GEOMETRY_SHADER_DESC();
				memset(pExtn, 0, sizeof(*pExtn));
				pExtn->baseVersion = NV_PSO_EXTENSION_DESC_VER;
				pExtn->psoExtension = NV_PSO_GEOMETRY_SHADER_EXTENSION;
				pExtn->version = NV_GEOMETRY_SHADER_PSO_EXTENSION_DESC_VER;

				pExtn->NumCustomSemantics = d.numCustomSemantics;
				pExtn->pCustomSemantics = d.numCustomSemantics ? &shader->customSemantics[0] : nullptr;
				pExtn->UseCoordinateSwizzle = d.pCoordinateSwizzling != nullptr;
				pExtn->pCoordinateSwizzling = d.pCoordinateSwizzling != nullptr ? &shader->coordinateSwizzling[0] : nullptr;
				pExtn->ForceFastGS = (d.fastGSFlags & FastGeometryShaderFlags::FORCE_FAST_GS) != 0;
				pExtn->UseViewportMask = (d.fastGSFlags & FastGeometryShaderFlags::USE_VIEWPORT_MASK) != 0;
				pExtn->OffsetRtIndexByVpIndex = (d.fastGSFlags & FastGeometryShaderFlags::OFFSET_RT_INDEX_BY_VP_INDEX) != 0;
				pExtn->DontUseViewportOrder = (d.fastGSFlags & FastGeometryShaderFlags::STRICT_API_ORDER) != 0;
				pExtn->UseSpecificShaderExt = d.useSpecificShaderExt;
				pExtn->UseAttributeSkipMask = false;

				shader->extensions.push_back(pExtn);
			}
			break;
		}
#endif


        uint32_t maxCB = 0, maxSRV = 0, maxSampler = 0, maxUAV = 0;

        if (d.metadataValid)
        {
            // VXGI stores metadata in shader binaries, and that metadata can be used here

            for (uint32_t word = 0; word < ARRAYSIZE(d.metadata.slotsSRV); word++)
            {
                if (d.metadata.slotsSRV[word])
                {
                    for (uint32_t bit = 0; bit < 32; bit++)
                    {
                        uint32_t i = (word << 5) | bit;

                        if (d.metadata.slotsSRV[word] & (1 << bit))
                        {
                            shader->minSRV = std::min(shader->minSRV, i);
                            maxSRV = std::max(maxSRV, i);
                            shader->slotsSRV.set(i);
                        }
                    }
                }
            }

            for (uint32_t word = 0; word < ARRAYSIZE(d.metadata.slotsSampler); word++)
            {
                if (d.metadata.slotsSampler[word])
                {
                    for (uint32_t bit = 0; bit < 32; bit++)
                    {
                        uint32_t i = (word << 5) | bit;

                        if (d.metadata.slotsSampler[word] & (1 << bit))
                        {
                            shader->minSampler = std::min(shader->minSampler, i);
                            maxSampler = std::max(maxSampler, i);
                            shader->slotsSampler.set(i);
                        }
                    }
                }
            }

            for (uint32_t i = 0; i < ARRAYSIZE(d.metadata.constantBufferSizes); i++)
            {
                if (d.metadata.constantBufferSizes[i])
                {
                    shader->minCB = std::min(shader->minCB, i);
                    maxCB = std::max(maxCB, i);
                    shader->slotsCB.set(i);
                }
            }
            
            if (d.metadata.slotsUAV)
            {
                for (uint32_t i = 0; i < 32; i++)
                {
                    if (d.metadata.slotsUAV & (1 << i))
                    {
                        shader->minUAV = std::min(shader->minUAV, i);
                        maxUAV = std::max(maxUAV, i);
                        shader->slotsUAV.set(i);
                    }
                }
            }
        }
        else
        { 
#if ENABLE_D3D_REFLECTION
            // Other apps may not have such metadata, so extract it using D3D reflection.

            // Use D3D11 shader reflection interfaces because the compiler DLL from Windows 10, which supports D3D12, 
            // compiles the emittance voxelization shaders differently, and the result is wrong. So the DLL from Windows 8 should be used.
            ID3D11ShaderReflection* pReflector = NULL;
            HRESULT hr = D3DReflect(binary, binarySize, IID_PPV_ARGS(&pReflector));
            CHECK_ERROR(SUCCEEDED(hr), "Failed to get shader reflection");

            if (FAILED(hr))
                return nullptr;

            D3D11_SHADER_DESC desc;
            pReflector->GetDesc(&desc);

            for (uint32_t i = 0; i < desc.BoundResources; i++)
            {
                D3D11_SHADER_INPUT_BIND_DESC bindingDesc;
                hr = pReflector->GetResourceBindingDesc(i, &bindingDesc);
                if (SUCCEEDED(hr))
                {
                    switch (bindingDesc.Type)
                    {
                    case D3D_SIT_CBUFFER:
                        shader->minCB = std::min(shader->minCB, bindingDesc.BindPoint);
                                maxCB = std::max(        maxCB, bindingDesc.BindPoint + bindingDesc.BindCount - 1);
                        shader->slotsCB.set(bindingDesc.BindPoint);
                        break;

                    case D3D_SIT_TBUFFER:
                    case D3D_SIT_TEXTURE:
                    case D3D_SIT_STRUCTURED:
                    case D3D_SIT_BYTEADDRESS:
                        shader->minSRV = std::min(shader->minSRV, bindingDesc.BindPoint);
                                maxSRV = std::max(        maxSRV, bindingDesc.BindPoint + bindingDesc.BindCount - 1);
                        shader->slotsSRV.set(bindingDesc.BindPoint);
                        break;

                    case D3D_SIT_SAMPLER:
                        shader->minSampler = std::min(shader->minSampler, bindingDesc.BindPoint);
                                maxSampler = std::max(        maxSampler, bindingDesc.BindPoint + bindingDesc.BindCount - 1);
                        shader->slotsSampler.set(bindingDesc.BindPoint);
                        break;

                    case D3D_SIT_UAV_RWTYPED:
                    case D3D_SIT_UAV_RWSTRUCTURED:
                    case D3D_SIT_UAV_RWBYTEADDRESS:
                    case D3D_SIT_UAV_APPEND_STRUCTURED:
                    case D3D_SIT_UAV_CONSUME_STRUCTURED:
                    case D3D_SIT_UAV_RWSTRUCTURED_WITH_COUNTER:
                        shader->minUAV = std::min(shader->minUAV, bindingDesc.BindPoint);
                                maxUAV = std::max(        maxUAV, bindingDesc.BindPoint + bindingDesc.BindCount - 1);
                        shader->slotsUAV.set(bindingDesc.BindPoint);
                        break;
                    }
                }
            }

            SAFE_RELEASE(pReflector);
#else
            // Reflection is disabled, and there is no metadata... Fail.

            SIGNAL_ERROR("No shader resource information is passed in ShaderDesc, and reflection is disabled. Cannot create shader.");
            return nullptr;
#endif
        }

        if (shader->minCB <= maxCB)
            shader->numCB = maxCB - shader->minCB + 1;

        if (shader->minSRV <= maxSRV)
            shader->numSRV = maxSRV - shader->minSRV + 1;

        if (shader->minUAV <= maxUAV)
            shader->numUAV = maxUAV - shader->minUAV + 1;

        if (shader->minSampler <= maxSampler)
            shader->numSamplers = maxSampler - shader->minSampler + 1;

        // numBindings does NOT include samplers
        shader->numBindings = shader->numCB + shader->numSRV + shader->numUAV;


        m_pResources->shaders.insert(shader);
        return shader;
    }

    ShaderHandle RendererInterfaceD3D12::createShaderFromAPIInterface(ShaderType::Enum, const void *)
    {
        SIGNAL_ERROR("createShaderFromAPIInterface is not supported on D3D12");
        return nullptr;
    }

    void RendererInterfaceD3D12::destroyShader(ShaderHandle _s)
    {
        Shader* s = static_cast<Shader*>(_s);

        if (s == nullptr)
            return;

        m_pResources->shaders.erase(s);

#if NVRHI_D3D12_WITH_NVAPI
		for (auto pExtn : s->extensions)
			delete pExtn;
		s->extensions.clear();
#endif

        // Step 1 - find the root signatures that reference this shader

        std::set<uint32_t> rootsigHashesToDelete;

        for (auto pair : m_pResources->rootsigCache)
        {
            if (pair.second->shaders.count(s))
                rootsigHashesToDelete.insert(pair.first);
        }

        // Step 2 - move the found root signatured to the deleted pool, find the pipeline states that reference the root signatures

        std::set<uint32_t> psoHashesToDelete;

        for (auto hash : rootsigHashesToDelete)
        {
            auto rootsig = m_pResources->rootsigCache[hash];
            m_pResources->rootsigCache.erase(hash);
            m_pResources->deletedResources.insert(rootsig);

            for (auto pair : m_pResources->psoCache)
                if (pair.second != nullptr && pair.second->rootSignature == rootsig)
                    psoHashesToDelete.insert(pair.first);
        }

        // Step 3 - move the pipeline states to the deleted pool

        for (auto hash : psoHashesToDelete)
        {
            auto pso = m_pResources->psoCache[hash];
            m_pResources->psoCache.erase(hash);
            m_pResources->deletedResources.insert(pso);
        }

        // no need to put shaders into the deleted resources pool: they do not have actual D3D resource associated
        delete s;
    }

    SamplerHandle RendererInterfaceD3D12::createSampler(const SamplerDesc & d)
    {
        Sampler* sampler = new Sampler(this);
        sampler->desc = d;
        m_pResources->samplers.insert(sampler);
        return sampler;
    }

    void RendererInterfaceD3D12::destroySampler(SamplerHandle _s)
    {
        Sampler* s = static_cast<Sampler*>(_s);

        if (s == nullptr)
            return;

        m_pResources->samplers.erase(s);

        // no need to put samplers into the deleted resources pool: they do not have actual D3D resource associated
        delete s;
    }

    InputLayoutHandle RendererInterfaceD3D12::createInputLayout(const VertexAttributeDesc * d, uint32_t attributeCount, const void * vertexShaderBinary, const size_t binarySize)
    {
        // These are not needed here, there are no separate IL objects in DX12
        (void)vertexShaderBinary;
        (void)binarySize;

        InputLayout* layout = new InputLayout(this);
        layout->attributes.resize(attributeCount);
        layout->inputElements.resize(attributeCount);

        for (uint32_t index = 0; index < attributeCount; index++)
        {
            VertexAttributeDesc& attr = layout->attributes[index];
            D3D12_INPUT_ELEMENT_DESC& desc = layout->inputElements[index];

            // Copy the description to get a stable name pointer in desc
            attr = d[index];

            desc.SemanticName = attr.name;
            desc.AlignedByteOffset = attr.offset;
            desc.Format = GetFormatMapping(attr.format).srvFormat;
            desc.InputSlot = attr.bufferIndex;
            desc.SemanticIndex = attr.semanticIndex;

            if (attr.isInstanced)
            {
                desc.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_INSTANCE_DATA;
                desc.InstanceDataStepRate = 1;
            }
            else
            {
                desc.InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
                desc.InstanceDataStepRate = 0;
            }
        }

        m_pResources->inputLayouts.insert(layout);
        return layout;
    }

    void RendererInterfaceD3D12::destroyInputLayout(InputLayoutHandle _i)
    {
        InputLayout* i = static_cast<InputLayout*>(_i);

        if (i == nullptr)
            return;

        m_pResources->inputLayouts.erase(i);

        // no need to put input layouts into the deleted resources pool: they do not have actual D3D resource associated
        delete i;
    }

    PerformanceQueryHandle RendererInterfaceD3D12::createPerformanceQuery(const char * name)
    {
        PerformanceQuery* query = new PerformanceQuery(this);

        if(name != nullptr)
            query->name = name;

        m_pResources->perfQueries.insert(query);

        query->beginIndex = m_pResources->nextQueryIndex++;
        query->endIndex = m_pResources->nextQueryIndex++;
        return query;
    }

    void RendererInterfaceD3D12::destroyPerformanceQuery(PerformanceQueryHandle _query)
    {
        PerformanceQuery* query = static_cast<PerformanceQuery*>(_query);

        if (query == nullptr)
            return;

        m_pResources->perfQueries.erase(query);
        m_pResources->deletedPerfQueries.insert(query);
    }

    void RendererInterfaceD3D12::beginPerformanceQuery(PerformanceQueryHandle _query, bool onlyAnnotation)
    {
        PerformanceQuery* query = static_cast<PerformanceQuery*>(_query);

        CHECK_ERROR(query->state != PerformanceQuery::STARTED && query->state != PerformanceQuery::ANNOTATION, "Query is already started");

        if (!query->name.empty())
            PIXBeginEvent(m_ActiveCommandList->commandList, 0, query->name.c_str());

        if (onlyAnnotation)
        {
            query->state = PerformanceQuery::ANNOTATION;
        }
        else
        {
            query->state = PerformanceQuery::STARTED;
            m_ActiveCommandList->commandList->EndQuery(m_pResources->perfQueryHeap, D3D12_QUERY_TYPE_TIMESTAMP, query->beginIndex);
            m_ActiveCommandList->size++;
        }
    }

    void RendererInterfaceD3D12::endPerformanceQuery(PerformanceQueryHandle _query)
    {
        PerformanceQuery* query = static_cast<PerformanceQuery*>(_query);

        CHECK_ERROR(query->state == PerformanceQuery::STARTED || query->state == PerformanceQuery::ANNOTATION, "Query is not started");

        if (!query->name.empty())
            PIXEndEvent(m_ActiveCommandList->commandList);

        if (query->state == PerformanceQuery::ANNOTATION)
        {
            query->state = PerformanceQuery::RESOLVED;
            query->time = 0.f;
        }
        else
        {
            query->state = PerformanceQuery::FINISHED;
            m_ActiveCommandList->commandList->EndQuery(m_pResources->perfQueryHeap, D3D12_QUERY_TYPE_TIMESTAMP, query->endIndex);
            m_ActiveCommandList->size++;
        }
    }

    float RendererInterfaceD3D12::getPerformanceQueryTimeMS(PerformanceQueryHandle _query)
    {
        PerformanceQuery* query = static_cast<PerformanceQuery*>(_query);

        CHECK_ERROR(query->state != PerformanceQuery::STARTED, "Query is in progress, can't get time");
        CHECK_ERROR(query->state != PerformanceQuery::NEW, "Query has never been started, can't get time");

        if (query->state == PerformanceQuery::RESOLVED)
            return query->time;

        requireBufferState(m_pResources->perfQueryResolveBuffer, D3D12_RESOURCE_STATE_COPY_DEST);
		commitBarriers();

        for (auto q : m_pResources->perfQueries)
        {
            if (q->state == PerformanceQuery::FINISHED)
            {
                m_ActiveCommandList->commandList->ResolveQueryData(
                    m_pResources->perfQueryHeap, 
                    D3D12_QUERY_TYPE_TIMESTAMP, 
                    q->beginIndex,      // StartIndex
                    2,                  // NumQueries
                    m_pResources->perfQueryResolveBuffer->resource, 
                    q->beginIndex * 8   // AlignedDestinationBufferOffset
                ); 
            }
        }
            
        uint64_t* data = new uint64_t[m_pResources->nextQueryIndex];
        size_t dataSize = m_pResources->nextQueryIndex * 8;
        readBuffer(m_pResources->perfQueryResolveBuffer, data, &dataSize);

        uint64_t frequency;
        m_pCommandQueue->GetTimestampFrequency(&frequency);

        for (auto q : m_pResources->perfQueries)
        {
            if (q->state == PerformanceQuery::FINISHED)
            {
                q->time = float(1000.0 * double(data[q->endIndex] - data[q->beginIndex]) / double(frequency));
                q->state = PerformanceQuery::RESOLVED;
            }
        }

        return query->time;
    }

    GraphicsAPI::Enum RendererInterfaceD3D12::getGraphicsAPI()
    {
        return GraphicsAPI::D3D12;
    }

    void * RendererInterfaceD3D12::getAPISpecificInterface(APISpecificInterface::Enum interfaceType)
    {
        if (interfaceType == APISpecificInterface::D3D12DEVICE)
            return m_pDevice;

        if (interfaceType == APISpecificInterface::D3D12QUEUE)
            return m_pCommandQueue;

        return nullptr;
    }

    bool RendererInterfaceD3D12::isOpenGLExtensionSupported(const char *)
    {
        return false;
    }

    void * RendererInterfaceD3D12::getOpenGLProcAddress(const char *)
    {
        return nullptr;
    }

    void RendererInterfaceD3D12::draw(const DrawCallState & state, const DrawArguments* args, uint32_t numDrawCalls)
    {
        applyState(state);
        //commitBarriers();

        for (uint32_t i = 0; i < numDrawCalls; i++)
            m_ActiveCommandList->commandList->DrawInstanced(args[i].vertexCount, args[i].instanceCount, args[i].startVertexLocation, args[i].startInstanceLocation);

        m_ActiveCommandList->size += numDrawCalls;
        loadBalanceCommandList();
    }

    void RendererInterfaceD3D12::drawIndexed(const DrawCallState & state, const DrawArguments* args, uint32_t numDrawCalls)
    {
        applyState(state);
        //commitBarriers();

        for (uint32_t i = 0; i < numDrawCalls; i++)
            m_ActiveCommandList->commandList->DrawIndexedInstanced(args[i].vertexCount, args[i].instanceCount, args[i].startIndexLocation, args[i].startVertexLocation, args[i].startInstanceLocation);

        m_ActiveCommandList->size += numDrawCalls;
        loadBalanceCommandList();
    }

    void RendererInterfaceD3D12::drawIndirect(const DrawCallState & state, BufferHandle indirectParams, uint32_t offsetBytes)
    {
        applyState(state);
        // Transition the indirect args buffer but only in case it wasn't already used as SRV in a shader
        if ((static_cast<Buffer*>(indirectParams)->state & D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT) == 0)
            requireBufferState(indirectParams, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
        commitBarriers();

        m_ActiveCommandList->commandList->ExecuteIndirect(m_pResources->drawIndirectSignature, 1, static_cast<Buffer*>(indirectParams)->resource, offsetBytes, nullptr, 0);
        m_ActiveCommandList->size++;
        loadBalanceCommandList();
    }

    void RendererInterfaceD3D12::dispatch(const DispatchState & state, uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ)
    {
        applyState(state);
        commitBarriers();

        m_ActiveCommandList->commandList->Dispatch(groupsX, groupsY, groupsZ);
        m_ActiveCommandList->size++;
        loadBalanceCommandList();
    }

    void RendererInterfaceD3D12::dispatchIndirect(const DispatchState & state, BufferHandle indirectParams, uint32_t offsetBytes)
    {
        applyState(state);
        // Transition the indirect args buffer but only in case it wasn't already used as SRV in a shader
        if ((static_cast<Buffer*>(indirectParams)->state & D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT) == 0)
            requireBufferState(indirectParams, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT);
        commitBarriers();

        m_ActiveCommandList->commandList->ExecuteIndirect(m_pResources->dispatchIndirectSignature, 1, static_cast<Buffer*>(indirectParams)->resource, offsetBytes, nullptr, 0);
        m_ActiveCommandList->size++;
        loadBalanceCommandList();
    }

	void RendererInterfaceD3D12::setModifiedWMode(bool enabled, uint32_t numViewports, const float* pA, const float* pB)
	{
#if NVRHI_D3D12_WITH_NVAPI
		NV_MODIFIED_W_PARAMS modifiedWParams = { };
		modifiedWParams.version = NV_MODIFIED_W_PARAMS_VER;

		if (enabled && numViewports)
		{
			CHECK_ERROR(pA != nullptr && pB != nullptr, "pA and pB are required");

			modifiedWParams.numEntries = numViewports;

			for (uint32_t vp = 0; vp < numViewports; vp++)
			{
				modifiedWParams.modifiedWCoefficients[vp].fA = pA[vp];
				modifiedWParams.modifiedWCoefficients[vp].fB = pB[vp];
			}
		}
		else
		{
			modifiedWParams.numEntries = 0;
		}


		if (NvAPI_D3D12_SetModifiedWMode(m_ActiveCommandList->commandList, &modifiedWParams) == NVAPI_OK)
			m_pResources->currentModifiedWParams = modifiedWParams;
		else
			m_pResources->currentModifiedWParams.numEntries = 0;
#else
		(void)enabled;
		(void)numViewports;
		(void)pA;
		(void)pB;
#endif
	}

	void RendererInterfaceD3D12::setSinglePassStereoMode(bool enabled, uint32_t renderTargetIndexOffset, bool independentViewportMask)
	{
#if NVRHI_D3D12_WITH_NVAPI
		if (NvAPI_D3D12_SetSinglePassStereoMode(m_ActiveCommandList->commandList, enabled ? 2 : 1, renderTargetIndexOffset, independentViewportMask) == NVAPI_OK)
		{
			m_pResources->spsEnabled = enabled;
			m_pResources->spsRenderTargetIndexOffset = renderTargetIndexOffset;
			m_pResources->spsIndependentViewportMask = independentViewportMask;
		}
		else
		{
			m_pResources->spsEnabled = false;
		}
#else
		(void)enabled;
		(void)renderTargetIndexOffset;
		(void)independentViewportMask;
#endif
	}

    uint32_t RendererInterfaceD3D12::getNumberOfAFRGroups()
    {
        return 1;
    }

    uint32_t RendererInterfaceD3D12::getAFRGroupOfCurrentFrame(uint32_t numAFRGroups)
    {
        (void)numAFRGroups;
        return 0;
    }

    void RendererInterfaceD3D12::setEnableUavBarriers(bool enableBarriers, const TextureHandle* textures, size_t numTextures, const BufferHandle* buffers, size_t numBuffers)
    {
        if (textures)
        {
            for (size_t nTexture = 0; nTexture < numTextures; nTexture++)
            {
                Texture* texture = static_cast<Texture*>(textures[nTexture]);
                
                texture->enableUavBarriers = enableBarriers;
                texture->firstUavBarrierPlaced = false;
            }
        }

        if (buffers)
        {
            for (size_t nBuffer = 0; nBuffer < numBuffers; nBuffer++)
            {
                Buffer* buffer = static_cast<Buffer*>(buffers[nBuffer]);

                buffer->enableUavBarriers = enableBarriers;
                buffer->firstUavBarrierPlaced = false;
            }
        }
    }

    void RendererInterfaceD3D12::applyState(const DrawCallState & state)
    {
		RootSignatureHandle pRS = getRootSignature(state);
        PipelineStateHandle pPSO = getPipelineState(state, pRS);

        if (pPSO == nullptr)
            return;

        // Generate the descriptor tables first because that may reset the command list

        uint32_t rootIndex = 0;
        D3D12_GPU_DESCRIPTOR_HANDLE rootDescriptorTables[10];

        if(state.VS.shader) bindShaderResources(rootIndex, rootDescriptorTables, state.VS);
        if(state.HS.shader) bindShaderResources(rootIndex, rootDescriptorTables, state.HS);
        if(state.DS.shader) bindShaderResources(rootIndex, rootDescriptorTables, state.DS);
        if(state.GS.shader) bindShaderResources(rootIndex, rootDescriptorTables, state.GS);
        if(state.PS.shader) bindShaderResources(rootIndex, rootDescriptorTables, state.PS);

        // Create the RTVs and DSVs - this may also reset the command list

        D3D12_CPU_DESCRIPTOR_HANDLE RTVs[8] = {};
        D3D12_CPU_DESCRIPTOR_HANDLE DSV = {};
            
        for (uint32_t rt = 0; rt < state.renderState.targetCount; rt++)
        {
            TextureHandle target = state.renderState.targets[rt];

            RTVs[rt] = m_pResources->dhRTV.GetCpuHandle(getRTV(target, state.renderState.targetIndicies[rt], state.renderState.targetMipSlices[rt]));

            requireTextureState(target, state.renderState.targetIndicies[rt], state.renderState.targetMipSlices[rt],
                D3D12_RESOURCE_STATE_RENDER_TARGET);
        }

        TextureHandle depth = state.renderState.depthTarget;
        if (depth)
        {
            DSV = m_pResources->dhDSV.GetCpuHandle(getDSV(depth, state.renderState.depthIndex, state.renderState.depthMipSlice));

            D3D12_RESOURCE_STATES resourceState = D3D12_RESOURCE_STATE_DEPTH_READ;
            if (state.renderState.depthStencilState.depthWriteMask == DepthStencilState::DEPTH_WRITE_MASK_ALL ||
                state.renderState.depthStencilState.stencilWriteMask != 0)
                resourceState = D3D12_RESOURCE_STATE_DEPTH_WRITE;

            requireTextureState(depth, state.renderState.depthIndex, state.renderState.depthMipSlice, resourceState);
        }

        // Clear the color and depth targets, if requested

        if (state.renderState.clearColorTarget)
        {
			commitBarriers();

            for (uint32_t rt = 0; rt < state.renderState.targetCount; rt++)
            {
                Texture* target = static_cast<Texture*>(state.renderState.targets[rt]);

                if (!target->desc.useClearValue)
                {
                    DEBUG_PRINT(CLEAR_VALUE, "WARNING: No clear value passed to createTexture. D3D will issue a warning here.\n");
                }
                else if (target->desc.clearValue != state.renderState.clearColor)
                {
					DEBUG_PRINT(CLEAR_VALUE, "WARNING: Clear value differs from one passed to createTexture. D3D will issue a warning here.\n");
                }

                m_ActiveCommandList->commandList->ClearRenderTargetView(RTVs[rt], &state.renderState.clearColor.r, 0, nullptr);
            }
        }

        if (state.renderState.depthTarget && (state.renderState.clearDepthTarget || state.renderState.clearStencilTarget))
        {
			commitBarriers();

			D3D12_CLEAR_FLAGS flags = D3D12_CLEAR_FLAGS(0);
            if (state.renderState.clearDepthTarget)
                flags |= D3D12_CLEAR_FLAG_DEPTH;
            if (state.renderState.clearStencilTarget)
                flags |= D3D12_CLEAR_FLAG_STENCIL;

            m_ActiveCommandList->commandList->ClearDepthStencilView(DSV, flags, state.renderState.clearDepth, state.renderState.clearStencil, 0, nullptr);
        }


        // Setup the graphics state
            
        pRS->fenceCounterAtLastUse = m_pResources->fenceCounter;
        pPSO->fenceCounterAtLastUse = m_pResources->fenceCounter;

		if (m_pResources->currentPSO != pPSO->handle)
		{
			m_ActiveCommandList->commandList->SetPipelineState(pPSO->handle);
			m_pResources->currentPSO = pPSO->handle;
		}

		if (m_pResources->currentRS != pRS->handle)
		{
			m_ActiveCommandList->commandList->SetGraphicsRootSignature(pRS->handle);
			m_pResources->currentRS = pRS->handle;
		}

        if (state.indexBuffer)
            requireBufferState(state.indexBuffer, D3D12_RESOURCE_STATE_INDEX_BUFFER);

        for (uint32_t i = 0; i < state.vertexBufferCount; i++)
        {
            requireBufferState(state.vertexBuffers[i].buffer, D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER);
        }

        commitBarriers();

		D3D12_INDEX_BUFFER_VIEW IBV = {};

		if (state.indexBuffer)
        {
            Buffer* indexBuffer = static_cast<Buffer*>(state.indexBuffer);

			IBV.BufferLocation = indexBuffer->gpuVA + state.indexBufferOffset;
			IBV.Format = GetFormatMapping(state.indexBufferFormat).srvFormat;
			IBV.SizeInBytes = indexBuffer->desc.byteSize - state.indexBufferOffset;
        }

		if (memcmp(&IBV, &m_pResources->currentIBV, sizeof(IBV)) != 0)
		{
			m_ActiveCommandList->commandList->IASetIndexBuffer(&IBV);
			m_pResources->currentIBV = IBV;
		}

		D3D12_VERTEX_BUFFER_VIEW VBVs[16] = {};

        for (uint32_t i = 0; i < state.vertexBufferCount; i++)
        {
            const VertexBufferBinding& binding = state.vertexBuffers[i];
            Buffer* vertexBuffer = static_cast<Buffer*>(binding.buffer);

			VBVs[binding.slot].BufferLocation = vertexBuffer->gpuVA + binding.offset;
			VBVs[binding.slot].StrideInBytes = binding.stride;
			VBVs[binding.slot].SizeInBytes = vertexBuffer->desc.byteSize - binding.offset;
        }
		for (uint32_t i = 0; i < ARRAYSIZE(VBVs); i++)
		{
			if (memcmp(&VBVs[i], &m_pResources->currentVBVs[i], sizeof(VBVs[i])) != 0)
			{
				m_ActiveCommandList->commandList->IASetVertexBuffers(i, 1, VBVs[i].BufferLocation != 0 ? &VBVs[i] : nullptr);
			}
		}
		memcpy(m_pResources->currentVBVs, VBVs, sizeof(VBVs));

		
        m_ActiveCommandList->commandList->IASetPrimitiveTopology(convertPrimitiveType(state.primType));

        for (uint32_t i = 0; i < rootIndex; i++)
            m_ActiveCommandList->commandList->SetGraphicsRootDescriptorTable(i, rootDescriptorTables[i]);

        uint32_t rtWidth = 0, rtHeight = 0;
        if (state.renderState.depthTarget)
        {
            Texture* depthTarget = static_cast<Texture*>(state.renderState.depthTarget);

            rtWidth = depthTarget->desc.width;
            rtHeight = depthTarget->desc.height;
        }
        else if (state.renderState.targetCount > 0)
        {
            Texture* target0 = static_cast<Texture*>(state.renderState.targets[0]);

            rtWidth = target0->desc.width;
            rtHeight = target0->desc.height;
        }

        D3D12_VIEWPORT viewports[16];
        D3D12_RECT scissorRects[16];

        for (uint32_t rt = 0; rt < state.renderState.viewportCount; rt++)
        {
            viewports[rt].TopLeftX = state.renderState.viewports[rt].minX;
            viewports[rt].TopLeftY = state.renderState.viewports[rt].minY;
            viewports[rt].Width = state.renderState.viewports[rt].maxX - state.renderState.viewports[rt].minX;
            viewports[rt].Height = state.renderState.viewports[rt].maxY - state.renderState.viewports[rt].minY;
            viewports[rt].MinDepth = state.renderState.viewports[rt].minZ;
            viewports[rt].MaxDepth = state.renderState.viewports[rt].maxZ;

            if (state.renderState.rasterState.scissorEnable)
            {
                scissorRects[rt].left = (LONG)state.renderState.scissorRects[rt].minX;
                scissorRects[rt].top = (LONG)state.renderState.scissorRects[rt].minY;
                scissorRects[rt].right = (LONG)state.renderState.scissorRects[rt].maxX;
                scissorRects[rt].bottom = (LONG)state.renderState.scissorRects[rt].maxY;
            }
            else
            {
                scissorRects[rt].left = (LONG)state.renderState.viewports[rt].minX;
                scissorRects[rt].top = (LONG)state.renderState.viewports[rt].minY;
                scissorRects[rt].right = (LONG)state.renderState.viewports[rt].maxX;
                scissorRects[rt].bottom = (LONG)state.renderState.viewports[rt].maxY;

                if (rtWidth > 0)
                {
                    scissorRects[rt].left = std::max(scissorRects[rt].left, LONG(0));
                    scissorRects[rt].top = std::max(scissorRects[rt].top, LONG(0));
                    scissorRects[rt].right = std::min(scissorRects[rt].right, LONG(rtWidth));
                    scissorRects[rt].bottom = std::min(scissorRects[rt].bottom, LONG(rtHeight));
                }
            }
        }

        m_ActiveCommandList->commandList->RSSetViewports(state.renderState.viewportCount, viewports);
        m_ActiveCommandList->commandList->RSSetScissorRects(state.renderState.viewportCount, scissorRects);

		if (memcmp(RTVs, m_pResources->currentRTVs, sizeof(RTVs)) != 0 || DSV.ptr != m_pResources->currentDSV.ptr)
		{
			m_ActiveCommandList->commandList->OMSetRenderTargets(state.renderState.targetCount, RTVs, false, state.renderState.depthTarget ? &DSV : nullptr);
			memcpy(m_pResources->currentRTVs, RTVs, sizeof(RTVs));
			m_pResources->currentDSV = DSV;
		}

        if (state.renderState.depthStencilState.stencilEnable)
        {
            m_ActiveCommandList->commandList->OMSetStencilRef(state.renderState.depthStencilState.stencilRefValue);
        }

        m_ActiveCommandList->size++;
    }

    void RendererInterfaceD3D12::applyState(const DispatchState & state)
    {
		uint32_t hash = getComputeStateHash(state);
        RootSignatureHandle pRS = getRootSignature(state, hash);
        PipelineStateHandle pPSO = getPipelineState(state, pRS, hash);

        if (pPSO == nullptr)
            return;

        // Generate the descriptor tables first because that may reset the command list

        uint32_t rootIndex = 0;
        D3D12_GPU_DESCRIPTOR_HANDLE rootDescriptorTables[2];

        bindShaderResources(rootIndex, rootDescriptorTables, state);


        // Setup the state

        pRS->fenceCounterAtLastUse = m_pResources->fenceCounter;
        pPSO->fenceCounterAtLastUse = m_pResources->fenceCounter;

		if (m_pResources->currentPSO != pPSO->handle)
		{
			m_ActiveCommandList->commandList->SetPipelineState(pPSO->handle);
			m_pResources->currentPSO = pPSO->handle;
		}

		if (m_pResources->currentRS != pRS->handle)
		{
			m_ActiveCommandList->commandList->SetComputeRootSignature(pRS->handle);
			m_pResources->currentRS = pRS->handle;
		}

        for (uint32_t i = 0; i < rootIndex; i++)
            m_ActiveCommandList->commandList->SetComputeRootDescriptorTable(i, rootDescriptorTables[i]);

        m_ActiveCommandList->size++;
    }
}
