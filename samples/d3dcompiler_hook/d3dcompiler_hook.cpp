#include <sdkddkver.h>
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <d3dcompiler.h>

static HRESULT(WINAPI* g_pFn_Original_D3DCompile)(LPCVOID pSrcData, SIZE_T SrcDataSize, LPCSTR pSourceName, CONST D3D_SHADER_MACRO* pDefines, ID3DInclude* pInclude, LPCSTR pEntrypoint, LPCSTR pTarget, UINT Flags1, UINT Flags2, ID3DBlob** ppCode, ID3DBlob** ppErrorMsgs) = NULL;
static HRESULT(WINAPI* g_pFn_Original_D3DReflect)(LPCVOID pSrcData, SIZE_T SrcDataSize, REFIID pInterface, void** ppReflector) = NULL;

static struct Initialize_Original_Function_Pointers
{
    Initialize_Original_Function_Pointers()
    {
        HMODULE hModule = LoadLibraryExW(D3DCOMPILER_DLL_W, NULL, 0U);
        g_pFn_Original_D3DCompile = reinterpret_cast<HRESULT(WINAPI*)(LPCVOID, SIZE_T, LPCSTR, CONST D3D_SHADER_MACRO*, ID3DInclude*, LPCSTR, LPCSTR, UINT, UINT, ID3DBlob**, ID3DBlob**)>(GetProcAddress(hModule, "D3DCompile"));
        g_pFn_Original_D3DReflect = reinterpret_cast<HRESULT(WINAPI*)(LPCVOID, SIZE_T, REFIID, void**)>(GetProcAddress(hModule, "D3DReflect"));
    };
} Instance_Initialize_Original_Function_Pointers;

extern "C" HRESULT WINAPI D3DCompile(LPCVOID pSrcData, SIZE_T SrcDataSize, LPCSTR pSourceName, CONST D3D_SHADER_MACRO * pDefines, ID3DInclude * pInclude, LPCSTR pEntrypoint, LPCSTR pTarget, UINT Flags1, UINT Flags2, ID3DBlob * *ppCode, ID3DBlob * *ppErrorMsgs)
{
    Flags1 = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION | D3DCOMPILE_ENABLE_BACKWARDS_COMPATIBILITY;

    return g_pFn_Original_D3DCompile(pSrcData, SrcDataSize, pSourceName, pDefines, pInclude, pEntrypoint, pTarget, Flags1, Flags2, ppCode, ppErrorMsgs);
}

extern "C" HRESULT WINAPI D3DReflect(LPCVOID pSrcData, SIZE_T SrcDataSize, REFIID pInterface, void** ppReflector)
{
    return g_pFn_Original_D3DReflect(pSrcData, SrcDataSize, pInterface, ppReflector);
}

extern "C" HRESULT WINAPI D3DStripShader(LPCVOID pShaderBytecode, SIZE_T BytecodeLength, UINT uStripFlags, ID3DBlob * *ppStrippedBlob)
{
    return E_FAIL;
}

#if 0

#include <ImageHlp.h>

// "ImportTableHook"                    Also Called "Function Redirection"
// "pLibraryExportName"                 Used By Win32 To Distinguish Different (Export)Library
// "pFuntionToHookNameVector"           SOA(Structure Of Array)
// "pFuntionToHookNewAddressVector"     SOA(Structure Of Array)
static void ImportTableHook(void* pLibraryImportBaseAddress,
    char const* pLibraryExportName,
    size_t FuntionToHookVectorCount,
    char const* const* pFuntionToHookNameVector,
    uintptr_t const* pFuntionToHookNewAddressVector)
{
    // On Win32, Functions Imported From Different (Export)Library Are Grouped Into Different ImportTable.
    // We Find The ImportTable That Matches The "pLibraryExportName" Argument .

    IMAGE_IMPORT_DESCRIPTOR* pImportDescriptor = NULL;
    {
        ULONG ImportDescriptorVectorSize;
        IMAGE_IMPORT_DESCRIPTOR* ImportDescriptorVector = static_cast<IMAGE_IMPORT_DESCRIPTOR*>(ImageDirectoryEntryToData(pLibraryImportBaseAddress, TRUE, IMAGE_DIRECTORY_ENTRY_IMPORT, &ImportDescriptorVectorSize));
        assert(0U == (ImportDescriptorVectorSize % sizeof(IMAGE_IMPORT_DESCRIPTOR)));
        ULONG ImportDescriptorVectorCount = (ImportDescriptorVectorSize / sizeof(IMAGE_IMPORT_DESCRIPTOR));
        for (ULONG iImportDescriptor = 0U; iImportDescriptor < ImportDescriptorVectorCount; ++iImportDescriptor)
        {
            char* NameDLL = reinterpret_cast<char*>(reinterpret_cast<ULONG_PTR>(pLibraryImportBaseAddress) + ImportDescriptorVector[iImportDescriptor].Name); // RVA
            if (0 == strcmp(NameDLL, pLibraryExportName))
            {
                pImportDescriptor = ImportDescriptorVector + iImportDescriptor;
                break;
            }
        }
    }

    // The ImportTable Can Be Considered As A SOA(Structure Of Array), Which Constists Of Two Arrays - ImportNameTable And ImportAddressTable.
    // We Use The ImportNameTable To Find The Index Of The Function In The ImportTable And Write The New Address To The ImportAddressTable.

    if (pImportDescriptor != NULL)
    {
        for (size_t iFuntionToHook = 0U; iFuntionToHook < FuntionToHookVectorCount; ++iFuntionToHook)
        {
            uintptr_t* pFunctionAddressToWrite = NULL;
            {
                // OriginalFirstThunk Is The RelativeVirtualAddress From LibraryBaseAddress
                IMAGE_THUNK_DATA* ImportNameTable = reinterpret_cast<IMAGE_THUNK_DATA*>(reinterpret_cast<ULONG_PTR>(pLibraryImportBaseAddress) + pImportDescriptor->OriginalFirstThunk);

                // FirstThunk Is The RelativeVirtualAddress From LibraryBaseAddress
                IMAGE_THUNK_DATA* ImportAddressTable = reinterpret_cast<IMAGE_THUNK_DATA*>(reinterpret_cast<ULONG_PTR>(pLibraryImportBaseAddress) + pImportDescriptor->FirstThunk);

                for (size_t iImportTable = 0U; ImportNameTable[iImportTable].u1.AddressOfData != 0U; ++iImportTable) // 'RVA = 0' Means End Of Table
                {
                    // Name Is The RelativeVirtualAddress From LibraryBaseAddress
                    char* FunctionName = reinterpret_cast<char*>(reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(reinterpret_cast<ULONG_PTR>(pLibraryImportBaseAddress) + ImportNameTable[iImportTable].u1.AddressOfData)->Name);
                    if (0 == strcmp(FunctionName, pFuntionToHookNameVector[iFuntionToHook]))
                    {
                        pFunctionAddressToWrite = &ImportAddressTable[iImportTable].u1.Function;
                        break;
                    }
                }
            }

            if (pFunctionAddressToWrite != NULL)
            {
                // We Use Write-Copy To Ensure The Write Will Success!
                DWORD flOldProtect1;
                VirtualProtect(pFunctionAddressToWrite, sizeof(ULONG_PTR), PAGE_WRITECOPY, &flOldProtect1);

                *pFunctionAddressToWrite = pFuntionToHookNewAddressVector[iFuntionToHook];

                DWORD flOldProtect2;
                VirtualProtect(pFunctionAddressToWrite, sizeof(ULONG_PTR), flOldProtect1, &flOldProtect2);
            }
        }
    }
}

#endif
