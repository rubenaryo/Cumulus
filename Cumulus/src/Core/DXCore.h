/*----------------------------------------------
Ruben Young (rubenaryo@gmail.com)
Date : 2025/3
Description : Holds all the central DX12 Data structures
----------------------------------------------*/
#ifndef MUON_DXCORE_H
#define MUON_DXCORE_H

#include <d3dx12.h>
#include <d3d12.h>
#include <d3dcompiler.h>

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "dxcompiler.lib")
#pragma comment(lib, "dxguid.lib")

namespace Muon
{
class DescriptorHeap;
}

namespace Muon
{
	ID3D12Device* GetDevice();
	ID3D12CommandQueue* GetCommandQueue();
	ID3D12GraphicsCommandList* GetCommandList();
	ID3D12CommandAllocator* GetCommandAllocator();
	ID3D12Fence* GetFence();
	DXGI_FORMAT GetRTVFormat();
	DescriptorHeap* GetSRVHeap();

	bool ResetCommandList(ID3D12PipelineState* pInitialPipelineState);
	bool CloseCommandList();
	bool PrepareForRender();
	bool FinalizeRender();
	bool ExecuteCommandList();
	bool Present();
	bool FlushCommandQueue();
	bool UpdateBackBufferIndex();
	bool CheckFeatureLevel(ID3D12Device* pDevice, D3D_FEATURE_LEVEL& outHighestLevel, std::wstring& outHighestLevelStr);
	bool UpdateTitleBar(uint32_t fps, uint32_t frameCount);

	bool InitDX12(HWND hwnd, int width, int height);
	bool DestroyDX12();

	struct RenderTarget
	{
		UINT mWidth;
		UINT mHeight;
		DXGI_FORMAT mFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

		CD3DX12_CPU_DESCRIPTOR_HANDLE mhCpuSrv;
		CD3DX12_GPU_DESCRIPTOR_HANDLE mhGpuSrv;
		CD3DX12_CPU_DESCRIPTOR_HANDLE mhCpuRtv;

		Microsoft::WRL::ComPtr<ID3D12Resource> mpResource;
	};
}

#endif