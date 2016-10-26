//***************************************************************************************
// Effects.cpp by Frank Luna (C) 2011 All Rights Reserved.
//***************************************************************************************

#include "Effects.h"

#pragma region Effect
Effect::Effect(ID3D11Device* device, const std::wstring& filename)
	: mFX(0)
{
	std::ifstream fin(filename, std::ios::binary);

	fin.seekg(0, std::ios_base::end);
	int size = (int)fin.tellg();
	fin.seekg(0, std::ios_base::beg);
	std::vector<char> compiledShader(size);

	fin.read(&compiledShader[0], size);
	fin.close();
	
	HR(D3DX11CreateEffectFromMemory(&compiledShader[0], size, 
		0, device, &mFX));
}

Effect::~Effect()
{
	ReleaseCOM(mFX);
}
#pragma endregion

#pragma region BasicEffect
BasicEffect::BasicEffect(ID3D11Device* device, const std::wstring& filename)
	: Effect(device, filename)
{
	Light1Tech        = mFX->GetTechniqueByName("Light1");
	Light2Tech        = mFX->GetTechniqueByName("Light2");
	Light3Tech        = mFX->GetTechniqueByName("Light3");
	GridTech		  = mFX->GetTechniqueByName("Grid1");
	WorldViewProj     = mFX->GetVariableByName("gWorldViewProj")->AsMatrix();
	World             = mFX->GetVariableByName("gWorld")->AsMatrix();
	WorldInvTranspose = mFX->GetVariableByName("gWorldInvTranspose")->AsMatrix();
	ShadowTransform   = mFX->GetVariableByName("gShadowTransform")->AsMatrix();
	EyePosW           = mFX->GetVariableByName("gEyePosW")->AsVector();
	DirLights         = mFX->GetVariableByName("gDirLights");
	Mat               = mFX->GetVariableByName("gMaterial");
	GridWorld		  = mFX->GetVariableByName("GridWVP")->AsMatrix();
	hasNormal		  = mFX->GetVariableByName("ghasNormal");
	hasTexture		  = mFX->GetVariableByName("ghasTexture");
	DiffuseMap		  = mFX->GetVariableByName("ObjTexture")->AsShaderResource();
	NormalMap		  = mFX->GetVariableByName("ObjNormMap")->AsShaderResource();
	ShadowMap		  = mFX->GetVariableByName("gShadowMap")->AsShaderResource();
	TexTransform      = mFX->GetVariableByName("gTexTransform")->AsMatrix();
}

BasicEffect::~BasicEffect()
{
}
#pragma endregion

#pragma region BuildShadowMapEffect
BuildShadowMapEffect::BuildShadowMapEffect(ID3D11Device* device, const std::wstring& filename)
: Effect(device, filename)
{
	BuildShadowMapTech = mFX->GetTechniqueByName("BuildShadowMapTech");
	BuildShadowMapAlphaClipTech = mFX->GetTechniqueByName("BuildShadowMapAlphaClipTech");

	TessBuildShadowMapTech = mFX->GetTechniqueByName("TessBuildShadowMapTech");
	TessBuildShadowMapAlphaClipTech = mFX->GetTechniqueByName("TessBuildShadowMapAlphaClipTech");

	ViewProj = mFX->GetVariableByName("gViewProj")->AsMatrix();
	WorldViewProj = mFX->GetVariableByName("gWorldViewProj")->AsMatrix();
	World = mFX->GetVariableByName("gWorld")->AsMatrix();
	WorldInvTranspose = mFX->GetVariableByName("gWorldInvTranspose")->AsMatrix();
	TexTransform = mFX->GetVariableByName("gTexTransform")->AsMatrix();
	EyePosW = mFX->GetVariableByName("gEyePosW")->AsVector();
	HeightScale = mFX->GetVariableByName("gHeightScale")->AsScalar();
	MaxTessDistance = mFX->GetVariableByName("gMaxTessDistance")->AsScalar();
	MinTessDistance = mFX->GetVariableByName("gMinTessDistance")->AsScalar();
	MinTessFactor = mFX->GetVariableByName("gMinTessFactor")->AsScalar();
	MaxTessFactor = mFX->GetVariableByName("gMaxTessFactor")->AsScalar();
	DiffuseMap = mFX->GetVariableByName("gDiffuseMap")->AsShaderResource();
	NormalMap = mFX->GetVariableByName("gNormalMap")->AsShaderResource();
}

BuildShadowMapEffect::~BuildShadowMapEffect()
{
}
#pragma endregion

#pragma region Effects

BasicEffect* Effects::BasicFX = 0;
BuildShadowMapEffect*  Effects::BuildShadowMapFX = 0;

void Effects::InitAll(ID3D11Device* device)
{
	BasicFX = new BasicEffect(device, L"FX/Basic.fxo");
	BuildShadowMapFX = new BuildShadowMapEffect(device, L"FX/BuildShadowMap.fxo");
}

void Effects::DestroyAll()
{
	SafeDelete(BasicFX);
	SafeDelete(BuildShadowMapFX);
}
#pragma endregion