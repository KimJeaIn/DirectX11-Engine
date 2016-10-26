#pragma once

#include "StructSetting.h"
#include "Vertex.h"

class LoadMain
{
public:
	LoadMain();
	virtual ~LoadMain();

	virtual void Setup(ID3D11Device* d3d11device, ID3D11DeviceContext* d3d11devCon, IDXGISwapChain* swapchain) = 0;
	virtual void Init(ID3D11SamplerState* texsamplerstate, ID3D11RasterizerState* rscullnone) = 0;

	virtual void Update(const XMFLOAT4X4& camview, const XMFLOAT4X4& camprojection, const float& tick) = 0;
	virtual void OnRender(const XMFLOAT4X4& shadowTransform) = 0;

	virtual void DrawSceneToShadowMap(const XMFLOAT4X4& mLightView, const XMFLOAT4X4& mLightProj, const XMVECTOR& camerapos) = 0;

	virtual void CleanUp() = 0;

	virtual std::wstring GetOBJname() = 0;

	virtual void GetPosText(HWND hwnd) = 0;
	virtual void GetRotationText(HWND hwnd) = 0;
	virtual void GetScaleText(HWND hwnd) = 0;

	virtual XMFLOAT3 GetPos() = 0;
	virtual XMFLOAT3 GetRotation() = 0;
	virtual XMFLOAT3 GetScale() = 0;

	virtual void SetPos(float x, float y, float z) = 0;
	virtual void SetRotation(float x, float y, float z) = 0;
	virtual void SetScale(float x, float y, float z) = 0;

	virtual void SetOBJname(WCHAR* name) = 0;

	virtual void SetFocus(bool focus) = 0;

	virtual void SetPROname(std::wstring name) = 0;
	virtual std::wstring GetPROname() = 0;

	virtual float pick(XMVECTOR pickRayInWorldSpacePos,
		XMVECTOR pickRayInWorldSpaceDir) = 0;

	virtual BYTE GetType() = 0;

	virtual void SetbSctipt(const bool& sc) = 0;
	virtual bool GetbScript() = 0;

	virtual std::vector<std::string> GetScriptList() = 0;
	virtual void PushScriptList(std::string str) = 0;

	virtual void SetSaveTranfrom() = 0;
	virtual void ResetSaveTranfrom() = 0;
};

