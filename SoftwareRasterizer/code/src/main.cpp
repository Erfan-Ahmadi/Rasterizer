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

void GetQuadMesh(Mesh * out) {
    if(nullptr != out) {
        out->vertices = std::vector<Vertex>({
            {{ 1.0f, 1.0f, 0.0f}, {255, 0, 0, 255}, {1.0f, 1.0f}}, // RB?
            {{ -1.0f,-1.0f, 0.0f}, {0, 255, 0, 255}, {0.0f, 0.0f}}, // LT?
            {{ -1.0f, 1.0f, 0.0f}, {0, 0, 255, 255}, {0.0f, 1.0f}}, // LB?
            {{ 1.0f, -1.0f, 0.0f}, {255, 255, 255, 255}, {1.0f, 0.0f}}, // RT?
        });
        
        out->indices = std::vector<uint16_t>({
            0, 1, 2, 0, 3, 1
        });

    } else {
        ::printf("GetQuadMesh: out is nullptr");
    }
}
void GetTriangleMesh(Mesh * out) {
    if(nullptr != out) {
        // TODO
        out->vertices = std::vector<Vertex>({
            {{ 1.0f, 1.0f, 0.0f}, {255, 0, 0, 255}, {1.0f, 1.0f}}, // RB?
            {{ -1.0f,-1.0f, 0.0f}, {0, 255, 0, 255}, {0.0f, 0.0f}}, // LT?
            {{ -1.0f, 1.0f, 0.0f}, {0, 0, 255, 255}, {0.0f, 1.0f}}, // LB?
            {{ 1.0f, -1.0f, 0.0f}, {255, 255, 255, 255}, {1.0f, 0.0f}}, // RT?
        });
        
        out->indices = std::vector<uint16_t>({
            0, 1, 2, 0, 3, 1
        });

    } else {
        ::printf("GetTriangleMesh: out is nullptr");
    }
}

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
    }

    // Create Logical Device
    ID3D12Device * d3d_device = nullptr;
    auto res = D3D12CreateDevice(adapters[0], D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&d3d_device));
    CHECK_AND_FAIL(res);
    
    for (uint32_t i = 0; i < MaxAdapters; ++i) {
        if(nullptr != adapters[i]) {
            adapters[i]->Release();
        }
    }

    // Create Command Queues
    ID3D12CommandQueue * direct_queue = nullptr;
    ID3D12CommandQueue * compute_queue = nullptr;
    ID3D12CommandQueue * copy_queue= nullptr;
    D3D12_COMMAND_QUEUE_DESC command_queue_desc = {};
    command_queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    command_queue_desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    command_queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    command_queue_desc.NodeMask = 0;
    res = d3d_device->CreateCommandQueue(&command_queue_desc, IID_PPV_ARGS(&direct_queue));
    CHECK_AND_FAIL(res);
    command_queue_desc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
    res = d3d_device->CreateCommandQueue(&command_queue_desc, IID_PPV_ARGS(&compute_queue));
    CHECK_AND_FAIL(res);
    command_queue_desc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
    res = d3d_device->CreateCommandQueue(&command_queue_desc, IID_PPV_ARGS(&copy_queue));
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
    res = dxgi_factory->CreateSwapChain(direct_queue, &swap_chain_desc, &swap_chain);
    CHECK_AND_FAIL(res);
    
    ShaderCompileHelper shader_compiler = {};
    shader_compiler.init();

    // ------------------------------------------ [BEGIN] Specific to a Demo ------------------------------------------

    ID3D12CommandAllocator * direct_cmd_allocator = nullptr;
    ID3D12CommandAllocator * compute_cmd_allocator = nullptr;
    ID3D12CommandAllocator * copy_cmd_allocator = nullptr;
    res = d3d_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&direct_cmd_allocator));
    CHECK_AND_FAIL(res);
    res = d3d_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COMPUTE, IID_PPV_ARGS(&compute_cmd_allocator));
    CHECK_AND_FAIL(res);
    res = d3d_device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COPY, IID_PPV_ARGS(&copy_cmd_allocator));
    CHECK_AND_FAIL(res);

    ID3D12GraphicsCommandList * copy_cmd_list = nullptr;
    res = d3d_device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COPY, copy_cmd_allocator, nullptr, IID_PPV_ARGS(&copy_cmd_list));
    CHECK_AND_FAIL(res);

    IDxcBlob * vertex_shader = shader_compiler.compile_from_file(L"../code/src/shaders/simple_mesh.vert.hlsl", L"VSMain", L"vs_6_5");
    IDxcBlob * pixel_shader = shader_compiler.compile_from_file(L"../code/src/shaders/simple_mesh.frag.hlsl", L"PSMain", L"ps_6_5");
    ASSERT(nullptr != vertex_shader);
    ASSERT(nullptr != pixel_shader);

    // RootSignatures, DescriptorHeaps etc...
    // Graphics Pipeline Creation

    vertex_shader->Release();
    pixel_shader->Release();

    Mesh triangle_mesh = {};
    GetTriangleMesh(&triangle_mesh);
    
    ID3D12Resource * vertex_buffer = nullptr;
    ID3D12Resource * index_buffer = nullptr;
    size_t vertex_buffer_size = sizeof(Vertex) * triangle_mesh.vertices.size();
    size_t index_buffer_size = sizeof(IndexType) * triangle_mesh.indices.size();

    CONSUME_VAR(index_buffer);
    CONSUME_VAR(index_buffer_size);

    // Vertex Buffer
    if(false)
    {
        // Buffer Allocation
        D3D12_HEAP_PROPERTIES   heap_props      = GetDefaultHeapProps(D3D12_HEAP_TYPE_DEFAULT);
        D3D12_RESOURCE_DESC     resource_desc   = GetBufferResourceDesc(vertex_buffer_size);
        res = d3d_device->CreateCommittedResource(&heap_props,
            D3D12_HEAP_FLAG_NONE,
            &resource_desc,
            D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
            nullptr,
            IID_PPV_ARGS(&vertex_buffer));
        CHECK_AND_FAIL(res);

        ID3D12Resource * staging_vertex_buffer = nullptr;

        // Staging
        D3D12_HEAP_PROPERTIES   staging_heap_props      = GetDefaultHeapProps(D3D12_HEAP_TYPE_UPLOAD);
        D3D12_RESOURCE_DESC     staging_resource_desc   = GetBufferResourceDesc(vertex_buffer_size);
        res = d3d_device->CreateCommittedResource(&staging_heap_props,
            D3D12_HEAP_FLAG_NONE,
            &staging_resource_desc,
            D3D12_RESOURCE_STATE_GENERIC_READ | D3D12_RESOURCE_STATE_COPY_SOURCE,
            nullptr,
            IID_PPV_ARGS(&staging_vertex_buffer));
        CHECK_AND_FAIL(res);
        
        // Copy to Buffer
        copy_cmd_list->CopyBufferRegion(vertex_buffer, 0, staging_vertex_buffer, 0, vertex_buffer_size);

        // Delete Staging Buffer
        staging_vertex_buffer->Release();
    }
    // Index Buffer 
    {
        // stage
        // copy
    }

    shader_compiler.exit();

    // Fence and Sync objects, Barriers
    // Main Loop and SDL Event Handling
    // Draw The Triangle !
    // Try WARP
    // Integrate STB to Load Images
    // Texture Sampling Demo
    // Abstract and Make Code Nicer and More Flexible (DemoFramework?)
    // Compute Shader and SoftwareRasterizer Begin Design and Implementation
    
    copy_cmd_list->Release();
    direct_cmd_allocator->Release();
    compute_cmd_allocator->Release();
    copy_cmd_allocator->Release();

    // ------------------------------------------ [END] Specific to a Demo ------------------------------------------

    // Cleanup

    swap_chain->Release();

    direct_queue->Release();
    compute_queue->Release();
    copy_queue->Release();

#if ENABLE_DEBUG_LAYER > 0
    debug_interface_dx->Release();
#endif
    d3d_device->Release();
    
    dxgi_factory->Release();
    
    // DXGI Debug for ReportLiveObjects
    {
        HINSTANCE hDLL;               // Handle to DLL
        typedef HRESULT (*DXGIGetDebugInterface_FN)(REFIID riid, void **ppDebug);
        DXGIGetDebugInterface_FN DXGIGetDebugInterface_FUNC = nullptr;
        hDLL = LoadLibrary(L"DXGIDebug.dll");
        if (hDLL != NULL)
        {
           DXGIGetDebugInterface_FUNC = (DXGIGetDebugInterface_FN)GetProcAddress(hDLL, "DXGIGetDebugInterface");
        }
        IDXGIDebug * dxgi_debug = nullptr;
        res = DXGIGetDebugInterface_FUNC(IID_PPV_ARGS(&dxgi_debug));
        CHECK_AND_FAIL(res);
        res = dxgi_debug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_DETAIL);
        CHECK_AND_FAIL(res);
        dxgi_debug->Release();
        FreeLibrary(hDLL);
    }

    SDL_DestroyWindow(wnd);
    SDL_Quit();


    return 0;
}