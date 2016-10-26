#pragma once

class MainCamera
{
public:
	MainCamera();
	~MainCamera();

	void Setup(const int& width, const int& height);
	void Init();
	void Update();

	XMFLOAT4X4 GetCamView() { return camView; }
	XMFLOAT4X4 GetCamProjection() { return camProjection; }
	XMVECTOR GetCamPosition() { return XMLoadFloat4(&camPosition); }
	XMVECTOR GetCamTarget() { return XMLoadFloat4(&camTarget); }

	void SetYawPitch(const float& yaw, const float& pitch) { CamYaw = yaw; CamPitch = pitch; }

	float GetYaw(){ return CamYaw; }
	float GetPitch(){ return CamPitch; }

	void SetPos(const float& lr, const float& fb) { moveLeftRight = lr; moveBackForward = fb; }

	float GetLR(){ return moveLeftRight; }
	float GetFB(){ return moveBackForward; }

	void SetTargetPosition(const XMFLOAT3& pos, const XMFLOAT4& campos);
	void SetbCharCamera(bool bchar) { CharCamera = bchar; }

private:

	XMFLOAT4X4 camView;
	XMFLOAT4X4 camProjection;

	XMFLOAT4 camPosition;
	XMFLOAT4 camTarget;
	XMFLOAT4 camUp;
	XMFLOAT4 charTarget;

	XMFLOAT4 SavecamPosition;
	XMFLOAT4 SavecamTarget;

	float x;
	float y;
	float z;

	int Width;
	int Height;

	float CamYaw = 0.0f;
	float CamPitch = 0.0f;

	XMFLOAT4 camForward;
	XMFLOAT4 camRight;

	float moveLeftRight = 0.0f;
	float moveBackForward = 0.0f;

	bool CharCamera = false;
};

