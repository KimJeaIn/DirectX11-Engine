//***************************************************************************************
// d3dApp.cpp by Frank Luna (C) 2011 All Rights Reserved.
//***************************************************************************************

#include "d3dApp.h"
#include "resource.h"
#include <WindowsX.h>
#include <sstream>

namespace
{
	// This is just used to forward Windows messages from a global window
	// procedure to our member function window procedure because we cannot
	// assign a member function to WNDCLASS::lpfnWndProc.
	D3DApp* gd3dApp = 0;
}

LRESULT CALLBACK
MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	// Forward hwnd on because we can get messages (e.g., WM_CREATE)
	// before CreateWindow returns, and thus before mhMainWnd is valid.
	return gd3dApp->MsgProc(hwnd, msg, wParam, lParam);
}

D3DApp::D3DApp(HINSTANCE hInstance)
:	mhAppInst(hInstance),
	mMainWndCaption(L"D3D11 Application"),
	md3dDriverType(D3D_DRIVER_TYPE_HARDWARE),
	mClientWidth(1024),
	mClientHeight(768),
	mEnable4xMsaa(false),
	mhMainWnd(0),
	mAppPaused(false),
	mMinimized(false),
	mMaximized(false),
	mResizing(false),
	m4xMsaaQuality(0),
 
	md3dDevice(0),
	md3dImmediateContext(0),
	mSwapChain(0),
	mDepthStencilBuffer(0),
	mRenderTargetView(0),
	mDepthStencilView(0)
{
	ZeroMemory(&mScreenViewport, sizeof(D3D11_VIEWPORT));

	// Get a pointer to the application object so we can forward 
	// Windows messages to the object's window procedure through
	// the global window procedure.
	gd3dApp = this;
}

D3DApp::~D3DApp()
{
	ReleaseCOM(mRenderTargetView);
	ReleaseCOM(mDepthStencilView);
	ReleaseCOM(mSwapChain);
	ReleaseCOM(mDepthStencilBuffer);

	// Restore all default settings.
	if( md3dImmediateContext )
		md3dImmediateContext->ClearState();

	ReleaseCOM(md3dImmediateContext);
	ReleaseCOM(md3dDevice);
}

HINSTANCE D3DApp::AppInst()const
{
	return mhAppInst;
}

HWND D3DApp::MainWnd()const
{
	return mhMainWnd;
}

float D3DApp::AspectRatio()const
{
	return static_cast<float>(mClientWidth) / mClientHeight;
}

int D3DApp::Run()
{
	MSG msg = {0};
 
	mTimer.Reset();

	while(msg.message != WM_QUIT)
	{
		// If there are Window messages then process them.
		if(PeekMessage( &msg, 0, 0, 0, PM_REMOVE ))
		{
            TranslateMessage( &msg );
            DispatchMessage( &msg );
		}
		// Otherwise, do animation/game stuff.
		else
        {	
			mTimer.Tick();

			if( !mAppPaused )
			{
				CalculateFrameStats();
				UpdateScene(mTimer.DeltaTime());	
				DrawScene();
			}
			else
			{
				Sleep(100);
			}
        }
    }

	return (int)msg.wParam;
}

bool D3DApp::Init()
{
	if(!InitMainWindow())
		return false;

	if(!InitDirect3D())
		return false;

	return true;
}
 
void D3DApp::OnResize()
{
	assert(md3dImmediateContext);
	assert(md3dDevice);
	assert(mSwapChain);

	// Release the old views, as they hold references to the buffers we
	// will be destroying.  Also release the old depth/stencil buffer.

	ReleaseCOM(mRenderTargetView);
	ReleaseCOM(mDepthStencilView);
	ReleaseCOM(mDepthStencilBuffer);


	// Resize the swap chain and recreate the render target view.

	HR(mSwapChain->ResizeBuffers(1, mClientWidth, mClientHeight, DXGI_FORMAT_R8G8B8A8_UNORM, 0));
	ID3D11Texture2D* backBuffer;
	HR(mSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(&backBuffer)));
	HR(md3dDevice->CreateRenderTargetView(backBuffer, 0, &mRenderTargetView));
	ReleaseCOM(backBuffer);

	// Create the depth/stencil buffer and view.

	D3D11_TEXTURE2D_DESC depthStencilDesc;
	
	depthStencilDesc.Width     = mClientWidth;
	depthStencilDesc.Height    = mClientHeight;
	depthStencilDesc.MipLevels = 1;
	depthStencilDesc.ArraySize = 1;
	depthStencilDesc.Format    = DXGI_FORMAT_D24_UNORM_S8_UINT;

	// Use 4X MSAA? --must match swap chain MSAA values.
	if( mEnable4xMsaa )
	{
		depthStencilDesc.SampleDesc.Count   = 4;
		depthStencilDesc.SampleDesc.Quality = m4xMsaaQuality-1;
	}
	// No MSAA
	else
	{
		depthStencilDesc.SampleDesc.Count   = 1;
		depthStencilDesc.SampleDesc.Quality = 0;
	}

	depthStencilDesc.Usage          = D3D11_USAGE_DEFAULT;
	depthStencilDesc.BindFlags      = D3D11_BIND_DEPTH_STENCIL;
	depthStencilDesc.CPUAccessFlags = 0; 
	depthStencilDesc.MiscFlags      = 0;

	HR(md3dDevice->CreateTexture2D(&depthStencilDesc, 0, &mDepthStencilBuffer));
	HR(md3dDevice->CreateDepthStencilView(mDepthStencilBuffer, 0, &mDepthStencilView));


	// Bind the render target view and depth/stencil view to the pipeline.

	md3dImmediateContext->OMSetRenderTargets(1, &mRenderTargetView, mDepthStencilView);
	

	// Set the viewport transform.

	mScreenViewport.TopLeftX = 0;
	mScreenViewport.TopLeftY = 0;
	mScreenViewport.Width    = static_cast<float>(mClientWidth);
	mScreenViewport.Height   = static_cast<float>(mClientHeight);
	mScreenViewport.MinDepth = 0.0f;
	mScreenViewport.MaxDepth = 1.0f;

	md3dImmediateContext->RSSetViewports(1, &mScreenViewport);
}
 
LRESULT D3DApp::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch( msg )
	{
	// WM_ACTIVATE is sent when the window is activated or deactivated.  
	// We pause the game when the window is deactivated and unpause it 
	// when it becomes active.  
	case WM_ACTIVATE:
		if( LOWORD(wParam) == WA_INACTIVE )
		{
			mAppPaused = true;
			mTimer.Stop();
		}
		else
		{
			mAppPaused = false;
			mTimer.Start();
		}
		return 0;

	// WM_SIZE is sent when the user resizes the window.  
	case WM_SIZE:
		// Save the new client area dimensions.
		mClientWidth  = LOWORD(lParam);
		mClientHeight = HIWORD(lParam);
		if( md3dDevice )
		{
			if( wParam == SIZE_MINIMIZED )
			{
				mAppPaused = true;
				mMinimized = true;
				mMaximized = false;
			}
			else if( wParam == SIZE_MAXIMIZED )
			{
				mAppPaused = false;
				mMinimized = false;
				mMaximized = true;
				OnResize();
			}
			else if( wParam == SIZE_RESTORED )
			{
				
				// Restoring from minimized state?
				if( mMinimized )
				{
					mAppPaused = false;
					mMinimized = false;
					OnResize();
				}

				// Restoring from maximized state?
				else if( mMaximized )
				{
					mAppPaused = false;
					mMaximized = false;
					OnResize();
				}
				else if( mResizing )
				{
					// If user is dragging the resize bars, we do not resize 
					// the buffers here because as the user continuously 
					// drags the resize bars, a stream of WM_SIZE messages are
					// sent to the window, and it would be pointless (and slow)
					// to resize for each WM_SIZE message received from dragging
					// the resize bars.  So instead, we reset after the user is 
					// done resizing the window and releases the resize bars, which 
					// sends a WM_EXITSIZEMOVE message.
				}
				else // API call such as SetWindowPos or mSwapChain->SetFullscreenState.
				{
					OnResize();
				}
			}
		}
		return 0;

	// WM_EXITSIZEMOVE is sent when the user grabs the resize bars.
	case WM_ENTERSIZEMOVE:
		mAppPaused = true;
		mResizing  = true;
		mTimer.Stop();
		return 0;

	// WM_EXITSIZEMOVE is sent when the user releases the resize bars.
	// Here we reset everything based on the new window dimensions.
	case WM_EXITSIZEMOVE:
		mAppPaused = false;
		mResizing  = false;
		mTimer.Start();
		OnResize();
		return 0;
 
	// WM_DESTROY is sent when the window is being destroyed.
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;

	// The WM_MENUCHAR message is sent when a menu is active and the user presses 
	// a key that does not correspond to any mnemonic or accelerator key. 
	case WM_MENUCHAR:
        // Don't beep when we alt-enter.
        return MAKELRESULT(0, MNC_CLOSE);

	// Catch this message so to prevent the window from becoming too small.
	case WM_GETMINMAXINFO:
		((MINMAXINFO*)lParam)->ptMinTrackSize.x = 200;
		((MINMAXINFO*)lParam)->ptMinTrackSize.y = 200; 
		return 0;

	case WM_LBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_RBUTTONDOWN:
		OnMouseDown(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	case WM_LBUTTONUP:
	case WM_MBUTTONUP:
	case WM_RBUTTONUP:
		OnMouseUp(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	case WM_MOUSEMOVE:
		OnMouseMove(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	}

	return DefWindowProc(hwnd, msg, wParam, lParam);
}


bool D3DApp::InitMainWindow()
{
	WNDCLASS wc;
	wc.style         = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc   = MainWndProc; 
	wc.cbClsExtra    = 0;
	wc.cbWndExtra    = 0;
	wc.hInstance     = mhAppInst;
	wc.hIcon         = LoadIcon(0, IDI_APPLICATION);
	wc.hCursor       = LoadCursor(0, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	wc.lpszMenuName = MAKEINTRESOURCE(IDC_MY150116_D3D11);
	wc.lpszClassName = L"D3DWndClassName";

	if( !RegisterClass(&wc) )
	{
		MessageBox(0, L"RegisterClass Failed.", 0, 0);
		return false;
	}
	DWORD dwstyle = WS_SYSMENU | WS_CAPTION;

	// Compute window rectangle dimensions based on requested client area dimensions.
	RECT R = { 0, 0, mClientWidth, mClientHeight };
	AdjustWindowRect(&R, dwstyle, false);
	int width  = R.right - R.left;
	int height = R.bottom - R.top;	

	mhMainWnd = CreateWindow(L"D3DWndClassName", mMainWndCaption.c_str(), 
		dwstyle, CW_USEDEFAULT, CW_USEDEFAULT, width, height, 0, 0, mhAppInst, 0);
	if( !mhMainWnd )
	{
		MessageBox(0, L"CreateWindow Failed.", 0, 0);
		return false;
	}

	ShowWindow(mhMainWnd, SW_SHOW);
	UpdateWindow(mhMainWnd);

	return true;
}

bool D3DApp::InitDirect3D()
{
	// Create the device and device context.

	UINT createDeviceFlags = 0;
#if defined(DEBUG) || defined(_DEBUG)  
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	D3D_FEATURE_LEVEL featureLevel;
	HRESULT hr = D3D11CreateDevice(
			0,                 // default adapter
			md3dDriverType,		// D3D_DRIVER_TYPE_HARDWARE 
			0,                 // no software device
			createDeviceFlags, 
			0, 0,              // default feature level array
			D3D11_SDK_VERSION,
			&md3dDevice,
			&featureLevel,
			&md3dImmediateContext);

	if( FAILED(hr) )
	{
		MessageBox(0, L"D3D11CreateDevice Failed.", 0, 0);
		return false;
	}

	if( featureLevel != D3D_FEATURE_LEVEL_11_0 )
	{
		MessageBox(0, L"Direct3D Feature Level 11 unsupported.", 0, 0);
		return false;
	}

	// Check 4X MSAA quality support for our back buffer format.
	// All Direct3D 11 capable devices support 4X MSAA for all render 
	// target formats, so we only need to check quality support.

	HR(md3dDevice->CheckMultisampleQualityLevels(
		DXGI_FORMAT_R8G8B8A8_UNORM, 4, &m4xMsaaQuality));
	assert( m4xMsaaQuality > 0 );

	// Fill out a DXGI_SWAP_CHAIN_DESC to describe our swap chain.

	DXGI_SWAP_CHAIN_DESC sd;
	sd.BufferDesc.Width  = mClientWidth;
	sd.BufferDesc.Height = mClientHeight;
	sd.BufferDesc.RefreshRate.Numerator = 60;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
	sd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

	// Use 4X MSAA? 
	if( mEnable4xMsaa )
	{
		sd.SampleDesc.Count   = 4;
		sd.SampleDesc.Quality = m4xMsaaQuality-1;
	}
	// No MSAA
	else
	{
		sd.SampleDesc.Count   = 1;
		sd.SampleDesc.Quality = 0;
	}

	sd.BufferUsage  = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.BufferCount  = 1;
	sd.OutputWindow = mhMainWnd;
	sd.Windowed     = true;
	sd.SwapEffect   = DXGI_SWAP_EFFECT_DISCARD;
	sd.Flags        = 0;

	// To correctly create the swap chain, we must use the IDXGIFactory that was
	// used to create the device.  If we tried to use a different IDXGIFactory instance
	// (by calling CreateDXGIFactory), we get an error: "IDXGIFactory::CreateSwapChain: 
	// This function is being called with a device from a different IDXGIFactory."

	IDXGIDevice* dxgiDevice = 0;
	HR(md3dDevice->QueryInterface(__uuidof(IDXGIDevice), (void**)&dxgiDevice));
	      
	IDXGIAdapter* dxgiAdapter = 0;
	HR(dxgiDevice->GetParent(__uuidof(IDXGIAdapter), (void**)&dxgiAdapter));

	IDXGIFactory* dxgiFactory = 0;
	HR(dxgiAdapter->GetParent(__uuidof(IDXGIFactory), (void**)&dxgiFactory));

	HR(dxgiFactory->CreateSwapChain(md3dDevice, &sd, &mSwapChain));
	
	ReleaseCOM(dxgiDevice);
	ReleaseCOM(dxgiAdapter);
	ReleaseCOM(dxgiFactory);

	// The remaining steps that need to be carried out for d3d creation
	// also need to be executed every time the window is resized.  So
	// just call the OnResize method here to avoid code duplication.
	
	OnResize();

	return true;
}

void D3DApp::CalculateFrameStats()
{
	// Code computes the average frames per second, and also the 
	// average time it takes to render one frame.  These stats 
	// are appended to the window caption bar.

	static int frameCnt = 0;
	static float timeElapsed = 0.0f;

	frameCnt++;

	// Compute averages over one second period.
	if( (mTimer.TotalTime() - timeElapsed) >= 1.0f )
	{
		float fps = (float)frameCnt; // fps = frameCnt / 1
		float mspf = 1000.0f / fps;

		std::wostringstream outs;   
		outs.precision(6);
		outs << mMainWndCaption << L"    "
			 << L"FPS: " << fps << L"    " 
			 << L"Frame Time: " << mspf << L" (ms)";
		SetWindowText(mhMainWnd, outs.str().c_str());
		
		// Reset for next average.
		frameCnt = 0;
		timeElapsed += 1.0f;
	}
}



/*

HRESULT D3D11CreateDevice(
	IDXGIAdapter *pAdapter,
	D3D_DRIVER_TYPE DriverType,
	HMODULE Software,
	UINT Flags,
	CONST D3D_FEATURE_LEVEL *pFeatureLevels,
	UINT FeatureLevels,
	UINT SDKVersion,
	ID3D11Device **ppDevice,
	D3D_FEATURE_LEVEL *pFeatureLevel,
	ID3D11DeviceContext **ppImmediateContext
	);

	1. pAdapter : 이함수로 생성할 장치를 나타내는 디스플레이 어댑터를 지정한다.
		이 매개변수에 널 값(NULL 또는 0)을 지정하면 기본 디스플레이 어댑터가 사용된다.
		이 책의 예제 프로그램들은 항상 기본 어댑터를 사용한다. 
	2. DriveType : 일반적으로는 렌더링에 3차원 그래픽 가속이 적용되게 하기 위해
		이 매개변수에 D3D_DRIVER_TYPE_HARDWARE를 지정한다. 상황에 따라서는 다음과 같은
		다른 값을 지정하기도 한다.

			(a) D3D_DRIVER_TYPE_REFERENCE : 소위 표준장치(reference device)를 생성한다.
				표준 장치는 정확성을 목표로 하는 Direct3D의 소프트웨어 구현이다. (그래서
				극도로 느리다). 표준 장치는 DirectX SDK 와 함께 설치되는 것으로, 개발자
				전용으로 만들어진 것이다.
			(b) D3D_DRIVER_TYPE_SOFTWARE : 3차원 하드웨어를 흉내 내는 소프트웨어 구동기
				를 생성한다. 개발자가 직접 구현했거나 서드파티가 제공한 소프트웨어 구동
				기가 있는 경우 선택할 수 있는 옵션이다. Direct3D가 제공하는 소프트웨어
				구동기는 아래에서 설명하는 WARP 구동기뿐이다.
			(c) D3D_DRIVER_TYPE_WARP : 고성능 Direct3D 10.1 소프트웨어 구동기를 생성한다.
				WARP는 Windows Advanced Raterization Platform 의 약자이다. 이 구동기는 
				Direct3D11을 지원하지 않으므로 이책에서는 다루지 않는다.
	3. Software : 소프트웨어 구동기를 지정한다. 이 책에서는 하드웨어를 사용해서 렌더링 하
				므로 이 매개변수에 항상 널 값을 지정한다.
	4. Flags : 추가적인 장치 생성 플래그들(OR로 결합 가능)을 지정한다. 흔히 쓰이는 플래그 
		두개를 들자면 다음과 같다.
			(a) D3D11_CREATE_DEVICE_DEBUG : 디버그 모드 빌드에서 디버그 계층을 활성화 하려면
				반드시 이 플래그를 설정해야 한다. 이 디버그 플래그를 지정하면 Direct3D는 
				VC++ 출력 창에 디버그 메시지를 보낸다.
			(b) D3D11_CREATE_DEVICE_SINGLETHREADED : Direct3D가 여러개의 스레드에서 호출되지 
				않는 다는 보장이 있을때, 이 플래그를 지정하면 성능이 향상된다. 이 플래그를 지정
				한 경우 ID3D11Device::CreateDeferredContext의 호출은 실패한다.
	5. pFeatureLevels : D3D_FEATURE_LEVEL 형식 원소들의 배열로, 원소들의 순서가 곧 기능 수준들을 
		점검하는 순서이다. 이 매개변수에 널 값을 지정하면 지원되는 최고 기능 수준이 선택된다.
		이책은 Direct3D11을 대상으로 하므로, 이 책의 예제 프레임워크는 항상 D3D_FEATURE_LEVEL_11_0이
		선택되게 한다.
	6. FeatureLevels: 배열 pFeatureLevels의 D3D_FEATURE_LEVEL 원소 개수이다.
		pFeatureLevels에 널 값을 지정했다면 이 매개변수는 0으로 지정하면 된다.
	7. SDKVersion : 항상 D3D11_SDK_VERSION을 지정한다.
	8. ppDevice : 함수가 생성한 장치를 돌려준다.
	9. pFeatureLevel : pFeatureLevels 배열에서 처음으로 지원되는 기능( 또는,
		pFeatureLevels를 널 값으로 한 경우 지원되는 가장 높은 기능 수준)을 돌려준다.
	10. ppImmediateContext : 생성된 장치 문맥을 돌려준다.


*/

/*


typedef struct DXGI_SWAP_CHAIN_DESC {
	DXGI_MODE_DESC		BufferDesc;
	DXGI_SAMPLE_DESC	SampleDesc;
	DXGI_USAGE			BufferUsage;
	UINT				BufferCount;
	HWND				OutputWindow;
	BOOL				Windowed;
	DXGI_SWAP_EFFECT	SwapEffect;
	UINT				Flags;
} DXGI_SWAP_CHAIN_DESC;

1. BufferDesc : 이것을 생성하고자 하는 후면 버퍼의 속성들을 서술하는 개별적인 구조체이다.
	현재 맥락에서 중요한 것은, 그 구조체에 버퍼의 너비와 높이, 픽셀 형식을 지정하는 
	맴버들이 있다는 것이다. 그 외 자료 멤버들과 구체적인 세부 사항은 SDK 문서화를 보기 바란다.

	typedef struct DXGI_MODE_DESC
	{
	UINT Width; // 원하는 후면 버퍼 너비
	UINT Height; // 원하는 후면 버퍼 높이
	DXGI_RATIONAL RefreshRate; // 디스플레이 모드 갱신율
	DXGI_FORMAT Format; //후면버퍼 픽셀 형식
	DXGI_MODE_SCANLINE_ORDER ScanlineOrdering; //디스플레이 스캔라인 모드
	DXGI_MODE_SCALING Scaling; //디스플레이 비례모드	
	} DXGI_MODE_DESC;

2. SampleDesc : 다중표본화를 위해 추출할 표본 개수와 품질 수준을 서술하는 구조체이다.
3. BufferUsage : 버퍼의 용도를 서술하는 구조체로, 지금 맥락에서는 후면 버퍼에 렌더링 할 것이므로
	(즉 버퍼를 렌더 대상으로 사용할 것이므로) DXGI_USAGE_RENDER_TARGET_OUTPUT을 지정한다.
4. BufferCount : 교환 사슬에서 사용할 후면 버퍼의 개수이다. 일반적으로는 후면 버퍼를 하나만
	사용하나(이중버퍼링), 필요하다면 두 개를 사용할 수도 있다(삼중 버퍼링)
5. OutputWindow : 렌더링 결과를 표시할 창의 핸들이다.
6. Windowed: 창 모드를 원하면 true를, 전체 화면 모드를 원하면 false를 지정한다.
7. SwapEffect : 교환 효과를 서술하는 구조체로, DXGI_SWP_EFFECT_DISCARD를 지정하면 디스플레이
	구동기가 가장 효율적인 방법을 선택하게 된다.
8. Flags :추가적인 플래그들이다. DXGI_CHAIN_FLAG_ALLOW_MODE_SWITCH를 지정하면, 응용프로그램이 
	전체화면 모드로 전환할 때 현재의 후면 버퍼 설정에 가장 잘 부함하는 디스플레이 모드가 자동으로
	선택된다. 이 플래그를 지정하지 않으면 응용 프로그램이 전체화면 모드로 전환할 때 그냥 현재의
	데스크톱 디스플레이 모드가 사용된다. 이 책의 예제 프레임워크에는 이 플래그를 지정하지 않는다.

*/