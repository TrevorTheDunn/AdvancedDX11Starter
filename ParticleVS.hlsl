struct Particle
{
	float EmitTime;
	float3 StartPos;
};

cbuffer externalData : register(b0)
{
	matrix view;
	matrix projection;
	float currentTime;
	float3 particleColor;
	//Particle particles[MAX_PARTICLES];
}

StructuredBuffer<Particle> ParticleData : register(t0);

struct VertexToPixel
{
	float4 position		:	SV_POSITION;
	float2 uv			:	TEXCOORD0;
	float4 colorTint	:	COLOR;
};

VertexToPixel main(uint id : SV_VertexID)
{
	// Set up output
	VertexToPixel output;

	// Get id info
	uint particleID = id / 4; // Every group of 4 verts are ONE particle! (int division)
	uint cornerID = id % 4; // 0,1,2,3 = which corner of the particle's "quad"

	// Grab one particle
	Particle p = ParticleData.Load(particleID); // Each vertex gets associated particle!
	
	float age = currentTime - p.EmitTime;

	float3 pos = p.StartPos + age * float3(0, 1, 0);

	// Offsets for the 4 corners of a quad - we'll only use one for each
	// vertex, but which one depends on the cornerID
	float2 offsets[4];
	offsets[0] = float2(-1.0f, +1.0f); // Top Left
	offsets[1] = float2(+1.0f, +1.0f); // Top Right
	offsets[2] = float2(+1.0f, -1.0f); // Bottom Right
	offsets[3] = float2(-1.0f, -1.0f); // Bottom Left

	// Billboarding!
	// Offset the position based on the camera's right and up vectors
	pos += float3(view._11, view._12, view._13) * offsets[cornerID].x; // RIGHT
	pos += float3(view._21, view._22, view._23) * offsets[cornerID].y; // UP

	// Finally, calculate output position here using View and Projection matrices
	matrix viewProj = mul(projection, view);
	output.position = mul(viewProj, float4(pos, 1.0f));

	float2 uvs[4];
	uvs[0] = float2(0, 0); // Top Left
	uvs[1] = float2(1, 0); // Top Right
	uvs[2] = float2(1, 1); // Bottom Right
	uvs[3] = float2(0, 1); // Bottom Left
	output.uv = uvs[cornerID];

	output.colorTint = float4(particleColor, 1);

	return output;
}