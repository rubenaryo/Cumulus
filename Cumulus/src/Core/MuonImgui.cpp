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

void ImguiNewFrame(float gameTime, const Camera& cam, DirectX::XMFLOAT3 sunDir, bool& isSunDynamic, int& timeOfDay)
{
    // (Your code process and dispatch Win32 messages)
    // Start the Dear ImGui frame
    ImGui_ImplDX12_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    //ImGui::ShowDemoWindow(); // Show demo window! :)

    ImGui::Begin("CUMULUS");
    ImGui::Text("Game Time(s): %f", gameTime);
    ImGui::Text("Add some more standard analytics here");

    ImGuiTabBarFlags tabFlags = ImGuiTabBarFlags_None;
    if (ImGui::BeginTabBar("Tabs", tabFlags))
    {   
        if (ImGui::BeginTabItem("Cam Info"))
        {
            using namespace DirectX;
            XMFLOAT3 cam_pos, cam_target, cam_fwd, cam_up, cam_r;
            XMVECTOR cam_fwd_v, cam_up_v, cam_r_v;
            cam.GetAxes(cam_fwd_v, cam_r_v, cam_up_v);
            XMStoreFloat3(&cam_pos, cam.GetPosition());
            XMStoreFloat3(&cam_fwd, cam_fwd_v);
            XMStoreFloat3(&cam_up, cam_up_v);
            XMStoreFloat3(&cam_r, cam_r_v);
            XMStoreFloat3(&cam_target, cam.GetTarget());
            ImGui::Text("Eye: %f, %f, %f", cam_pos.x, cam_pos.y, cam_pos.z);
            ImGui::Text("Forward: %f, %f, %f", cam_fwd.x, cam_fwd.y, cam_fwd.z);
            ImGui::Text("Right: %f, %f, %f", cam_r.x, cam_r.y, cam_r.z);
            ImGui::Text("Up: %f, %f, %f", cam_up.x, cam_up.y, cam_up.z);
            ImGui::Text("Target: %f, %f, %f", cam_target.x, cam_target.y, cam_target.z);
            ImGui::Text("Azimuth: %f, Zenith: %f", cam.GetAzimuth(), cam.GetZenith());

            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Atmosphere"))
        {
            ImGui::Text("Sun Direction: %f, %f, %f", sunDir.x, sunDir.y, sunDir.z);
            ImGui::Checkbox("Toggle Dynamic Sun", &isSunDynamic);
            if (!isSunDynamic)
            {
                ImGui::SliderInt("Time Of Day", &timeOfDay, 0, 2400);
            }
            else
            {
                // NOTE: this is the same code as in AtmosphereUtils, so if that changes then this gets out of sync
                float mapped_time = fmodf(gameTime * 60.f, 2400.f);
                ImGui::Text("Current Time: %.0f", mapped_time);
            }

            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();
}

void ImguiRender()
{
    // TODO: pass in cmd list as param
    ImGui::Render();
    ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), Muon::GetCommandList());
}

}
