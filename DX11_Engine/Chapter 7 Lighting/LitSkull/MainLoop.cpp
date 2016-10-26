#include "MainLoop.h"

HWND MainLoop::MainhWnd = NULL;
HWND MainLoop::prohwnd = NULL;
HWND MainLoop::Prolisthwnd = NULL;
HWND MainLoop::OBJHwnd = NULL;
HWND MainLoop::OBJListhwnd = NULL;
HWND MainLoop::objSethwnd = NULL;
HWND MainLoop::md5Sethwnd = NULL;
HWND MainLoop::md5AniHwnd = NULL;
HWND MainLoop::objscriptHwnd = NULL;
HWND MainLoop::md5scriptHwnd = NULL;
int MainLoop::SelectProNum = 99;
int MainLoop::SelectListNum = 0;
int MainLoop::CurrentListNum = 99;
int MainLoop::SelectAniNum = 0;
TCHAR MainLoop::sPath[MAX_PATH] = { 0, };
ObjMap MainLoop::objMapList;
ObjList MainLoop::objList;
int MainLoop::FocusNum = 0;
HINSTANCE MainLoop::hInst;
LoadMain* MainLoop::CurrentScriptObj = NULL;
double MainLoop::tick;
BYTE MainLoop::keyboardState[256];
MainCamera MainLoop::camera;

#define lua_open()  luaL_newstate()

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance,
	PSTR cmdLine, int showCmd)
{
	// Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	MainLoop theApp(hInstance);

	if (!theApp.Init())
		return 0;

	return theApp.Run();
}
MainLoop::MainLoop(HINSTANCE hInstance)
:D3DApp(hInstance)
{
	// Estimate the scene bounding sphere manually since we know how the scene was constructed.
	// The grid is the "widest object" with a width of 20 and depth of 30.0f, and centered at
	// the world space origin.  In general, you need to loop over every world space vertex
	// position and compute the bounding sphere.
	mSceneBounds.Center = XMFLOAT3(0.0f, 0.0f, 0.0f);
	mSceneBounds.Radius = sqrtf(100.0f*100.0f + 150.0f*150.0f);

	mDirLights[0].Ambient = XMFLOAT4(0.2f, 0.2f, 0.2f, 1.0f);
	mDirLights[0].Diffuse = XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f);
	mDirLights[0].Specular = XMFLOAT4(0.5f, 0.5f, 0.5f, 1.0f);
	mDirLights[0].Direction = XMFLOAT3(0.57735f, -0.57735f, 0.57735f);

	mDirLights[1].Ambient = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
	mDirLights[1].Diffuse = XMFLOAT4(0.20f, 0.20f, 0.20f, 1.0f);
	mDirLights[1].Specular = XMFLOAT4(0.25f, 0.25f, 0.25f, 1.0f);
	mDirLights[1].Direction = XMFLOAT3(-0.57735f, -0.57735f, 0.57735f);

	mDirLights[2].Ambient = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
	mDirLights[2].Diffuse = XMFLOAT4(0.2f, 0.2f, 0.2f, 1.0f);
	mDirLights[2].Specular = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
	mDirLights[2].Direction = XMFLOAT3(0.0f, -0.707f, -0.707f);

	mOriginalLightDir[0] = mDirLights[0].Direction;
	mOriginalLightDir[1] = mDirLights[1].Direction;
	mOriginalLightDir[2] = mDirLights[2].Direction;

	GridCount = 220;
	DefultGridCount = GridCount;
	GridBorder = 3.0f;

	L = lua_open();

	luaL_openlibs(L);

	lua_register(L, "GetObj", GetObj);
	lua_register(L, "ForwordMove", ForwordMove);
	lua_register(L, "CharRotation", CharRotation);
	lua_register(L, "MainCameraSet", MainCameraSet);
	lua_register(L, "PlayAni", PlayAni);
}
MainLoop::~MainLoop()
{
	SafeDelete(mSmap);
	Effects::DestroyAll();
	InputLayouts::DestroyAll();

	if (GridVertBuffer != NULL)
		GridVertBuffer->Release();

	TexSamplerState->Release();
	RSCullNone->Release();

	SAFE_DELETE_ARRAY(GridV);

	ObjMap::iterator it;
	for (it = objMapList.begin(); it != objMapList.end();)
	{
		it->second->CleanUp();
		SAFE_DELETE(it->second);
		it = objMapList.erase(it);
	}

	ObjList::iterator it1;
	for (it1 = objList.begin(); it1 != objList.end();)
	{
		(*it1)->CleanUp();
		SAFE_DELETE(*it1);
		it1 = objList.erase(it1);
	}

	lua_close(L);

	smrv->Release();

	DSLessEqual->Release();
}

bool MainLoop::Init()
{
	if (!D3DApp::Init())
		return false;

	::GetCurrentDirectory(MAX_PATH, sPath);

	// Must init Effects first since InputLayouts depend on shader signatures.
	Effects::InitAll(md3dDevice);
	InputLayouts::InitAll(md3dDevice);

	// Create DXGI factory to enumerate adapters///////////////////////////////////////////////////////////////////////////
	IDXGIFactory1 *DXGIFactory;

	mSmap = new ShadowMap(md3dDevice, SMapSize, SMapSize);

	HRESULT hr = CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void**)&DXGIFactory);

	// Use the first adapter	
	IDXGIAdapter1 *Adapter;

	hr = DXGIFactory->EnumAdapters1(0, &Adapter);

	DXGIFactory->Release();

	MainhWnd = mhMainWnd;
	hInst = mhAppInst;

	InitDirectInput(hInst, MainhWnd);

	camera.Setup(mClientWidth, mClientHeight);
	camera.Init();

	// Describe the Sample State
	D3D11_SAMPLER_DESC sampDesc;
	ZeroMemory(&sampDesc, sizeof(sampDesc));
	sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	sampDesc.MinLOD = 0;
	sampDesc.MaxLOD = D3D11_FLOAT32_MAX;

	///////////////**************new**************////////////////////	
	//Create the Sample State
	md3dDevice->CreateSamplerState(&sampDesc, &TexSamplerState);

	D3D11_RASTERIZER_DESC cmdesc;

	ZeroMemory(&cmdesc, sizeof(D3D11_RASTERIZER_DESC));
	cmdesc.FillMode = D3D11_FILL_SOLID;
	cmdesc.CullMode = D3D11_CULL_NONE;

	cmdesc.FrontCounterClockwise = false;

	md3dDevice->CreateRasterizerState(&cmdesc, &RSCullNone);

	///////////////**************new**************////////////////////
	///Load Skymap's cube texture///
	D3DX11_IMAGE_LOAD_INFO loadSMInfo;
	loadSMInfo.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;

	//Load the texture
	ID3D11Texture2D* SMTexture = 0;
	D3DX11CreateTextureFromFile(md3dDevice, L"skymap.dds",
		&loadSMInfo, 0, (ID3D11Resource**)&SMTexture, 0);

	//Create the textures description
	D3D11_TEXTURE2D_DESC SMTextureDesc;
	SMTexture->GetDesc(&SMTextureDesc);

	//Tell D3D We have a cube texture, which is an array of 2D textures
	D3D11_SHADER_RESOURCE_VIEW_DESC SMViewDesc;
	SMViewDesc.Format = SMTextureDesc.Format;
	SMViewDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
	SMViewDesc.TextureCube.MipLevels = SMTextureDesc.MipLevels;
	SMViewDesc.TextureCube.MostDetailedMip = 0;

	//Create the Resource view
	md3dDevice->CreateShaderResourceView(SMTexture, &SMViewDesc, &smrv);
	///////////////**************new**************////////////////////

	///////////////**************new**************////////////////////
	cmdesc.CullMode = D3D11_CULL_NONE;
	md3dDevice->CreateRasterizerState(&cmdesc, &RSCullNone);

	D3D11_DEPTH_STENCIL_DESC dssDesc;
	ZeroMemory(&dssDesc, sizeof(D3D11_DEPTH_STENCIL_DESC));
	dssDesc.DepthEnable = true;
	dssDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
	dssDesc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;

	md3dDevice->CreateDepthStencilState(&dssDesc, &DSLessEqual);
	///////////////**************new**************////////////////////
	GridVerSetting();

	return true;
}
void MainLoop::OnResize()
{
	if (md3dDevice)
		D3DApp::OnResize();
}

void MainLoop::UpdateScene(float dt)
{
	tick = dt;
	camera.Update();
	DetectInput(dt);

	XMFLOAT4 camerapos;
	XMStoreFloat4(&camerapos, camera.GetCamPosition());

	float cameraposx = fabsf(camerapos.x);
	float cameraposz = fabsf(camerapos.z);

	if (cameraposx >= GridBorder || cameraposz >= GridBorder)
	{
		GridCount += 24;
		GridVerSetting();
		GridBorder += 3.0f;
	}
	else if (cameraposx < GridBorder / 2.0f && cameraposz < GridBorder / 2.0f)
	{
		GridCount -= 24;
		GridVerSetting();
		GridBorder -= 3.0f;

		if (GridCount < DefultGridCount)
		{
			GridCount = DefultGridCount;
			GridBorder = 3.0f;
			GridVerSetting();
		}
	}
	ObjList::iterator it;
	if (bPlayGame)
	{
		int cur = 0;
		for (it = objList.begin(); it != objList.end(); it++)
		{
			if ((*it)->GetbScript())
			{
				CurrentScriptObj = (*it);
				std::vector<std::string> Slist = (*it)->GetScriptList();
				for (int i = 0; i < Slist.size(); i++)
				{
					luaL_dofile(L, Slist[i].c_str());
				}
			}
			cur++;
		}
	}
	else
	{
		camera.SetbCharCamera(false);
	}

	for (it = objList.begin(); it != objList.end(); it++)
	{
		(*it)->Update(camera.GetCamView(), camera.GetCamProjection(), dt);
	}

	sphereWorld = XMMatrixIdentity();

	mLightRotationAngle += 0.1f*dt;

	XMMATRIX R = XMMatrixRotationY(mLightRotationAngle);
	for (int i = 0; i < 3; ++i)
	{
		XMVECTOR lightDir = XMLoadFloat3(&mOriginalLightDir[i]);
		lightDir = XMVector3TransformNormal(lightDir, R);
		XMStoreFloat3(&mDirLights[i].Direction, lightDir);
	}

	BuildShadowTransform();

	//Define sphereWorld's world space matrix
	XMMATRIX Scale = XMMatrixScaling(5.0f, 5.0f, 5.0f);
	//Make sure the sphere is always centered around camera
	XMMATRIX Translation = XMMatrixTranslation(XMVectorGetX(camera.GetCamPosition()), XMVectorGetY(camera.GetCamPosition()), XMVectorGetZ(camera.GetCamPosition()));

	//Set sphereWorld's world space using the transformations
	sphereWorld = Scale * Translation;
}
void MainLoop::DrawScene()
{
	mSmap->BindDsvAndSetNullRenderTarget(md3dImmediateContext);

	ObjList::iterator it;

	for (it = objList.begin(); it != objList.end(); it++)
	{
		(*it)->DrawSceneToShadowMap(mLightView, mLightProj, camera.GetCamPosition());
	}

	md3dImmediateContext->RSSetState(0);

	ID3D11RenderTargetView* renderTargets[1] = { mRenderTargetView };
	md3dImmediateContext->OMSetRenderTargets(1, renderTargets, mDepthStencilView);
	md3dImmediateContext->RSSetViewports(1, &mScreenViewport);

	float bgColor[4] = { 0.1f, 0.1f, 0.1f, 1.0f };
	md3dImmediateContext->ClearRenderTargetView(mRenderTargetView, bgColor);
	md3dImmediateContext->ClearDepthStencilView(mDepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

	md3dImmediateContext->IASetInputLayout(InputLayouts::PosNormal);
	md3dImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	//Set the default blend state (no blending) for opaque objects
	md3dImmediateContext->OMSetBlendState(0, 0, 0xffffffff);

	UINT stride = sizeof(Vertex);
	UINT offset = 0;	

	XMFLOAT4 campos4;
	XMStoreFloat4(&campos4, camera.GetCamPosition());
	XMFLOAT3 campos = { campos4.x, campos4.y, campos4.z };

	// Set per frame constants.
	Effects::BasicFX->SetDirLights(mDirLights);
	Effects::BasicFX->SetEyePosW(campos);
	Effects::BasicFX->SetShadowMap(mSmap->DepthMapSRV());

	int i = 0;
	for (it = objList.begin(); it != objList.end(); it++)
	{
		if (FocusNum == i)
			(*it)->SetFocus(true);
		else
			(*it)->SetFocus(false);

		(*it)->OnRender(mShadowTransform);
		i++;
	}	

	GridOnRender();

	HR(mSwapChain->Present(0, 0));
}

LRESULT MainLoop::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{

	int wmId, wmEvent;
	PAINTSTRUCT ps;
	HDC hdc;
	RECT rc;
	int i;

	OPENFILENAME OFN;
	WCHAR lpstrfile[MAX_PATH] = L"";
	WCHAR filename[MAX_PATH] = L"";
	WCHAR ext[MAX_PATH] = L"";
	WCHAR filename2[MAX_PATH] = L"";

	std::wstring wstr;

	HFONT hFont = NULL;
	HDC hDC = NULL;
	SIZE sz = { 0 };

	std::size_t filenamenum;

	static TCHAR str[256];
	int len;

	switch (msg)
	{
	case WM_CREATE:
		MainhWnd = hwnd;
		if (!IsWindow(prohwnd))
		{
			prohwnd = CreateDialog(mhAppInst, MAKEINTRESOURCE(IDD_PROWIN), MainhWnd, ProDlgProc);
			OBJHwnd = CreateDialog(mhAppInst, MAKEINTRESOURCE(IDD_OBJLISTWIN), MainhWnd, ObjDlgProc);
			Prolisthwnd = GetDlgItem(prohwnd, IDC_PROLISTBOX);
			OBJListhwnd = GetDlgItem(OBJHwnd, IDC_OBJLISTBOX);
			ShowWindow(prohwnd, SW_SHOW);
			ShowWindow(OBJHwnd, SW_SHOW);

			GetWindowRect(MainhWnd, &rc);
			GetWindowRect(prohwnd, &ObjWindowRect);

			ObjWinWidth = ObjWindowRect.right - ObjWindowRect.left;
			ObjWinHeight = ObjWindowRect.bottom - ObjWindowRect.top;

			MoveWindow(prohwnd, rc.right, rc.top, ObjWinWidth, ObjWinHeight, true);
			MoveWindow(OBJHwnd, rc.right, ObjWindowRect.bottom, ObjWinWidth, ObjWinHeight, true);
		}
		break;
	case WM_MOVE:
		if (prohwnd != NULL && OBJHwnd != NULL)
		{
			GetWindowRect(MainhWnd, &rc);
			GetWindowRect(prohwnd, &ObjWindowRect);
			MoveWindow(prohwnd, rc.right, rc.top, ObjWinWidth, ObjWinHeight, true);
			MoveWindow(OBJHwnd, rc.right, ObjWindowRect.bottom, ObjWinWidth, ObjWinHeight, true);
		}
		OnResize();
		break;
	case WM_SIZE:
		if (prohwnd != NULL && OBJHwnd != NULL)
		{
			GetWindowRect(MainhWnd, &rc);
			GetWindowRect(prohwnd, &ObjWindowRect);
			MoveWindow(prohwnd, rc.right, rc.top, ObjWinWidth, ObjWinHeight, true);
			MoveWindow(OBJHwnd, rc.right, ObjWindowRect.bottom, ObjWinWidth, ObjWinHeight, true);
		}
		OnResize();
		break;
	case WM_COMMAND:
		wmId = LOWORD(wParam);
		wmEvent = HIWORD(wParam);

		// 메뉴 선택을 구문 분석합니다.
		switch (wmId)
		{
		case IDC_PLAYGAME:

			if (!bPlayGame)
			{
				SetMenuDisable();
				bPlayGame = true;
				FocusNum = 99;
			}
			else
			{
				SetMenuEnable();
				bPlayGame = false;
			}
			break;
		case ID_SKYON:
			SkyMapDraw = true;
			break;
		case ID_SKYOFF:
			SkyMapDraw = false;
			break;
		case ID_FILE_LOAD_OBJ:
			SetCurrentDirectory(sPath);

			memset(&OFN, 0, sizeof(OPENFILENAME));
			OFN.lStructSize = sizeof(OPENFILENAME);
			OFN.hwndOwner = hwnd;
			OFN.lpstrFilter = L"Obj File(*.obj)\0*.obj\0Text File\0*.txt;*.doc\0";
			OFN.lpstrFile = lpstrfile;
			OFN.nMaxFile = 256;
			OFN.lpstrInitialDir = L".\\model";

			if (GetOpenFileName(&OFN) != 0)
			{
				TCHAR str[100];
				wsprintf(str, L"%s 파일을 선택했슴", OFN.lpstrFile); // lpstrFile <- 받아짐
				MessageBox(hwnd, str, L"파일 열기 성공", MB_OK);

				//LoadObjMesh("여기에 경로를 넣는다");

				_wsplitpath(lpstrfile, NULL, NULL, filename, ext);

				wstr = L".\\model\\";
				wstr += filename;
				wstr.append(ext);

				SetCurrentDirectory(sPath);
				if (LoadObjOpen(wstr.c_str()))
				{
					if (Prolisthwnd != NULL)
					{
						hFont = (HFONT)SendMessage(Prolisthwnd, WM_GETFONT, 0, 0);
						hDC = GetDC(Prolisthwnd);
						SelectObject(hDC, (HGDIOBJ)hFont);
					}

					GetTextExtentPoint32(hDC, lpstrfile, wstr.length(), &sz);
					if (max(sz.cx, nWidth) > nWidth)
						nWidth = max(sz.cx, nWidth);

					SendMessage(Prolisthwnd, LB_INSERTSTRING, 0, (LPARAM)wstr.c_str());
					SendMessage(Prolisthwnd, LB_SETHORIZONTALEXTENT, nWidth + 20, 0);

					ReleaseDC(Prolisthwnd, hDC);

					return 0;
				}
			}
			break;

		case ID_FILE_LOAD_MD5:
			SetCurrentDirectory(sPath);

			memset(&OFN, 0, sizeof(OPENFILENAME));
			OFN.lStructSize = sizeof(OPENFILENAME);
			OFN.hwndOwner = hwnd;
			OFN.lpstrFilter = L"md5mesh File(*.md5mesh)\0*.md5mesh\0Text File\0*.txt;*.doc\0";
			OFN.lpstrFile = lpstrfile;
			OFN.nMaxFile = 256;
			OFN.lpstrInitialDir = L".\\model";

			if (GetOpenFileName(&OFN) != 0)
			{
				TCHAR str[100];
				wsprintf(str, L"%s 파일을 선택했슴", OFN.lpstrFile); // lpstrFile <- 받아짐
				MessageBox(hwnd, str, L"파일 열기 성공", MB_OK);

				//LoadObjMesh("여기에 경로를 넣는다");

				_wsplitpath(lpstrfile, NULL, NULL, filename, ext);

				wstr = L".\\model\\";
				wstr += filename;
				wstr.append(ext);

				SetCurrentDirectory(sPath);
				if (LoadMd5Open(wstr.c_str()))
				{
					if (Prolisthwnd != NULL)
					{
						hFont = (HFONT)SendMessage(Prolisthwnd, WM_GETFONT, 0, 0);
						hDC = GetDC(Prolisthwnd);
						SelectObject(hDC, (HGDIOBJ)hFont);
					}

					GetTextExtentPoint32(hDC, lpstrfile, wstr.length(), &sz);
					if (max(sz.cx, nWidth) > nWidth)
						nWidth = max(sz.cx, nWidth);

					SendMessage(Prolisthwnd, LB_INSERTSTRING, 0, (LPARAM)wstr.c_str());
					SendMessage(Prolisthwnd, LB_SETHORIZONTALEXTENT, nWidth + 20, 0);

					ReleaseDC(Prolisthwnd, hDC);

					return 0;
				}
			}
			break;
		case ID_WORLDSAVE:
			SetCurrentDirectory(sPath);

			memset(&OFN, 0, sizeof(OPENFILENAME));
			OFN.lStructSize = sizeof(OPENFILENAME);
			OFN.hwndOwner = hwnd;
			OFN.lpstrFilter = L"Obj File(*.wld)\0*.wld";
			OFN.lpstrFile = lpstrfile;
			OFN.nMaxFile = 256;
			OFN.lpstrInitialDir = L".\\save";

			if (GetSaveFileName(&OFN) != 0)
			{
				TCHAR str[100];

				std::wofstream fout;
				std::wstring wstr = OFN.lpstrFile;

				_wsplitpath(wstr.c_str(), NULL, NULL, NULL, filename2);

				if (lstrcmpW(filename2, L".wld") != 0)
				{
					wstr.append(L".wld");
					wsprintf(str, L"%s.wld 파일 저장완료", OFN.lpstrFile);
					MessageBox(hwnd, str, L"파일 저장 성공", MB_OK);
				}
				else
				{
					wsprintf(str, L"%s 파일 저장완료", OFN.lpstrFile);
					MessageBox(hwnd, str, L"파일 저장 성공", MB_OK);
				}

				fout.open(wstr.c_str());

				fout.precision(6);
				fout.setf(std::ios_base::fixed, std::ios_base::floatfield);

				SaveWorld(fout);

				fout.close();

				return 0;
			}

			break;
		case ID_WORLDLOAD:
			SetCurrentDirectory(sPath);

			memset(&OFN, 0, sizeof(OPENFILENAME));
			OFN.lStructSize = sizeof(OPENFILENAME);
			OFN.hwndOwner = hwnd;
			OFN.lpstrFilter = L"Obj File(*.wld)\0*.wld";
			OFN.lpstrFile = lpstrfile;
			OFN.nMaxFile = 256;
			OFN.lpstrInitialDir = L".\\save";

			if (GetOpenFileName(&OFN) != 0)
			{
				TCHAR str[100];
				wsprintf(str, L"%s 월드를 불러왔습니다", OFN.lpstrFile); // lpstrFile <- 받아짐
				MessageBox(hwnd, str, L"파일 열기 성공", MB_OK);

				std::wstring filena = lpstrfile;

				LoadWorld(filena);
			}
			break;
		case IDM_EXIT:
			DestroyWindow(hwnd);
			break;
		default:
			return DefWindowProc(hwnd, msg, wParam, lParam);
		}
		break;
	case WM_PAINT:
		hdc = BeginPaint(hwnd, &ps);
		// TODO: 여기에 그리기 코드를 추가합니다.

		EndPaint(hwnd, &ps);
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hwnd, msg, wParam, lParam);
	}
	return 0;

	return DefWindowProc(hwnd, msg, wParam, lParam);
}

void MainLoop::SetMenuDisable()
{
	EnableMenuItem(GetSubMenu(GetMenu(MainhWnd), 0), ID_FILE_LOAD_OBJ, MF_DISABLED | MF_GRAYED);
	EnableMenuItem(GetSubMenu(GetMenu(MainhWnd), 0), ID_FILE_LOAD_MD5, MF_DISABLED | MF_GRAYED);
	EnableMenuItem(GetSubMenu(GetMenu(MainhWnd), 0), ID_WORLDSAVE, MF_DISABLED | MF_GRAYED);
	EnableMenuItem(GetSubMenu(GetMenu(MainhWnd), 0), ID_WORLDLOAD, MF_DISABLED | MF_GRAYED);
	EnableMenuItem(GetSubMenu(GetMenu(MainhWnd), 1), ID_SKYON, MF_DISABLED | MF_GRAYED);
	EnableMenuItem(GetSubMenu(GetMenu(MainhWnd), 1), ID_SKYOFF, MF_DISABLED | MF_GRAYED);
	CheckMenuItem(GetSubMenu(GetMenu(MainhWnd), 1), IDC_PLAYGAME, MF_CHECKED);

	ShowWindow(prohwnd, SW_HIDE);
	ShowWindow(OBJHwnd, SW_HIDE);
	ShowWindow(objSethwnd, SW_HIDE);
	ShowWindow(md5Sethwnd, SW_HIDE);

	SetSaveObjTransfrom();
}
void MainLoop::SetMenuEnable()
{
	EnableMenuItem(GetSubMenu(GetMenu(MainhWnd), 0), ID_FILE_LOAD_OBJ, MF_ENABLED);
	EnableMenuItem(GetSubMenu(GetMenu(MainhWnd), 0), ID_FILE_LOAD_MD5, MF_ENABLED);
	EnableMenuItem(GetSubMenu(GetMenu(MainhWnd), 0), ID_WORLDSAVE, MF_ENABLED);
	EnableMenuItem(GetSubMenu(GetMenu(MainhWnd), 0), ID_WORLDLOAD, MF_ENABLED);
	EnableMenuItem(GetSubMenu(GetMenu(MainhWnd), 1), ID_SKYON, MF_ENABLED);
	EnableMenuItem(GetSubMenu(GetMenu(MainhWnd), 1), ID_SKYOFF, MF_ENABLED);
	CheckMenuItem(GetSubMenu(GetMenu(MainhWnd), 1), IDC_PLAYGAME, MF_UNCHECKED);

	RECT rc;

	ShowWindow(prohwnd, SW_SHOW);
	ShowWindow(OBJHwnd, SW_SHOW);
	ShowWindow(objSethwnd, SW_SHOW);
	ShowWindow(md5Sethwnd, SW_SHOW);

	GetWindowRect(MainhWnd, &rc);
	GetWindowRect(prohwnd, &ObjWindowRect);

	ObjWinWidth = ObjWindowRect.right - ObjWindowRect.left;
	ObjWinHeight = ObjWindowRect.bottom - ObjWindowRect.top;

	MoveWindow(prohwnd, rc.right, rc.top, ObjWinWidth, ObjWinHeight, true);
	MoveWindow(OBJHwnd, rc.right, ObjWindowRect.bottom, ObjWinWidth, ObjWinHeight, true);

	ResetSaveObjTransfrom();
}

void MainLoop::SaveWorld(std::wofstream& fout)
{
	int getcount = 0;
	WCHAR str[MAX_PATH];

	// project List Save

	getcount = SendMessage(Prolisthwnd, LB_GETCOUNT, 0, 0);

	for (int i = getcount - 1; i >= 0; i--)
	{
		fout << "p ";
		SendMessage(Prolisthwnd, LB_GETTEXT, i, (LPARAM)str);
		fout << str << "\n";
	}
	fout << "\n";

	getcount = SendMessage(OBJListhwnd, LB_GETCOUNT, 0, 0);

	for (int i = getcount - 1; i >= 0; i--)
	{
		LoadMain* obj = GetObjIndex(i);
		if (obj->GetType() == 0)
		{
			fout << "o ";

			fout << obj->GetPROname() << "\n";
			fout << "on " << obj->GetOBJname() << "\n";
			fout << "op " << obj->GetPos().x << " " << obj->GetPos().y << " " << obj->GetPos().z << "\n";
			fout << "or " << obj->GetRotation().x << " " << obj->GetRotation().y << " " << obj->GetRotation().z << "\n";
			fout << "os " << obj->GetScale().x << " " << obj->GetScale().y << " " << obj->GetScale().z << "\n";

			if (obj->GetbScript())
			{
				std::vector<std::string> sclist = obj->GetScriptList();
				for (int i = 0; i < sclist.size(); i++)
				{
					std::wstring wstr;
					wstr.assign(sclist[i].begin(), sclist[i].end());

					fout << "ot " << wstr << "\n";
				}
			}
			fout << "\n";
		}
		else
		{
			fout << "m ";

			fout << obj->GetPROname() << "\n";
			fout << "mn " << obj->GetOBJname() << "\n";
			fout << "mp " << obj->GetPos().x << " " << obj->GetPos().y << " " << obj->GetPos().z << "\n";
			fout << "mr " << obj->GetRotation().x << " " << obj->GetRotation().y << " " << obj->GetRotation().z << "\n";
			fout << "ms " << obj->GetScale().x << " " << obj->GetScale().y << " " << obj->GetScale().z << "\n";
			for (int k = 0; k < dynamic_cast<LoadMD5*>(obj)->GetAniCount(); k++)
			{
				fout << "ma " << dynamic_cast<LoadMD5*>(obj)->GetAniName(k) << "\n";
			}
			if (obj->GetbScript())
			{
				std::vector<std::string> sclist = obj->GetScriptList();
				for (int i = 0; i < sclist.size(); i++)
				{
					std::wstring wstr;
					wstr.assign(sclist[i].begin(), sclist[i].end());

					fout << "mt " << wstr << "\n";
				}
			}
			fout << "\n";
		}
	}

	//fout << std::endl;
}

void MainLoop::LoadWorld(std::wstring filename)
{
	std::wifstream fileIn(filename.c_str());
	std::wstring obname;
	WCHAR str[MAX_PATH];
	WCHAR ext[MAX_PATH];

	HFONT hFont = NULL;
	HDC hDC = NULL;
	SIZE sz = { 0 };

	wchar_t checkChar;
	SetCurrentDirectory(sPath);
	if (fileIn)
	{
		while (fileIn)
		{
			checkChar = fileIn.get();

			switch (checkChar)
			{
			case 'p':
				checkChar = fileIn.get();

				if (checkChar == ' ')
				{
					fileIn >> str;

					std::wstring ss = str;

					SetCurrentDirectory(sPath);
					_wsplitpath(str, NULL, NULL, NULL, ext);
					if (lstrcmpW(ext, L".obj") == 0)
					{
						if (LoadObjOpen(str))
						{
							if (Prolisthwnd != NULL)
							{
								hFont = (HFONT)SendMessage(Prolisthwnd, WM_GETFONT, 0, 0);
								hDC = GetDC(Prolisthwnd);
								SelectObject(hDC, (HGDIOBJ)hFont);
							}

							GetTextExtentPoint32(hDC, str, ss.length(), &sz);
							if (max(sz.cx, nWidth) > nWidth)
								nWidth = max(sz.cx, nWidth);

							SendMessage(Prolisthwnd, LB_INSERTSTRING, 0, (LPARAM)ss.c_str());
							SendMessage(Prolisthwnd, LB_SETHORIZONTALEXTENT, nWidth + 20, 0);

							ReleaseDC(Prolisthwnd, hDC);
						}
					}
					else if (lstrcmpW(ext, L".md5mesh") == 0)
					{
						if (LoadMd5Open(str))
						{
							if (Prolisthwnd != NULL)
							{
								hFont = (HFONT)SendMessage(Prolisthwnd, WM_GETFONT, 0, 0);
								hDC = GetDC(Prolisthwnd);
								SelectObject(hDC, (HGDIOBJ)hFont);
							}

							GetTextExtentPoint32(hDC, str, ss.length(), &sz);
							if (max(sz.cx, nWidth) > nWidth)
								nWidth = max(sz.cx, nWidth);

							SendMessage(Prolisthwnd, LB_INSERTSTRING, 0, (LPARAM)ss.c_str());
							SendMessage(Prolisthwnd, LB_SETHORIZONTALEXTENT, nWidth + 20, 0);

							ReleaseDC(Prolisthwnd, hDC);
						}
					}
				}
				break;
			case 'o':
				checkChar = fileIn.get();
				if (checkChar == ' ')
				{
					fileIn >> str;
					AddObjFile(str);
				}
				else if (checkChar == 'n')
				{
					checkChar = fileIn.get();
					if (checkChar == ' ')
					{
						fileIn >> str;
						LoadMain* obj = GetObjIndex(0);
						obj->SetOBJname(str);
						obname = str;
						obname += L"(obj)";
						SendMessage(OBJListhwnd, LB_INSERTSTRING, 0, (LPARAM)obname.c_str());
					}
				}
				else if (checkChar == 'p')
				{
					checkChar = fileIn.get();
					if (checkChar == ' ')
					{
						float xx;
						float yy;
						float zz;
						fileIn >> xx >> yy >> zz;
						LoadMain* obj = GetObjIndex(0);
						obj->SetPos(xx, yy, zz);
					}
				}
				else if (checkChar == 'r')
				{
					checkChar = fileIn.get();
					if (checkChar == ' ')
					{
						float xx;
						float yy;
						float zz;
						fileIn >> xx >> yy >> zz;
						LoadMain* obj = GetObjIndex(0);
						obj->SetRotation(xx, yy, zz);
					}
				}
				else if (checkChar == 's')
				{
					checkChar = fileIn.get();
					if (checkChar == ' ')
					{
						float xx;
						float yy;
						float zz;
						fileIn >> xx >> yy >> zz;
						LoadMain* obj = GetObjIndex(0);
						obj->SetScale(xx, yy, zz);
					}
				}
				else if (checkChar == 't')
				{
					checkChar = fileIn.get();
					if (checkChar == ' ')
					{
						std::wstring wstr;
						fileIn >> wstr;

						std::string sstr;
						sstr.assign(wstr.begin(), wstr.end());

						LoadMain* obj = GetObjIndex(0);

						obj->PushScriptList(sstr);
						obj->SetbSctipt(true);
					}
				}
				break;
			case 'm':
				checkChar = fileIn.get();
				if (checkChar == ' ')
				{
					fileIn >> str;
					AddMd5File(str);
				}
				else if (checkChar == 'n')
				{
					checkChar = fileIn.get();
					if (checkChar == ' ')
					{
						fileIn >> str;
						LoadMain* obj = GetObjIndex(0);
						obj->SetOBJname(str);
						obname = str;
						obname += L"(md5)";
						SendMessage(OBJListhwnd, LB_INSERTSTRING, 0, (LPARAM)obname.c_str());
					}
				}
				else if (checkChar == 'p')
				{
					checkChar = fileIn.get();
					if (checkChar == ' ')
					{
						float xx;
						float yy;
						float zz;
						fileIn >> xx >> yy >> zz;
						LoadMain* obj = GetObjIndex(0);
						obj->SetPos(xx, yy, zz);
					}
				}
				else if (checkChar == 'r')
				{
					checkChar = fileIn.get();
					if (checkChar == ' ')
					{
						float xx;
						float yy;
						float zz;
						fileIn >> xx >> yy >> zz;
						LoadMain* obj = GetObjIndex(0);
						obj->SetRotation(xx, yy, zz);
					}
				}
				else if (checkChar == 's')
				{
					checkChar = fileIn.get();
					if (checkChar == ' ')
					{
						float xx;
						float yy;
						float zz;
						fileIn >> xx >> yy >> zz;
						LoadMain* obj = GetObjIndex(0);
						obj->SetScale(xx, yy, zz);
					}
				}
				else if (checkChar == 'a')
				{
					checkChar = fileIn.get();
					if (checkChar == ' ')
					{
						std::wstring anipath;
						fileIn >> anipath;
						LoadMain* obj = GetObjIndex(0);
						dynamic_cast<LoadMD5*>(obj)->LoadMD5Anim(anipath);
					}
				}
				else if (checkChar == 't')
				{
					checkChar = fileIn.get();
					if (checkChar == ' ')
					{
						std::wstring wstr;
						fileIn >> wstr;

						std::string sstr;
						sstr.assign(wstr.begin(), wstr.end());

						LoadMain* obj = GetObjIndex(0);

						obj->PushScriptList(sstr);
						obj->SetbSctipt(true);
					}
				}
				break;
			default:
				break;
			}
		}
	}
}

BOOL CALLBACK MainLoop::ProDlgProc(HWND hDlg, UINT iMessage, WPARAM wParam, LPARAM lParam)
{
	int i;
	LoadObj* obj;

	WCHAR str[MAX_PATH];
	switch (iMessage)
	{
	case WM_INITDIALOG:

		return TRUE;
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDCLOSE:
			DestroyWindow(prohwnd);
			EndDialog(hDlg, IDCANCEL);
			prohwnd = NULL;
			return TRUE;

		case IDC_PROLISTBOX:
			switch (HIWORD(wParam))
			{
			case LBN_SELCHANGE:
				i = SendMessage(Prolisthwnd, LB_GETCURSEL, 0, 0);
				SendMessage(Prolisthwnd, LB_GETTEXT, i, (LPARAM)str);

				SelectProNum = i;

				return TRUE;
			}
			break;
		case IDC_OBJADDB:
			if (SelectProNum != 99)
			{
				wchar_t filename[MAX_PATH];
				SendMessage(Prolisthwnd, LB_GETTEXT, SelectProNum, (LPARAM)str);
				_wsplitpath(str, NULL, NULL, NULL, filename);

				if (lstrcmpW(filename, L".obj") == 0)
					SendMessage(OBJListhwnd, LB_INSERTSTRING, 0, (LPARAM)AddObjFile(str).c_str());
				else if (lstrcmpW(filename, L".md5mesh") == 0)
					SendMessage(OBJListhwnd, LB_INSERTSTRING, 0, (LPARAM)AddMd5File(str).c_str());

				InvalidateRect(MainhWnd, NULL, TRUE);

				i = SendMessage(OBJListhwnd, LB_GETCURSEL, 0, 0);
				SendMessage(OBJListhwnd, LB_GETTEXT, i, (LPARAM)str);
				CurrentListNum = i;
			}

			return TRUE;

			break;
		}
		break;
	}
	return FALSE;
}
BOOL CALLBACK MainLoop::ObjDlgProc(HWND hDlg, UINT iMessage, WPARAM wParam, LPARAM lParam)
{
	int i;
	LoadMain* obj;

	WCHAR filename[MAX_PATH] = L"";
	WCHAR ext[MAX_PATH] = L"";

	std::wstring wstr;

	WCHAR str[MAX_PATH];
	switch (iMessage)
	{
	case WM_INITDIALOG:

		return TRUE;
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDCLOSE:
			DestroyWindow(OBJHwnd);
			EndDialog(hDlg, IDCANCEL);
			OBJHwnd = NULL;
			return TRUE;

		case IDC_OBJLISTBOX:
			switch (HIWORD(wParam))
			{
			case LBN_SELCHANGE:
				i = SendMessage(OBJListhwnd, LB_GETCURSEL, 0, 0);
				SendMessage(OBJListhwnd, LB_GETTEXT, i, (LPARAM)str);

				SelectListNum = i;
				obj = GetObjIndex(SelectListNum);

				if (obj->GetType() == 0)
				{
					if (objSethwnd == NULL)
					{
						DestroyWindow(md5AniHwnd);
						EndDialog(md5AniHwnd, IDCANCEL);
						md5AniHwnd = NULL;
						DestroyWindow(md5Sethwnd);
						EndDialog(md5Sethwnd, IDCANCEL);
						md5Sethwnd = NULL;

						objSethwnd = CreateDialog(hInst, MAKEINTRESOURCE(IDD_OBJSET), MainhWnd, ObjSettingDlgProc);
						ShowWindow(objSethwnd, SW_SHOW);
						objscriptHwnd = GetDlgItem(objSethwnd, IDC_OBJSCRIPTCOMBO);
					}
					if (CurrentListNum != SelectListNum)
					{
						SetDlgItemText(objSethwnd, IDC_OBJNAME, obj->GetOBJname().c_str());

						obj->GetPosText(objSethwnd);
						obj->GetRotationText(objSethwnd);
						obj->GetScaleText(objSethwnd);
						SetFocusObj(SelectListNum);

						InvalidateRect(MainhWnd, NULL, TRUE);

						SendMessage(objscriptHwnd, CB_RESETCONTENT, 0, 0);

						if (obj->GetbScript())
						{
							std::vector<std::string> sclist = obj->GetScriptList();
							for (int j = 0; j < sclist.size(); j++)
							{
								i = SendMessage(objscriptHwnd, CB_GETCOUNT, 0, 0);

								wstr.assign(sclist[i].begin(), sclist[i].end());

								SendMessage(objscriptHwnd, CB_INSERTSTRING, i, (LPARAM)wstr.c_str());
							}
							SendMessage(objscriptHwnd, CB_SETCURSEL, 0, 0);
						}

						CurrentListNum = SelectListNum;
					}
				}
				else
				{
					if (md5Sethwnd == NULL)
					{
						DestroyWindow(objSethwnd);
						EndDialog(objSethwnd, IDCANCEL);
						objSethwnd = NULL;

						md5Sethwnd = CreateDialog(hInst, MAKEINTRESOURCE(IDD_MD5SET), MainhWnd, Md5SettingDlgProc);
						ShowWindow(md5Sethwnd, SW_SHOW);
						md5AniHwnd = GetDlgItem(md5Sethwnd, IDC_ANICOMBO);
						md5scriptHwnd = GetDlgItem(md5Sethwnd, IDC_MD5SCRIPTCOMBO);
					}
					if (CurrentListNum != SelectListNum)
					{
						SetDlgItemText(md5Sethwnd, IDC_MD5NAME, obj->GetOBJname().c_str());

						obj->GetPosText(md5Sethwnd);
						obj->GetRotationText(md5Sethwnd);
						obj->GetScaleText(md5Sethwnd);
						SetFocusObj(SelectListNum);

						InvalidateRect(MainhWnd, NULL, TRUE);

						SendMessage(md5AniHwnd, CB_RESETCONTENT, 0, 0);

						for (int j = 0; j < dynamic_cast<LoadMD5*>(obj)->GetAniCount(); j++)
						{
							i = SendMessage(md5AniHwnd, CB_GETCOUNT, 0, 0);

							_wsplitpath(dynamic_cast<LoadMD5*>(obj)->GetAniName(j).c_str(), NULL, NULL, filename, ext);

							wstr = filename;
							wstr.append(ext);

							SendMessage(md5AniHwnd, CB_INSERTSTRING, i, (LPARAM)wstr.c_str());
						}
						SendMessage(md5AniHwnd, CB_SETCURSEL, 0, 0);

						SendMessage(md5scriptHwnd, CB_RESETCONTENT, 0, 0);

						if (obj->GetbScript())
						{
							std::vector<std::string> sclist = obj->GetScriptList();
							for (int j = 0; j < sclist.size(); j++)
							{
								i = SendMessage(md5scriptHwnd, CB_GETCOUNT, 0, 0);

								wstr.assign(sclist[i].begin(), sclist[i].end());

								SendMessage(md5scriptHwnd, CB_INSERTSTRING, i, (LPARAM)wstr.c_str());
							}
							SendMessage(md5scriptHwnd, CB_SETCURSEL, 0, 0);
						}

						CurrentListNum = SelectListNum;
					}
				}

				SendMessage(OBJListhwnd, LB_SETCURSEL, SelectListNum, 0);
				return TRUE;
			}
			break;
		}
		break;
	}
	return FALSE;
}
BOOL CALLBACK MainLoop::Md5SettingDlgProc(HWND hDlg, UINT iMessage, WPARAM wParam, LPARAM lParam)
{
	int i;
	LoadMain* obj;

	std::wifstream tvalue;
	std::wstring plusname;

	float x;
	float y;
	float z;

	OPENFILENAME OFN;
	WCHAR lpstrfile[MAX_PATH] = L"";
	WCHAR filename[MAX_PATH] = L"";
	WCHAR ext[MAX_PATH] = L"";
	WCHAR filename2[MAX_PATH] = L"";

	std::wstring wstr;

	WCHAR str[MAX_PATH];
	switch (iMessage)
	{
	case WM_INITDIALOG:
		obj = GetObjIndex(SelectListNum);
		SetDlgItemText(hDlg, IDC_MD5NAME, obj->GetOBJname().c_str());

		obj->GetPosText(hDlg);
		obj->GetRotationText(hDlg);
		obj->GetScaleText(hDlg);

		return TRUE;
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDC_ANICOMBO:
			switch (HIWORD(wParam))
			{
			case CBN_SELCHANGE:
				obj = GetObjIndex(SelectListNum);
				i = SendMessage(md5AniHwnd, CB_GETCURSEL, 0, 0);

				dynamic_cast<LoadMD5*>(obj)->StopAni();
				dynamic_cast<LoadMD5*>(obj)->SetPlayAniNum(i);

				return TRUE;
			}
			return TRUE;
		case IDC_ANIPLAY:
			obj = GetObjIndex(SelectListNum);
			if (!dynamic_cast<LoadMD5*>(obj)->IsPlayingAni())
				dynamic_cast<LoadMD5*>(obj)->PlayAni();
			else
				dynamic_cast<LoadMD5*>(obj)->StopAni();

			return TRUE;
		case IDC_ANIADDBUTTON:
			obj = GetObjIndex(SelectListNum);
			SetCurrentDirectory(sPath);

			memset(&OFN, 0, sizeof(OPENFILENAME));
			OFN.lStructSize = sizeof(OPENFILENAME);
			OFN.hwndOwner = MainhWnd;
			OFN.lpstrFilter = L"md5anim File(*.md5anim)\0*.md5anim\0";
			OFN.lpstrFile = lpstrfile;
			OFN.nMaxFile = 256;
			OFN.lpstrInitialDir = L".\\model";

			if (GetOpenFileName(&OFN) != 0)
			{
				TCHAR str[100];
				wsprintf(str, L"%s 파일을 선택했슴", OFN.lpstrFile); // lpstrFile <- 받아짐
				MessageBox(MainhWnd, str, L"파일 열기 성공", MB_OK);

				//LoadObjMesh("여기에 경로를 넣는다");

				_wsplitpath(lpstrfile, NULL, NULL, filename, ext);

				wstr = filename;
				wstr.append(ext);

				SetCurrentDirectory(sPath);
				if (dynamic_cast<LoadMD5*>(obj)->LoadMD5Anim(lpstrfile))
				{
					i = SendMessage(md5AniHwnd, CB_GETCOUNT, 0, 0);
					SendMessage(md5AniHwnd, CB_INSERTSTRING, i, (LPARAM)wstr.c_str());
					SendMessage(md5AniHwnd, CB_SETCURSEL, 0, 0);
					MessageBox(MainhWnd, L"애니메이션 열기 성공", L"파일 열기 성공", MB_OK);
				}
				else
				{
					MessageBox(MainhWnd, L"애니메이션 열기 실패", L"파일 열기 실패", MB_OK);
				}
			}

			return TRUE;
		case IDC_MD5SCRIPTBUTTON:
			obj = GetObjIndex(SelectListNum);
			SetCurrentDirectory(sPath);

			memset(&OFN, 0, sizeof(OPENFILENAME));
			OFN.lStructSize = sizeof(OPENFILENAME);
			OFN.hwndOwner = MainhWnd;
			OFN.lpstrFilter = L"lua File(*.lua)\0*.lua\0";
			OFN.lpstrFile = lpstrfile;
			OFN.nMaxFile = 256;
			OFN.lpstrInitialDir = L".\\lua";

			if (GetOpenFileName(&OFN) != 0)
			{
				TCHAR str[100];
				wsprintf(str, L"%s 파일을 선택했슴", OFN.lpstrFile); // lpstrFile <- 받아짐
				MessageBox(MainhWnd, str, L"파일 열기 성공", MB_OK);

				//LoadObjMesh("여기에 경로를 넣는다");

				_wsplitpath(lpstrfile, NULL, NULL, filename, ext);

				wstr = L".\\lua\\";
				wstr += filename;
				wstr.append(ext);

				std::string sstr;
				sstr.assign(wstr.begin(), wstr.end());

				SetCurrentDirectory(sPath);
				obj->SetbSctipt(true);
				obj->PushScriptList(sstr);

				i = SendMessage(md5scriptHwnd, CB_GETCOUNT, 0, 0);
				SendMessage(md5scriptHwnd, CB_INSERTSTRING, i, (LPARAM)wstr.c_str());
				SendMessage(md5scriptHwnd, CB_SETCURSEL, 0, 0);
				MessageBox(MainhWnd, L"스크립트 열기 성공", L"파일 열기 성공", MB_OK);
			}

			return TRUE;
		case IDCLOSE:
			DestroyWindow(md5Sethwnd);
			EndDialog(hDlg, IDCANCEL);
			md5Sethwnd = NULL;
			return TRUE;

		case IDC_MD5NAME:
			obj = GetObjIndex(SelectListNum);
			GetDlgItemText(hDlg, IDC_MD5NAME, str, sizeof(str));
			obj->SetOBJname(str);

			plusname = str;

			plusname += L"(md5)";

			SendMessage(OBJListhwnd, LB_DELETESTRING, SelectListNum, 0);
			SendMessage(OBJListhwnd, LB_INSERTSTRING, SelectListNum, (LPARAM)plusname.c_str());
			return TRUE;

		case IDC_MD5POSX:
		case IDC_MD5POSY:
		case IDC_MD5POSZ:
		case IDC_MD5ROPITCH:
		case IDC_MD5ROYAW:
		case IDC_MD5ROROLL:
		case IDC_MD5SCALEX:
		case IDC_MD5SCALEY:
		case IDC_MD5SCALEZ:
			obj = GetObjIndex(SelectListNum);

			if (CurrentListNum == SelectListNum)
			{
				GetDlgItemText(hDlg, IDC_MD5POSX, str, sizeof(str));
				x = _wtof(str);
				GetDlgItemText(hDlg, IDC_MD5POSY, str, sizeof(str));
				y = _wtof(str);
				GetDlgItemText(hDlg, IDC_MD5POSZ, str, sizeof(str));
				z = _wtof(str);
				obj->SetPos(x, y, z);

				GetDlgItemText(hDlg, IDC_MD5ROPITCH, str, sizeof(str));
				x = _wtof(str);
				GetDlgItemText(hDlg, IDC_MD5ROYAW, str, sizeof(str));
				y = _wtof(str);
				GetDlgItemText(hDlg, IDC_MD5ROROLL, str, sizeof(str));
				z = _wtof(str);
				obj->SetRotation(x, y, z);

				GetDlgItemText(hDlg, IDC_MD5SCALEX, str, sizeof(str));
				x = _wtof(str);
				GetDlgItemText(hDlg, IDC_MD5SCALEY, str, sizeof(str));
				y = _wtof(str);
				GetDlgItemText(hDlg, IDC_MD5SCALEZ, str, sizeof(str));
				z = _wtof(str);
				obj->SetScale(x, y, z);

				if (HIWORD(wParam) == EN_KILLFOCUS)
				{
					SetDlgItemText(hDlg, IDC_MD5NAME, obj->GetOBJname().c_str());

					obj->GetPosText(hDlg);
					obj->GetRotationText(hDlg);
					obj->GetScaleText(hDlg);

					//InvalidateRect(MainhWnd, NULL, TRUE);
				}
			}

			return TRUE;
		}
	}
	return FALSE;
}
BOOL CALLBACK MainLoop::ObjSettingDlgProc(HWND hDlg, UINT iMessage, WPARAM wParam, LPARAM lParam)
{
	int i;
	LoadMain* obj;

	std::wifstream tvalue;
	std::wstring plusname;

	float x;
	float y;
	float z;

	OPENFILENAME OFN;
	WCHAR lpstrfile[MAX_PATH] = L"";
	WCHAR filename[MAX_PATH] = L"";
	WCHAR ext[MAX_PATH] = L"";
	WCHAR filename2[MAX_PATH] = L"";

	std::wstring wstr;

	WCHAR str[MAX_PATH];
	switch (iMessage)
	{
	case WM_INITDIALOG:
		obj = GetObjIndex(SelectListNum);
		SetDlgItemText(hDlg, IDC_OBJNAME, obj->GetOBJname().c_str());

		obj->GetPosText(hDlg);
		obj->GetRotationText(hDlg);
		obj->GetScaleText(hDlg);

		return TRUE;
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDCLOSE:
			DestroyWindow(objSethwnd);
			EndDialog(hDlg, IDCANCEL);
			objSethwnd = NULL;
			return TRUE;
		case IDC_OBJSCRIPTBUTTON:
			obj = GetObjIndex(SelectListNum);
			SetCurrentDirectory(sPath);

			memset(&OFN, 0, sizeof(OPENFILENAME));
			OFN.lStructSize = sizeof(OPENFILENAME);
			OFN.hwndOwner = MainhWnd;
			OFN.lpstrFilter = L"lua File(*.lua)\0*.lua\0";
			OFN.lpstrFile = lpstrfile;
			OFN.nMaxFile = 256;
			OFN.lpstrInitialDir = L".\\lua";

			if (GetOpenFileName(&OFN) != 0)
			{
				TCHAR str[100];
				wsprintf(str, L"%s 파일을 선택했슴", OFN.lpstrFile); // lpstrFile <- 받아짐
				MessageBox(MainhWnd, str, L"파일 열기 성공", MB_OK);

				//LoadObjMesh("여기에 경로를 넣는다");

				_wsplitpath(lpstrfile, NULL, NULL, filename, ext);

				wstr = L".\\lua\\";
				wstr += filename;
				wstr.append(ext);

				std::string sstr;
				sstr.assign(wstr.begin(), wstr.end());

				SetCurrentDirectory(sPath);
				obj->SetbSctipt(true);
				obj->PushScriptList(sstr);

				i = SendMessage(objscriptHwnd, CB_GETCOUNT, 0, 0);
				SendMessage(objscriptHwnd, CB_INSERTSTRING, i, (LPARAM)wstr.c_str());
				SendMessage(objscriptHwnd, CB_SETCURSEL, 0, 0);
				MessageBox(MainhWnd, L"스크립트 열기 성공", L"파일 열기 성공", MB_OK);
			}

			return TRUE;

		case IDC_OBJNAME:
			obj = GetObjIndex(SelectListNum);
			GetDlgItemText(hDlg, IDC_OBJNAME, str, sizeof(str));
			obj->SetOBJname(str);

			plusname = str;

			plusname += L"(obj)";

			SendMessage(OBJListhwnd, LB_DELETESTRING, SelectListNum, 0);
			SendMessage(OBJListhwnd, LB_INSERTSTRING, SelectListNum, (LPARAM)plusname.c_str());
			return TRUE;

		case IDC_OBJPOSX:
		case IDC_OBJPOSY:
		case IDC_OBJPOSZ:
		case IDC_OBJROPITCH:
		case IDC_OBJROYAW:
		case IDC_OBJROROLL:
		case IDC_SCALEX:
		case IDC_SCALEY:
		case IDC_SCALEZ:
			obj = GetObjIndex(SelectListNum);

			if (CurrentListNum == SelectListNum)
			{
				GetDlgItemText(hDlg, IDC_OBJPOSX, str, sizeof(str));
				x = _wtof(str);
				GetDlgItemText(hDlg, IDC_OBJPOSY, str, sizeof(str));
				y = _wtof(str);
				GetDlgItemText(hDlg, IDC_OBJPOSZ, str, sizeof(str));
				z = _wtof(str);
				obj->SetPos(x, y, z);

				GetDlgItemText(hDlg, IDC_OBJROPITCH, str, sizeof(str));
				x = _wtof(str);
				GetDlgItemText(hDlg, IDC_OBJROYAW, str, sizeof(str));
				y = _wtof(str);
				GetDlgItemText(hDlg, IDC_OBJROROLL, str, sizeof(str));
				z = _wtof(str);
				obj->SetRotation(x, y, z);

				GetDlgItemText(hDlg, IDC_SCALEX, str, sizeof(str));
				x = _wtof(str);
				GetDlgItemText(hDlg, IDC_SCALEY, str, sizeof(str));
				y = _wtof(str);
				GetDlgItemText(hDlg, IDC_SCALEZ, str, sizeof(str));
				z = _wtof(str);
				obj->SetScale(x, y, z);

				if (HIWORD(wParam) == EN_KILLFOCUS)
				{
					SetDlgItemText(hDlg, IDC_OBJNAME, obj->GetOBJname().c_str());

					obj->GetPosText(hDlg);
					obj->GetRotationText(hDlg);
					obj->GetScaleText(hDlg);

					//InvalidateRect(MainhWnd, NULL, TRUE);
				}
			}

			return TRUE;
		}
	}
	return FALSE;
}

bool MainLoop::LoadObjOpen(std::wstring wstr)
{
	std::wstring str = wstr;

	ObjMap::iterator it = objMapList.find(str);
	if (it == objMapList.end())
	{
		LoadMain* obj = new LoadObj;
		obj->Setup(md3dDevice, md3dImmediateContext, mSwapChain);
		obj->Init(TexSamplerState, RSCullNone);

		if (dynamic_cast<LoadObj*>(obj)->LoadObjModel(str, true, false))
		{
			obj->SetPROname(str);
			objMapList.insert(std::make_pair(str, obj));
			MessageBox(MainhWnd, L"파일이 추가됬습니다", L"파일 열기 성공", MB_OK);

			return true;
		}
		else
		{
			SAFE_DELETE(obj);
			MessageBox(MainhWnd, L"파일을 열수없습니다!", L"파일 열기 실패", MB_OK);

			std::wstring fail = L"";

			return false;
		}
	}
	else
	{
		MessageBox(MainhWnd, L"이미 추가된 파일입니다", L"중복되는 파일있음", MB_OK);

		return false;
	}
}
bool MainLoop::LoadMd5Open(std::wstring wstr)
{
	std::wstring str = wstr;

	ObjMap::iterator it = objMapList.find(str);
	if (it == objMapList.end())
	{
		LoadMain* obj = new LoadMD5;
		obj->Setup(md3dDevice, md3dImmediateContext, mSwapChain);
		obj->Init(TexSamplerState, RSCullNone);

		if (dynamic_cast<LoadMD5*>(obj)->LoadMD5Model(str))
		{
			obj->SetPROname(str);
			objMapList.insert(std::make_pair(str, obj));
			MessageBox(MainhWnd, L"파일이 추가됬습니다", L"파일 열기 성공", MB_OK);

			return true;
		}
		else
		{
			SAFE_DELETE(obj);
			MessageBox(MainhWnd, L"파일을 열수없습니다!", L"파일 열기 실패", MB_OK);

			std::wstring fail = L"";

			return false;
		}
	}
	else
	{
		MessageBox(MainhWnd, L"이미 추가된 파일입니다", L"중복되는 파일있음", MB_OK);

		return false;
	}
}

LoadMain* MainLoop::GetObjIndex(int index)
{
	int i = 0;
	ObjList::iterator it;
	for (it = objList.begin(); it != objList.end(); it++)
	{
		if (i == index)
		{
			return (*it);
		}
		else
			i++;
	}

	return NULL;
}
std::wstring MainLoop::AddObjFile(LPWSTR wstr)
{
	std::wstring str = wstr;

	ObjMap::iterator it = objMapList.find(str);
	if (it == objMapList.end())
	{
		MessageBox(MainhWnd, L"오브젝트 생성에 실패했습니다!", L"오브젝트 생성 실패", MB_OK);

		return L"";
	}
	else
	{
		LoadMain* obj = dynamic_cast<LoadObj*>(it->second)->MakeClone();
		obj->SetPROname(str);
		objList.push_front(obj);
		MessageBox(MainhWnd, L"리스트에 오브젝트가 추가됬습니다!", L"오브젝트 추가 완료", MB_OK);

		std::wstring returnobjname = obj->GetOBJname();
		returnobjname += L"(obj)";

		return returnobjname;
	}
}
std::wstring MainLoop::AddMd5File(LPWSTR wstr)
{
	std::wstring str = wstr;

	ObjMap::iterator it = objMapList.find(str);
	if (it == objMapList.end())
	{
		MessageBox(MainhWnd, L"오브젝트 생성에 실패했습니다!", L"오브젝트 생성 실패", MB_OK);

		return L"";
	}
	else
	{
		LoadMain* obj = dynamic_cast<LoadMD5*>(it->second)->MakeClone();
		obj->SetPROname(str);
		objList.push_front(obj);
		MessageBox(MainhWnd, L"리스트에 오브젝트가 추가됬습니다!", L"오브젝트 추가 완료", MB_OK);

		std::wstring returnobjname = obj->GetOBJname();
		returnobjname += L"(md5)";

		return returnobjname;
	}
}
void MainLoop::SetFocusObj(int index)
{
	FocusNum = index;
}

void MainLoop::SetSaveObjTransfrom()
{
	ObjList::iterator it;
	for (it = objList.begin(); it != objList.end(); it++)
	{
		(*it)->SetSaveTranfrom();
	}
}
void MainLoop::ResetSaveObjTransfrom()
{
	ObjList::iterator it;
	for (it = objList.begin(); it != objList.end(); it++)
	{
		(*it)->ResetSaveTranfrom();
	}
}

void MainLoop::DetectInput(double time)
{
	DIKeyboard->Acquire();
	DIMouse->Acquire();

	DIMouse->GetDeviceState(sizeof(DIMOUSESTATE), &mouseCurrState);

	DIKeyboard->GetDeviceState(sizeof(keyboardState), (LPVOID)&keyboardState);

	/*if (keyboardState[DIK_ESCAPE] & 0x80)
	PostMessage(hWnd, WM_DESTROY, 0, 0);*/

	float speed = 15.0f * time;

	float moveLeftRight = camera.GetLR();
	float moveBackForward = camera.GetFB();

	if ((mouseCurrState.rgbButtons[1] & 0x80) && !bPlayGame)
	{
		if (keyboardState[DIK_A] & 0x80)
		{
			moveLeftRight -= speed;
		}
		if (keyboardState[DIK_D] & 0x80)
		{
			moveLeftRight += speed;
		}
		if (keyboardState[DIK_W] & 0x80)
		{
			moveBackForward += speed;
		}
		if (keyboardState[DIK_S] & 0x80)
		{
			moveBackForward -= speed;
		}

		camera.SetPos(moveLeftRight, moveBackForward);

		float camYaw = camera.GetYaw();
		float camPitch = camera.GetPitch();

		if ((mouseCurrState.lX != mouseLastState.lX) || (mouseCurrState.lY != mouseLastState.lY) ||
			(mouseCurrState.lZ != mouseLastState.lZ))
		{
			camYaw += mouseCurrState.lX * 0.001f;

			camPitch += mouseCurrState.lY * 0.001f;

			camera.SetYawPitch(camYaw, camPitch);

			if (mouseCurrState.lZ != mouseLastState.lZ)
			{
				moveBackForward += mouseCurrState.lZ * 0.01f;
				camera.SetPos(moveLeftRight, moveBackForward);
			}

			mouseLastState = mouseCurrState;
		}
	}

	if ((mouseCurrState.rgbButtons[0] & 0x80) && !bPlayGame)
	{
		if (MainhWnd == GetActiveWindow())
		{
			if (isShoot == false)
			{
				POINT mousePos;

				GetCursorPos(&mousePos);
				ScreenToClient(MainhWnd, &mousePos);

				int mousex = mousePos.x;
				int mousey = mousePos.y;

				float tempDist;
				float closestDist = FLT_MAX;
				int hitIndex;

				XMVECTOR prwsPos, prwsDir;
				pickRayVector(mousex, mousey, prwsPos, prwsDir);

				int i = 0;
				ObjList::iterator it;
				for (it = objList.begin(); it != objList.end(); it++)
				{
					tempDist = (*it)->pick(prwsPos, prwsDir);

					if (tempDist < closestDist)
					{
						closestDist = tempDist;
						hitIndex = i;
					}
					i++;
				}

				if (closestDist < FLT_MAX)
				{
					SetFocusObj(hitIndex);
					SendMessage(OBJListhwnd, LB_SETCURSEL, hitIndex, 0);

					SendMessage(OBJHwnd, WM_COMMAND, MAKEWPARAM(IDC_OBJLISTBOX, LBN_SELCHANGE), 0);
					SendMessage(OBJListhwnd, LB_SETCURSEL, hitIndex, 0);
				}

				isShoot = true;
			}
		}
	}
	if (!mouseCurrState.rgbButtons[0])
	{
		isShoot = false;
	}
	return;
}
void MainLoop::GridVerSetting()
{
	SAFE_DELETE_ARRAY(GridV);

	GridV = new GridVertex[GridCount + 2];

	int GridMemberCount = 0;

	for (int i = 0; i < GridCount / 4; i++)
	{
		float x = i - (((GridCount / 4) - 1.0f) / 2.0f);
		float z = ((GridCount / 4) - 1.0f) / 2.0f;

		if (i == ((GridCount / 4) - 1) / 2)
		{
			GridV[i + GridMemberCount].pos.x = x;
			GridV[i + GridMemberCount].pos.z = z;
			GridV[i + GridMemberCount].pos.y = 0.0f;
			GridV[i + GridMemberCount].color = XMFLOAT4{ 0.0f, 0.0f, 1.0f, 1.0f };

			GridV[i + GridMemberCount + 1].pos.x = x;
			GridV[i + GridMemberCount + 1].pos.z = -z;
			GridV[i + GridMemberCount + 1].pos.y = 0.0f;
			GridV[i + GridMemberCount + 1].color = XMFLOAT4{ 0.0f, 0.0f, 1.0f, 1.0f };
		}
		else
		{
			GridV[i + GridMemberCount].pos.x = x;
			GridV[i + GridMemberCount].pos.z = z;
			GridV[i + GridMemberCount].pos.y = 0.0f;
			GridV[i + GridMemberCount].color = XMFLOAT4{ 0.3f, 0.3f, 0.3f, 1.0f };

			GridV[i + GridMemberCount + 1].pos.x = x;
			GridV[i + GridMemberCount + 1].pos.z = -z;
			GridV[i + GridMemberCount + 1].pos.y = 0.0f;
			GridV[i + GridMemberCount + 1].color = XMFLOAT4{ 0.3f, 0.3f, 0.3f, 1.0f };
		}

		GridMemberCount++;
	}
	for (int i = GridCount / 4; i < GridCount / 2; i++)
	{
		float x = -((GridCount / 4) - 1.0f) / 2.0f;
		float z = (((GridCount / 4) - 1.0f) / 2.0f) - (i - GridCount / 4);

		if (i == ((GridCount / 4) + ((GridCount / 4) - 1) / 2))
		{
			GridV[i + GridMemberCount].pos.x = x;
			GridV[i + GridMemberCount].pos.z = z;
			GridV[i + GridMemberCount].pos.y = 0.0f;
			GridV[i + GridMemberCount].color = XMFLOAT4{ 1.0f, 0.0f, 0.0f, 1.0f };

			GridV[i + GridMemberCount + 1].pos.x = -x;
			GridV[i + GridMemberCount + 1].pos.z = z;
			GridV[i + GridMemberCount + 1].pos.y = 0.0f;
			GridV[i + GridMemberCount + 1].color = XMFLOAT4{ 1.0f, 0.0f, 0.0f, 1.0f };
		}
		else
		{
			GridV[i + GridMemberCount].pos.x = x;
			GridV[i + GridMemberCount].pos.z = z;
			GridV[i + GridMemberCount].pos.y = 0.0f;
			GridV[i + GridMemberCount].color = XMFLOAT4{ 0.3f, 0.3f, 0.3f, 1.0f };

			GridV[i + GridMemberCount + 1].pos.x = -x;
			GridV[i + GridMemberCount + 1].pos.z = z;
			GridV[i + GridMemberCount + 1].pos.y = 0.0f;
			GridV[i + GridMemberCount + 1].color = XMFLOAT4{ 0.3f, 0.3f, 0.3f, 1.0f };
		}

		GridMemberCount++;
	}
	GridV[GridCount].pos.x = 0.0f;
	GridV[GridCount].pos.z = 0.0f;
	GridV[GridCount].pos.y = -100.0f;
	GridV[GridCount].color = XMFLOAT4{ 0.0f, 1.0f, 0.0f, 1.0f };

	GridV[GridCount + 1].pos.x = 0.0f;
	GridV[GridCount + 1].pos.z = 0.0f;
	GridV[GridCount + 1].pos.y = 100.0f;
	GridV[GridCount + 1].color = XMFLOAT4{ 0.0f, 1.0f, 0.0f, 1.0f };

	if (GridVertBuffer != NULL)
		GridVertBuffer->Release();

	D3D11_BUFFER_DESC vertexBufferDesc;
	ZeroMemory(&vertexBufferDesc, sizeof(vertexBufferDesc));

	vertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	vertexBufferDesc.ByteWidth = sizeof(GridVertex)* (GridCount + 2);
	vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vertexBufferDesc.CPUAccessFlags = 0;
	vertexBufferDesc.MiscFlags = 0;

	D3D11_SUBRESOURCE_DATA vertexBufferData;

	ZeroMemory(&vertexBufferData, sizeof(vertexBufferData));
	vertexBufferData.pSysMem = GridV;
	md3dDevice->CreateBuffer(&vertexBufferDesc, &vertexBufferData, &GridVertBuffer);
}

void MainLoop::pickRayVector(float mouseX, float mouseY, XMVECTOR& pickRayInWorldSpacePos, XMVECTOR& pickRayInWorldSpaceDir)
{
	XMVECTOR pickRayInViewSpaceDir = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
	XMVECTOR pickRayInViewSpacePos = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);

	float PRVecX, PRVecY, PRVecZ;

	//Transform 2D pick position on screen space to 3D ray in View space
	PRVecX = (((2.0f * mouseX) / Width) - 1) / camera.GetCamProjection()(0, 0);
	PRVecY = -(((2.0f * mouseY) / Height) - 1) / camera.GetCamProjection()(1, 1);
	PRVecZ = 1.0f;	//View space's Z direction ranges from 0 to 1, so we set 1 since the ray goes "into" the screen

	pickRayInViewSpaceDir = XMVectorSet(PRVecX, PRVecY, PRVecZ, 0.0f);

	//Uncomment this line if you want to use the center of the screen (client area)
	//to be the point that creates the picking ray (eg. first person shooter)
	//pickRayInViewSpaceDir = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);

	// Transform 3D Ray from View space to 3D ray in World space
	XMMATRIX pickRayToWorldSpaceMatrix;
	XMVECTOR matInvDeter;	//We don't use this, but the xna matrix inverse function requires the first parameter to not be null

	pickRayToWorldSpaceMatrix = XMMatrixInverse(&matInvDeter, XMLoadFloat4x4(&camera.GetCamView()));	//Inverse of View Space matrix is World space matrix

	pickRayInWorldSpacePos = XMVector3TransformCoord(pickRayInViewSpacePos, pickRayToWorldSpaceMatrix);
	pickRayInWorldSpaceDir = XMVector3TransformNormal(pickRayInViewSpaceDir, pickRayToWorldSpaceMatrix);
}

bool MainLoop::InitDirectInput(HINSTANCE hInstance, HWND hwnd)
{
	HRESULT hr;

	hr = DirectInput8Create(hInstance,
		DIRECTINPUT_VERSION,
		IID_IDirectInput8,
		(void**)&DirectInput,
		NULL);

	hr = DirectInput->CreateDevice(GUID_SysKeyboard,
		&DIKeyboard,
		NULL);

	hr = DirectInput->CreateDevice(GUID_SysMouse,
		&DIMouse,
		NULL);

	hr = DIKeyboard->SetDataFormat(&c_dfDIKeyboard);
	hr = DIKeyboard->SetCooperativeLevel(hwnd, DISCL_FOREGROUND | DISCL_NONEXCLUSIVE);

	hr = DIMouse->SetDataFormat(&c_dfDIMouse);
	hr = DIMouse->SetCooperativeLevel(hwnd, DISCL_NOWINKEY);

	return true;
}

void MainLoop::GridOnRender()
{
	//Set Primitive Topology
	md3dImmediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

	//Set the Input Layout
	md3dImmediateContext->IASetInputLayout(InputLayouts::GridPos);

	ID3DX11EffectTechnique* activeTech = Effects::BasicFX->GridTech;

	UINT stride = sizeof(GridVertex);
	UINT offset = 0;
	md3dImmediateContext->IASetVertexBuffers(0, 1, &GridVertBuffer, &stride, &offset);

	XMMATRIX Rotation = XMMatrixRotationY(3.14f);
	XMMATRIX Scale = XMMatrixScaling(1.0f, 1.0f, 1.0f);
	XMMATRIX Translation = XMMatrixTranslation(0.0f, 0.0f, 0.0f);

	XMMATRIX world = Rotation * Scale * Translation;

	XMMATRIX wvp = world * XMLoadFloat4x4(&camera.GetCamView()) * XMLoadFloat4x4(&camera.GetCamProjection());

	Effects::BasicFX->SetGridWorldViewProj(wvp);

	activeTech->GetPassByIndex(0)->Apply(0, md3dImmediateContext);

	md3dImmediateContext->Draw(GridCount + 2, 0);
}
void MainLoop::BuildShadowTransform()
{
	// Only the first "main" light casts a shadow.
	XMVECTOR lightDir = XMLoadFloat3(&mDirLights[0].Direction);
	XMVECTOR lightPos = -2.0f*mSceneBounds.Radius*lightDir;
	XMVECTOR targetPos = XMLoadFloat3(&mSceneBounds.Center);
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	XMMATRIX V = XMMatrixLookAtLH(lightPos, targetPos, up);

	// Transform bounding sphere to light space.
	XMFLOAT3 sphereCenterLS;
	XMStoreFloat3(&sphereCenterLS, XMVector3TransformCoord(targetPos, V));

	// Ortho frustum in light space encloses scene.
	float l = sphereCenterLS.x - mSceneBounds.Radius;
	float b = sphereCenterLS.y - mSceneBounds.Radius;
	float n = sphereCenterLS.z - mSceneBounds.Radius;
	float r = sphereCenterLS.x + mSceneBounds.Radius;
	float t = sphereCenterLS.y + mSceneBounds.Radius;
	float f = sphereCenterLS.z + mSceneBounds.Radius;
	XMMATRIX P = XMMatrixOrthographicOffCenterLH(l, r, b, t, n, f);

	// Transform NDC space [-1,+1]^2 to texture space [0,1]^2
	XMMATRIX T(
		0.5f, 0.0f, 0.0f, 0.0f,
		0.0f, -0.5f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.5f, 0.5f, 0.0f, 1.0f);

	XMMATRIX S = V*P*T;

	XMStoreFloat4x4(&mLightView, V);
	XMStoreFloat4x4(&mLightProj, P);
	XMStoreFloat4x4(&mShadowTransform, S);
}

int MainLoop::GetObj(lua_State *L)
{
	lua_pushlightuserdata(L, CurrentScriptObj);

	return 1;
}
int MainLoop::ForwordMove(lua_State *L)
{
	LoadMain* obj = (LoadMain*)lua_touserdata(L, 1);

	int forback = lua_tonumber(L, 2);
	double speed = lua_tonumber(L, 3);
	std::string key = lua_tostring(L, 4);

	XMVECTOR forword;

	if (GetVK(key))
	{
		if (forback == 0)
		{
			forword = XMVectorSet(0.0f, 0.0f, 1.0f, 1.0f);
		}
		else
		{
			forword = XMVectorSet(0.0f, 0.0f, -1.0f, 1.0f);
		}

		XMFLOAT3 rot = obj->GetRotation();

		XMMATRIX rotm = XMMatrixRotationRollPitchYaw(rot.x * 0.0174532925f, rot.y * 0.0174532925f, rot.z * 0.0174532925f);

		forword = XMVector3TransformNormal(forword, rotm);

		XMFLOAT3 pos = obj->GetPos();

		XMVECTOR posv = XMVectorSet(pos.x, pos.y, pos.z, 1.0f);

		posv = posv + (forword * tick * (speed * 10.0f));

		XMStoreFloat3(&pos, posv);

		obj->SetPos(pos.x, pos.y, pos.z);

		lua_pushboolean(L, true);

		return 1;
	}

	lua_pushboolean(L, false);

	return 1;
}
int MainLoop::CharRotation(lua_State *L)
{
	LoadMain* obj = (LoadMain*)lua_touserdata(L, 1);

	int rightleft = lua_tonumber(L, 2);
	double speed = lua_tonumber(L, 3);
	std::string key = lua_tostring(L, 4);

	float right;

	if (GetVK(key))
	{
		if (rightleft == 0)
		{
			right = 1.0f;
		}
		else
		{
			right = -1.0;
		}
		float test = right * (speed * 10.0f) * tick;
		XMVECTOR rot = XMQuaternionRotationAxis(XMVectorSet(0.0f, 1.0f, 0.0f, 1.0f), right * (speed * 2.0f) * tick);
		XMFLOAT3 rof3 = obj->GetRotation();
		XMVECTOR objrom = XMQuaternionRotationRollPitchYaw(rof3.x * 0.0174532925f, rof3.y * 0.0174532925f, rof3.z * 0.0174532925f);

		XMVECTOR rotv = XMQuaternionMultiply(rot, objrom);

		XMFLOAT4 rof4;
		XMStoreFloat4(&rof4, rotv);

		float x;
		float y;
		float z;

		quat_2_euler_d3d(rof4, x, y, z);
		obj->SetRotation(x * 57.2957795, y * 57.2957795, z * 57.2957795);

		lua_pushboolean(L, true);

		return 1;
	}
	lua_pushboolean(L, false);

	return 1;
}
int MainLoop::MainCameraSet(lua_State *L)
{
	LoadMain* obj = (LoadMain*)lua_touserdata(L, 1);

	float xx = lua_tonumber(L, 2);
	float yy = lua_tonumber(L, 3);
	float zz = lua_tonumber(L, 4);

	XMVECTOR camposv = XMVectorSet(xx, yy, zz, 1.0f);
	XMVECTOR charposv = XMVectorSet(obj->GetPos().x, obj->GetPos().y, obj->GetPos().z, 1.0f);
	XMMATRIX charrom = XMMatrixRotationRollPitchYaw(obj->GetRotation().x * 0.0174532925f, obj->GetRotation().y * 0.0174532925f, obj->GetRotation().z * 0.0174532925f);

	camposv = XMVector4Transform(camposv, charrom);
	camposv += charposv;

	XMFLOAT4 campos;
	XMStoreFloat4(&campos, camposv);

	camera.SetbCharCamera(true);

	camera.SetTargetPosition(obj->GetPos(), campos);

	return 0;
}
int MainLoop::PlayAni(lua_State *L)
{
	LoadMain* obj = (LoadMain*)lua_touserdata(L, 1);

	int aninum = lua_tonumber(L, 2);

	if (obj->GetType() == 1)
	{
		if (dynamic_cast<LoadMD5*>(obj)->GetAniCount() > aninum)
		{
			dynamic_cast<LoadMD5*>(obj)->PlayAni();
			dynamic_cast<LoadMD5*>(obj)->SetPlayAniNum(aninum);
		}
	}

	return 0;
}
bool MainLoop::GetVK(std::string key)
{
	switch (key.c_str()[0])
	{
	case 'a': case 'A': if (keyboardState[DIK_A] & 0x80) { return true; } break;
	case 'b': case 'B': if (keyboardState[DIK_B] & 0x80) { return true; } break;
	case 'c': case 'C': if (keyboardState[DIK_C] & 0x80) { return true; } break;
	case 'd': case 'D': if (keyboardState[DIK_D] & 0x80) { return true; } break;
	case 'e': case 'E': if (keyboardState[DIK_E] & 0x80) { return true; } break;
	case 'f': case 'F': if (keyboardState[DIK_F] & 0x80) { return true; } break;
	case 'g': case 'G': if (keyboardState[DIK_G] & 0x80) { return true; } break;
	case 'h': case 'H': if (keyboardState[DIK_H] & 0x80) { return true; } break;
	case 'i': case 'I': if (keyboardState[DIK_I] & 0x80) { return true; } break;
	case 'j': case 'J': if (keyboardState[DIK_J] & 0x80) { return true; } break;
	case 'k': case 'K': if (keyboardState[DIK_K] & 0x80) { return true; } break;
	case 'l': case 'L': if (keyboardState[DIK_L] & 0x80) { return true; } break;
	case 'm': case 'M': if (keyboardState[DIK_M] & 0x80) { return true; } break;
	case 'n': case 'N': if (keyboardState[DIK_N] & 0x80) { return true; } break;
	case 'o': case 'O': if (keyboardState[DIK_O] & 0x80) { return true; } break;
	case 'p': case 'P': if (keyboardState[DIK_P] & 0x80) { return true; } break;
	case 'q': case 'Q': if (keyboardState[DIK_Q] & 0x80) { return true; } break;
	case 'r': case 'R': if (keyboardState[DIK_R] & 0x80) { return true; } break;
	case 's': case 'S': if (keyboardState[DIK_S] & 0x80) { return true; } break;
	case 't': case 'T': if (keyboardState[DIK_T] & 0x80) { return true; } break;
	case 'u': case 'U': if (keyboardState[DIK_U] & 0x80) { return true; } break;
	case 'v': case 'V': if (keyboardState[DIK_V] & 0x80) { return true; } break;
	case 'w': case 'W': if (keyboardState[DIK_W] & 0x80) { return true; } break;
	case 'x': case 'X': if (keyboardState[DIK_X] & 0x80) { return true; } break;
	case 'y': case 'Y': if (keyboardState[DIK_Y] & 0x80) { return true; } break;
	case 'z': case 'Z': if (keyboardState[DIK_Z] & 0x80) { return true; } break;
	}
	return false;
}
void MainLoop::quat_2_euler_d3d(const XMFLOAT4& q, float& pitch, float& yaw, float& roll)
{
	float sqw = q.w*q.w;
	float sqx = q.x*q.x;
	float sqy = q.y*q.y;
	float sqz = q.z*q.z;
	pitch = asinf(2.0f * (q.w*q.x - q.y*q.z)); // rotation about x-axis
	yaw = atan2f(2.0f * (q.x*q.z + q.w*q.y), (-sqx - sqy + sqz + sqw)); // rotation about y-axis
	roll = atan2f(2.0f * (q.x*q.y + q.w*q.z), (-sqx + sqy - sqz + sqw)); // rotation about z-axis
}