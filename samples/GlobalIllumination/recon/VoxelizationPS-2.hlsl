#define REGISTER(type, slot) register(type##slot)
#define NV_SHADER_EXTN_SLOT u0
#define VXGI_VOXELIZE_CB_SLOT 1
#define VXGI_SCISSOR_REGIONS_SRV_SLOT 2
#define VXGI_ALLOCATION_MAP_UAV_SLOT 1
#define VXGI_VOXELTEX_0_UAV_SLOT 2
#define VXGI_VOXELTEX_1_UAV_SLOT 3
#define VXGI_VOXELTEX_2_UAV_SLOT 4
#define VXGI_VOXELTEX_3_UAV_SLOT 5
#define VXGI_VOXELTEX_4_UAV_SLOT 6
#define VXGI_VOXELTEX_5_UAV_SLOT 7
#define VXGI_SCISSOR_STATS_UAV_SLOT 8
#define VXGI_COVERAGE_MASKS_SRV_SLOT 3
#define VXGI_VOXELIZE_MATERIAL_CB_SLOT 2
#define VXGI_IRRADIANCE_MAP_SRV_SLOT 4
#define VXGI_IRRADIANCE_MAP_SAMPLER_SLOT 2
#define VXGI_INVALIDATE_BITMAP_SRV_SLOT 5

struct NvShaderExtnStruct
{
	uint opcode;
	uint rid;
	uint sid;

	uint4 dst1u;
	uint4 padding0[3];

	uint4 src0u;
	uint4 src1u;
	uint4 src2u;
	uint4 dst0u;

	uint markUavRef;
	float padding1[28];
};

RWStructuredBuffer<NvShaderExtnStruct> g_NvidiaExt : register(NV_SHADER_EXTN_SLOT);

int __NvGetShflMaskFromWidth(uint width)
{
	return ((32 - width) << 8) | 0x1F;
}

void __NvReferenceUAVForOp(RWByteAddressBuffer uav)
{
	uint index = g_NvidiaExt.IncrementCounter();
	g_NvidiaExt[index].markUavRef = 1;
	uav.Store(index, 0);
}

void __NvReferenceUAVForOp(RWTexture1D<float2> uav)
{
	uint index = g_NvidiaExt.IncrementCounter();
	g_NvidiaExt[index].markUavRef = 1;
	uav[index] = float2(0, 0);
}

void __NvReferenceUAVForOp(RWTexture2D<float2> uav)
{
	uint index = g_NvidiaExt.IncrementCounter();
	g_NvidiaExt[index].markUavRef = 1;
	uav[uint2(index, index)] = float2(0, 0);
}

void __NvReferenceUAVForOp(RWTexture3D<float2> uav)
{
	uint index = g_NvidiaExt.IncrementCounter();
	g_NvidiaExt[index].markUavRef = 1;
	uav[uint3(index, index, index)] = float2(0, 0);
}

void __NvReferenceUAVForOp(RWTexture1D<float4> uav)
{
	uint index = g_NvidiaExt.IncrementCounter();
	g_NvidiaExt[index].markUavRef = 1;
	uav[index] = float4(0, 0, 0, 0);
}

void __NvReferenceUAVForOp(RWTexture2D<float4> uav)
{
	uint index = g_NvidiaExt.IncrementCounter();
	g_NvidiaExt[index].markUavRef = 1;
	uav[uint2(index, index)] = float4(0, 0, 0, 0);
}

void __NvReferenceUAVForOp(RWTexture3D<float4> uav)
{
	uint index = g_NvidiaExt.IncrementCounter();
	g_NvidiaExt[index].markUavRef = 1;
	uav[uint3(index, index, index)] = float4(0, 0, 0, 0);
}

void __NvReferenceUAVForOp(RWTexture1D<float> uav)
{
	uint index = g_NvidiaExt.IncrementCounter();
	g_NvidiaExt[index].markUavRef = 1;
	uav[index] = 0.0f;
}

void __NvReferenceUAVForOp(RWTexture2D<float> uav)
{
	uint index = g_NvidiaExt.IncrementCounter();
	g_NvidiaExt[index].markUavRef = 1;
	uav[uint2(index, index)] = 0.0f;
}

void __NvReferenceUAVForOp(RWTexture3D<float> uav)
{
	uint index = g_NvidiaExt.IncrementCounter();
	g_NvidiaExt[index].markUavRef = 1;
	uav[uint3(index, index, index)] = 0.0f;
}

void __NvReferenceUAVForOp(RWTexture1D<uint2> uav)
{
	uint index = g_NvidiaExt.IncrementCounter();
	g_NvidiaExt[index].markUavRef = 1;
	uav[index] = uint2(0, 0);
}

void __NvReferenceUAVForOp(RWTexture2D<uint2> uav)
{
	uint index = g_NvidiaExt.IncrementCounter();
	g_NvidiaExt[index].markUavRef = 1;
	uav[uint2(index, index)] = uint2(0, 0);
}

void __NvReferenceUAVForOp(RWTexture3D<uint2> uav)
{
	uint index = g_NvidiaExt.IncrementCounter();
	g_NvidiaExt[index].markUavRef = 1;
	uav[uint3(index, index, index)] = uint2(0, 0);
}

void __NvReferenceUAVForOp(RWTexture1D<uint4> uav)
{
	uint index = g_NvidiaExt.IncrementCounter();
	g_NvidiaExt[index].markUavRef = 1;
	uav[index] = uint4(0, 0, 0, 0);
}

void __NvReferenceUAVForOp(RWTexture2D<uint4> uav)
{
	uint index = g_NvidiaExt.IncrementCounter();
	g_NvidiaExt[index].markUavRef = 1;
	uav[uint2(index, index)] = uint4(0, 0, 0, 0);
}

void __NvReferenceUAVForOp(RWTexture3D<uint4> uav)
{
	uint index = g_NvidiaExt.IncrementCounter();
	g_NvidiaExt[index].markUavRef = 1;
	uav[uint3(index, index, index)] = uint4(0, 0, 0, 0);
}

void __NvReferenceUAVForOp(RWTexture1D<uint> uav)
{
	uint index = g_NvidiaExt.IncrementCounter();
	g_NvidiaExt[index].markUavRef = 1;
	uav[index] = 0;
}

void __NvReferenceUAVForOp(RWTexture2D<uint> uav)
{
	uint index = g_NvidiaExt.IncrementCounter();
	g_NvidiaExt[index].markUavRef = 1;
	uav[uint2(index, index)] = 0;
}

void __NvReferenceUAVForOp(RWTexture3D<uint> uav)
{
	uint index = g_NvidiaExt.IncrementCounter();
	g_NvidiaExt[index].markUavRef = 1;
	uav[uint3(index, index, index)] = 0;
}

void __NvReferenceUAVForOp(RWTexture1D<int2> uav)
{
	uint index = g_NvidiaExt.IncrementCounter();
	g_NvidiaExt[index].markUavRef = 1;
	uav[index] = int2(0, 0);
}

void __NvReferenceUAVForOp(RWTexture2D<int2> uav)
{
	uint index = g_NvidiaExt.IncrementCounter();
	g_NvidiaExt[index].markUavRef = 1;
	uav[uint2(index, index)] = int2(0, 0);
}

void __NvReferenceUAVForOp(RWTexture3D<int2> uav)
{
	uint index = g_NvidiaExt.IncrementCounter();
	g_NvidiaExt[index].markUavRef = 1;
	uav[uint3(index, index, index)] = int2(0, 0);
}

void __NvReferenceUAVForOp(RWTexture1D<int4> uav)
{
	uint index = g_NvidiaExt.IncrementCounter();
	g_NvidiaExt[index].markUavRef = 1;
	uav[index] = int4(0, 0, 0, 0);
}

void __NvReferenceUAVForOp(RWTexture2D<int4> uav)
{
	uint index = g_NvidiaExt.IncrementCounter();
	g_NvidiaExt[index].markUavRef = 1;
	uav[uint2(index, index)] = int4(0, 0, 0, 0);
}

void __NvReferenceUAVForOp(RWTexture3D<int4> uav)
{
	uint index = g_NvidiaExt.IncrementCounter();
	g_NvidiaExt[index].markUavRef = 1;
	uav[uint3(index, index, index)] = int4(0, 0, 0, 0);
}

void __NvReferenceUAVForOp(RWTexture1D<int> uav)
{
	uint index = g_NvidiaExt.IncrementCounter();
	g_NvidiaExt[index].markUavRef = 1;
	uav[index] = 0;
}

void __NvReferenceUAVForOp(RWTexture2D<int> uav)
{
	uint index = g_NvidiaExt.IncrementCounter();
	g_NvidiaExt[index].markUavRef = 1;
	uav[uint2(index, index)] = 0;
}

void __NvReferenceUAVForOp(RWTexture3D<int> uav)
{
	uint index = g_NvidiaExt.IncrementCounter();
	g_NvidiaExt[index].markUavRef = 1;
	uav[uint3(index, index, index)] = 0;
}

uint __NvAtomicOpFP16x2(RWByteAddressBuffer uav, uint byteAddress, uint fp16x2Val, uint atomicOpType)
{
	__NvReferenceUAVForOp(uav);
	uint index = g_NvidiaExt.IncrementCounter();
	g_NvidiaExt[index].src0u.x = byteAddress;
	g_NvidiaExt[index].src1u.x = fp16x2Val;
	g_NvidiaExt[index].src2u.x = atomicOpType;
	g_NvidiaExt[index].opcode = 12;

	return g_NvidiaExt[index].dst0u.x;
}

uint __NvAtomicOpFP16x2(RWTexture1D<float2> uav, uint address, uint fp16x2Val, uint atomicOpType)
{
	__NvReferenceUAVForOp(uav);
	uint index = g_NvidiaExt.IncrementCounter();
	g_NvidiaExt[index].src0u.x = address;
	g_NvidiaExt[index].src1u.x = fp16x2Val;
	g_NvidiaExt[index].src2u.x = atomicOpType;
	g_NvidiaExt[index].opcode = 12;

	return g_NvidiaExt[index].dst0u.x;
}

uint __NvAtomicOpFP16x2(RWTexture2D<float2> uav, uint2 address, uint fp16x2Val, uint atomicOpType)
{
	__NvReferenceUAVForOp(uav);
	uint index = g_NvidiaExt.IncrementCounter();
	g_NvidiaExt[index].src0u.xy = address;
	g_NvidiaExt[index].src1u.x = fp16x2Val;
	g_NvidiaExt[index].src2u.x = atomicOpType;
	g_NvidiaExt[index].opcode = 12;

	return g_NvidiaExt[index].dst0u.x;
}

uint __NvAtomicOpFP16x2(RWTexture3D<float2> uav, uint3 address, uint fp16x2Val, uint atomicOpType)
{
	__NvReferenceUAVForOp(uav);
	uint index = g_NvidiaExt.IncrementCounter();
	g_NvidiaExt[index].src0u.xyz = address;
	g_NvidiaExt[index].src1u.x = fp16x2Val;
	g_NvidiaExt[index].src2u.x = atomicOpType;
	g_NvidiaExt[index].opcode = 12;

	return g_NvidiaExt[index].dst0u.x;
}

uint2 __NvAtomicOpFP16x2(RWTexture1D<float4> uav, uint address, uint2 fp16x2Val, uint atomicOpType)
{
	__NvReferenceUAVForOp(uav);

	uint2 retVal;

	uint index = g_NvidiaExt.IncrementCounter();
	g_NvidiaExt[index].src0u.x = address * 2;
	g_NvidiaExt[index].src1u.x = fp16x2Val.x;
	g_NvidiaExt[index].src2u.x = atomicOpType;
	g_NvidiaExt[index].opcode = 12;
	retVal.x = g_NvidiaExt[index].dst0u.x;

	index = g_NvidiaExt.IncrementCounter();
	g_NvidiaExt[index].src0u.x = address * 2 + 1;
	g_NvidiaExt[index].src1u.x = fp16x2Val.y;
	g_NvidiaExt[index].src2u.x = atomicOpType;
	g_NvidiaExt[index].opcode = 12;
	retVal.y = g_NvidiaExt[index].dst0u.x;

	return retVal;
}

uint2 __NvAtomicOpFP16x2(RWTexture2D<float4> uav, uint2 address, uint2 fp16x2Val, uint atomicOpType)
{
	__NvReferenceUAVForOp(uav);

	uint2 retVal;

	uint2 addressTemp = uint2(address.x * 2, address.y);
	uint index = g_NvidiaExt.IncrementCounter();
	g_NvidiaExt[index].src0u.xy = addressTemp;
	g_NvidiaExt[index].src1u.x = fp16x2Val.x;
	g_NvidiaExt[index].src2u.x = atomicOpType;
	g_NvidiaExt[index].opcode = 12;
	retVal.x = g_NvidiaExt[index].dst0u.x;

	addressTemp.x++;
	index = g_NvidiaExt.IncrementCounter();
	g_NvidiaExt[index].src0u.xy = addressTemp;
	g_NvidiaExt[index].src1u.x = fp16x2Val.y;
	g_NvidiaExt[index].src2u.x = atomicOpType;
	g_NvidiaExt[index].opcode = 12;
	retVal.y = g_NvidiaExt[index].dst0u.x;

	return retVal;
}

uint2 __NvAtomicOpFP16x2(RWTexture3D<float4> uav, uint3 address, uint2 fp16x2Val, uint atomicOpType)
{
	__NvReferenceUAVForOp(uav);

	uint2 retVal;

	uint3 addressTemp = uint3(address.x * 2, address.y, address.z);
	uint index = g_NvidiaExt.IncrementCounter();
	g_NvidiaExt[index].src0u.xyz = addressTemp;
	g_NvidiaExt[index].src1u.x = fp16x2Val.x;
	g_NvidiaExt[index].src2u.x = atomicOpType;
	g_NvidiaExt[index].opcode = 12;
	retVal.x = g_NvidiaExt[index].dst0u.x;

	addressTemp.x++;
	index = g_NvidiaExt.IncrementCounter();
	g_NvidiaExt[index].src0u.xyz = addressTemp;
	g_NvidiaExt[index].src1u.x = fp16x2Val.y;
	g_NvidiaExt[index].src2u.x = atomicOpType;
	g_NvidiaExt[index].opcode = 12;
	retVal.y = g_NvidiaExt[index].dst0u.x;

	return retVal;
}

uint __fp32x2Tofp16x2(float2 val)
{
	return (f32tof16(val.y) << 16) | f32tof16(val.x);
}

uint2 __fp32x4Tofp16x4(float4 val)
{
	return uint2((f32tof16(val.y) << 16) | f32tof16(val.x), (f32tof16(val.w) << 16) | f32tof16(val.z));
}

float __NvAtomicAddFP32(RWByteAddressBuffer uav, uint byteAddress, float val)
{
	__NvReferenceUAVForOp(uav);
	uint index = g_NvidiaExt.IncrementCounter();
	g_NvidiaExt[index].src0u.x = byteAddress;
	g_NvidiaExt[index].src1u.x = asuint(val);
	g_NvidiaExt[index].src2u.x = 3;
	g_NvidiaExt[index].opcode = 13;

	return asfloat(g_NvidiaExt[index].dst0u.x);
}

float __NvAtomicAddFP32(RWTexture1D<float> uav, uint address, float val)
{
	__NvReferenceUAVForOp(uav);
	uint index = g_NvidiaExt.IncrementCounter();
	g_NvidiaExt[index].src0u.x = address;
	g_NvidiaExt[index].src1u.x = asuint(val);
	g_NvidiaExt[index].src2u.x = 3;
	g_NvidiaExt[index].opcode = 13;

	return asfloat(g_NvidiaExt[index].dst0u.x);
}

float __NvAtomicAddFP32(RWTexture2D<float> uav, uint2 address, float val)
{
	__NvReferenceUAVForOp(uav);
	uint index = g_NvidiaExt.IncrementCounter();
	g_NvidiaExt[index].src0u.xy = address;
	g_NvidiaExt[index].src1u.x = asuint(val);
	g_NvidiaExt[index].src2u.x = 3;
	g_NvidiaExt[index].opcode = 13;

	return asfloat(g_NvidiaExt[index].dst0u.x);
}

float __NvAtomicAddFP32(RWTexture3D<float> uav, uint3 address, float val)
{
	__NvReferenceUAVForOp(uav);
	uint index = g_NvidiaExt.IncrementCounter();
	g_NvidiaExt[index].src0u.xyz = address;
	g_NvidiaExt[index].src1u.x = asuint(val);
	g_NvidiaExt[index].src2u.x = 3;
	g_NvidiaExt[index].opcode = 13;

	return asfloat(g_NvidiaExt[index].dst0u.x);
}

int NvShfl(int val, uint srcLane, int width = 32)
{
	uint index = g_NvidiaExt.IncrementCounter();
	g_NvidiaExt[index].src0u.x = val;
	g_NvidiaExt[index].src0u.y = srcLane;
	g_NvidiaExt[index].src0u.z = __NvGetShflMaskFromWidth(width);
	g_NvidiaExt[index].opcode = 1;

	return g_NvidiaExt.IncrementCounter();
}

int NvShflUp(int val, uint delta, int width = 32)
{
	uint index = g_NvidiaExt.IncrementCounter();
	g_NvidiaExt[index].src0u.x = val;
	g_NvidiaExt[index].src0u.y = delta;
	g_NvidiaExt[index].src0u.z = (32 - width) << 8;
	g_NvidiaExt[index].opcode = 2;
	return g_NvidiaExt.IncrementCounter();
}

int NvShflDown(int val, uint delta, int width = 32)
{
	uint index = g_NvidiaExt.IncrementCounter();
	g_NvidiaExt[index].src0u.x = val;
	g_NvidiaExt[index].src0u.y = delta;
	g_NvidiaExt[index].src0u.z = __NvGetShflMaskFromWidth(width);
	g_NvidiaExt[index].opcode = 3;
	return g_NvidiaExt.IncrementCounter();
}

int NvShflXor(int val, uint laneMask, int width = 32)
{
	uint index = g_NvidiaExt.IncrementCounter();
	g_NvidiaExt[index].src0u.x = val;
	g_NvidiaExt[index].src0u.y = laneMask;
	g_NvidiaExt[index].src0u.z = __NvGetShflMaskFromWidth(width);
	g_NvidiaExt[index].opcode = 4;
	return g_NvidiaExt.IncrementCounter();
}

uint NvAny(int predicate)
{
	uint index = g_NvidiaExt.IncrementCounter();
	g_NvidiaExt[index].src0u.x = predicate;
	g_NvidiaExt[index].opcode = 6;
	return g_NvidiaExt.IncrementCounter();
}

uint NvAll(int predicate)
{
	uint index = g_NvidiaExt.IncrementCounter();
	g_NvidiaExt[index].src0u.x = predicate;
	g_NvidiaExt[index].opcode = 5;
	return g_NvidiaExt.IncrementCounter();
}

uint NvBallot(int predicate)
{
	uint index = g_NvidiaExt.IncrementCounter();
	g_NvidiaExt[index].src0u.x = predicate;
	g_NvidiaExt[index].opcode = 7;
	return g_NvidiaExt.IncrementCounter();
}

int NvGetLaneId()
{
	uint index = g_NvidiaExt.IncrementCounter();
	g_NvidiaExt[index].opcode = 8;
	return g_NvidiaExt.IncrementCounter();
}

uint NvInterlockedAddFp16x2(RWByteAddressBuffer uav, uint byteAddress, uint fp16x2Val)
{
	return __NvAtomicOpFP16x2(uav, byteAddress, fp16x2Val, 3);
}

uint NvInterlockedMinFp16x2(RWByteAddressBuffer uav, uint byteAddress, uint fp16x2Val)
{
	return __NvAtomicOpFP16x2(uav, byteAddress, fp16x2Val, 7);
}

uint NvInterlockedMaxFp16x2(RWByteAddressBuffer uav, uint byteAddress, uint fp16x2Val)
{
	return __NvAtomicOpFP16x2(uav, byteAddress, fp16x2Val, 6);
}

uint NvInterlockedAddFp16x2(RWByteAddressBuffer uav, uint byteAddress, float2 val)
{
	return __NvAtomicOpFP16x2(uav, byteAddress, __fp32x2Tofp16x2(val), 3);
}

uint NvInterlockedMinFp16x2(RWByteAddressBuffer uav, uint byteAddress, float2 val)
{
	return __NvAtomicOpFP16x2(uav, byteAddress, __fp32x2Tofp16x2(val), 7);
}

uint NvInterlockedMaxFp16x2(RWByteAddressBuffer uav, uint byteAddress, float2 val)
{
	return __NvAtomicOpFP16x2(uav, byteAddress, __fp32x2Tofp16x2(val), 6);
}

uint NvInterlockedAddFp16x2(RWTexture1D<float2> uav, uint address, uint fp16x2Val)
{
	return __NvAtomicOpFP16x2(uav, address, fp16x2Val, 3);
}

uint NvInterlockedMinFp16x2(RWTexture1D<float2> uav, uint address, uint fp16x2Val)
{
	return __NvAtomicOpFP16x2(uav, address, fp16x2Val, 7);
}

uint NvInterlockedMaxFp16x2(RWTexture1D<float2> uav, uint address, uint fp16x2Val)
{
	return __NvAtomicOpFP16x2(uav, address, fp16x2Val, 6);
}

uint NvInterlockedAddFp16x2(RWTexture2D<float2> uav, uint2 address, uint fp16x2Val)
{
	return __NvAtomicOpFP16x2(uav, address, fp16x2Val, 3);
}

uint NvInterlockedMinFp16x2(RWTexture2D<float2> uav, uint2 address, uint fp16x2Val)
{
	return __NvAtomicOpFP16x2(uav, address, fp16x2Val, 7);
}

uint NvInterlockedMaxFp16x2(RWTexture2D<float2> uav, uint2 address, uint fp16x2Val)
{
	return __NvAtomicOpFP16x2(uav, address, fp16x2Val, 6);
}

uint NvInterlockedAddFp16x2(RWTexture3D<float2> uav, uint3 address, uint fp16x2Val)
{
	return __NvAtomicOpFP16x2(uav, address, fp16x2Val, 3);
}

uint NvInterlockedMinFp16x2(RWTexture3D<float2> uav, uint3 address, uint fp16x2Val)
{
	return __NvAtomicOpFP16x2(uav, address, fp16x2Val, 7);
}

uint NvInterlockedMaxFp16x2(RWTexture3D<float2> uav, uint3 address, uint fp16x2Val)
{
	return __NvAtomicOpFP16x2(uav, address, fp16x2Val, 6);
}

uint NvInterlockedAddFp16x2(RWTexture1D<float2> uav, uint address, float2 val)
{
	return __NvAtomicOpFP16x2(uav, address, __fp32x2Tofp16x2(val), 3);
}

uint NvInterlockedMinFp16x2(RWTexture1D<float2> uav, uint address, float2 val)
{
	return __NvAtomicOpFP16x2(uav, address, __fp32x2Tofp16x2(val), 7);
}

uint NvInterlockedMaxFp16x2(RWTexture1D<float2> uav, uint address, float2 val)
{
	return __NvAtomicOpFP16x2(uav, address, __fp32x2Tofp16x2(val), 6);
}

uint NvInterlockedAddFp16x2(RWTexture2D<float2> uav, uint2 address, float2 val)
{
	return __NvAtomicOpFP16x2(uav, address, __fp32x2Tofp16x2(val), 3);
}

uint NvInterlockedMinFp16x2(RWTexture2D<float2> uav, uint2 address, float2 val)
{
	return __NvAtomicOpFP16x2(uav, address, __fp32x2Tofp16x2(val), 7);
}

uint NvInterlockedMaxFp16x2(RWTexture2D<float2> uav, uint2 address, float2 val)
{
	return __NvAtomicOpFP16x2(uav, address, __fp32x2Tofp16x2(val), 6);
}

uint NvInterlockedAddFp16x2(RWTexture3D<float2> uav, uint3 address, float2 val)
{
	return __NvAtomicOpFP16x2(uav, address, __fp32x2Tofp16x2(val), 3);
}

uint NvInterlockedMinFp16x2(RWTexture3D<float2> uav, uint3 address, float2 val)
{
	return __NvAtomicOpFP16x2(uav, address, __fp32x2Tofp16x2(val), 7);
}

uint NvInterlockedMaxFp16x2(RWTexture3D<float2> uav, uint3 address, float2 val)
{
	return __NvAtomicOpFP16x2(uav, address, __fp32x2Tofp16x2(val), 6);
}

uint2 NvInterlockedAddFp16x4(RWTexture1D<float4> uav, uint address, uint2 fp16x2Val)
{
	return __NvAtomicOpFP16x2(uav, address, fp16x2Val, 3);
}

uint2 NvInterlockedMinFp16x4(RWTexture1D<float4> uav, uint address, uint2 fp16x2Val)
{
	return __NvAtomicOpFP16x2(uav, address, fp16x2Val, 7);
}

uint2 NvInterlockedMaxFp16x4(RWTexture1D<float4> uav, uint address, uint2 fp16x2Val)
{
	return __NvAtomicOpFP16x2(uav, address, fp16x2Val, 6);
}

uint2 NvInterlockedAddFp16x4(RWTexture2D<float4> uav, uint2 address, uint2 fp16x2Val)
{
	return __NvAtomicOpFP16x2(uav, address, fp16x2Val, 3);
}

uint2 NvInterlockedMinFp16x4(RWTexture2D<float4> uav, uint2 address, uint2 fp16x2Val)
{
	return __NvAtomicOpFP16x2(uav, address, fp16x2Val, 7);
}

uint2 NvInterlockedMaxFp16x4(RWTexture2D<float4> uav, uint2 address, uint2 fp16x2Val)
{
	return __NvAtomicOpFP16x2(uav, address, fp16x2Val, 6);
}

uint2 NvInterlockedAddFp16x4(RWTexture3D<float4> uav, uint3 address, uint2 fp16x2Val)
{
	return __NvAtomicOpFP16x2(uav, address, fp16x2Val, 3);
}

uint2 NvInterlockedMinFp16x4(RWTexture3D<float4> uav, uint3 address, uint2 fp16x2Val)
{
	return __NvAtomicOpFP16x2(uav, address, fp16x2Val, 7);
}

uint2 NvInterlockedMaxFp16x4(RWTexture3D<float4> uav, uint3 address, uint2 fp16x2Val)
{
	return __NvAtomicOpFP16x2(uav, address, fp16x2Val, 6);
}

uint2 NvInterlockedAddFp16x4(RWTexture1D<float4> uav, uint address, float4 val)
{
	return __NvAtomicOpFP16x2(uav, address, __fp32x4Tofp16x4(val), 3);
}

uint2 NvInterlockedMinFp16x4(RWTexture1D<float4> uav, uint address, float4 val)
{
	return __NvAtomicOpFP16x2(uav, address, __fp32x4Tofp16x4(val), 7);
}

uint2 NvInterlockedMaxFp16x4(RWTexture1D<float4> uav, uint address, float4 val)
{
	return __NvAtomicOpFP16x2(uav, address, __fp32x4Tofp16x4(val), 6);
}

uint2 NvInterlockedAddFp16x4(RWTexture2D<float4> uav, uint2 address, float4 val)
{
	return __NvAtomicOpFP16x2(uav, address, __fp32x4Tofp16x4(val), 3);
}

uint2 NvInterlockedMinFp16x4(RWTexture2D<float4> uav, uint2 address, float4 val)
{
	return __NvAtomicOpFP16x2(uav, address, __fp32x4Tofp16x4(val), 7);
}

uint2 NvInterlockedMaxFp16x4(RWTexture2D<float4> uav, uint2 address, float4 val)
{
	return __NvAtomicOpFP16x2(uav, address, __fp32x4Tofp16x4(val), 6);
}

uint2 NvInterlockedAddFp16x4(RWTexture3D<float4> uav, uint3 address, float4 val)
{
	return __NvAtomicOpFP16x2(uav, address, __fp32x4Tofp16x4(val), 3);
}

uint2 NvInterlockedMinFp16x4(RWTexture3D<float4> uav, uint3 address, float4 val)
{
	return __NvAtomicOpFP16x2(uav, address, __fp32x4Tofp16x4(val), 7);
}

uint2 NvInterlockedMaxFp16x4(RWTexture3D<float4> uav, uint3 address, float4 val)
{
	return __NvAtomicOpFP16x2(uav, address, __fp32x4Tofp16x4(val), 6);
}

float NvInterlockedAddFp32(RWByteAddressBuffer uav, uint byteAddress, float val)
{
	return __NvAtomicAddFP32(uav, byteAddress, val);
}

float NvInterlockedAddFp32(RWTexture1D<float> uav, uint address, float val)
{
	return __NvAtomicAddFP32(uav, address, val);
}

float NvInterlockedAddFp32(RWTexture2D<float> uav, uint2 address, float val)
{
	return __NvAtomicAddFP32(uav, address, val);
}

float NvInterlockedAddFp32(RWTexture3D<float> uav, uint3 address, float val)
{
	return __NvAtomicAddFP32(uav, address, val);
}

float4 NvLoadUavTyped(RWTexture1D<float4> uav, uint address)
{
	__NvReferenceUAVForOp(uav);

	uint index = g_NvidiaExt.IncrementCounter();
	g_NvidiaExt[index].src0u.x = address;
	g_NvidiaExt[index].opcode = 14;

	return asfloat(g_NvidiaExt[index].dst0u);
}

float4 NvLoadUavTyped(RWTexture2D<float4> uav, uint2 address)
{
	__NvReferenceUAVForOp(uav);

	uint index = g_NvidiaExt.IncrementCounter();
	g_NvidiaExt[index].src0u.xy = address;
	g_NvidiaExt[index].opcode = 14;

	return asfloat(g_NvidiaExt[index].dst0u);
}

float4 NvLoadUavTyped(RWTexture3D<float4> uav, uint3 address)
{
	__NvReferenceUAVForOp(uav);

	uint index = g_NvidiaExt.IncrementCounter();
	g_NvidiaExt[index].src0u.xyz = address;
	g_NvidiaExt[index].opcode = 14;

	return asfloat(g_NvidiaExt[index].dst0u);
}

float2 NvLoadUavTyped(RWTexture1D<float2> uav, uint address)
{
	__NvReferenceUAVForOp(uav);

	uint index = g_NvidiaExt.IncrementCounter();
	g_NvidiaExt[index].src0u.x = address;
	g_NvidiaExt[index].opcode = 14;

	return asfloat(g_NvidiaExt[index].dst0u.xy);
}

float2 NvLoadUavTyped(RWTexture2D<float2> uav, uint2 address)
{
	__NvReferenceUAVForOp(uav);

	uint index = g_NvidiaExt.IncrementCounter();
	g_NvidiaExt[index].src0u.xy = address;
	g_NvidiaExt[index].opcode = 14;

	return asfloat(g_NvidiaExt[index].dst0u.xy);
}

float2 NvLoadUavTyped(RWTexture3D<float2> uav, uint3 address)
{
	__NvReferenceUAVForOp(uav);

	uint index = g_NvidiaExt.IncrementCounter();
	g_NvidiaExt[index].src0u.xyz = address;
	g_NvidiaExt[index].opcode = 14;

	return asfloat(g_NvidiaExt[index].dst0u.xy);
}

float NvLoadUavTyped(RWTexture1D<float> uav, uint address)
{
	__NvReferenceUAVForOp(uav);

	uint index = g_NvidiaExt.IncrementCounter();
	g_NvidiaExt[index].src0u.x = address;
	g_NvidiaExt[index].opcode = 14;

	return asfloat(g_NvidiaExt[index].dst0u.x);
}

float NvLoadUavTyped(RWTexture2D<float> uav, uint2 address)
{
	__NvReferenceUAVForOp(uav);

	uint index = g_NvidiaExt.IncrementCounter();
	g_NvidiaExt[index].src0u.xy = address;
	g_NvidiaExt[index].opcode = 14;

	return asfloat(g_NvidiaExt[index].dst0u.x);
}

float NvLoadUavTyped(RWTexture3D<float> uav, uint3 address)
{
	__NvReferenceUAVForOp(uav);

	uint index = g_NvidiaExt.IncrementCounter();
	g_NvidiaExt[index].src0u.xyz = address;
	g_NvidiaExt[index].opcode = 14;

	return asfloat(g_NvidiaExt[index].dst0u.x);
}

uint4 NvLoadUavTyped(RWTexture1D<uint4> uav, uint address)
{
	__NvReferenceUAVForOp(uav);

	uint index = g_NvidiaExt.IncrementCounter();
	g_NvidiaExt[index].src0u.x = address;
	g_NvidiaExt[index].opcode = 14;

	return (g_NvidiaExt[index].dst0u);
}

uint4 NvLoadUavTyped(RWTexture2D<uint4> uav, uint2 address)
{
	__NvReferenceUAVForOp(uav);

	uint index = g_NvidiaExt.IncrementCounter();
	g_NvidiaExt[index].src0u.xy = address;
	g_NvidiaExt[index].opcode = 14;

	return (g_NvidiaExt[index].dst0u);
}

uint4 NvLoadUavTyped(RWTexture3D<uint4> uav, uint3 address)
{
	__NvReferenceUAVForOp(uav);

	uint index = g_NvidiaExt.IncrementCounter();
	g_NvidiaExt[index].src0u.xyz = address;
	g_NvidiaExt[index].opcode = 14;

	return (g_NvidiaExt[index].dst0u);
}

uint2 NvLoadUavTyped(RWTexture1D<uint2> uav, uint address)
{
	__NvReferenceUAVForOp(uav);

	uint index = g_NvidiaExt.IncrementCounter();
	g_NvidiaExt[index].src0u.x = address;
	g_NvidiaExt[index].opcode = 14;

	return (g_NvidiaExt[index].dst0u.xy);
}

uint2 NvLoadUavTyped(RWTexture2D<uint2> uav, uint2 address)
{
	__NvReferenceUAVForOp(uav);

	uint index = g_NvidiaExt.IncrementCounter();
	g_NvidiaExt[index].src0u.xy = address;
	g_NvidiaExt[index].opcode = 14;

	return (g_NvidiaExt[index].dst0u.xy);
}

uint2 NvLoadUavTyped(RWTexture3D<uint2> uav, uint3 address)
{
	__NvReferenceUAVForOp(uav);

	uint index = g_NvidiaExt.IncrementCounter();
	g_NvidiaExt[index].src0u.xyz = address;
	g_NvidiaExt[index].opcode = 14;

	return (g_NvidiaExt[index].dst0u.xy);
}

uint NvLoadUavTyped(RWTexture1D<uint> uav, uint address)
{
	__NvReferenceUAVForOp(uav);

	uint index = g_NvidiaExt.IncrementCounter();
	g_NvidiaExt[index].src0u.x = address;
	g_NvidiaExt[index].opcode = 14;

	return (g_NvidiaExt[index].dst0u.x);
}

uint NvLoadUavTyped(RWTexture2D<uint> uav, uint2 address)
{
	__NvReferenceUAVForOp(uav);

	uint index = g_NvidiaExt.IncrementCounter();
	g_NvidiaExt[index].src0u.xy = address;
	g_NvidiaExt[index].opcode = 14;

	return (g_NvidiaExt[index].dst0u.x);
}

uint NvLoadUavTyped(RWTexture3D<uint> uav, uint3 address)
{
	__NvReferenceUAVForOp(uav);

	uint index = g_NvidiaExt.IncrementCounter();
	g_NvidiaExt[index].src0u.xyz = address;
	g_NvidiaExt[index].opcode = 14;

	return (g_NvidiaExt[index].dst0u.x);
}

int4 NvLoadUavTyped(RWTexture1D<int4> uav, uint address)
{
	__NvReferenceUAVForOp(uav);

	uint index = g_NvidiaExt.IncrementCounter();
	g_NvidiaExt[index].src0u.x = address;
	g_NvidiaExt[index].opcode = 14;

	return asint(g_NvidiaExt[index].dst0u);
}

int4 NvLoadUavTyped(RWTexture2D<int4> uav, uint2 address)
{
	__NvReferenceUAVForOp(uav);

	uint index = g_NvidiaExt.IncrementCounter();
	g_NvidiaExt[index].src0u.xy = address;
	g_NvidiaExt[index].opcode = 14;

	return asint(g_NvidiaExt[index].dst0u);
}

int4 NvLoadUavTyped(RWTexture3D<int4> uav, uint3 address)
{
	__NvReferenceUAVForOp(uav);

	uint index = g_NvidiaExt.IncrementCounter();
	g_NvidiaExt[index].src0u.xyz = address;
	g_NvidiaExt[index].opcode = 14;

	return asint(g_NvidiaExt[index].dst0u);
}

int2 NvLoadUavTyped(RWTexture1D<int2> uav, uint address)
{
	__NvReferenceUAVForOp(uav);

	uint index = g_NvidiaExt.IncrementCounter();
	g_NvidiaExt[index].src0u.x = address;
	g_NvidiaExt[index].opcode = 14;

	return asint(g_NvidiaExt[index].dst0u.xy);
}

int2 NvLoadUavTyped(RWTexture2D<int2> uav, uint2 address)
{
	__NvReferenceUAVForOp(uav);

	uint index = g_NvidiaExt.IncrementCounter();
	g_NvidiaExt[index].src0u.xy = address;
	g_NvidiaExt[index].opcode = 14;

	return asint(g_NvidiaExt[index].dst0u.xy);
}

int2 NvLoadUavTyped(RWTexture3D<int2> uav, uint3 address)
{
	__NvReferenceUAVForOp(uav);

	uint index = g_NvidiaExt.IncrementCounter();
	g_NvidiaExt[index].src0u.xyz = address;
	g_NvidiaExt[index].opcode = 14;

	return asint(g_NvidiaExt[index].dst0u.xy);
}

int NvLoadUavTyped(RWTexture1D<int> uav, uint address)
{
	__NvReferenceUAVForOp(uav);

	uint index = g_NvidiaExt.IncrementCounter();
	g_NvidiaExt[index].src0u.x = address;
	g_NvidiaExt[index].opcode = 14;

	return asint(g_NvidiaExt[index].dst0u.x);
}

int NvLoadUavTyped(RWTexture2D<int> uav, uint2 address)
{
	__NvReferenceUAVForOp(uav);

	uint index = g_NvidiaExt.IncrementCounter();
	g_NvidiaExt[index].src0u.xy = address;
	g_NvidiaExt[index].opcode = 14;

	return asint(g_NvidiaExt[index].dst0u.x);
}

int NvLoadUavTyped(RWTexture3D<int> uav, uint3 address)
{
	__NvReferenceUAVForOp(uav);

	uint index = g_NvidiaExt.IncrementCounter();
	g_NvidiaExt[index].src0u.xyz = address;
	g_NvidiaExt[index].opcode = 14;

	return asint(g_NvidiaExt[index].dst0u.x);
}

#pragma pack_matrix(row_major)
static const float VxgiPI = 3.14159265;

float2 float2scalar(float x) { return float2(x, x); }
float3 float3scalar(float x) { return float3(x, x, x); }
float4 float4scalar(float x) { return float4(x, x, x, x); }
int2 int2scalar(int x) { return int2(x, x); }
int3 int3scalar(int x) { return int3(x, x, x); }
int4 int4scalar(int x) { return int4(x, x, x, x); }
uint2 uint2scalar(uint x) { return uint2(x, x); }
uint3 uint3scalar(uint x) { return uint3(x, x, x); }
uint4 uint4scalar(uint x) { return uint4(x, x, x, x); }

struct VxgiFullScreenQuadOutput
{
	float2 uv : TEXCOORD;
	float4 posProj : RAY;
	float instanceID : INSTANCEID;
};

float3 VxgiProjToRay(float4 posProj, float3 cameraPos, float4x4 viewProjMatrixInv)
{
	float4 farPoint = mul(posProj, viewProjMatrixInv);
	farPoint.xyz /= farPoint.w;

	return normalize(farPoint.xyz - cameraPos.xyz);
}

struct VxgiBox4i
{
	int4 lower;
	int4 upper;
};

struct VxgiBox4f
{
	float4 lower;
	float4 upper;
};

bool VxgiPartOfRegion(int3 coords, VxgiBox4i region)
{
	return coords.x <= region.upper.x && coords.y <= region.upper.y && coords.z <= region.upper.z &&
		coords.x >= region.lower.x && coords.y >= region.lower.y && coords.z >= region.lower.z;
}

bool VxgiPartOfRegionWithBorder(int3 coords, VxgiBox4i region, int border)
{
	return coords.x <= region.upper.x + border && coords.y <= region.upper.y + border && coords.z <= region.upper.z + border &&
		coords.x >= region.lower.x - border && coords.y >= region.lower.y - border && coords.z >= region.lower.z - border;
}

bool VxgiPartOfRegionf(float3 coords, float3 lower, float3 upper)
{
	return coords.x <= upper.x && coords.y <= upper.y && coords.z <= upper.z &&
		coords.x >= lower.x && coords.y >= lower.y && coords.z >= lower.z;
}

float VxgiToSRGB(float x)
{
	x = saturate(x);
	return x <= 0.0031308f ? x * 12.92f : saturate(1.055f * pow(x, 0.4166667f) - 0.055f);
}

float VxgiFromSRGB(float x)
{
	return x <= 0.04045f ? saturate(x * 0.0773994f) : pow(saturate(x * 0.9478672f + 0.0521327f), 2.4f);
}

float4 VxgiPackEmittance(float4 v)
{
	return float4(v);
}

float4 VxgiUnpackEmittance(float4 n)
{
	return float4(n.rgba);
}

uint2 VxgiPackEmittanceForAtomic(float4 v)
{

	uint4 parts = f32tof16(v);
	return uint2(parts.x | (parts.y << 16), parts.z | (parts.w << 16));
}

uint VxgiPackOpacity(float3 opacity)
{
	return uint(1023 * opacity.x) | (uint(1023 * opacity.y) << 10) | (uint(1023 * opacity.z) << 20);
}

float4 VxgiUnpackOpacity(uint opacity)
{
	float4 fOpacity;
	fOpacity.x = float((opacity) & 0x3ff) / 1023.0;
	fOpacity.y = float((opacity >> 10) & 0x3ff) / 1023.0;
	fOpacity.z = float((opacity >> 20) & 0x3ff) / 1023.0;
	fOpacity.w = 0;
	return fOpacity;
}

float VxgiGetNormalProjection(float3 normal, uint direction)
{
	float normalProjection = 0;

	switch (direction)
	{
	case 0:
		normalProjection = saturate(normal.x);
		break;
	case 3:
		normalProjection = saturate(-normal.x);
		break;
	case 1:
		normalProjection = saturate(normal.y);
		break;
	case 4:
		normalProjection = saturate(-normal.y);
		break;
	case 2:
		normalProjection = saturate(normal.z);
		break;
	case 5:
		normalProjection = saturate(-normal.z);
		break;
	}

	return normalProjection;
}

float VxgiAverage4(float a, float b, float c, float d)
{
	return (a + b + c + d) / 4;
}

float VxgiMultiplyComplements(float a, float b)
{
	return 1 - (1 - a) * (1 - b);
}

bool VxgiIsOdd(int x)
{
	return (x & 1) != 0;
}

struct VxgiVoxelizationConstants
{
	float4 GridCenter;
	float4 GridCenterPrevious;
	int4 ToroidalOffset;
	VxgiBox4f ScissorRegionsClipSpace[5];
	int4 TextureToAmapTranslation[5];
	float IrradianceMapSize;
	uint ClipLevelSize;
	uint PackingStride;
	uint AllocationMapSize;
	uint ClipLevelMask;
	uint MaxClipLevel;
	int FirstLevelToDiscard;
	float DiscardLower;
	float DiscardUpper;
	float DiscardClipSpace;
	uint UseCullFunction;
	float EmittanceStorageScale;
	uint UseIrradianceMap;
	uint Use6DOpacity;
	uint UseFP32Emittance;
	uint PersistentVoxelData;
	uint UseInvalidateBitmap;
};

struct VxgiVoxelizationMaterialConstants
{
	float4 ResolutionFactors[5];
	float NoiseScale;
	float NoiseBias;
	int TwoSided;
	int ProportionalEmittance;
	int DepthSamples;
	int FrontCCW;
	int OmnidirectionalLight;
};

cbuffer VoxelizationCB : REGISTER(b, VXGI_VOXELIZE_CB_SLOT)
{
	VxgiVoxelizationConstants g_VxgiVoxelizationCB;
};

cbuffer VoxelizationMaterialCB : REGISTER(b, VXGI_VOXELIZE_MATERIAL_CB_SLOT)
{
	VxgiVoxelizationMaterialConstants g_VxgiVoxelizationMaterialCB;
};

struct VxgiVoxelizationGSOutputData
{
	nointerpolation uint gl_ViewportIndex : SV_ViewportArrayIndex;
};

struct VxgiVoxelizationPSInputData
{
	VxgiVoxelizationGSOutputData GSData;
	float4 gl_FragCoord : SV_Position;
	uint gl_SampleMaskIn : SV_Coverage;
	bool gl_FrontFacing : SV_IsFrontFace;
};

struct VxgiScissorStats
{
	uint pixelsRasterized;
	uint pixelsKilledByZViewport;
	uint pixelsKilledByScissorBox;
	uint pixelsKilledByFinerLOD;
	uint voxelsWithNoCoverage;
	uint voxelsKilledByScissorBox;
	uint voxelsKilledByAllocationMap;
};

static const float2 g_VxgiSamplePositions[] = {
	float2(0.0625, -0.1875), float2(-0.0625, 0.1875), float2(0.3125, 0.0625), float2(-0.1875, -0.3125),
	float2(-0.3125, 0.3125), float2(-0.4375, 0.0625), float2(0.1875, 0.4375), float2(0.4375, -0.4375),
	float2(0, 0) };

static const float g_VxgiSampleZOffsets[] = {
	0.9, 0.4, 0.5, 0.6, 0.1, 0.8, 0.7, 0.3, 0.2 };

bool VxgiIsVoxelInDiscardArea(float3 voxelCoordinates, int clipLevel)
{

	if (clipLevel >= g_VxgiVoxelizationCB.FirstLevelToDiscard && all((voxelCoordinates >= float3scalar(g_VxgiVoxelizationCB.DiscardLower))) && all((voxelCoordinates <= float3scalar(g_VxgiVoxelizationCB.DiscardUpper))))
	{
		;
		return true;
	}
	return false;
}

static const int VxgiIsEmissiveVoxelizationPass = 1;

RWTexture3D<uint> u_AllocationMap : REGISTER(u, VXGI_ALLOCATION_MAP_UAV_SLOT);

RWTexture3D<float4> u_EmittanceEven : REGISTER(u, VXGI_VOXELTEX_0_UAV_SLOT);
RWTexture3D<float4> u_EmittanceOdd : REGISTER(u, VXGI_VOXELTEX_3_UAV_SLOT);
RWTexture3D<uint> u_Opacity_Pos : REGISTER(u, VXGI_VOXELTEX_1_UAV_SLOT);

RWStructuredBuffer<VxgiScissorStats> u_ScissorStats : REGISTER(u, VXGI_SCISSOR_STATS_UAV_SLOT);

SamplerState s_IrradianceMapSampler : REGISTER(s, VXGI_IRRADIANCE_MAP_SAMPLER_SLOT);
Texture3D<float4> t_IrradianceMap : REGISTER(t, VXGI_IRRADIANCE_MAP_SRV_SLOT);

float3 VxgiGetLayeredEmittance(float inputZ, uint coverage)
{
	float3 layeredEmittance = float3(0, 0, 0);

	float2 dZ = float2(ddx(inputZ), ddy(inputZ));
	float fracZ = frac(inputZ);

	[unroll] for (int nSample = 0; nSample < 8; nSample++)
	{
		if ((coverage & (1 << nSample)) != 0)
		{
			float relativeZ = fracZ + dot(dZ, g_VxgiSamplePositions[nSample]);
			const float delta = 1.0f / 8;

			if (g_VxgiVoxelizationMaterialCB.ProportionalEmittance != 0)
			{
				layeredEmittance.x += delta * saturate(1 - abs(relativeZ + 0.5));
				layeredEmittance.y += delta * saturate(1 - abs(relativeZ - 0.5));
				layeredEmittance.z += delta * saturate(1 - abs(relativeZ - 1.5));
			}
			else
			{
				if (relativeZ < 0)
					layeredEmittance.x += delta;
				else if (relativeZ >= 1)
					layeredEmittance.z += delta;
				else
					layeredEmittance.y += delta;
			}
		}
	}

	return layeredEmittance;
}

void VxgiWriteEmittance(int3 coordinates, int clipLevel, float perVoxelScale, float3 emittance, float3 normal, inout int prevPageCoordinatesHash, inout uint page)
{
	emittance = max(emittance * perVoxelScale, 0);

	if (!any((emittance != float3(0, 0, 0))))
	{
		;
		return;
	}

	if (((coordinates.x | coordinates.y | coordinates.z) & ~(g_VxgiVoxelizationCB.ClipLevelSize - 1)) != 0)
	{
		return;
	}

	{
		int4 translation = g_VxgiVoxelizationCB.TextureToAmapTranslation[clipLevel];
		int3 pageCoordinates = int3(((coordinates >> translation.w) + translation.xyz) & (g_VxgiVoxelizationCB.AllocationMapSize - 1));
		int pageCoordinatesHash = pageCoordinates.x + pageCoordinates.y + pageCoordinates.z;

		if (pageCoordinatesHash != prevPageCoordinatesHash)
			page = u_AllocationMap[pageCoordinates].x;

		prevPageCoordinatesHash = pageCoordinatesHash;

		if ((page & 0x08) == 0)
			return;

		if ((page & 0x02) == 0)
		{
			page |= 0x02;
			(u_AllocationMap[pageCoordinates] = page);
		}
	}

	int3 voxelCoords = int3(((coordinates)+(g_VxgiVoxelizationCB.ToroidalOffset.xyz >> clipLevel)) & ((g_VxgiVoxelizationCB.ClipLevelSize) - 1));
	int3 address = voxelCoords + int3(1, 0, int(g_VxgiVoxelizationCB.PackingStride) * (clipLevel >> 1) + 1);

	emittance.rgb *= g_VxgiVoxelizationCB.EmittanceStorageScale;

	bool omni = bool(g_VxgiVoxelizationMaterialCB.OmnidirectionalLight) || all((normal == float3(0, 0, 0)));

	[unroll] for (uint direction = 0; direction < 6; ++direction)
	{
		float multiplier = omni ? rcp(6.0) : VxgiGetNormalProjection(normal, direction);

		if (multiplier > 0)
		{
			uint2 packedEmittance = VxgiPackEmittanceForAtomic(float4(emittance * multiplier, 0));

			if (VxgiIsOdd(clipLevel))
				NvInterlockedAddFp16x4(u_EmittanceOdd, address, packedEmittance);
			else
				NvInterlockedAddFp16x4(u_EmittanceEven, address, packedEmittance);
		}

		address.x += int(g_VxgiVoxelizationCB.PackingStride);
	}

	(u_Opacity_Pos[voxelCoords + int3(0, 0, int(g_VxgiVoxelizationCB.PackingStride) * clipLevel + 1)] = u_Opacity_Pos[voxelCoords + int3(0, 0, int(g_VxgiVoxelizationCB.PackingStride) * clipLevel + 1)] | 0xC0000000u);
}

void VoxelizeEmittancePS(VxgiVoxelizationPSInputData IN, float3 emissiveColor)
{
	;

	bool frontFacing = IN.gl_FrontFacing;
	if (g_VxgiVoxelizationMaterialCB.FrontCCW != 0)
		frontFacing = !frontFacing;

	float fVP = float(IN.GSData.gl_ViewportIndex) * 0.334;
	int projectionIndex = int(floor(frac(fVP) * 3));
	int clipLevel = int(floor(fVP));

	float inputZ = IN.gl_FragCoord.z;

	float unscaledZ = inputZ;

	inputZ *= (1 << (g_VxgiVoxelizationCB.MaxClipLevel - clipLevel));
	inputZ = inputZ * 0.5 + 0.5;
	inputZ *= g_VxgiVoxelizationCB.ClipLevelSize;

	const float DesnapQuantum = 0.0078125;
	if (abs(inputZ - round(inputZ)) < DesnapQuantum)
		inputZ += 2 * DesnapQuantum;

	float resolutionFactor = g_VxgiVoxelizationMaterialCB.ResolutionFactors[clipLevel].x;
	float rResolutionFactor = g_VxgiVoxelizationMaterialCB.ResolutionFactors[clipLevel].y;

	float3 texelSpace = float3(IN.gl_FragCoord.xy, inputZ);
	texelSpace.xy *= rResolutionFactor;

	texelSpace.y = g_VxgiVoxelizationCB.ClipLevelSize - texelSpace.y;

	if (VxgiIsVoxelInDiscardArea(texelSpace, clipLevel))
	{
		discard;
		return;
	}

	int3 offsetDir;
	bool kill = false;

	if (projectionIndex == 0)
	{
		kill = unscaledZ < g_VxgiVoxelizationCB.ScissorRegionsClipSpace[clipLevel].lower.z || unscaledZ > g_VxgiVoxelizationCB.ScissorRegionsClipSpace[clipLevel].upper.z;

		offsetDir = int3(0, 0, 1);
	}
	else if (projectionIndex == 1)
	{
		kill = unscaledZ < g_VxgiVoxelizationCB.ScissorRegionsClipSpace[clipLevel].lower.y || unscaledZ > g_VxgiVoxelizationCB.ScissorRegionsClipSpace[clipLevel].upper.y;

		texelSpace.zxy = texelSpace.xyz;
		offsetDir = int3(0, 1, 0);
	}
	else
	{
		kill = unscaledZ < g_VxgiVoxelizationCB.ScissorRegionsClipSpace[clipLevel].lower.x || unscaledZ > g_VxgiVoxelizationCB.ScissorRegionsClipSpace[clipLevel].upper.x;

		texelSpace.yzx = texelSpace.xyz;
		offsetDir = int3(1, 0, 0);
	}

	if (kill)
	{
		discard;
		return;
	}

	int3 voxelCoordinates = int3(floor(texelSpace));

	uint coverage = uint(IN.gl_SampleMaskIn);

	float3 layeredEmittance = VxgiGetLayeredEmittance(inputZ, coverage);

	float3 normal = normalize(cross(ddx(texelSpace), ddy(texelSpace)));
	if (!frontFacing)
		normal = -normal;

	float areaScale = rcp(max(abs(normal.x), max(abs(normal.y), abs(normal.z))));

	areaScale *= rResolutionFactor * rResolutionFactor;

	int pageCoordinatesHash = -1;
	uint page = 0;

	voxelCoordinates -= offsetDir;

	[loop] for (uint voxel = 0; voxel < 3; voxel++)
	{
		VxgiWriteEmittance(voxelCoordinates, clipLevel, layeredEmittance.x * areaScale, emissiveColor, normal, pageCoordinatesHash, page);
		voxelCoordinates += offsetDir;
		layeredEmittance.xy = layeredEmittance.yz;
	}

	discard;
}

float4 VxgiNormalizeIrradiance(float4 irradiance)
{
	if (irradiance.a <= 0)
		return float4scalar(0);

	return float4(irradiance.rgb / irradiance.a, 1);
}

float3 VxgiGetIndirectIrradiance(float3 worldPos, float3 normal)
{
	if (bool(g_VxgiVoxelizationCB.UseIrradianceMap))
	{
		float3 relativePos = (worldPos - g_VxgiVoxelizationCB.GridCenterPrevious.xyz) * g_VxgiVoxelizationCB.GridCenterPrevious.w;

		float3 validities = saturate(g_VxgiVoxelizationCB.IrradianceMapSize * (float3scalar(1.0) - 2 * abs(relativePos.xyz)));
		float validity = validities.x * validities.y * validities.z;

		if (validity == 0)
			return float3scalar(0.0);

		relativePos += 0.5;

		const float zstep = rcp(6.0);
		relativePos.z *= zstep;

		float4 irradianceX = t_IrradianceMap.SampleLevel(s_IrradianceMapSampler, relativePos + float3(0, 0, zstep * (normal.x > 0 ? 0 : 3)), 0);
		float4 irradianceY = t_IrradianceMap.SampleLevel(s_IrradianceMapSampler, relativePos + float3(0, 0, zstep * (normal.y > 0 ? 1 : 4)), 0);
		float4 irradianceZ = t_IrradianceMap.SampleLevel(s_IrradianceMapSampler, relativePos + float3(0, 0, zstep * (normal.z > 0 ? 2 : 5)), 0);

		return (VxgiNormalizeIrradiance(irradianceX).rgb * abs(normal.x) + VxgiNormalizeIrradiance(irradianceY).rgb * abs(normal.y) + VxgiNormalizeIrradiance(irradianceZ).rgb * abs(normal.z)) * validity;
	}

	return float3scalar(0);
}

void VxgiStoreVoxelizationData(VxgiVoxelizationPSInputData inputData, float3 emissiveColor)
{
	VoxelizeEmittancePS(inputData, emissiveColor);
}

//////////////////APP CODE BOUNDARY/////////////
/*
 * Copyright (c) 2012-2016, NVIDIA CORPORATION. All rights reserved.
 *
 * NVIDIA CORPORATION and its licensors retain all intellectual property
 * and proprietary rights in and to this software, related documentation
 * and any modifications thereto. Any use, reproduction, disclosure or
 * distribution of this software and related documentation without an express
 * license agreement from NVIDIA CORPORATION is strictly prohibited.
 */

struct PSInput
{
	float4 position : SV_Position;
	float2 texCoord : TEXCOORD;
	float3 normal : NORMAL;
	float3 tangent : TANGENT;
	float3 binormal : BINORMAL;
	float3 positionWS : WSPOSITION;
	VxgiVoxelizationPSInputData vxgiData;
};

cbuffer GlobalConstants : register(b0)
{
	float4x4 g_ViewProjMatrix;
	float4x4 g_ViewProjMatrixInv;
	float4x4 g_LightViewProjMatrix;
	float4 g_LightDirection;
	float4 g_DiffuseColor;
	float4 g_LightColor;
	float g_rShadowMapSize;
	uint g_EnableIndirectDiffuse;
	uint g_EnableIndirectSpecular;
};

Texture2D<float4> t_DiffuseColor : register(t0);
Texture2D t_ShadowMap : register(t1);
SamplerState g_SamplerLinearWrap : register(s0);
SamplerComparisonState g_SamplerComparison : register(s1);

static const float PI = 3.14159265;

float GetShadowFast(float3 fragmentPos)
{
	float4 clipPos = mul(float4(fragmentPos, 1.0f), g_LightViewProjMatrix);

	// Early out
	if (abs(clipPos.x) > clipPos.w || abs(clipPos.y) > clipPos.w || clipPos.z <= 0)
	{
		return 0;
	}

	clipPos.xyz /= clipPos.w;
	clipPos.x = clipPos.x * 0.5f + 0.5f;
	clipPos.y = 0.5f - clipPos.y * 0.5f;

	return t_ShadowMap.SampleCmpLevelZero(g_SamplerComparison, clipPos.xy, clipPos.z);
}

void main(PSInput IN)
{
	if (VxgiIsEmissiveVoxelizationPass)
	{
		float3 worldPos = IN.positionWS.xyz;
		float3 normal = normalize(IN.normal.xyz);

		float3 albedo = g_DiffuseColor.rgb;

		if (g_DiffuseColor.a > 0)
			albedo = t_DiffuseColor.Sample(g_SamplerLinearWrap, IN.texCoord.xy).rgb;

		float NdotL = saturate(-dot(normal, g_LightDirection.xyz));

		float3 radiosity = 0;

		if (NdotL > 0)
		{
			float shadow = GetShadowFast(worldPos);

			radiosity += albedo.rgb * g_LightColor.rgb * (NdotL * shadow);
		}

		radiosity += albedo.rgb * VxgiGetIndirectIrradiance(worldPos, normal) / PI;

		VxgiStoreVoxelizationData(IN.vxgiData, radiosity);
	}
	else
	{
		VxgiStoreVoxelizationData(IN.vxgiData, 0);
	}
};
//////////////////APP CODE BOUNDARY/////////////
