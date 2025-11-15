/*----------------------------------------------
Ruben Young (rubenaryo@gmail.com)
Date : 2025/11
Description : Helpers for initializing/using ImGui
----------------------------------------------*/

#include <Core/MuonImgui.h>

#include <Core/DXCore.h>

#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx12.h>

namespace Muon
{

void ImguiInit()
{
    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();

    // Setup Platform/Renderer backends
    ImGui_ImplDX12_InitInfo init_info = {};
    init_info.Device = Muon::GetDevice();
    init_info.CommandQueue = Muon::GetCommandQueue();
    init_info.NumFramesInFlight = 2;
    init_info.RTVFormat = Muon::GetRTVFormat();

    // Allocating SRV descriptors (for textures) is up to the application, so we provide callbacks.
    // The example_win32_directx12/main.cpp application include a simple free-list based allocator.
    //init_info.SrvDescriptorHeap = YOUR_SRV_DESC_HEAP;
    //init_info.SrvDescriptorAllocFn = [](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_handle) { return YOUR_ALLOCATOR_FUNCTION_FOR_SRV_DESCRIPTORS(...); };
    //init_info.SrvDescriptorFreeFn = [](ImGui_ImplDX12_InitInfo*, D3D12_CPU_DESCRIPTOR_HANDLE cpu_handle, D3D12_GPU_DESCRIPTOR_HANDLE gpu_handle) { return YOUR_FREE_FUNCTION_FOR_SRV_DESCRIPTORS(...); };

}

void ImguiShutdown()
{
    // (Your code process and dispatch Win32 messages)
    // Start the Dear ImGui frame
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    ImGui::ShowDemoWindow(); // Show demo window! :)
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
