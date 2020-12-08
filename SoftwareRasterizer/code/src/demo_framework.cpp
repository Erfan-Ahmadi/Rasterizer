#include "demo_framework.hpp"

#include <utility>
#include <cstdio>
#include <cstdlib>
#include <functional>

struct DemoInstance {
    char const *            name;
    Demo *                  instance;
    std::function<Demo*()>  factory;
};

struct DemoState {
    static constexpr uint32_t MaxDemoCount = 512;
    DemoInstance    demos[MaxDemoCount];
    uint32_t        demos_count     = 0;
    uint32_t        current_index   = 0;
};

static DemoState * GetDemoState() {
    static DemoState state;
    return &state;
}

Demo::Demo() {
}

Demo::~Demo() {
    Exit();
}

void Demo::Init() {
    if(!initialized) {
        if(DoInitWindow()) {
            if(DoInitRenderer()) {
                if(DoInitResources()) {
                    initialized = true;
                } else {
                    ::printf("DoInitResources() failed");
                    DoExitWindow();
                    DoExitRenderer();
                }
            } else {
                ::printf("DoInitRenderer() failed");
                DoExitRenderer();
            }
        } else {
            ::printf("DoInitWindow() failed");
        }
    }
}

void Demo::Exit()
{
    if(initialized) {
        if(DoExitResources()) {
            if(DoExitRenderer()) {
                if(DoExitWindow()) {
                    initialized = false;
                } else {
                    ::printf("DoExitWindow() failed");
                }
            } else {
                ::printf("DoExitRenderer() failed");
            }
        } else {
            ::printf("DoExitResources() failed");
        }
    }
}

void Demo::Run() {
    SDL_Event event;
    while(false == should_quit) {
        while(SDL_PollEvent(&event)) {
            
            ImGui_ImplSDL2_ProcessEvent(&event);

            if(SDL_QUIT == event.type) {
                should_quit = true;
            }

            // TODO: Handle Resize
            /* snippet for resize + handling imgui
            WaitForLastSubmittedFrame();
            ImGui_ImplDX12_InvalidateDeviceObjects();
            CleanupRenderTarget();
            ResizeSwapChain(hWnd, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam));
            CreateRenderTarget();
            ImGui_ImplDX12_CreateDeviceObjects();
            */

        }

        if(imgui_initialized) {
            OnUI();
        }

        OnUpdate();
        OnRender();
    }
}

bool Demo::DoInitRenderer() {
    bool ret = false;

    // Enable Debug Layer
    UINT dxgiFactoryFlags = 0;
    #if ENABLE_DEBUG_LAYER > 0
        if(SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debug_interface_dx)))) {
            debug_interface_dx->EnableDebugLayer();
            dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
        }
    #endif

    // Query Adapter (PhysicalDevice)
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

    HRESULT res;
    AdapterPreference preference = GetAdapterPreference();
    if(AdapterPreference::Hardware == preference) {
        // Create Logical Device
        res = D3D12CreateDevice(adapters[0], D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&device));
        CHECK_AND_FAIL(res);
    
    } else if(AdapterPreference::Software == preference) {
        IDXGIAdapter * warp_adapter = nullptr;
        dxgi_factory->EnumWarpAdapter(IID_PPV_ARGS(&warp_adapter));
        res = D3D12CreateDevice(warp_adapter, D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&device));
        CHECK_AND_FAIL(res);
        warp_adapter->Release();
    }

    for (uint32_t i = 0; i < MaxAdapters; ++i) {
        if(nullptr != adapters[i]) {
            adapters[i]->Release();
        }
    }

    // Create Command Queues
    D3D12_COMMAND_QUEUE_DESC command_queue_desc = {};
    command_queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    command_queue_desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
    command_queue_desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
    command_queue_desc.NodeMask = 0;
    res = device->CreateCommandQueue(&command_queue_desc, IID_PPV_ARGS(&direct_queue));
    CHECK_AND_FAIL(res);
    command_queue_desc.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
    res = device->CreateCommandQueue(&command_queue_desc, IID_PPV_ARGS(&compute_queue));
    CHECK_AND_FAIL(res);
    command_queue_desc.Type = D3D12_COMMAND_LIST_TYPE_COPY;
    res = device->CreateCommandQueue(&command_queue_desc, IID_PPV_ARGS(&copy_queue));
    CHECK_AND_FAIL(res);
    
    // For DXC Shader Compilation
    res = DxcCreateInstance(CLSID_DxcLibrary, IID_PPV_ARGS(&library));
    CHECK_AND_FAIL(res);
    res = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler));
    CHECK_AND_FAIL(res);
    
    ret = true;
    return ret;
}

bool Demo::DoInitWindow() {
    bool ret = false;

    // SDL_Init
    SDL_Init(SDL_INIT_VIDEO);
    
    // Create a Window
    render_window.sdl_wnd = SDL_CreateWindow(
        GetWindowTitle(),
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        window_width, window_height,
        0);

    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    SDL_bool sdl_res = SDL_GetWindowWMInfo(render_window.sdl_wnd, &wmInfo);
    ASSERT(SDL_TRUE == sdl_res);
    render_window.hwnd = wmInfo.info.win.window;
    if(render_window.hwnd) {
        ret = true;
    }

    return ret;
}

bool Demo::DoExitRenderer() {
    bool ret = false;
    
    // For DXC Shader Compilation
    compiler->Release();
    library->Release();

    direct_queue->Release();
    compute_queue->Release();
    copy_queue->Release();

#if ENABLE_DEBUG_LAYER > 0
    debug_interface_dx->Release();
#endif

    device->Release();
    
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
        HRESULT res = DXGIGetDebugInterface_FUNC(IID_PPV_ARGS(&dxgi_debug));
        CHECK_AND_FAIL(res);
        res = dxgi_debug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_DETAIL);
        CHECK_AND_FAIL(res);
        dxgi_debug->Release();
        FreeLibrary(hDLL);
    }

    ret = true;
    return ret;
}

bool Demo::DoExitWindow() {
    bool ret = false;
    SDL_DestroyWindow(render_window.sdl_wnd);
    SDL_Quit();
    ret = true;
    return ret;
}

void Demo::InitUI(uint32_t frame_queue_length, DXGI_FORMAT render_target_format) {
    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    desc.NumDescriptors = 1;
    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    HRESULT res = device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&g_pd3dSrvDescHeap));
    CHECK_AND_FAIL(res);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsClassic();
    
    bool imgui_dx12_init_success = ImGui_ImplDX12_Init(device, frame_queue_length,
        render_target_format, g_pd3dSrvDescHeap,
        g_pd3dSrvDescHeap->GetCPUDescriptorHandleForHeapStart(),
        g_pd3dSrvDescHeap->GetGPUDescriptorHandleForHeapStart());

    ASSERT(imgui_dx12_init_success);

    ImGui_ImplSDL2_InitForD3D(render_window.sdl_wnd);

    imgui_initialized = true;
}

void Demo::ExitUI() {
    if (g_pd3dSrvDescHeap) { g_pd3dSrvDescHeap->Release(); g_pd3dSrvDescHeap = NULL; }
    ImGui_ImplSDL2_Shutdown();
    ImGui_ImplDX12_Shutdown();
    ImGui::DestroyContext();
    imgui_initialized = false;
}

void Demo::RenderUI(ID3D12GraphicsCommandList * cmd) {
    // Rendering
    cmd->SetDescriptorHeaps(1, &g_pd3dSrvDescHeap);
    ImGui::Render();
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), cmd);
}

bool Demo_Register(char const * name, std::function<Demo*()> demo_factory) {
    bool ret = false;
    DemoState * demo_state = GetDemoState();
    if (nullptr != name && demo_factory && demo_state->demos_count < DemoState::MaxDemoCount) {
        DemoInstance & new_instance = demo_state->demos[demo_state->demos_count++];
        new_instance.name = name;
        new_instance.factory = std::move(demo_factory);
        new_instance.instance = nullptr;
        ret = true;
    }
    return ret;
}

int main(int argc, char * argv[]) {
    DemoState * demo_state = GetDemoState();
    uint32_t selected_demo = demo_state->demos_count;
    if(argc < 2) {
        ::printf("List of All Demos :\n");
        for(uint32_t d = 0; d < demo_state->demos_count; ++d) {
            ::printf("%-3d: %s\n", d, demo_state->demos[d].name);
        }
        ::printf("Select Demo to Run: ");
        if(0 == ::scanf_s("%u", &selected_demo)) {
            selected_demo = demo_state->demos_count;
        }
    } else {
        selected_demo = static_cast<uint32_t>(::atoi(argv[1]));
    }

    if(selected_demo < demo_state->demos_count) {
        demo_state->current_index = selected_demo;
        DemoInstance & demo = demo_state->demos[demo_state->current_index];
        ::printf("Selected Demo is %s (#%u)\n", demo.name, demo_state->current_index);
        demo.instance = demo.factory();
        if(nullptr != demo.instance) {
            demo.instance->SetWindowTitle(demo.name);
            demo.instance->Init();
            demo.instance->Run();
            demo.instance->Exit();
            delete demo.instance;
            demo.instance = nullptr;
        } else {
            ::printf("Demo Instance Creation Failed\n");
        }
    } else {
        ::printf("Invalid Selection.\n");
    }
    return 0;
}
