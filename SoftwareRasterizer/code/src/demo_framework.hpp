#pragma once
#include "common.h"

#if COMPILER_IS_MSVC
    #pragma warning (push)
    #pragma warning (disable: 4127)     // conditional expression is constant
    #pragma warning (disable: 4201)     // Non-standard extension used: nameless struct/union
    #pragma warning (disable: 26495)    // Struct member x/y/z/etc. is uninitialized. Always initialize a struct member!
    #pragma warning (disable: 26812)    // normal enum used instead of enum class!
    #pragma warning (disable: 4100)     // unreferenced formal parameter
#endif

class Demo {
public:
    Demo();
    virtual ~Demo();
    void Init();
    void Exit();
    void Run();
    void SetWindowTitle(char const * title) { window_title = title; }

protected:
    virtual bool DoInitRenderer();
    virtual bool DoInitWindow();
    virtual bool DoInitResources() = 0;
    virtual bool DoExitRenderer();
    virtual bool DoExitWindow();
    virtual bool DoExitResources() = 0;

    virtual void OnResize(uint32_t new_width, uint32_t new_height) {}
    virtual void OnUpdate() = 0;
    virtual void OnRender() = 0;
    virtual char const * GetWindowTitle() const { return window_title; }

    // Helpers
protected:
     IDxcBlob * CompileShaderFromFile(LPCWSTR path, LPCWSTR entry_point, LPCWSTR target_profile) {
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

protected:
    bool should_quit = false;
    
    ID3D12Debug *   debug_interface_dx = nullptr;
    IDXGIFactory4 * dxgi_factory = nullptr;
    ID3D12Device *  device = nullptr;
    
    ID3D12CommandQueue * direct_queue = nullptr;
    ID3D12CommandQueue * compute_queue = nullptr;
    ID3D12CommandQueue * copy_queue= nullptr;
    
    IDxcLibrary * library       = nullptr;
    IDxcCompiler * compiler     = nullptr;
    
    bool            initialized        = false;
    char const *    window_title       = "Demo";
    struct {
        SDL_Window *    sdl_wnd    = nullptr;
        HWND            hwnd       = NULL;
    } render_window;
    
    uint32_t    window_width = 1280;
    uint32_t    window_height = 720;
};

[[nodiscard]] bool
Demo_Register(char const * name, std::function<Demo*()> demo_factory);

#if COMPILER_IS_MSVC
    #pragma warning (pop)
#endif