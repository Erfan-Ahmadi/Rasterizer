#include <stdio.h>

// DirectX 12 specific headers.
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>

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
#pragma comment (lib, "dxguid")
#pragma comment (lib, "uuid")
#pragma comment (lib, "gdi32")

#define _CRT_SECURE_NO_WARNINGS 1

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#define SDL_MAIN_HANDLED
#include "SDL2/SDL.h"

#include <cstdio>
#include <algorithm>

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

#define SUCCEEDED(hr)   (((HRESULT)(hr)) >= 0)
#define FAILED(hr)      (((HRESULT)(hr)) < 0)
#define CHECK_AND_FAIL(hr)                          \
    if (FAILED(hr)) {                               \
        ::printf("[ERROR] " #hr "() failed. \n");   \
        ::abort();                                  \
    }                                               \
    /**/

#if defined(_DEBUG)
#define ENABLE_DEBUG_LAYER 1
#else
#define ENABLE_DEBUG_LAYER 0
#endif

enum class Rasterizer {
    HardwareD3D12 = 0,
    SoftwareWARP,
    MyRasterizer,
};

int main () 
{
    // Configurable Options
    bool vsync_on = false;

    // SDL_Init
    SDL_Init(SDL_INIT_VIDEO);

    // Enable Debug Layer
#if ENABLE_DEBUG_LAYER > 0
    ID3D12Debug * debug_interface_dx = nullptr;
    if(SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_interface_dx)))) {
        debug_interface_dx->EnableDebugLayer();
    }
#endif
    // Create a Window
    SDL_Window * wnd = SDL_CreateWindow("LearningD3D12", 0, 0, 1280, 720, 0);
    if(nullptr == wnd) {
        ::abort();
    }
    // Query Adapter (PhysicalDevice)

    IDXGIFactory * dxgi_factory = nullptr;
    CHECK_AND_FAIL(CreateDXGIFactory(IID_PPV_ARGS(&dxgi_factory)));

    constexpr uint32_t MaxAdapters = 8;
    IDXGIAdapter * adapters[MaxAdapters] = {};
    IDXGIAdapter * pAdapter;
    for (UINT i = 0; dxgi_factory->EnumAdapters(i, &pAdapter) != DXGI_ERROR_NOT_FOUND; ++i)
    {
        adapters[i] = pAdapter;
        DXGI_ADAPTER_DESC adapter_desc = {};
        ::printf("GPU Info [%d] :\n", i);
        if(SUCCEEDED(pAdapter->GetDesc(&adapter_desc))) {
            ::printf("\tDescription: %ls\n", adapter_desc.Description);
            ::printf("\tDedicatedVideoMemory: %zu\n", adapter_desc.DedicatedVideoMemory);
        }
    } // WARP -> Windows Advanced Rasterization ...

    // Create Logical Device
    ID3D12Device * d3d_device = nullptr;
    auto res = D3D12CreateDevice(adapters[0], D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&d3d_device));
    CHECK_AND_FAIL(res);

    // Create Command Queues

    // Create Swapchain 

    dxgi_factory->Release();
    debug_interface_dx->Release();

    // Loop 
    /*
    * 
        // Wait for fences
        CHECK(YRB::WaitForFences(&fences_framedone[frame_index], 1));

        - SwapChain -> Give me the next image to render to.

        - Render to Image

        - Present SwapChain Image
    */


    // Other stuff -> Shaders, DescriptorManagement (DescriptorHeap, RootSignature), PSO, Sync Objects, Buffers, Textures, ...
    
    return 0;
}