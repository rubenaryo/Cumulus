/*----------------------------------------------
Ruben Young (rubenaryo@gmail.com)
Date : 2025/3
Description : Protocol and device resources for DX12
...
Most of this initialization code is adapted from https://github.com/microsoft/DirectX-Graphics-Samples
or "Introduction to 3D Game Programming with DirectX 12" by Frank Luna
----------------------------------------------*/
#include <Utils/Utils.h>
#include <Core/DXCore.h>
#include <Core/ThrowMacros.h>
#include <Core/CBufferStructs.h>

#include <d3dx12.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <dxgidebug.h>
#include <stdint.h>
#include <wrl/client.h>
#include <sstream>

#define CHECK_SUCCESS(s, msg)       \
do {                                \
    if (!s)                         \
    {                               \
        Muon::Print(msg);           \
        return false;               \
    }                               \
} while (0)                         \

namespace Muon
{
    Microsoft::WRL::ComPtr<ID3D12Device> gDevice;

    Microsoft::WRL::ComPtr<ID3D12Fence> gFence;
    UINT64 gFenceVal = 0;

    UINT gRTVSize = 0;
    UINT gDSVSize = 0;
    UINT gCBVSize = 0;
    UINT gMSAAQuality = 0;

    Microsoft::WRL::ComPtr<ID3D12CommandQueue> gCommandQueue;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> gCommandAllocator;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> gCommandList;

    DXGI_FORMAT BackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    DXGI_FORMAT DepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
    const int SWAP_CHAIN_BUFFER_COUNT = 2;
    int CurrentBackBuffer = 0;

    Microsoft::WRL::ComPtr<IDXGISwapChain3> gSwapChain;
    Microsoft::WRL::ComPtr<ID3D12Resource> gSwapChainBuffers[SWAP_CHAIN_BUFFER_COUNT];
    Microsoft::WRL::ComPtr<ID3D12Resource> gDepthStencilBuffer;

    D3D12_VIEWPORT gViewport = { 0 };

    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> gRTVHeap;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> gDSVHeap;

    tagRECT gScissorRect;

    HWND gHwnd;

    D3D_FEATURE_LEVEL gFeatureLevel;
    const wchar_t* gFeatureLevelStr = nullptr;

    /////////////////////////////////////////////////////////////////////
    // Accessors

    ID3D12Device* GetDevice() { return gDevice.Get(); }
    ID3D12Fence* GetFence() { return gFence.Get(); }
    UINT GetRTVDescriptorSize() { return gRTVSize; }
    UINT GetDSVDescriptorSize() { return gDSVSize; }
    UINT GetCBVDescriptorSize() { return gCBVSize; }
    UINT GetMSAAQualityLevel() { return gMSAAQuality; }
    ID3D12CommandQueue* GetCommandQueue() { return gCommandQueue.Get(); }
    ID3D12GraphicsCommandList* GetCommandList() { return gCommandList.Get(); }
    ID3D12CommandAllocator* GetCommandAllocator() { return gCommandAllocator.Get(); }
    IDXGISwapChain3* GetSwapChain() { return gSwapChain.Get(); }
    DXGI_FORMAT GetBackBufferFormat() { return BackBufferFormat; }
    DXGI_FORMAT GetDepthStencilFormat() { return DepthStencilFormat; }

    /////////////////////////////////////////////////////////////////////
    /// Interface Utility Functions

    D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferView()
    {
        return CD3DX12_CPU_DESCRIPTOR_HANDLE(
            gRTVHeap->GetCPUDescriptorHandleForHeapStart(),
            CurrentBackBuffer,
            gRTVSize
        );
    }

    D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView()
    {
        return gDSVHeap->GetCPUDescriptorHandleForHeapStart();
    }

    const wchar_t* GetDirect3DNameForFeatureLevel(D3D_FEATURE_LEVEL level)
    {
        switch (level)
        {
        case D3D_FEATURE_LEVEL_12_2: return L"Direct3D 12.2";
        case D3D_FEATURE_LEVEL_12_1: return L"Direct3D 12.1";
        case D3D_FEATURE_LEVEL_12_0: return L"Direct3D 12.0";
        }

        return L"Direct3D ???";
    }

    /////////////////////////////////////////////////////////////////////
    /// Init Steps

    bool IsDirectXRaytracingSupported(ID3D12Device* testDevice)
    {
        D3D12_FEATURE_DATA_D3D12_OPTIONS5 featureSupport = {};

        if (FAILED(testDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &featureSupport, sizeof(featureSupport))))
            return false;

        return featureSupport.RaytracingTier != D3D12_RAYTRACING_TIER_NOT_SUPPORTED;
    }

    bool EnableDX12DebugFeatures(DWORD& out_dxgiFactoryFlags)
    {
        using Microsoft::WRL::ComPtr;

        ComPtr<ID3D12Debug> debugInterface;
        if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugInterface))))
        {
            debugInterface->EnableDebugLayer();

            uint32_t useGPUBasedValidation = 1; // always validate for now
            if (useGPUBasedValidation)
            {
                Microsoft::WRL::ComPtr<ID3D12Debug1> debugInterface1;
                if (SUCCEEDED((debugInterface->QueryInterface(IID_PPV_ARGS(&debugInterface1)))))
                {
                    debugInterface1->SetEnableGPUBasedValidation(true);
                }
            }
        }
        else
        {
            Muon::Print("WARNING:  Unable to enable D3D12 debug validation layer\n");
            return false;
        }

    #if MN_DEBUG
        ComPtr<IDXGIInfoQueue> dxgiInfoQueue;
        if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(dxgiInfoQueue.GetAddressOf()))))
        {
            out_dxgiFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;

            dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, true);
            dxgiInfoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, true);

            DXGI_INFO_QUEUE_MESSAGE_ID hide[] =
            {
                80 /* IDXGISwapChain33::GetContainingOutput: The swapchain's adapter does not control the output on which the swapchain's window resides. */,
            };
            DXGI_INFO_QUEUE_FILTER filter = {};
            filter.DenyList.NumIDs = _countof(hide);
            filter.DenyList.pIDList = hide;
            dxgiInfoQueue->AddStorageFilterEntries(DXGI_DEBUG_DXGI, &filter);
        }
    #endif
        return true;
    }

    bool CreateDevice(IDXGIFactory6* pFactory, Microsoft::WRL::ComPtr<ID3D12Device>& out_device)
    {
        Microsoft::WRL::ComPtr<IDXGIAdapter1> pAdapter;
        Microsoft::WRL::ComPtr<ID3D12Device> pTempDevice;
        SIZE_T MaxSize = 0;

        // Determine best adapter and device
        for (uint32_t Idx = 0; DXGI_ERROR_NOT_FOUND != pFactory->EnumAdapters1(Idx, pAdapter.ReleaseAndGetAddressOf()); ++Idx)
        {
            DXGI_ADAPTER_DESC1 desc;
            pAdapter->GetDesc1(&desc);

            // Is a software adapter?
            if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
                continue;

            // Can create a D3D12 device?
            Microsoft::WRL::ComPtr<ID3D12Device> testDevice;
            if (FAILED(D3D12CreateDevice(pAdapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(testDevice.GetAddressOf()))))
            {
                Muon::Print("Error: Failed to create device!\n");
                continue;
            }

            if (!IsDirectXRaytracingSupported(testDevice.Get()))
            {
                Muon::Print("Warning: Found device does NOT support DXR raytracing.\n");
            }

            // By default, search for the adapter with the most memory because that's usually the dGPU.
            if (desc.DedicatedVideoMemory < MaxSize)
                continue;

            MaxSize = desc.DedicatedVideoMemory;
            pTempDevice = testDevice;
            Muon::Printf(L"Selected GPU:  %s (%u MB)\n", desc.Description, desc.DedicatedVideoMemory >> 20);
        }

        if (pTempDevice.Get() != nullptr)
        {
            out_device = pTempDevice;
        }
        return out_device.Get() != nullptr;
    }

    bool CreateFence(ID3D12Device* pDevice, Microsoft::WRL::ComPtr<ID3D12Fence>& out_fence)
    {
        if (!pDevice)
        {
            Muon::Print("Failed to create fence because of null device!\n");
            return false;
        }

        HRESULT hr = pDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(out_fence.GetAddressOf()));
        COM_EXCEPT(hr);

        gFenceVal = 1;

        FlushCommandQueue();
        UpdateBackBufferIndex();
        return SUCCEEDED(hr);
    }

    bool GetDescriptorSizes(ID3D12Device* pDevice, UINT* out_rtv, UINT* out_dsv, UINT* out_cbv)
    {
        if (!pDevice)
        {
            Muon::Print("Failed to get descriptor sizes because of null device!\n");
            return false;
        }

        *out_rtv = pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
        *out_dsv = pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
        *out_cbv = pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
        return true;
    }

    bool DetermineMSAAQuality(ID3D12Device* pDevice, UINT* out_quality)
    {
        D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msQualityLevels;
        msQualityLevels.Format = GetBackBufferFormat();
        msQualityLevels.SampleCount = 4;
        msQualityLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
        msQualityLevels.NumQualityLevels = 0;
        HRESULT hr = pDevice->CheckFeatureSupport(
            D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,
            &msQualityLevels,
            sizeof(msQualityLevels));

        *out_quality = msQualityLevels.NumQualityLevels;
        return SUCCEEDED(hr);
    }

    bool CreateCommandObjects(ID3D12Device* pDevice,
        Microsoft::WRL::ComPtr<ID3D12CommandQueue>& out_queue,
        Microsoft::WRL::ComPtr<ID3D12CommandAllocator>& out_alloc,
        Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList>& out_list)
    {
        HRESULT hr;

        D3D12_COMMAND_QUEUE_DESC queueDesc = {};
        queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
        hr = pDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(out_queue.GetAddressOf()));
        COM_EXCEPT(hr);

        hr = pDevice->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(out_alloc.GetAddressOf()));
        COM_EXCEPT(hr);

        hr = pDevice->CreateCommandList(
            0,
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            out_alloc.Get(), // Associated command allocator
            nullptr,         // Initial PipelineStateObject
            IID_PPV_ARGS(out_list.GetAddressOf()));
        COM_EXCEPT(hr);

        // Start off in a closed state.  This is because the first time we refer 
        // to the command list we will Reset it, and it needs to be closed before
        // calling Reset.
        out_list->Close();

        return SUCCEEDED(hr);
    }

    bool CreateSwapChain(ID3D12Device* pDevice, IDXGIFactory6* pFactory, ID3D12CommandQueue* pQueue, HWND hwnd, int width, int height, Microsoft::WRL::ComPtr<IDXGISwapChain3>& out_swapchain)
    {
        DXGI_SWAP_CHAIN_DESC1 sd = { 0 };
        sd.BufferCount = SWAP_CHAIN_BUFFER_COUNT;
        sd.Width = width;
        sd.Height = height;
        sd.Format = BackBufferFormat;
        sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
        sd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
        sd.SampleDesc.Count = 1;

        Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain;
        HRESULT hr = pFactory->CreateSwapChainForHwnd(
            pQueue,
            hwnd,
            &sd,
            nullptr,
            nullptr,
            &swapChain
        );
        COM_EXCEPT(hr);

        hr = swapChain.As(&out_swapchain);
        CurrentBackBuffer = out_swapchain->GetCurrentBackBufferIndex();

        return SUCCEEDED(hr);
    }

    bool CreateDescriptorHeaps(ID3D12Device* pDevice,
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& out_rtv,
        Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& out_dsv)
    {
        D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
        rtvHeapDesc.NumDescriptors = SWAP_CHAIN_BUFFER_COUNT;
        rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        rtvHeapDesc.NodeMask = 0;

        HRESULT hr = pDevice->CreateDescriptorHeap(
            &rtvHeapDesc, IID_PPV_ARGS(out_rtv.GetAddressOf()));
        COM_EXCEPT(hr);

        D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
        dsvHeapDesc.NumDescriptors = 1;
        dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
        dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
        dsvHeapDesc.NodeMask = 0;
        hr = pDevice->CreateDescriptorHeap(
            &dsvHeapDesc, IID_PPV_ARGS(out_dsv.GetAddressOf()));
        COM_EXCEPT(hr);

        return SUCCEEDED(hr);
    }

    bool CreateRenderTargetView(ID3D12Device* pDevice, IDXGISwapChain3* pSwapChain, Microsoft::WRL::ComPtr<ID3D12Resource> pSwapChainBuffers[SWAP_CHAIN_BUFFER_COUNT])
    {
        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(gRTVHeap->GetCPUDescriptorHandleForHeapStart());
        HRESULT hr;
        for (UINT i = 0; i != SWAP_CHAIN_BUFFER_COUNT; ++i)
        {
            hr = pSwapChain->GetBuffer(i, IID_PPV_ARGS(pSwapChainBuffers[i].GetAddressOf()));
            COM_EXCEPT(hr);

            pDevice->CreateRenderTargetView(pSwapChainBuffers[i].Get(), nullptr, rtvHandle);
            rtvHandle.Offset(1, gRTVSize);
        }

        return true;
    }

    bool CreateDepthStencilBuffer(ID3D12Device* pDevice, ID3D12GraphicsCommandList* pCommandList, ID3D12CommandQueue* pCommandQueue, int width, int height, Microsoft::WRL::ComPtr<ID3D12Resource>& out_depthStencilBuffer)
    {
        if (!pDevice || !pCommandList)
        {
            Muon::Print("Error: Failed to create depth stencil buffer! Null device or command list\n");
            return false;
        }

        D3D12_RESOURCE_DESC depthStencilDesc;
        depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        depthStencilDesc.Alignment = 0;
        depthStencilDesc.Width = width;
        depthStencilDesc.Height = height;
        depthStencilDesc.DepthOrArraySize = 1;
        depthStencilDesc.MipLevels = 1;
        depthStencilDesc.SampleDesc.Count = 1;
        depthStencilDesc.SampleDesc.Quality = GetMSAAQualityLevel();
        depthStencilDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
        depthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
        depthStencilDesc.Format = DepthStencilFormat;

        D3D12_CLEAR_VALUE optClear;
        optClear.Format = DepthStencilFormat;
        optClear.DepthStencil.Depth = 1.0f;
        optClear.DepthStencil.Stencil = 0;
        HRESULT hr = pDevice->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
            D3D12_HEAP_FLAG_NONE,
            &depthStencilDesc,
            D3D12_RESOURCE_STATE_COMMON,
            &optClear,
            IID_PPV_ARGS(out_depthStencilBuffer.GetAddressOf()));
        COM_EXCEPT(hr);

        // Create a depth stencil view
        D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
        dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
        dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        dsvDesc.Format = DepthStencilFormat;
        dsvDesc.Texture2D.MipSlice = 0;
        pDevice->CreateDepthStencilView(out_depthStencilBuffer.Get(), &dsvDesc, DepthStencilView());

        // Transition from initial -> depth buffer use
        pCommandList->Reset(GetCommandAllocator(), nullptr);

        pCommandList->ResourceBarrier(1,
            &CD3DX12_RESOURCE_BARRIER::Transition(out_depthStencilBuffer.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE));

        return SUCCEEDED(hr);
    }

    bool SetViewport(ID3D12GraphicsCommandList* pCommandList, int x, int y, int width, int height, float minDepth, float maxDepth)
    {
        if (!pCommandList)
        {
            Muon::Print("Error: Failed to set viewport due to null command list!\n");
            return false;
        }

        D3D12_VIEWPORT& vp = gViewport;
        vp.TopLeftX = (float)x;
        vp.TopLeftY = (float)y;
        vp.Width = (float)width;
        vp.Height = (float)height;
        vp.MinDepth = minDepth;
        vp.MaxDepth = maxDepth;

        pCommandList->RSSetViewports(1, &vp);
        return true;
    }

    bool SetScissorRects(ID3D12GraphicsCommandList* pCommandList, long left, long top, long right, long bottom)
    {
        if (!pCommandList)
            return false;

        gScissorRect.left = left;
        gScissorRect.top = top;
        gScissorRect.right = right;
        gScissorRect.bottom = bottom;
        pCommandList->RSSetScissorRects(1, &gScissorRect);
        return true;
    }

    /////////////////////////////////////////////////////////////////////

    bool ResetCommandList(ID3D12PipelineState* pInitialPipelineState)
    {
        ID3D12CommandAllocator* pAllocator = GetCommandAllocator();
        ID3D12GraphicsCommandList* pCommandList = GetCommandList();
        if (!pAllocator || !pCommandList)
            return false;

        FlushCommandQueue();

        HRESULT hr = pAllocator->Reset();
        COM_EXCEPT(hr);

        hr = pCommandList->Reset(pAllocator, pInitialPipelineState);
        COM_EXCEPT(hr);

        return SUCCEEDED(hr);
    }

    bool CloseCommandList()
    {
        ID3D12GraphicsCommandList* pCommandList = GetCommandList();
        return pCommandList && SUCCEEDED(pCommandList->Close());
    }

    bool PrepareForRender()
    {
        ID3D12GraphicsCommandList* pCommandList = GetCommandList();

        //pCommandList->SetGraphicsRootSignature(gRootSig.Get());
        pCommandList->RSSetViewports(1, &gViewport);
        pCommandList->RSSetScissorRects(1, &gScissorRect);

        // Set back buffer as render target
        pCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(gSwapChainBuffers[CurrentBackBuffer].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

        CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(gRTVHeap->GetCPUDescriptorHandleForHeapStart(), CurrentBackBuffer, gRTVSize);
        pCommandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

        // Clear the back buffer
        const float clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
        pCommandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

        return true;
    }

    bool FinalizeRender()
    {
        // Now set back buffer as present target
        ID3D12GraphicsCommandList* pCommandList = GetCommandList();
        pCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(gSwapChainBuffers[CurrentBackBuffer].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
        return true;
    }

    bool ExecuteCommandList()
    {
        if (!GetCommandList())
            return false;

        ID3D12CommandList* lists[] = { GetCommandList() };
        GetCommandQueue()->ExecuteCommandLists(_countof(lists), lists);
        return true;
    }

    bool Present()
    {
        if (!GetSwapChain())
            return false;

        HRESULT hr = GetSwapChain()->Present(1, 0);
        return SUCCEEDED(hr);
    }

    bool FlushCommandQueue()
    {
        const UINT64 currFence = gFenceVal;
        HRESULT hr = GetCommandQueue()->Signal(gFence.Get(), currFence);
        gFenceVal++;

        if (gFence->GetCompletedValue() < currFence)
        {
            HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
            if (!eventHandle)
            {
                hr = HRESULT_FROM_WIN32(GetLastError());
                COM_EXCEPT(hr);
            }
            hr = gFence->SetEventOnCompletion(currFence, eventHandle);
            WaitForSingleObject(eventHandle, INFINITE);
            CloseHandle(eventHandle);
        }

        return SUCCEEDED(hr);
    }

    bool UpdateBackBufferIndex()
    {
        if (!GetSwapChain())
            return false;

        CurrentBackBuffer = GetSwapChain()->GetCurrentBackBufferIndex();
        return true;
    }

    bool CheckFeatureLevel(ID3D12Device* pDevice, D3D_FEATURE_LEVEL& outHighestLevel, const wchar_t** ppFeatureLevelStr)
    {
        const D3D_FEATURE_LEVEL featureLevelsList[] = {
            D3D_FEATURE_LEVEL_12_2,
            D3D_FEATURE_LEVEL_12_1,
            D3D_FEATURE_LEVEL_12_0,
        };

        D3D12_FEATURE_DATA_FEATURE_LEVELS featureLevels = {};
        featureLevels.NumFeatureLevels = _countof(featureLevelsList);
        featureLevels.pFeatureLevelsRequested = featureLevelsList;

        HRESULT hr = pDevice->CheckFeatureSupport(D3D12_FEATURE_FEATURE_LEVELS,
            &featureLevels,
            sizeof(featureLevels));

        if (FAILED(hr))
            return false;

        outHighestLevel = featureLevels.MaxSupportedFeatureLevel;
        *ppFeatureLevelStr = GetDirect3DNameForFeatureLevel(outHighestLevel);
        return true;
    }

    bool UpdateTitleBar(uint32_t fps, uint32_t frameCount)
    {
        // Update title bar every 120 frames
        if (frameCount % 120 == 0)
            return false;

        std::wstringstream wss;

        // Window Information
        wss << L"Width: " << gViewport.Width << 
            L"    Height: " << gViewport.Height <<
            L"    FPS: " << fps;

        wss << "    " << gFeatureLevelStr;

        // MSAA Level
        //wss << L"    " << mMSAASampleCount << L"xMSAA";

        SetWindowText(gHwnd, wss.str().c_str());
        return true;
    }

    bool InitDX12(HWND hwnd, int width, int height)
    {
        using Microsoft::WRL::ComPtr;

        ComPtr<ID3D12Device> pDevice;
        DWORD dxgiFactoryFlags = 0;
        HRESULT hr;
        bool success = true;

        uint32_t debugMode = 1; // Always debug for now
        if (debugMode)
        {
            success = EnableDX12DebugFeatures(dxgiFactoryFlags);
            CHECK_SUCCESS(success, "Warning: Failed to enable DX12 Debug Features!\n");
        }

        // Obtain the DXGI factory
        ComPtr<IDXGIFactory6> dxgiFactory;
        hr = CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&dxgiFactory));
        COM_EXCEPT(hr);

        D3D12EnableExperimentalFeatures(0, nullptr, nullptr, nullptr);

        success = CreateDevice(dxgiFactory.Get(), pDevice);
        CHECK_SUCCESS(success, "Error: Failed to create DX12 Device!\n");

        if (!pDevice)
        {
            // TODO: Fall back to WARP.
            return false;
        }

        gDevice = pDevice;
        gHwnd = hwnd;

        success &= GetDescriptorSizes(GetDevice(), &gRTVSize, &gDSVSize, &gCBVSize);
        CHECK_SUCCESS(success, "Error: Failed to get descriptor sizes!\n");

        success &= CheckFeatureLevel(GetDevice(), gFeatureLevel, &gFeatureLevelStr);
        CHECK_SUCCESS(success, "Error: Failed to get feature level!\n");

        //success &= DetermineMSAAQuality(GetDevice(), &gMSAAQuality);
        //CHECK_SUCCESS(success, "Error: Failed to determine MSAA quality!");

        success &= CreateCommandObjects(GetDevice(), gCommandQueue, gCommandAllocator, gCommandList);
        CHECK_SUCCESS(success, "Error: Failed to create command objects!\n");

        success &= CreateSwapChain(GetDevice(), dxgiFactory.Get(), GetCommandQueue(), hwnd, width, height, gSwapChain);
        CHECK_SUCCESS(success, "Error: Failed to create swap chain!\n");

        success &= CreateFence(GetDevice(), gFence);
        CHECK_SUCCESS(success, "Error: Failed to create fence!\n");

        success &= CreateDescriptorHeaps(GetDevice(), gRTVHeap, gDSVHeap);
        CHECK_SUCCESS(success, "Error: Failed to create descriptor heaps!\n");

        success &= CreateRenderTargetView(GetDevice(), GetSwapChain(), gSwapChainBuffers);
        CHECK_SUCCESS(success, "Error: Failed to create render target view!\n");

        success &= CreateDepthStencilBuffer(GetDevice(), GetCommandList(), GetCommandQueue(), width, height, gDepthStencilBuffer);
        CHECK_SUCCESS(success, "Error: Failed to create depth stencil buffer!\n");

        success &= SetViewport(GetCommandList(), 0, 0, width, height, 0.001f, 1000.0f);
        CHECK_SUCCESS(success, "Error: Failed to set viewport!\n");

        success &= SetScissorRects(GetCommandList(), 0, 0, width, height);
        CHECK_SUCCESS(success, "Error: Failed to set scissor rects!\n");

        // We've written a bunch of commands, close the list and execute it.
        hr = GetCommandList()->Close();
        COM_EXCEPT(hr);

        ExecuteCommandList();
        FlushCommandQueue();
        UpdateBackBufferIndex();

        return success;
    }

#if MN_DEBUG
    void ReportLiveD3D12Objects()
    {
        Microsoft::WRL::ComPtr<IDXGIDebug1> dxgiDebug;
        if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug))))
        {
            Muon::Print("\n========================================\n");
            Muon::Print("D3D12 & DXGI Live Objects Report\n");
            Muon::Print("========================================\n");
            DXGI_DEBUG_RLO_FLAGS flags = (DXGI_DEBUG_RLO_FLAGS)(DXGI_DEBUG_RLO_DETAIL | DXGI_DEBUG_RLO_IGNORE_INTERNAL);
            HRESULT hr = dxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL, flags);

            if (SUCCEEDED(hr))
            {
                Muon::Print("If you see no DXGI/D3D12 warnings above, cleanup was successful\n");
            }
            else
            {
                Muon::Printf("Failed to generate report. HRESULT: 0x%08X\n", hr);
            }
            Muon::Print("========================================\n\n");
        }
    }
#endif

    bool DestroyDX12()
    {
        // Ensure all GPU work is complete before releasing resources
        FlushCommandQueue();

        // Release all resources - ComPtr handles this automatically
        for (int i = 0; i < SWAP_CHAIN_BUFFER_COUNT; ++i)
        {
            gSwapChainBuffers[i].Reset();
        }

        gDepthStencilBuffer.Reset();
        gRTVHeap.Reset();
        gDSVHeap.Reset();
        gCommandList.Reset();
        gCommandAllocator.Reset();
        gCommandQueue.Reset();
        gSwapChain.Reset();
        gFence.Reset();
        gDevice.Reset();

    #if MN_DEBUG
        // Validate everything has been freed
        ReportLiveD3D12Objects();
    #endif

        // Reset global state
        gFeatureLevelStr = nullptr;
        gFenceVal = 0;
        gRTVSize = 0;
        gDSVSize = 0;
        gCBVSize = 0;
        gMSAAQuality = 0;
        CurrentBackBuffer = 0;
        gHwnd = nullptr;

        return true;
    }

#undef CHECK_SUCCESS
}