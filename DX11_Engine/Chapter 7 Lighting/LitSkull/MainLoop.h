#include "d3dApp.h"
#include "d3dx11Effect.h"
#include "GeometryGenerator.h"
#include "MathHelper.h"
#include "LightHelper.h"
#include "Effects.h"
#include "Vertex.h"
#include "resource.h"
#include "LoadMD5.h"
#include "LoadObj.h"
#include "MainCamera.h"
#include "ShadowMap.h"

#define SAFE_DELETE(p) { if(p){ delete (p); (p) = 0; } }
#define SAFE_DELETE_ARRAY(p) { if(p){ delete[] (p); (p) = 0; } }
typedef std::map<std::wstring, LoadMain*> ObjMap;
typedef std::list<LoadMain*> ObjList;

class MainLoop : public D3DApp
{
public:
	MainLoop(HINSTANCE hInstance);
	~MainLoop();

	bool Init(); //완?
	void OnResize(); //완
	void UpdateScene(float dt); //완
	void DrawScene(); //완
	LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam); // 완

	static BOOL CALLBACK ObjSettingDlgProc(HWND hDlg, UINT iMessage, WPARAM wParam, LPARAM lParam); //완
	static BOOL CALLBACK Md5SettingDlgProc(HWND hDlg, UINT iMessage, WPARAM wParam, LPARAM lParam); //완
	static BOOL CALLBACK ProDlgProc(HWND hDlg, UINT iMessage, WPARAM wParam, LPARAM lParam); //완
	static BOOL CALLBACK ObjDlgProc(HWND hDlg, UINT iMessage, WPARAM wParam, LPARAM lParam); //완

	void SetMenuDisable(); //완
	void SetMenuEnable(); //완
	void SaveWorld(std::wofstream& fout); //완
	void LoadWorld(std::wstring fin); //완
public:
	bool InitDirectInput(HINSTANCE hInstance, HWND hwnd); //완
	void DetectInput(double time); //완

	void GridOnRender(); //완
	void GridVerSetting(); //완

	bool LoadObjOpen(std::wstring wstr); //완
	bool LoadMd5Open(std::wstring wstr); //완

	void SetSaveObjTransfrom(); //완
	void ResetSaveObjTransfrom(); //완

	static LoadMain* GetObjIndex(int index); //완
	static std::wstring AddObjFile(LPWSTR wstr); //완
	static std::wstring AddMd5File(LPWSTR wstr); //완
	static void SetFocusObj(int index); //완

	void pickRayVector(float mouseX, float mouseY, XMVECTOR& pickRayInWorldSpacePos, XMVECTOR& pickRayInWorldSpaceDir); //완

	static int GetObj(lua_State *L);
	static int ForwordMove(lua_State *L);
	static int CharRotation(lua_State *L);
	static int MainCameraSet(lua_State *L);
	static int PlayAni(lua_State *L);

	static bool GetVK(std::string key);
	static void quat_2_euler_d3d(const XMFLOAT4& q, float& pitch, float& yaw, float& roll);

public :
	void BuildShadowTransform();	

private:

	static HWND MainhWnd;
	static HWND prohwnd;
	static HWND Prolisthwnd;
	static HWND OBJHwnd;
	static HWND OBJListhwnd;
	static HWND objSethwnd;
	static HWND md5Sethwnd;
	static HWND md5AniHwnd;
	static HWND objscriptHwnd;
	static HWND md5scriptHwnd;

	static HINSTANCE hInst;

	static int SelectProNum;
	static int SelectListNum;
	static int CurrentListNum;
	static int SelectAniNum;

private :
	XMMATRIX sphereWorld;

private :
	static LoadMain* CurrentScriptObj;
	static double tick;
	static BYTE keyboardState[256];

private:
	ID3D11Buffer* GridVertBuffer = NULL;

	//Gird Vertex
	GridVertex* GridV = NULL;
	int GridCount;
	int DefultGridCount;
	float GridBorder;

private :
	int Width;
	int Height;

	RECT ObjWindowRect;
	RECT ObjSetRect;

	int ObjWinWidth;
	int ObjWinHeight;

	DirectionalLight mDirLights[3];
	XMFLOAT3 mOriginalLightDir[3];
	float mLightRotationAngle = 0.0f;

private:
	int nWidth = 0;
	static TCHAR sPath[MAX_PATH];

private:
	lua_State *L;
	bool bPlayGame = false;
	bool SkyMapDraw = false;

private:
	static ObjMap objMapList;
	static ObjList objList;

	static int FocusNum;
	bool isShoot = false;

private :
	ID3D11SamplerState* TexSamplerState;
	ID3D11RasterizerState* RSCullNone;
	ID3D11ShaderResourceView* smrv;
	ID3D11DepthStencilState* DSLessEqual;

private :
	static MainCamera camera;
	// DXInput
	IDirectInputDevice8* DIKeyboard;
	IDirectInputDevice8* DIMouse;

	DIMOUSESTATE mouseLastState;
	LPDIRECTINPUT8 DirectInput;

	DIMOUSESTATE mouseCurrState;

private :
	BoundingSphere mSceneBounds;

	ShadowMap* mSmap;
	static const int SMapSize = 2048;

	XMFLOAT4X4 mLightView;
	XMFLOAT4X4 mLightProj;
	XMFLOAT4X4 mShadowTransform;
};