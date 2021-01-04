#pragma once
#include <stdio.h>
#include <cstdint>
#include <functional>
#include <filesystem>
#include <chrono>

// DirectX 12 specific headers.
#include <d3d12.h>
#include <dxgi1_6.h>
#include <dxgidebug.h>
#include <d3dcompiler.h>
#include <dxcapi.h>
#include <DirectXMath.h>

// std :(
#include <vector>
#include <string>

#if !defined(NDEBUG) && !defined(_DEBUG)
#error "Define at least one."
#elif defined(NDEBUG) && defined(_DEBUG)
#error "Define at most one."
#endif

#if defined(_WIN64)
#if defined(_DEBUG)
#pragma comment (lib, "64/SDL2-staticd")
#else
#pragma comment (lib, "64/SDL2-static")
#endif
#else
#if defined(_DEBUG)
#pragma comment (lib, "32/SDL2-staticd")
#else
#pragma comment (lib, "32/SDL2-static")
#endif
#endif

#pragma comment (lib, "Imm32")
#pragma comment (lib, "Setupapi")
#pragma comment (lib, "Version")
#pragma comment (lib, "Winmm")

#pragma comment (lib, "d3d12")
#pragma comment (lib, "dxgi")
#pragma comment (lib, "dxcompiler")
#pragma comment (lib, "dxguid")
#pragma comment (lib, "uuid")
#pragma comment (lib, "gdi32")

#define _CRT_SECURE_NO_WARNINGS 1

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#define SDL_MAIN_HANDLED
#include "SDL2/SDL.h"
#include "SDL2/SDL_syswm.h"

#include <cstdio>
#include <algorithm>

#if defined(_MSC_VER)
#define COMPILER_IS_MSVC 1
#endif

// windows runtime library. needed for microsoft::wrl::comptr<> template class.
//#include <wrl.h>
//using namespace microsoft::wrl;

/*
d3d12.lib
dxgi.lib
dxguid.lib
uuid.lib
kernel32.lib
user32.lib
gdi32.lib
winspool.lib
comdlg32.lib
advapi32.lib
shell32.lib
ole32.lib
oleaut32.lib
odbc32.lib
odbccp32.lib
runtimeobject.lib
*/

#define CONSUME_VAR(v_)       ((void)(v_))
#define ASSERT(x_)                                  \
    if (false == (x_)) {                             \
        ::printf("[ERROR] " #x_ "() failed. \n");   \
        ::abort();                                  \
    }                                               \
    /**/
#define SUCCEEDED(hr)   (((HRESULT)(hr)) >= 0)
#define FAILED(hr)      (((HRESULT)(hr)) < 0)
#define CHECK_AND_FAIL(hr)                          \
    if (FAILED(hr)) {                               \
        ::printf("[ERROR] " #hr "() failed at line %d. \n", __LINE__);   \
        ::abort();                                  \
    }                                               \
    /**/

#if defined(_DEBUG)
#define ENABLE_DEBUG_LAYER 1
#else
#define ENABLE_DEBUG_LAYER 0
#endif

// Vertex/Index Buffer:
struct Vertex {
    float       pos[3];
    float       normal[3];
    uint8_t     col[4];
    float       uv[2];
};


using IndexType = uint32_t;
static DXGI_FORMAT IndexBufferFormat = DXGI_FORMAT_R32_UINT;

struct Mesh {
    std::vector<Vertex> vertices;
    std::vector<IndexType> indices;
};

inline D3D12_HEAP_PROPERTIES GetDefaultHeapProps(D3D12_HEAP_TYPE type) {
    D3D12_HEAP_PROPERTIES ret = {};
    ret.Type = type;
    ret.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
    ret.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
    ret.CreationNodeMask = 0;
    ret.VisibleNodeMask = 0;
    return ret;
}

inline D3D12_RESOURCE_DESC GetBufferResourceDesc(
    UINT64 width,
    D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE,
    UINT64 alignment = 0 ) noexcept
{
    D3D12_RESOURCE_DESC ret = {};
    ret.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
    ret.Alignment = alignment;
    ret.Width = width;
    ret.Height = 1;
    ret.DepthOrArraySize = 1;
    ret.MipLevels = 1;
    ret.Format = DXGI_FORMAT_UNKNOWN;
    ret.SampleDesc.Count = 1;
    ret.SampleDesc.Quality = 0;
    ret.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    ret.Flags = flags;
    return ret;
}