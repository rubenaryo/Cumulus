/*----------------------------------------------
Ruben Young (rubenaryo@gmail.com)
Date : 2025/4
Description : Interface for Pipeline state objects (PSO)
----------------------------------------------*/
#ifndef MUON_PIPELINESTATE_H
#define MUON_PIPELINESTATE_H

#include <d3d12.h>

namespace Muon
{
	struct VertexShader;
	struct PixelShader;
}

namespace Muon
{

class PipelineState
{
public:	
	virtual void SetRootSignature(ID3D12RootSignature** ppRootSig) { mppRootSignature = ppRootSig; }
	ID3D12RootSignature* GetRootSignature() const { return *mppRootSignature; }
	ID3D12PipelineState* GetPipelineState() const { return mpPipelineState; }

	bool Bind() const;
	void Destroy();

protected:
	ID3D12RootSignature*const* mppRootSignature = nullptr;
	ID3D12PipelineState* mpPipelineState = nullptr;
};

class GraphicsPipelineState : public PipelineState
{
public:
	GraphicsPipelineState();

public:
	virtual void SetRootSignature(ID3D12RootSignature** ppRootSig) override;
	void SetVertexShader(const Muon::VertexShader& vs);
	void SetPixelShader(const Muon::PixelShader& ps);

	// Generates the mpPipelineState from the parameters held in the description
	bool Generate();

private:
	D3D12_GRAPHICS_PIPELINE_STATE_DESC mDesc;
};

}

#endif