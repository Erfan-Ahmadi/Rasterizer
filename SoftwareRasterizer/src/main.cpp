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
#include "SDL2/SDL_syswm.h"

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
        ::printf("[ERROR] " #hr "() failed at line %d. \n", __LINE__);   \
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

static constexpr uint32_t frame_queue_length = 2;
static uint32_t window_width = 1280;
static uint32_t window_height = 720;

int main () 
{
    // Configurable Options
    Rasterizer rasterizer = Rasterizer::HardwareD3D12;
    bool vsync_on = false;

    // SDL_Init
    SDL_Init(SDL_INIT_VIDEO);

    // Enable Debug Layer
    UINT dxgiFactoryFlags = 0;
#if ENABLE_DEBUG_LAYER > 0
    ID3D12Debug * debug_interface_dx = nullptr;
    if(SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_interface_dx)))) {
        debug_interface_dx->EnableDebugLayer();
        dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
    }
#endif

    // Create a Window
    SDL_Window * wnd = SDL_CreateWindow("LearningD3D12", 0, 0, window_width, window_height, 0);
    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    SDL_bool sdl_res = SDL_GetWindowWMInfo(wnd, &wmInfo);
    HWND hwnd = wmInfo.info.win.window;
    if(nullptr == wnd) {
        ::abort();
    }

    // Query Adapter (PhysicalDevice)
    IDXGIFactory * dxgi_factory = nullptr;
    CHECK_AND_FAIL(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&dxgi_factory)));

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
    ID3D12CommandQueue * command_queue = nullptr;
    D3D12_COMMAND_QUEUE_DESC command_queue_desc = {};
    command_queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    command_queue_desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    command_queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    command_queue_desc.NodeMask = 0;
    res = d3d_device->CreateCommandQueue(&command_queue_desc, IID_PPV_ARGS(&command_queue));
    CHECK_AND_FAIL(res);

    // Create Swapchain 
    IDXGISwapChain * swap_chain = nullptr;
    DXGI_SWAP_CHAIN_DESC swap_chain_desc = {};
    swap_chain_desc.BufferDesc.Width = window_width;
    swap_chain_desc.BufferDesc.Height = window_height;
    swap_chain_desc.BufferDesc.RefreshRate = DXGI_RATIONAL {};
    swap_chain_desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swap_chain_desc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    swap_chain_desc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
    swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swap_chain_desc.SampleDesc.Count = 1;
    swap_chain_desc.SampleDesc.Quality = 0;
    swap_chain_desc.BufferCount = frame_queue_length;
    swap_chain_desc.OutputWindow = hwnd;
    swap_chain_desc.Windowed = true;
    swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swap_chain_desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
    res = dxgi_factory->CreateSwapChain(command_queue, &swap_chain_desc, &swap_chain);
    CHECK_AND_FAIL(res);

    // Using DXC
    // Shaders, RootSignatures, DescriptorHeaps etc...
    // Graphics Pipeline Creation
    // Memory Allocation in D3D12, Vertes/Index Buffers
    // Command Submission and Recording in D3D12
    // Fence and Sync objects
    // Main Loop and SDL Event Handling
    // Try WARP
    // Integrate STB to Load Images
    // Texture Sampling Demo
    // Abstract and Make Code Nicer and More Flexible
    // Compute Shader and SoftwareRasterizer Begin Design and Implementation

    // Cleanup
    swap_chain->Release();
    command_queue->Release();
    dxgi_factory->Release();
    debug_interface_dx->Release();
    SDL_DestroyWindow(wnd);
    SDL_Quit();

    return 0;
}