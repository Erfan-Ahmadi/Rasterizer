#include "common.h"

enum class Rasterizer {
    HardwareD3D12 = 0,
    SoftwareWARP,
    MyRasterizer,
};

static constexpr uint32_t frame_queue_length = 2;
static uint32_t window_width = 1280;
static uint32_t window_height = 720;

struct ShaderCompileHelper {
private:
    IDxcLibrary * library;
    IDxcCompiler * compiler;
public:
    void init() {
        HRESULT res = DxcCreateInstance(CLSID_DxcLibrary, IID_PPV_ARGS(&library));
        CHECK_AND_FAIL(res);
        res = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler));
        CHECK_AND_FAIL(res);
    }
    void exit() {
        compiler->Release();
        library->Release();
    }
    IDxcBlob * compile_from_file(LPCWSTR path, LPCWSTR entry_point, LPCWSTR target_profile) {
        IDxcBlob * code = nullptr;

        uint32_t codePage = CP_UTF8;
        IDxcBlobEncoding * sourceBlob;
        HRESULT res = library->CreateBlobFromFile(path, &codePage, &sourceBlob);
        CHECK_AND_FAIL(res);

        IDxcOperationResult * result;

        res = compiler->Compile(
            sourceBlob, // pSource
            path, // pSourceName
            entry_point, // pEntryPoint
            target_profile, // pTargetProfile
            NULL, 0, // pArguments, argCount
            NULL, 0, // pDefines, defineCount
            NULL, // pIncludeHandler
            &result); // ppResult
        
        if(SUCCEEDED(res))
            result->GetStatus(&res);
        if(FAILED(res))
        {
            ASSERT(nullptr != result);
            if(result)
            {
                IDxcBlobEncoding * errorsBlob;
                res = result->GetErrorBuffer(&errorsBlob);
                if(SUCCEEDED(res) && errorsBlob)
                {
                    wprintf(L"Compilation failed with errors:\n%hs\n",
                        (const char*)errorsBlob->GetBufferPointer());
                }
            }
        } else {
            result->GetResult(&code);
        }

        sourceBlob->Release();
        return code;
    }
};

int main () 
{
    // Configurable Options
    Rasterizer rasterizer = Rasterizer::HardwareD3D12;
    bool vsync_on = false;
    CONSUME_VAR(rasterizer);

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
    ASSERT(SDL_TRUE == sdl_res);
    HWND hwnd = wmInfo.info.win.window;
    if(nullptr == wnd) {
        ::abort();
    }
    
    ShaderCompileHelper shader_compiler = {};
    shader_compiler.init();

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
    swap_chain_desc.Flags = (vsync_on) ? 0 : DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
    res = dxgi_factory->CreateSwapChain(command_queue, &swap_chain_desc, &swap_chain);
    CHECK_AND_FAIL(res);


    IDxcBlob * shader = shader_compiler.compile_from_file(L"../code/src/shaders/simple_mesh.frag.hlsl", L"PSMain", L"ps_6_0");
    shader->Release();

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
    shader_compiler.exit();
    swap_chain->Release();
    command_queue->Release();
    dxgi_factory->Release();
    debug_interface_dx->Release();
    SDL_DestroyWindow(wnd);
    SDL_Quit();

    return 0;
}