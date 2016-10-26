#pragma once
#include "Effects.h"
#include "resource.h"
#include "LoadMain.h"


class LoadObj : public LoadMain
{
public:
	LoadObj();
	~LoadObj();

	void Setup(ID3D11Device* d3d11device, ID3D11DeviceContext* d3d11devCon, IDXGISwapChain* swapchain);
	void Init(ID3D11SamplerState* texsamplerstate, ID3D11RasterizerState* rscullnone);

	void Update(const XMFLOAT4X4& camview, const XMFLOAT4X4& camprojection, const float& tick);
	void OnRender(const XMFLOAT4X4& shadowTransform);
	void DrawSceneToShadowMap(const XMFLOAT4X4& mLightView, const XMFLOAT4X4& mLightProj, const XMVECTOR& camerapos);

	//Define LoadObjModel function after we create surfaceMaterial structure
	bool LoadObjModel(std::wstring filename,			//.obj filename
		bool isRHCoordSys,							//true if model was created in right hand coord system
		bool computeNormals);						//true to compute the normals, false to use the files normals

	LoadMain* MakeClone();

	void CleanUp();

	std::wstring GetOBJname() { return OBJname; }

	void GetPosText(HWND hwnd);
	void GetRotationText(HWND hwnd);
	void GetScaleText(HWND hwnd);

	XMFLOAT3 GetPos() { return PositionValue; }
	XMFLOAT3 GetRotation() { return RotationValue; }
	XMFLOAT3 GetScale() { return ScaleValue; }

	void SetPos(float x, float y, float z);
	void SetRotation(float x, float y, float z);
	void SetScale(float x, float y, float z);

	void SetOBJname(WCHAR* name) { OBJname = name; }

	void SetPROname(std::wstring name) { PROname = name; }
	std::wstring GetPROname() { return PROname; }

	void SetFocus(bool focus) { FocusSetting = focus; }

	float pick(XMVECTOR pickRayInWorldSpacePos,
		XMVECTOR pickRayInWorldSpaceDir);

	bool PointInTriangle(XMVECTOR& triV1, XMVECTOR& triV2, XMVECTOR& triV3, XMVECTOR& point);

	BYTE GetType() { return 0; }

	void SetbSctipt(const bool& sc){ bScript = sc; }
	bool GetbScript(){ return bScript; }

	void PushScriptList(std::string str) { ScriptPath.push_back(str); }
	std::vector<std::string> GetScriptList() { return ScriptPath; }

	void SetSaveTranfrom();
	void ResetSaveTranfrom();

private :
	std::wstring OBJname;
	std::wstring PROname;

private :

	IDXGISwapChain* SwapChain;
	ID3D11Device* d3d11Device;
	ID3D11DeviceContext* d3d11DevCon;

	XMFLOAT4X4 camView;
	XMFLOAT4X4 camProjection;

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

private :
	//Mesh variables. Each loaded mesh will need its own set of these
	ID3D11Buffer* meshVertBuff;
	ID3D11Buffer* meshIndexBuff;
	ID3D11SamplerState* TexSamplerState;
	ID3D11RasterizerState* RSCullNone;

	XMFLOAT4X4 meshWorld;
	int meshSubsets = 0;
	std::vector<int> meshSubsetIndexStart;
	std::vector<int> meshSubsetTexture;
	std::vector<SurfaceMaterial> material;
	//Textures and material variables, used for all mesh's loaded
	std::vector<ID3D11ShaderResourceView*> meshSRV;
	std::vector<std::wstring> textureNameArray;

	std::vector<XMFLOAT3> vertPosArray;
	std::vector<DWORD> indexPosArray;

private :
	bool FocusSetting = false;

private:
	std::vector<std::string> ScriptPath;
	bool bScript = false;
};

