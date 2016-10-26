
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
	VS_OUTPUT output;

	output.Pos = mul(inPos, GridWVP);
	output.Color = inColor;

	return output;
}

float4 GridPS(GridVS_OUTPUT input) : SV_TARGET
{
	return input.Color;
}