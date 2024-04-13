#include "Emitter.h"

#define RandomRange(min, max) ((float)rand() / RAND_MAX * (max - min) + min)

Emitter::Emitter(
	Microsoft::WRL::ComPtr<ID3D11Device> device,
	std::shared_ptr<Material> material,
	int maxParticles,
	int particlesPerSecond,
	float maxLifetime,
	float startSize,
	float endSize,
	DirectX::XMFLOAT4 startColor,
	DirectX::XMFLOAT4 endColor,
	DirectX::XMFLOAT3 position,
	DirectX::XMFLOAT3 posRandRange,
	DirectX::XMFLOAT2 rotationStart,
	DirectX::XMFLOAT2 rotationEnd,
	DirectX::XMFLOAT3 startVelocity,
	DirectX::XMFLOAT3 velRandRange,
	DirectX::XMFLOAT3 acceleration) : 
	device(device), material(material),
	maxParticles(maxParticles),
	particlesPerSecond(particlesPerSecond),
	maxLifetime(maxLifetime),
	startSize(startSize),
	endSize(endSize),
	startColor(startColor),
	endColor(endColor),
	startVelocity(startVelocity),
	acceleration(acceleration),
	posRandRange(posRandRange),
	velRandRange(velRandRange),
	rotationStart(rotationStart),
	rotationEnd(rotationEnd)
{
	secondsPerParticle = 1.0f / particlesPerSecond;

	timeSinceLastEmit = 0.0f;
	currentlyLiving = 0;
	firstAliveIndex = 0;
	firstDeadIndex = 0;

	this->transform.SetPosition(position);

	CreateParticlesAndGPUResources();
}

Emitter::~Emitter() { delete[] particles; }

void Emitter::Update(float dt, float currentTime)
{
	// Checks if there's even particles to update
	if (currentlyLiving > 0)
	{
		// Alive particles are all before dead particles
		if (firstAliveIndex < firstDeadIndex)
		{
			for (int i = firstAliveIndex; i < firstDeadIndex; i++)
				UpdateSingleParticle(currentTime, i);
		}
		// Alive particles are broken up due to wrap
		else if (firstDeadIndex < firstAliveIndex)
		{
			for (int i = firstAliveIndex; i < maxParticles; i++)
				UpdateSingleParticle(currentTime, i);

			for (int i = 0; i < firstDeadIndex; i++)
				UpdateSingleParticle(currentTime, i);
		}
		// Particles are all alive
		else
		{
			for (int i = 0; i < maxParticles; i++)
				UpdateSingleParticle(currentTime, i);
		}
	}

	timeSinceLastEmit += dt;
	while (timeSinceLastEmit > secondsPerParticle)
	{
		EmitParticle(currentTime);
		timeSinceLastEmit -= secondsPerParticle;
	}
}

void Emitter::Draw(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context, std::shared_ptr<Camera> camera, float currentTime)
{
	CopyParticlesToGPU(context);

	UINT stride = 0;
	UINT offset = 0;
	ID3D11Buffer* null = 0;
	context->IASetVertexBuffers(0, 1, &null, &stride, &offset);
	context->IASetIndexBuffer(indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);

	material->PrepareMaterial(&transform, camera);

	std::shared_ptr<SimpleVertexShader> vs = material->GetVertexShader();
	vs->SetMatrix4x4("view", camera->GetView());
	vs->SetMatrix4x4("projection", camera->GetProjection());
	vs->SetFloat4("startColor", startColor);
	vs->SetFloat4("endColor", endColor);
	vs->SetFloat("currentTime", currentTime);
	vs->SetFloat3("acceleration", acceleration);
	vs->SetFloat("startSize", startSize);
	vs->SetFloat("endSize", endSize);
	vs->SetFloat("lifetime", maxLifetime);
	vs->CopyAllBufferData();

	vs->SetShaderResourceView("ParticleData", particleDataSRV);

	context->DrawIndexed(currentlyLiving * 6, 0, 0);
}

int Emitter::GetMaxParticles() { return maxParticles; }
void Emitter::SetMaxParticles(int maxParticles) { this->maxParticles = maxParticles; }

void Emitter::CreateParticlesAndGPUResources()
{
	particles = new Particle[maxParticles];

	// Create array of indices
	int numIndices = maxParticles * 6;
	unsigned int* indices = new unsigned int[numIndices];
	int indexCount = 0;
	for (int i = 0; i < maxParticles * 4; i += 4)
	{
		indices[indexCount++] = i;
		indices[indexCount++] = i + 1;
		indices[indexCount++] = i + 2;
		indices[indexCount++] = i;
		indices[indexCount++] = i + 2;
		indices[indexCount++] = i + 3;
	}

	// Use array of indices to create an index buffer
	D3D11_SUBRESOURCE_DATA indexData = {};
	indexData.pSysMem = indices;

	D3D11_BUFFER_DESC ibDesc = {};
	ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	ibDesc.CPUAccessFlags = 0;
	ibDesc.Usage = D3D11_USAGE_DEFAULT;
	ibDesc.ByteWidth = sizeof(unsigned int) * maxParticles * 6;
	device->CreateBuffer(&ibDesc, &indexData, indexBuffer.GetAddressOf());
	delete[] indices; // delete indices as we are finished with them

	// Buffer for particle data on GPU
	D3D11_BUFFER_DESC desc = {};
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	desc.Usage = D3D11_USAGE_DYNAMIC;
	desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	desc.StructureByteStride = sizeof(Particle);
	desc.ByteWidth = sizeof(Particle) * maxParticles;
	device->CreateBuffer(&desc, 0, particleDataBuffer.GetAddressOf());

	// Create structured buffer
	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	srvDesc.Format = DXGI_FORMAT_UNKNOWN;
	srvDesc.Buffer.FirstElement = 0;
	srvDesc.Buffer.NumElements = maxParticles;
	device->CreateShaderResourceView(particleDataBuffer.Get(), &srvDesc, particleDataSRV.GetAddressOf());
}

void Emitter::CopyParticlesToGPU(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context)
{
	D3D11_MAPPED_SUBRESOURCE mapped{};
	context->Map(particleDataBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);

	// How are living particles arranged in the buffer?
	if (firstAliveIndex < firstDeadIndex)
	{
		// Only copy from FirstAlive -> FirstDead
		memcpy(
			mapped.pData,						 // Destination = start of particle buffer
			particles + firstAliveIndex,		 // Source = particle array, offset to first living particle
			sizeof(Particle) * currentlyLiving); // Amount = number of particles (measured in BYTES)
	}
	else
	{
		// Copy from 0 -> FirstDead
		memcpy(	
			mapped.pData,						 // Destination = start of particle buffer
			particles,							 // Source = start of particle array
			sizeof(Particle) * currentlyLiving); // Amount = particles up to first dead (measured in BYTES)

		// ALSO copy from FirstAlive -> End
		memcpy(
			(void*)((Particle*)mapped.pData + firstDeadIndex),	  // Destination = particle buffer, AFTER the data we copied in previous memcpy()
			particles + firstAliveIndex,						  // Source = particle array, offset to first living particle
			sizeof(Particle) * (maxParticles - firstAliveIndex)); // Amount = number of living particles at end of array (measured in BYTES)
	}

	context->Unmap(particleDataBuffer.Get(), 0);
}

void Emitter::UpdateSingleParticle(float currentTime, int index)
{
	float age = currentTime - particles[index].EmitTime;

	if (age >= maxLifetime)
	{
		firstAliveIndex++;
		firstAliveIndex %= maxParticles;
		currentlyLiving--;
	}
}

void Emitter::EmitParticle(float currentTime)
{
	if (currentlyLiving == maxParticles)
		return;

	particles[firstDeadIndex].EmitTime = currentTime;

	particles[firstDeadIndex].StartPos = transform.GetPosition();
	particles[firstDeadIndex].StartPos.x += posRandRange.x * RandomRange(-1.0f, 1.0f);
	particles[firstDeadIndex].StartPos.y += posRandRange.y * RandomRange(-1.0f, 1.0f);
	particles[firstDeadIndex].StartPos.z += posRandRange.z * RandomRange(-1.0f, 1.0f);

	particles[firstDeadIndex].StartVel = startVelocity;
	particles[firstDeadIndex].StartVel.x += velRandRange.x * RandomRange(-1.0f, 1.0f);
	particles[firstDeadIndex].StartVel.y += velRandRange.y * RandomRange(-1.0f, 1.0f);
	particles[firstDeadIndex].StartVel.z += velRandRange.z * RandomRange(-1.0f, 1.0f);

	particles[firstDeadIndex].StartRotation = RandomRange(rotationStart.x, rotationStart.y);
	particles[firstDeadIndex].EndRotation = RandomRange(rotationEnd.x, rotationEnd.y);

	firstDeadIndex++;
	firstDeadIndex %= maxParticles;

	currentlyLiving++;
}
