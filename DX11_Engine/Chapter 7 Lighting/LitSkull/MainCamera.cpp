#include "StructSetting.h"
#include "MainCamera.h"


MainCamera::MainCamera()
{
	x = 0.0f;
	y = 5.0f;
	z = 0.0f;

	XMStoreFloat4(&camForward, XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f));
	XMStoreFloat4(&camRight, XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f));
}


MainCamera::~MainCamera()
{
}
void MainCamera::Setup(const int& width, const int& height)
{
	Width = width;
	Height = height;
}
void MainCamera::Init()
{
	//Camera information
	XMVECTOR camposition = XMVectorSet(x, y, z, 0.0f);
	XMStoreFloat4(&camPosition, camposition);
	XMVECTOR camtarget = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
	XMStoreFloat4(&camTarget, camtarget);
	XMVECTOR camup = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	XMStoreFloat4(&camUp, camup);

	//Set the View matrix
	XMMATRIX camview = XMMatrixLookAtLH(camposition, camtarget, camup);
	XMStoreFloat4x4(&camView, camview);

	//Set the Projection matrix
	XMMATRIX camprojection = XMMatrixPerspectiveFovLH(0.4f*3.14f, Width / Height, 1.0f, 1000.0f);
	XMStoreFloat4x4(&camProjection, camprojection);
}
void MainCamera::Update()
{
	if (!CharCamera)
	{
		XMVECTOR DefaultForward = XMVectorSet(0.0f, 0.0f, 1.0f, 0.0f);
		XMVECTOR DefaultRight = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);

		XMVECTOR campos = XMLoadFloat4(&camPosition);

		XMMATRIX camRotationMatrix = XMMatrixRotationRollPitchYaw(CamPitch, CamYaw, 0);
		DefaultForward = XMVector3TransformNormal(DefaultForward, camRotationMatrix);
		DefaultRight = XMVector3TransformNormal(DefaultRight, camRotationMatrix);
		XMVECTOR camtarget = XMVector3TransformCoord(DefaultForward, camRotationMatrix);

		camtarget = XMVector3Normalize(camtarget);

		XMMATRIX RotateYTempMatrix;
		RotateYTempMatrix = XMMatrixRotationY(CamYaw);

		//walk
		XMStoreFloat4(&camRight, XMVector3TransformCoord(DefaultRight, RotateYTempMatrix));
		XMStoreFloat4(&camUp, XMVector3TransformCoord(XMLoadFloat4(&camUp), RotateYTempMatrix));
		XMStoreFloat4(&camForward, XMVector3TransformCoord(DefaultForward, RotateYTempMatrix));

		campos += moveLeftRight*XMLoadFloat4(&camRight);
		campos += moveBackForward*XMLoadFloat4(&camForward);

		moveLeftRight = 0.0f;
		moveBackForward = 0.0f;

		camtarget = campos + camtarget;

		XMStoreFloat4x4(&camView, XMMatrixLookAtLH(campos, camtarget, XMLoadFloat4(&camUp)));

		XMStoreFloat4(&camPosition, campos);
		XMStoreFloat4(&camTarget, camtarget);
	}
	else
	{
		XMVECTOR campos = XMLoadFloat4(&camPosition);
		XMVECTOR chartarget = XMLoadFloat4(&charTarget);

		XMStoreFloat4x4(&camView, XMMatrixLookAtLH(campos, chartarget, XMLoadFloat4(&camUp)));
	}
}
void MainCamera::SetTargetPosition(const XMFLOAT3& pos, const XMFLOAT4& campos)
{
	charTarget = XMFLOAT4{ pos.x, pos.y, pos.z, 1.0f };
	camPosition = campos;
}