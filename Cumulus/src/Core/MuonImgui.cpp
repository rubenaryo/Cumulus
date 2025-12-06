/*----------------------------------------------
Ruben Young (rubenaryo@gmail.com)
Date : 2025/11
Description : Helpers for initializing/using ImGui
----------------------------------------------*/

#include <Core/MuonImgui.h>
#include <Core/DXCore.h>
#include <Core/DescriptorHeap.h>
#include <Utils/Utils.h>
#include <algorithm>

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

void ImguiNewFrame(float gameTime, const Camera& cam, SceneSettings& settings)
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
            ImGui::SliderFloat("Sun Size", &settings.atmosphere.sunSize, 0.5, 5.0, "%.1f");
            ImGui::Checkbox("Toggle Dynamic Sun", &settings.atmosphere.isSunDynamic);
            if (!settings.atmosphere.isSunDynamic)
            {
                ImGui::SliderFloat3("Sun Direction", &settings.atmosphere.sunDir.x, -1.0, 1.0, "%.1f");
            }
            else
            {
                ImGui::SliderFloat("Time Scale:", &settings.atmosphere.timeScale, 0.0f, 5.f, "%.1f");
                ImGui::Text("Sun Direction: %f, %f, %f", settings.atmosphere.sunDir.x, settings.atmosphere.sunDir.y, settings.atmosphere.sunDir.z);
            }


            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Interactables"))
        {
            ImGui::Checkbox("Visualize Convex Hull", &settings.drawObjects);
            ImGui::Checkbox("Draw Objects", &settings.drawObjects);
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Clouds"))
        {
            int cloudNum = settings.numClouds;
            float cloudScale = settings.cloudScale;
            ImGui::SliderInt("Number of Clouds", &cloudNum, 0, 16);
            ImGui::SliderFloat("Cloud Scale", &cloudScale, 0.1f, 25.f, "%.1f");
            if (ImGui::Button("Regenerate Clouds") || cloudNum != settings.numClouds || cloudScale != settings.cloudScale)
            {
                settings.numClouds = cloudNum;
                settings.cloudScale = cloudScale;
                settings.updateClouds = true;
            }
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Lighting"))
        {
            // === Raymarch settings ===
            int   maxSteps = settings.lighting.maxSteps;
            float densityScale = settings.lighting.densityScale;
            float minTransmittance = settings.lighting.minTransmittance;

            ImGui::SliderInt("Max Steps", &maxSteps, 0, 512);
            ImGui::SliderFloat("Density Scale", &densityScale, 0.0f, 3.0f);
            ImGui::SliderFloat("Min Transmittance", &minTransmittance, 0.0f, 0.1f);

            float directExtinctionScale = settings.lighting.directExtinctionScale;
            ImGui::SliderFloat("Direct / Secondary Extinction",
                &directExtinctionScale, 0.0f, 5.0f);

            float directStrength = settings.lighting.directStrength;
            ImGui::SliderFloat("Direct Strength",
                &directStrength, 0.0f, 5.0f);

            // === Sun light color & direction ===
            float lightSun[3] =
            {
                settings.lighting.lightSun.x,
                settings.lighting.lightSun.y,
                settings.lighting.lightSun.z
            };

            ImGui::Text("Sun Light");
            ImGui::ColorEdit3("##LightSun", lightSun);

            // Direction (editable, then normalized)
            float dirSun[3] =
            {
                settings.lighting.dirSun.x,
                settings.lighting.dirSun.y,
                settings.lighting.dirSun.z
            };

            ImGui::SliderFloat3("Sun Direction", dirSun, -1.0f, 1.0f, "%.2f");

            // === Secondary lighting ===
            // Engine stores linear:
            float secondaryColorLinear[3] =
            {
                settings.lighting.secondaryColor.x,
                settings.lighting.secondaryColor.y,
                settings.lighting.secondaryColor.z
            };

            // Convert linear -> sRGB for UI
            DirectX::XMFLOAT3 secondarySrgb = LinearToSrgb3(
                secondaryColorLinear[0],
                secondaryColorLinear[1],
                secondaryColorLinear[2]
            );

            float secondaryColor[3] =
            {
                secondarySrgb.x,
                secondarySrgb.y,
                secondarySrgb.z
            };

            float secondaryStrength = settings.lighting.secondaryStrength;

            ImGui::Text("Secondary Color");
            ImGui::ColorEdit3("##SecondaryColor", secondaryColor);
            ImGui::SliderFloat("Secondary Strength", &secondaryStrength, 0.0f, 5.0f);


            // Secondary extinction is implicitly tied to directExtinctionScale

            // === Ambient lighting (independent extinction) ===
            // Engine stores linear
            float ambientColorLinear[3] =
            {
                settings.lighting.ambientColor.x,
                settings.lighting.ambientColor.y,
                settings.lighting.ambientColor.z
            };

            // Convert linear -> sRGB for the UI
            DirectX::XMFLOAT3 ambientSrgb = LinearToSrgb3(
                ambientColorLinear[0],
                ambientColorLinear[1],
                ambientColorLinear[2]
            );

            float ambientColor[3] =
            {
                ambientSrgb.x,
                ambientSrgb.y,
                ambientSrgb.z
            };

            float ambientStrength = settings.lighting.ambientStrength;
            float ambientExtinctionScale = settings.lighting.ambientExtinctionScale;

            ImGui::Text("Ambient Color");
            ImGui::ColorEdit3("##AmbientColor", ambientColor);
            ImGui::SliderFloat("Ambient Extinction",
                &ambientExtinctionScale, 0.0f, 10.0f, "%.3f");
            ImGui::SliderFloat("Ambient Strength", &ambientStrength, 0.0f, 5.0f);

            // === Detect changes ===
            bool dataChanged =
                maxSteps != settings.lighting.maxSteps ||
                densityScale != settings.lighting.densityScale ||
                minTransmittance != settings.lighting.minTransmittance ||
                directExtinctionScale != settings.lighting.directExtinctionScale ||
                directStrength != settings.lighting.directStrength ||
                lightSun[0] != settings.lighting.lightSun.x ||
                lightSun[1] != settings.lighting.lightSun.y ||
                lightSun[2] != settings.lighting.lightSun.z ||
                dirSun[0] != settings.lighting.dirSun.x ||
                dirSun[1] != settings.lighting.dirSun.y ||
                dirSun[2] != settings.lighting.dirSun.z ||
                secondaryColor[0] != settings.lighting.secondaryColor.x ||
                secondaryColor[1] != settings.lighting.secondaryColor.y ||
                secondaryColor[2] != settings.lighting.secondaryColor.z ||
                secondaryStrength != settings.lighting.secondaryStrength ||
                ambientColor[0] != settings.lighting.ambientColor.x ||
                ambientColor[1] != settings.lighting.ambientColor.y ||
                ambientColor[2] != settings.lighting.ambientColor.z ||
                ambientStrength != settings.lighting.ambientStrength ||
                ambientExtinctionScale != settings.lighting.ambientExtinctionScale;

            if (ImGui::Button("Update Lighting") || dataChanged)
            {
                settings.lighting.maxSteps = maxSteps;
                settings.lighting.densityScale = densityScale;
                settings.lighting.minTransmittance = minTransmittance;

                // Direct is the master extinction scale
                settings.lighting.directExtinctionScale = directExtinctionScale;
                settings.lighting.directStrength = directStrength;

                // Normalize sun direction before using it in the engine
                DirectX::XMVECTOR dir = DirectX::XMVectorSet(dirSun[0], dirSun[1], dirSun[2], 0.0f);
                //dir = DirectX::XMVector3Normalize(dir);

                DirectX::XMFLOAT3 dirNorm;
                DirectX::XMStoreFloat3(&dirNorm, dir);

                settings.lighting.dirSun = { dirNorm.x, dirNorm.y, dirNorm.z };
                settings.lighting.lightSun = { lightSun[0], lightSun[1], lightSun[2] };
                settings.lighting.secondaryColor = SrgbToLinear3(secondaryColor); 
                settings.lighting.secondaryStrength = secondaryStrength;

                // Tie secondary extinction to direct
                settings.lighting.secondaryExtinctionScale = directExtinctionScale;

                settings.lighting.ambientColor = SrgbToLinear3(ambientColor);
                settings.lighting.ambientExtinctionScale = ambientExtinctionScale;
                settings.lighting.ambientStrength = ambientStrength;

                settings.updateLighting = true;
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
