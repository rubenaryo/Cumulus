/*----------------------------------------------
Ruben Young (rubenaryo@gmail.com)
Date : 2025/11
Description : Helpers for initializing/using ImGui
----------------------------------------------*/

#include <Core/MuonImgui.h>
#include <Core/DXCore.h>
#include <Core/DescriptorHeap.h>
#include <Utils/Utils.h>

#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx12.h>

namespace Muon
{

bool ImguiInit()
{
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    ImguiInitWin32(Muon::GetHwnd());

    // Setup Platform/Renderer backends
    ImGui_ImplDX12_InitInfo init_info = {};
    init_info.Device = Muon::GetDevice();
    init_info.CommandQueue = Muon::GetCommandQueue();
    init_info.NumFramesInFlight = 2;
    init_info.RTVFormat = Muon::GetRTVFormat();

    // Allocating SRV descriptors (for textures) is up to the application, so we provide callbacks.
    // The example_win32_directx12/main.cpp application include a simple free-list based allocator.
    DescriptorHeap* pSRVHeap = GetSRVHeap();
    if (!pSRVHeap)
    {
        Print("Fatal Error: Failed to get SRV Heap for when initilizing ImGui.\n");
        return false;
    }

    init_info.SrvDescriptorHeap = pSRVHeap->GetHeap();
    init_info.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_handle)
        { 
            DescriptorHeap* pSRVHeap = GetSRVHeap();
            pSRVHeap->Allocate(out_cpu_handle, out_gpu_handle); 
        };
    
    init_info.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle)
        {
            DescriptorHeap* pSRVHeap = GetSRVHeap();
            pSRVHeap->Free(cpu_handle, gpu_handle);
        };

    return ImGui_ImplDX12_Init(&init_info);
}

bool ImguiInitWin32(HWND hwnd)
{
    return ImGui_ImplWin32_Init(hwnd);
}

void ImguiShutdown()
{
    ImGui_ImplDX12_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}

void ImguiNewFrame()
{
    // (Your code process and dispatch Win32 messages)
    // Start the Dear ImGui frame
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    ImGui::ShowDemoWindow(); // Show demo window! :)
}

void ImguiRender()
{
    // TODO: pass in cmd list as param
    ImGui::Render();
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), Muon::GetCommandList());
}

}
