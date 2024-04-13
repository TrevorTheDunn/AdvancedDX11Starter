#include "Emitter.h"

Emitter::Emitter(
	Microsoft::WRL::ComPtr<ID3D11Device> device,
	std::shared_ptr<Material> material,
	int maxParticles,
	int particlesPerSecond,
	float maxLifetime,
	DirectX::XMFLOAT3 position) : 
	device(device), material(material),
	maxParticles(maxParticles),
	particlesPerSecond(particlesPerSecond),
	maxLifetime(maxLifetime)
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
	if (currentlyLiving > 0)
	{
		if (firstAliveIndex < firstDeadIndex)
		{
			for (int i = firstAliveIndex; i < firstDeadIndex; i++)
				UpdateSingleParticle(currentTime, i);
		}

		else if (firstDeadIndex < firstAliveIndex)
		{
			for (int i = firstAliveIndex; i < maxParticles; i++)
				UpdateSingleParticle(currentTime, i);

			for (int i = 0; i < firstDeadIndex; i++)
				UpdateSingleParticle(currentTime, i);
		}

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
	vs->SetFloat("currentTime", currentTime);
	vs->SetFloat3("particleColor", DirectX::XMFLOAT3(1, 0, 0));
	vs->CopyAllBufferData();

	vs->SetShaderResourceView("ParticleData", particleDataSRV);

	context->DrawIndexed(currentlyLiving * 6, 0, 0);
}

int Emitter::GetMaxParticles() { return maxParticles; }
void Emitter::SetMaxParticles(int maxParticles) { this->maxParticles = maxParticles; }

void Emitter::CreateParticlesAndGPUResources()
{
	particles = new Particle[maxParticles];

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
	D3D11_SUBRESOURCE_DATA indexData = {};
	indexData.pSysMem = indices;

	D3D11_BUFFER_DESC ibDesc = {};
	ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	ibDesc.CPUAccessFlags = 0;
	ibDesc.Usage = D3D11_USAGE_DEFAULT;
	ibDesc.ByteWidth = sizeof(unsigned int) * maxParticles * 6;
	device->CreateBuffer(&ibDesc, &indexData, indexBuffer.GetAddressOf());
	delete[] indices;

	D3D11_BUFFER_DESC desc = {};
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	desc.Usage = D3D11_USAGE_DYNAMIC;
	desc.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	desc.StructureByteStride = sizeof(Particle);
	desc.ByteWidth = sizeof(Particle) * maxParticles;
	device->CreateBuffer(&desc, 0, particleDataBuffer.GetAddressOf());

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

	firstDeadIndex++;
	firstDeadIndex %= maxParticles;

	currentlyLiving++;
}
