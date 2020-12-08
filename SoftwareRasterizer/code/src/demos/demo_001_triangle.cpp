/*
- DEMO_000
- NOTHING

This Demo is just inheriting from DemoFramework and is nothing other than an empty window we don't render to.
*/

#include "../demo_framework.hpp"

class Demo_001_Triangle : public Demo {
protected:
    virtual bool DoInitResources() override;
    virtual bool DoExitResources() override;
    virtual void OnUpdate() override;
    virtual void OnRender() override;
    virtual AdapterPreference GetAdapterPreference() const override { return AdapterPreference::Hardware; };

private:
    void GetTriangleMesh(Mesh * out) {
        if(nullptr != out) {
            out->vertices = std::vector<Vertex>({
                { { 0.0f,    0.4f,   0.0f }, {uint8_t(255 * 0.8f), uint8_t(255 * 0.0f), uint8_t(255 * 0.6f), 255}, {0.0f, 0.0f} }, // MIDDLE_TOP
                { { 0.25f,   -0.4f,  0.0f }, {uint8_t(255 * 0.1f), uint8_t(255 * 0.6f), uint8_t(255 * 0.4f), 255}, {0.0f, 0.0f} }, // BOTTOM_RIGHT
                { { -0.25f,  -0.4f,  0.0f }, {uint8_t(255 * 0   ), uint8_t(255 * 0.5f), uint8_t(255 * 1.0f), 255}, {0.0f, 0.0f} }, // BOTTOM_LEFT
            });
        
            out->indices = std::vector<IndexType>({
                0, 1, 2,
            });

        } else {
            ::printf("GetTriangleMesh: out is nullptr");
        }
    }
    void GetQuadMesh(Mesh * out) {
        if(nullptr != out) {
            out->vertices = std::vector<Vertex>({
                { { 0.25f,    0.4f,   0.0f }, {uint8_t(255 * 0.5f),   uint8_t(255 * 0.3f),   uint8_t(255 * 0.6f), 255}, {0.0f, 0.0f} }, // TOP_RIGHT
                { { -0.25f,   0.4f,   0.0f }, {uint8_t(255 * 0.8f),   uint8_t(255 * 0.0f),   uint8_t(255 * 0.6f), 255}, {0.0f, 0.0f} }, // TOP_LEFT
                { { 0.25f,   -0.4f,  0.0f },  {uint8_t(255 * 0.1f),   uint8_t(255 * 0.6f),   uint8_t(255 * 0.4f), 255}, {0.0f, 0.0f} }, // BOTTOM_RIGHT
                { { -0.25f,  -0.4f,  0.0f },  {uint8_t(255 * 0  ),    uint8_t(255 * 0.5f),   uint8_t(255 * 1.0f), 255}, {0.0f, 0.0f} }, // BOTTOM_LEFT
            });
        
            out->indices = std::vector<IndexType>({
                1, 2, 3, // LowerLeft Triangle
                0, 2, 1, // UpperRight Triangle
            });

        } else {
            ::printf("GetQuadMesh: out is nullptr");
        }
    }

private:
    static constexpr uint32_t FrameQueueLength = 2;

    // SwapChain and It's RenderTarget Resources
    IDXGISwapChain4 * swap_chain = nullptr;
    ID3D12Resource * swap_chain_render_targets[FrameQueueLength];

    ID3D12CommandAllocator * direct_cmd_allocator = nullptr;
    ID3D12CommandAllocator * compute_cmd_allocator = nullptr;
    ID3D12CommandAllocator * copy_cmd_allocator = nullptr;
    
    ID3D12GraphicsCommandList * direct_cmd_list [FrameQueueLength] = {};
    ID3D12GraphicsCommandList * copy_cmd_list = nullptr;

    ID3D12DescriptorHeap * rtv_heap = nullptr;
    uint32_t rtv_handle_increment_size = 0;

    ID3D12RootSignature * root_signature = nullptr;
    ID3D12PipelineState * graphics_pso = nullptr;

    Mesh mesh = {};
    ID3D12Resource * vertex_buffer  = nullptr;
    ID3D12Resource * index_buffer   = nullptr;
    size_t vertex_buffer_bytes  = 0;
    size_t index_buffer_bytes   = 0;

    // FrameSync
    ID3D12Fence * fence = {};
    HANDLE event = {};
    UINT64 fence_values[FrameQueueLength] = {};
    uint32_t frame_index = 0;

    void WaitForQueue(ID3D12CommandQueue * queue) {
        if(nullptr != queue) {
            queue->Signal(fence, fence_values[frame_index]);
            
            // Wait until the fence has been processed.
            HRESULT res = fence->SetEventOnCompletion(fence_values[frame_index], event);
            CHECK_AND_FAIL(res);

            WaitForSingleObjectEx(event, INFINITE, FALSE);

            // Increment the fence value for the current frame.
            fence_values[frame_index]++;
        } else {
            ::printf("WaitForQueue(queue): queue is nullptr");
        }
    }
    void MoveToNextFrame(ID3D12CommandQueue * queue) {
        if(nullptr != queue) {
            if(nullptr != swap_chain) {
                // Schedule a Signal command in the queue.
                const UINT64 current_fence_value = fence_values[frame_index];
                HRESULT res = queue->Signal(fence, current_fence_value);
                CHECK_AND_FAIL(res);

                // Update the frame index.
                frame_index = swap_chain->GetCurrentBackBufferIndex();

                // If the next frame is not ready to be rendered yet, wait until it is ready.
                if (fence->GetCompletedValue() < fence_values[frame_index])
                {
                    res = fence->SetEventOnCompletion(fence_values[frame_index], event);
                    CHECK_AND_FAIL(res);
                    WaitForSingleObjectEx(event, INFINITE, FALSE);
                }

                // Set the fence value for the next frame.
                fence_values[frame_index] = current_fence_value + 1;
            } else {
                ::printf("MoveToNextFrame(queue, swap_chain): swap_chain is nullptr");
            }
        } else {
            ::printf("MoveToNextFrame(queue, swap_chain): queue is nullptr");
        }

    }
    void InitSyncResources() {
        HRESULT res = device->CreateFence(fence_values[frame_index], D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
        CHECK_AND_FAIL(res);
        fence_values[frame_index]++;
        event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if(nullptr == event) {
            res = HRESULT_FROM_WIN32(GetLastError());
            CHECK_AND_FAIL(res);
        }
    }
    void ExitSyncResources() {
        fence->Release();
        CloseHandle(event);
    }
};

static auto _ = Demo_Register("Triangle", [] { return new Demo_001_Triangle(); });

bool Demo_001_Triangle::DoInitResources() {
    bool vsync_on = false;

    // Create Swapchain 
    IDXGISwapChain1 * swap_chain1 = nullptr;
    DXGI_SWAP_CHAIN_DESC1 swap_chain_desc = {};
    swap_chain_desc.Width = window_width;
    swap_chain_desc.Height = window_height;
    swap_chain_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swap_chain_desc.Stereo = FALSE;
    swap_chain_desc.SampleDesc.Count = 1;
    swap_chain_desc.SampleDesc.Quality = 0;
    swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swap_chain_desc.BufferCount = FrameQueueLength;
    swap_chain_desc.Scaling = DXGI_SCALING_STRETCH;
    swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    swap_chain_desc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    swap_chain_desc.Flags = (vsync_on) ? 0 : DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;
    HRESULT res = dxgi_factory->CreateSwapChainForHwnd(direct_queue, render_window.hwnd, &swap_chain_desc, nullptr, nullptr, &swap_chain1);
    CHECK_AND_FAIL(res);

    swap_chain = reinterpret_cast<IDXGISwapChain4 *>(swap_chain1);

    res = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&direct_cmd_allocator)); CHECK_AND_FAIL(res);
    res = direct_cmd_allocator->Reset(); CHECK_AND_FAIL(res);
    res = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COMPUTE, IID_PPV_ARGS(&compute_cmd_allocator)); CHECK_AND_FAIL(res);
    res = compute_cmd_allocator->Reset(); CHECK_AND_FAIL(res);
    res = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COPY, IID_PPV_ARGS(&copy_cmd_allocator)); CHECK_AND_FAIL(res);
    res = copy_cmd_allocator->Reset(); CHECK_AND_FAIL(res);

    res = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_COPY, copy_cmd_allocator, nullptr, IID_PPV_ARGS(&copy_cmd_list));
    copy_cmd_list->Close();
    CHECK_AND_FAIL(res);
    
    for(uint32_t i = 0; i < FrameQueueLength; ++i) {
        res = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, direct_cmd_allocator, nullptr, IID_PPV_ARGS(&direct_cmd_list[i]));
        direct_cmd_list[i]->Close();
        CHECK_AND_FAIL(res);
    }
    
    InitSyncResources();

    // Create RTV DesriptorHeap for SwapChain RenderTargets
    {
        D3D12_DESCRIPTOR_HEAP_DESC heap_desc = {};
        heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        heap_desc.NumDescriptors = FrameQueueLength;
        heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        heap_desc.NodeMask = 0;

        device->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&rtv_heap));
    }
    
    // Create RTVs into Heap
    D3D12_CPU_DESCRIPTOR_HANDLE current_rtv_handle = rtv_heap->GetCPUDescriptorHandleForHeapStart();
    rtv_handle_increment_size = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    for(uint32_t n = 0; n < FrameQueueLength; ++n) {
        swap_chain->GetBuffer(n, IID_PPV_ARGS(&swap_chain_render_targets[n]));
        device->CreateRenderTargetView(swap_chain_render_targets[n], nullptr, current_rtv_handle);
        current_rtv_handle.ptr = SIZE_T(current_rtv_handle.ptr + INT64(rtv_handle_increment_size));
    }

    IDxcBlob * vertex_shader = Demo::CompileShaderFromFile(L"../code/src/shaders/demo001/simple_mesh.vert.hlsl", L"VSMain", L"vs_6_0");
    IDxcBlob * pixel_shader = Demo::CompileShaderFromFile(L"../code/src/shaders/demo001/simple_mesh.frag.hlsl", L"PSMain", L"ps_6_0");
    ASSERT(nullptr != vertex_shader);
    ASSERT(nullptr != pixel_shader);

    ID3DBlob * rs_blob = nullptr;
    ID3DBlob * rs_error_blob = nullptr;
    D3D12_VERSIONED_ROOT_SIGNATURE_DESC root_signature_desc = {};
    root_signature_desc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
    root_signature_desc.Desc_1_1 = {};
    root_signature_desc.Desc_1_1.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    res = D3D12SerializeVersionedRootSignature(&root_signature_desc, &rs_blob, &rs_error_blob);
    CHECK_AND_FAIL(res);
    if(nullptr != rs_error_blob) {
        ASSERT(nullptr != rs_error_blob);
        rs_error_blob->Release();
    }

    res = device->CreateRootSignature(0, rs_blob->GetBufferPointer(), rs_blob->GetBufferSize(), IID_PPV_ARGS(&root_signature));
    CHECK_AND_FAIL(res);

    {
        constexpr uint32_t VertexInputElemCount = 3;
        D3D12_INPUT_ELEMENT_DESC vertex_layout[VertexInputElemCount] = {};
        vertex_layout[0].SemanticName = "POSITION";
        vertex_layout[0].SemanticIndex = 0;
        vertex_layout[0].Format = DXGI_FORMAT_R32G32B32_FLOAT;
        vertex_layout[0].InputSlot = 0;
        vertex_layout[0].AlignedByteOffset = 0;
        vertex_layout[0].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
        vertex_layout[0].InstanceDataStepRate = 0;
        //
        vertex_layout[1].SemanticName = "COLOR";
        vertex_layout[1].SemanticIndex = 0;
        vertex_layout[1].Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        vertex_layout[1].InputSlot = 0;
        vertex_layout[1].AlignedByteOffset = offsetof(Vertex, col);
        vertex_layout[1].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
        vertex_layout[1].InstanceDataStepRate = 0;
        //
        vertex_layout[2].SemanticName = "TEXCOORD";
        vertex_layout[2].SemanticIndex = 0;
        vertex_layout[2].Format = DXGI_FORMAT_R32G32_FLOAT;
        vertex_layout[2].InputSlot = 0;
        vertex_layout[2].AlignedByteOffset = offsetof(Vertex, uv);
        vertex_layout[2].InputSlotClass = D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA;
        vertex_layout[2].InstanceDataStepRate = 0;

        D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc = {};
        pso_desc.pRootSignature = root_signature;
        pso_desc.VS.pShaderBytecode = vertex_shader->GetBufferPointer();
        pso_desc.VS.BytecodeLength = vertex_shader->GetBufferSize();
        pso_desc.PS.pShaderBytecode = pixel_shader->GetBufferPointer();
        pso_desc.PS.BytecodeLength = pixel_shader->GetBufferSize();
        pso_desc.BlendState.AlphaToCoverageEnable = FALSE;
        pso_desc.BlendState.IndependentBlendEnable = FALSE;

        const D3D12_RENDER_TARGET_BLEND_DESC defaultRenderTargetBlendDesc =
        {
            FALSE,FALSE,
            D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
            D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
            D3D12_LOGIC_OP_NOOP,
            D3D12_COLOR_WRITE_ENABLE_ALL,
        };
        for (UINT i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
            pso_desc.BlendState.RenderTarget[i] = defaultRenderTargetBlendDesc;

        pso_desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
        pso_desc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
        pso_desc.RasterizerState.FrontCounterClockwise = FALSE;
        pso_desc.RasterizerState.DepthBias = 0;
        pso_desc.RasterizerState.DepthBiasClamp = 0.0f;
        pso_desc.RasterizerState.SlopeScaledDepthBias = 0.0f;
        pso_desc.RasterizerState.DepthClipEnable = TRUE;
        pso_desc.RasterizerState.MultisampleEnable = FALSE;
        pso_desc.RasterizerState.AntialiasedLineEnable = FALSE;
        pso_desc.RasterizerState.ForcedSampleCount = 0;
        pso_desc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
        pso_desc.SampleMask = UINT_MAX;
        pso_desc.DepthStencilState.DepthEnable = FALSE;
        pso_desc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
        pso_desc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
        pso_desc.DepthStencilState.StencilEnable = FALSE;
        pso_desc.InputLayout.pInputElementDescs = vertex_layout;
        pso_desc.InputLayout.NumElements = VertexInputElemCount;
        pso_desc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
        pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        pso_desc.NumRenderTargets = 1;
        pso_desc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
        pso_desc.DSVFormat = DXGI_FORMAT_UNKNOWN;
        pso_desc.SampleDesc.Count = 1;
        pso_desc.SampleDesc.Quality = 0;
        pso_desc.NodeMask = 0;
        pso_desc.CachedPSO = {};
        pso_desc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
        res = device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&graphics_pso)); CHECK_AND_FAIL(res);
    }

    rs_blob->Release();

    vertex_shader->Release();
    pixel_shader->Release();

    GetTriangleMesh(&mesh);
    
    vertex_buffer_bytes = sizeof(Vertex) * mesh.vertices.size();
    index_buffer_bytes = sizeof(IndexType) * mesh.indices.size();

    // Vertex Buffer
    {
        // Buffer Allocation
        D3D12_HEAP_PROPERTIES   heap_props      = GetDefaultHeapProps(D3D12_HEAP_TYPE_DEFAULT);
        D3D12_RESOURCE_DESC     resource_desc   = GetBufferResourceDesc(vertex_buffer_bytes);
        res = device->CreateCommittedResource(&heap_props,
            D3D12_HEAP_FLAG_NONE,
            &resource_desc,
            D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER,
            nullptr,
            IID_PPV_ARGS(&vertex_buffer));
        CHECK_AND_FAIL(res);

        ID3D12Resource * staging_vertex_buffer = nullptr;

        // Staging
        D3D12_HEAP_PROPERTIES   staging_heap_props      = GetDefaultHeapProps(D3D12_HEAP_TYPE_UPLOAD);
        D3D12_RESOURCE_DESC     staging_resource_desc   = GetBufferResourceDesc(vertex_buffer_bytes);
        res = device->CreateCommittedResource(&staging_heap_props,
            D3D12_HEAP_FLAG_NONE,
            &staging_resource_desc,
            D3D12_RESOURCE_STATE_GENERIC_READ | D3D12_RESOURCE_STATE_COPY_SOURCE,
            nullptr,
            IID_PPV_ARGS(&staging_vertex_buffer));
        CHECK_AND_FAIL(res);

        D3D12_RANGE read_range = {}; read_range.Begin = 0; read_range.End = 0;
        using Byte = uint8_t;
        Byte * vertex_data_begin = nullptr;
        res = staging_vertex_buffer->Map(0, &read_range, reinterpret_cast<void**>(&vertex_data_begin)); CHECK_AND_FAIL(res);
        memcpy(vertex_data_begin, mesh.vertices.data(), vertex_buffer_bytes);
        staging_vertex_buffer->Unmap(0, nullptr); 

        // Copy to Buffer
        {
            D3D12_RESOURCE_BARRIER barrier_before = {};
            barrier_before.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier_before.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            barrier_before.Transition.pResource = vertex_buffer;
            barrier_before.Transition.StateBefore = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
            barrier_before.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
            direct_cmd_list[0]->Reset(direct_cmd_allocator, nullptr);
            direct_cmd_list[0]->ResourceBarrier(1, &barrier_before);

            direct_cmd_list[0]->CopyBufferRegion(vertex_buffer, 0, staging_vertex_buffer, 0, vertex_buffer_bytes);

            D3D12_RESOURCE_BARRIER barrier_after = {};
            barrier_after.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier_after.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            barrier_after.Transition.pResource = vertex_buffer;
            barrier_after.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
            barrier_after.Transition.StateAfter = D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER;
            direct_cmd_list[0]->ResourceBarrier(1, &barrier_after);
            direct_cmd_list[0]->Close();
            
            ID3D12CommandList * execute_cmds[1] = { direct_cmd_list[0] };
            direct_queue->ExecuteCommandLists(1, execute_cmds);

            WaitForQueue(direct_queue);
        }

        // Delete Staging Buffer
        staging_vertex_buffer->Release();
    }
    // Index Buffer 
    {
        // Buffer Allocation
        D3D12_HEAP_PROPERTIES   heap_props      = GetDefaultHeapProps(D3D12_HEAP_TYPE_DEFAULT);
        D3D12_RESOURCE_DESC     resource_desc   = GetBufferResourceDesc(index_buffer_bytes);
        res = device->CreateCommittedResource(&heap_props,
            D3D12_HEAP_FLAG_NONE,
            &resource_desc,
            D3D12_RESOURCE_STATE_INDEX_BUFFER,
            nullptr,
            IID_PPV_ARGS(&index_buffer));
        CHECK_AND_FAIL(res);

        ID3D12Resource * staging_index_buffer = nullptr;

        // Staging
        D3D12_HEAP_PROPERTIES   staging_heap_props      = GetDefaultHeapProps(D3D12_HEAP_TYPE_UPLOAD);
        D3D12_RESOURCE_DESC     staging_resource_desc   = GetBufferResourceDesc(index_buffer_bytes);
        res = device->CreateCommittedResource(&staging_heap_props,
            D3D12_HEAP_FLAG_NONE,
            &staging_resource_desc,
            D3D12_RESOURCE_STATE_GENERIC_READ | D3D12_RESOURCE_STATE_COPY_SOURCE,
            nullptr,
            IID_PPV_ARGS(&staging_index_buffer));
        CHECK_AND_FAIL(res);

        D3D12_RANGE read_range = {}; read_range.Begin = 0; read_range.End = 0;
        using Byte = uint8_t;
        Byte * index_data_begin = nullptr;
        staging_index_buffer->Map(0, &read_range, reinterpret_cast<void**>(&index_data_begin));
        memcpy(index_data_begin, mesh.indices.data(), index_buffer_bytes);
        staging_index_buffer->Unmap(0, nullptr);

        // Copy to Buffer
        {
            D3D12_RESOURCE_BARRIER barrier_before = {};
            barrier_before.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier_before.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            barrier_before.Transition.pResource = index_buffer;
            barrier_before.Transition.StateBefore = D3D12_RESOURCE_STATE_INDEX_BUFFER;
            barrier_before.Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
            
            direct_cmd_list[0]->Reset(direct_cmd_allocator, nullptr);
            direct_cmd_list[0]->ResourceBarrier(1, &barrier_before);

            direct_cmd_list[0]->CopyBufferRegion(index_buffer, 0, staging_index_buffer, 0, index_buffer_bytes);

            D3D12_RESOURCE_BARRIER barrier_after = {};
            barrier_after.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barrier_after.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
            barrier_after.Transition.pResource = index_buffer;
            barrier_after.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
            barrier_after.Transition.StateAfter = D3D12_RESOURCE_STATE_INDEX_BUFFER;
            direct_cmd_list[0]->ResourceBarrier(1, &barrier_after);
            direct_cmd_list[0]->Close();
            
            ID3D12CommandList * execute_cmds[1] = { direct_cmd_list[0] };
            direct_queue->ExecuteCommandLists(1, execute_cmds);
            
            WaitForQueue(direct_queue);
        }

        // Delete Staging Buffer
        staging_index_buffer->Release();
    }

    return true;
}

bool Demo_001_Triangle::DoExitResources() { 
    WaitForQueue(direct_queue);

    root_signature->Release();
    graphics_pso->Release();

    vertex_buffer->Release();
    index_buffer->Release();

    for(uint32_t n = 0; n < FrameQueueLength; ++n) {
        swap_chain_render_targets[n]->Release();
    }

    rtv_heap->Release();
    
    ExitSyncResources();

    copy_cmd_list->Release();
    for(uint32_t i = 0; i < FrameQueueLength; ++i) {
        direct_cmd_list[i]->Release();
    }

    direct_cmd_allocator->Release();
    compute_cmd_allocator->Release();
    copy_cmd_allocator->Release();

    swap_chain->Release();

    return true;
}

void Demo_001_Triangle::OnRender() {

    // Populate Command List
    ID3D12GraphicsCommandList * current_cmd_list = direct_cmd_list[frame_index];
    current_cmd_list->Reset(direct_cmd_allocator, nullptr);
    {
        current_cmd_list->SetGraphicsRootSignature(root_signature);
            
        // Set Viewport Scissor
        {
            D3D12_VIEWPORT viewport;
            viewport.TopLeftX = 0;
            viewport.TopLeftY = 0;
            viewport.Width = (FLOAT)window_width;
            viewport.Height = (FLOAT)window_height;
            viewport.MinDepth = 0.0f;
            viewport.MaxDepth = 1.0f;
            current_cmd_list->RSSetViewports(1, &viewport);

            D3D12_RECT scissor_rect = {};
            scissor_rect.left = 0;
            scissor_rect.top = 0;
            scissor_rect.right = window_width;
            scissor_rect.bottom = window_height;
            current_cmd_list->RSSetScissorRects(1, &scissor_rect);
        } 

        D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle = rtv_heap->GetCPUDescriptorHandleForHeapStart();
        rtv_handle.ptr = rtv_handle.ptr + SIZE_T(frame_index * rtv_handle_increment_size);

        // Transition RTV from D3D12_RESOURCE_STATE_PRESENT to D3D12_RESOURCE_STATE_RENDER_TARGET.
        D3D12_RESOURCE_BARRIER barrier_before_render = {};
        barrier_before_render.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier_before_render.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier_before_render.Transition.pResource = swap_chain_render_targets[frame_index];
        barrier_before_render.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
        barrier_before_render.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
        current_cmd_list->ResourceBarrier(1, &barrier_before_render);

        // Set RenderTargets
        current_cmd_list->OMSetRenderTargets(1, &rtv_handle, FALSE, nullptr);
        const float clear_color[] = { 0.0f, 0.05f, 0.05f, 1.0f };
        current_cmd_list->ClearRenderTargetView(rtv_handle, clear_color, 0, nullptr);

        current_cmd_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        // Set Vertex and Index Buffer

        D3D12_VERTEX_BUFFER_VIEW vbv = {};
        vbv.BufferLocation = vertex_buffer->GetGPUVirtualAddress();
        vbv.SizeInBytes = (uint32_t)vertex_buffer_bytes;
        vbv.StrideInBytes = sizeof(Vertex);
        current_cmd_list->IASetVertexBuffers(0, 1, &vbv);

        D3D12_INDEX_BUFFER_VIEW ibv = {};
        ibv.BufferLocation = index_buffer->GetGPUVirtualAddress();
        ibv.Format = IndexBufferFormat;
        ibv.SizeInBytes = (uint32_t)index_buffer_bytes;
            current_cmd_list->IASetIndexBuffer(&ibv);

        // Set PSO
        current_cmd_list->SetPipelineState(graphics_pso);

        // Draw Indexed
        current_cmd_list->DrawIndexedInstanced((uint32_t)mesh.indices.size(), 1, 0, 0, 0);

        // Transition RTV from D3D12_RESOURCE_STATE_RENDER_TARGET to D3D12_RESOURCE_STATE_PRESENT
        D3D12_RESOURCE_BARRIER barrier_before_present = {};
        barrier_before_present.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier_before_present.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
        barrier_before_present.Transition.pResource = swap_chain_render_targets[frame_index];
        barrier_before_present.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
        barrier_before_present.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
        current_cmd_list->ResourceBarrier(1, &barrier_before_present);
    }
    current_cmd_list->Close();

    // Execute Command List
    ID3D12CommandList * execute_cmds[1] = { current_cmd_list };
    direct_queue->ExecuteCommandLists(1, execute_cmds);
        
    // Present the frame.
    swap_chain->Present(1, 0);

    MoveToNextFrame(direct_queue);
}

void Demo_001_Triangle::OnUpdate() {}
