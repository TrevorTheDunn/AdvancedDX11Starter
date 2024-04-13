struct VertexToPixel
{
	float4 position		: SV_POSITION;
	float2 uv			: TEXCOORD0;
	float4 colorTint	: COLOR;
};

Texture2D Particle		  : register(t0);
SamplerState BasicSampler : register(s0);

float4 main(VertexToPixel input) : SV_TARGET
{
	return Particle.Sample(BasicSampler, input.uv) * input.colorTint;
}