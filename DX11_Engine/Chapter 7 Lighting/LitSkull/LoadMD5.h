#pragma once

#include "Effects.h"
#include "resource.h"
#include "LoadMain.h"

class LoadMD5 : public LoadMain
{
public:
	LoadMD5();
	~LoadMD5();

	void Setup(ID3D11Device* d3d11device, ID3D11DeviceContext* d3d11devCon, IDXGISwapChain* swapchain);
	void Init(ID3D11SamplerState* texsamplerstate, ID3D11RasterizerState* rscullnone);
	void CleanUp();

	void Update(const XMFLOAT4X4& camview, const XMFLOAT4X4& camprojection, const float& tick);
	void OnRender(const XMFLOAT4X4& shadowTransform);
	void DrawSceneToShadowMap(const XMFLOAT4X4& mLightView, const XMFLOAT4X4& mLightProj, const XMVECTOR& camerapos);

	LoadMain* MakeClone();

	std::wstring GetOBJname() { return OBJname; }

	void GetPosText(HWND hwnd);
	void GetRotationText(HWND hwnd);
	void GetScaleText(HWND hwnd);

	XMFLOAT3 GetPos() { return PositionValue; };
	XMFLOAT3 GetRotation() { return RotationValue; };
	XMFLOAT3 GetScale() { return ScaleValue; };

	void SetPos(float x, float y, float z);
	void SetRotation(float x, float y, float z);
	void SetScale(float x, float y, float z);

	void SetOBJname(WCHAR* name) { OBJname = name; }

	void SetFocus(bool focus) { FocusSetting = focus; }

	void SetPROname(std::wstring name) { PROname = name; }
	std::wstring GetPROname() { return PROname; }

	float pick(XMVECTOR pickRayInWorldSpacePos,
		XMVECTOR pickRayInWorldSpaceDir);

	bool LoadMD5Model(std::wstring filename);
	bool LoadMD5Anim(std::wstring filename);
	void UpdateMD5Model(float deltaTime, int animation);

	bool PointInTriangle(XMVECTOR& triV1, XMVECTOR& triV2, XMVECTOR& triV3, XMVECTOR& point);

	BYTE GetType() { return 1; }
	void PlayAni() { AniPlay = true; }
	void StopAni() { AniPlay = false; }
	bool IsPlayingAni() { return AniPlay; }
	void SetPlayAniNum(int num) { PlayAniNum = num; }
	std::wstring GetAniName(int num) { return MD5Model.animations[num].AniName; }
	int GetAniCount() { return AniCount; }

	void SetbSctipt(const bool& sc){ bScript = sc; }
	bool GetbScript(){ return bScript; }

	void PushScriptList(std::string str) { ScriptPath.push_back(str); }
	std::vector<std::string> GetScriptList() { return ScriptPath; }

	void SetSaveTranfrom();
	void ResetSaveTranfrom();

private :
	IDXGISwapChain* SwapChain;
	ID3D11Device* d3d11Device;
	ID3D11DeviceContext* d3d11DevCon;

	XMFLOAT4X4 camView;
	XMFLOAT4X4 camProjection;

private :
	ID3D11SamplerState* TexSamplerState;
	ID3D11RasterizerState* RSCullNone;

private:
	std::wstring OBJname;
	std::wstring PROname;

private :

	XMFLOAT4X4 Rotation;
	XMFLOAT4X4 Scale;
	XMFLOAT4X4 Translation;
	XMFLOAT4X4 MeshWorld;

	XMFLOAT3 PositionValue;
	XMFLOAT3 RotationValue;
	XMFLOAT3 ScaleValue;

	XMFLOAT3 SavePositionValue;
	XMFLOAT3 SaveRotationValue;
	XMFLOAT3 SaveScaleValue;

	Material MeshMat;
	Material FocusMeshMat;

	Model3D MD5Model;

	float AniSpeed = 1.0f;

	bool AniPlay = false;
	int PlayAniNum = 0;
	int AniCount = 0;

	bool FocusSetting = false;

private :
	std::vector<ID3D11ShaderResourceView*> meshSRV;
	std::vector<std::wstring> textureNameArray;

private :
	std::vector<std::string> ScriptPath;
	bool bScript = false;
};

