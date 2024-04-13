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

	DirectX::XMFLOAT3 StartVel;
	float StartRotation;

	float EndRotation;
	DirectX::XMFLOAT3 padding;
};

class Emitter
{
public:
	Emitter(
		Microsoft::WRL::ComPtr<ID3D11Device> device,
		std::shared_ptr<Material> material,
		int maxParticles,
		int particlesPerSecond,
		float maxLifetime,
		float startSize = 1.0f,
		float endSize = 2.0f,
		DirectX::XMFLOAT4 startColor = DirectX::XMFLOAT4(1, 1, 1, 1),
		DirectX::XMFLOAT4 endColor = DirectX::XMFLOAT4(1, 1, 1, 1),
		DirectX::XMFLOAT3 position = DirectX::XMFLOAT3(0, 1, 0),
		DirectX::XMFLOAT3 posRandRange = DirectX::XMFLOAT3(0, 0, 0),
		DirectX::XMFLOAT2 rotationStart = DirectX::XMFLOAT2(0, 0),
		DirectX::XMFLOAT2 rotationEnd = DirectX::XMFLOAT2(0, 0),
		DirectX::XMFLOAT3 startVelocity = DirectX::XMFLOAT3(0, 1, 0),
		DirectX::XMFLOAT3 velRandRange = DirectX::XMFLOAT3(0, 0, 0),
		DirectX::XMFLOAT3 acceleration = DirectX::XMFLOAT3(0, 0, 0));
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

	float startSize;
	float endSize;
	DirectX::XMFLOAT4 startColor;
	DirectX::XMFLOAT4 endColor;
	DirectX::XMFLOAT3 startVelocity;
	DirectX::XMFLOAT3 acceleration;

	DirectX::XMFLOAT3 posRandRange;
	DirectX::XMFLOAT3 velRandRange;
	DirectX::XMFLOAT2 rotationStart;
	DirectX::XMFLOAT2 rotationEnd;

	Transform transform;
	std::shared_ptr<Material> material;

	void CreateParticlesAndGPUResources();
	void CopyParticlesToGPU(Microsoft::WRL::ComPtr<ID3D11DeviceContext> context);

	void UpdateSingleParticle(float currentTime, int index);
	void EmitParticle(float currentTime);
};