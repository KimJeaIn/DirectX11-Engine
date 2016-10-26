//=============================================================================
// Basic.fx by Frank Luna (C) 2011 All Rights Reserved.
//
// Basic effect that currently supports transformations, lighting, and texturing.
//=============================================================================

#include "LightHelper.fx"
 
cbuffer cbPerFrame
{
	DirectionalLight gDirLights[3];
	float3 gEyePosW;

	float  gFogStart;
	float  gFogRange;
	float4 gFogColor;
};

cbuffer cbPerObject
{
	float4x4 gWorld;
	float4x4 gWorldInvTranspose;
	float4x4 gWorldViewProj;
	float4x4 gTexTransform;
	float4x4 gShadowTransform;
	Material gMaterial;
	bool ghasTexture;
	bool ghasNormal;
}; 

SamplerComparisonState samShadow
{
	Filter = COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
	AddressU = BORDER;
	AddressV = BORDER;
	AddressW = BORDER;
	BorderColor = float4(0.0f, 0.0f, 0.0f, 0.0f);

	ComparisonFunc = LESS;
};

// Nonnumeric values cannot be added to a cbuffer.
Texture2D ObjTexture;
Texture2D ObjNormMap;
Texture2D gShadowMap;
SamplerState ObjSamplerState;
TextureCube SkyMap;

SamplerState samAnisotropic
{
	Filter = ANISOTROPIC;
	MaxAnisotropy = 4;

	AddressU = WRAP;
	AddressV = WRAP;
};

struct VertexIn
{
	float4 inPos : POSITION;
	float2 TexCoord : TEXCOORD;
	float3 normal : NORMAL;
	float3 tangent : TANGENT;
};

struct VertexOut
{
	float4 Pos : SV_POSITION;
	float4 worldPos : POSITION;
	float2 TexCoord : TEXCOORD0;
	float3 normal : NORMAL;
	float3 tangent : TANGENT;
	float4 ShadowPosH : TEXCOORD1;
};

cbuffer GridcbPerObject
{
	float4x4 GridWVP;
};

struct GridVS_OUTPUT
{
	float4 Pos : SV_POSITION;
	float4 Color : COLOR;
};

GridVS_OUTPUT GridVS(float4 inPos : POSITION, float4 inColor : COLOR)
{
	GridVS_OUTPUT output;

	output.Pos = mul(inPos, GridWVP);
	output.Color = inColor;

	return output;
}

float4 GridPS(GridVS_OUTPUT input) : SV_TARGET
{
	return input.Color;
}

VertexOut VS(VertexIn vin)
{
	/*VertexOut vout;
	
	// Transform to world space space.
	vout.PosW    = mul(float4(vin.PosL, 1.0f), gWorld).xyz;
	vout.NormalW = mul(vin.NormalL, (float3x3)gWorldInvTranspose);
		
	// Transform to homogeneous clip space.
	vout.PosH = mul(float4(vin.PosL, 1.0f), gWorldViewProj);
	
	return vout;*/

	VertexOut output;

	output.Pos = mul(vin.inPos, gWorldViewProj);
	output.worldPos = mul(vin.inPos, gWorld);

	output.normal = mul(vin.normal, (float3x3)gWorldInvTranspose);

	output.tangent = mul(vin.tangent, gWorld);

	output.TexCoord = mul(float4(vin.TexCoord, 0.0f, 1.0f), gTexTransform).xy;

	// Generate projective tex-coords to project shadow map onto scene.
	output.ShadowPosH = mul(vin.inPos, gShadowTransform);

	return output;
}
 
float4 PS(VertexOut pin, uniform int gLightCount) : SV_Target
{
	// Interpolating normal can unnormalize it, so normalize it.
	pin.normal = normalize(pin.normal);

	// The toEye vector is used in lighting.
	float3 toEye = gEyePosW - pin.worldPos;

	// Cache the distance to the eye from this surface point.
	float distToEye = length(toEye); 

	// Normalize.
	toEye /= distToEye;
	
	//
	// Lighting.
	//

	float4 texColor = float4(1, 1, 1, 1);

	if (ghasTexture)
		texColor = ObjTexture.Sample(ObjSamplerState, pin.TexCoord);

	float4 litColor = texColor;
	if (gLightCount > 0)
	{
		// Start with a sum of zero. 
		float4 ambient = float4(0.0f, 0.0f, 0.0f, 0.0f);
		float4 diffuse = float4(0.0f, 0.0f, 0.0f, 0.0f);
		float4 spec = float4(0.0f, 0.0f, 0.0f, 0.0f);

		if (ghasNormal)
		{
			//Load normal from normal map
			float4 normalMap = ObjNormMap.Sample(ObjSamplerState, pin.TexCoord);

			//Change normal map range from [0, 1] to [-1, 1]
			normalMap = (2.0f*normalMap) - 1.0f;

			//Make sure tangent is completely orthogonal to normal
			pin.tangent = normalize(pin.tangent - dot(pin.tangent, pin.normal)*pin.normal);

			//Create the biTangent
			float3 biTangent = cross(pin.normal, pin.tangent);

			//Create the "Texture Space"
			float3x3 texSpace = float3x3(pin.tangent, biTangent, pin.normal);

			//Convert normal from normal map to texture space and store in input.normal
			pin.normal = normalize(mul(normalMap, texSpace));

		}

		// Only the first light casts a shadow.
		float3 shadow = float3(1.0f, 1.0f, 1.0f);
			shadow[0] = CalcShadowFactor(samShadow, gShadowMap, pin.ShadowPosH);

		// Sum the light contribution from each light source.  
		[unroll]
		for (int i = 0; i < gLightCount; ++i)
		{
			float4 A, D, S;
			ComputeDirectionalLight(gMaterial, gDirLights[i], pin.normal, toEye,
				A, D, S);

			ambient += A;
			diffuse += shadow[i] * D;
			spec += shadow[i] * S;
		}

		litColor = texColor*(ambient + diffuse) + spec;
	}

	// Common to take alpha from diffuse material.
	litColor.a = gMaterial.Diffuse.a * texColor.a;

    return litColor;
}

technique11 Light1
{
    pass P0
    {
        SetVertexShader( CompileShader( vs_5_0, VS() ) );
		SetGeometryShader( NULL );
        SetPixelShader( CompileShader( ps_5_0, PS(1) ) );
    }
}

technique11 Light2
{
    pass P0
    {
        SetVertexShader( CompileShader( vs_5_0, VS() ) );
		SetGeometryShader( NULL );
        SetPixelShader( CompileShader( ps_5_0, PS(2) ) );
    }
}

technique11 Light3
{
    pass P0
    {
        SetVertexShader( CompileShader( vs_5_0, VS() ) );
		SetGeometryShader( NULL );
        SetPixelShader( CompileShader( ps_5_0, PS(3) ) );
    }
}

technique11 Grid1
{
	pass P0
	{
		SetVertexShader(CompileShader(vs_5_0, GridVS()));
		SetGeometryShader(NULL);
		SetPixelShader(CompileShader(ps_5_0, GridPS()));
	}
}