#pragma once

#include <wrl/client.h>
#include <DirectXMath.h>
#include <d3d11.h>
#include <memory>

#include "Transform.h"
#include "Camera.h"
#include "Material.h"

struct Particle
{
	float EmitTime;
	DirectX::XMFLOAT3 StartPos;
};

class Emitter
{
public:
	Emitter(Microsoft::WRL::ComPtr<ID3D11Device> device, 
		std::shared_ptr<Material> material, int maxParticles,
		int particlesPerSecond, float maxLifetime, DirectX::XMFLOAT3 position);
	~Emitter();

	void Update(float dt, float currentTime);
	void Draw(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context, 
		std::shared_ptr<Camera> camera, float currentTime);

	int GetMaxParticles();
	void SetMaxParticles(int maxParticles);
private:
	int maxParticles;
	Particle* particles;

	int currentlyLiving;
	int firstAliveIndex;
	int firstDeadIndex;

	Microsoft::WRL::ComPtr<ID3D11Device> device;
	Microsoft::WRL::ComPtr<ID3D11Buffer> particleDataBuffer;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> particleDataSRV;
	Microsoft::WRL::ComPtr<ID3D11Buffer> indexBuffer;

	int particlesPerSecond;
	float secondsPerParticle;
	float timeSinceLastEmit;
	float maxLifetime;

	Transform transform;
	std::shared_ptr<Material> material;

	void CreateParticlesAndGPUResources();
	void CopyParticlesToGPU(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context);

	void UpdateSingleParticle(float currentTime, int index);
	void EmitParticle(float currentTime);
};