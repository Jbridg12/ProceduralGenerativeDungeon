// From https://www.rastertek.com/dx10tut39.html
// Tutorial on Particle Systems

#pragma once
#include "Particle.h"
#include "Terrain.h"
using namespace DirectX;

class ParticleSystemClass
{
private:
	struct ParticleType
	{
		float					positionX; 
		float					positionY;
		float					positionZ;

		float					red;
		float					green;
		float					blue;

		float					velocityX;
		float					velocityY;
		float					velocityZ;
		float					colorVelocityR;
		float					colorVelocityG;
		float					colorVelocityB;
		bool					active;
	};
	struct VertexType
	{
		DirectX::SimpleMath::Vector3 position;
		DirectX::SimpleMath::Vector2 texture;
		DirectX::SimpleMath::Vector4 color;
	};

public:
	enum SystemType
	{
		Fire,
		Space,
		Snow
	};

	ParticleSystemClass();
	ParticleSystemClass(const ParticleSystemClass&);
	~ParticleSystemClass();
	bool Initialize(ID3D11Device*, const wchar_t*, SystemType, Terrain*);
	void Shutdown();
	bool Frame(float, ID3D11DeviceContext*);
	void Render(ID3D11DeviceContext*);

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> GetTexture();
	int GetIndexCount();
	bool CheckInitialized();
	void EmitParticlesFromMouse(float, DirectX::SimpleMath::Vector3);

private:
	bool LoadTexture(ID3D11Device*, const wchar_t*);

	bool InitializeFireSystem();
	bool InitializeSnowSystem();
	bool InitializeSpaceSystem();

	void ShutdownParticleSystem();

	bool InitializeBuffers(ID3D11Device*);
	void ShutdownBuffers();

	void EmitParticles(float);
	void UpdateParticles(float);
	void KillParticles();

	bool UpdateBuffers(ID3D11DeviceContext*);

	void RenderBuffers(ID3D11DeviceContext*);
	bool GroundCollisionBasic(Terrain*);


private:
	// Parameters to initialize each particle
	float												m_particleDeviationX;
	float												m_particleDeviationY;
	float												m_particleDeviationZ;
	float												m_particleVelocityX;
	float												m_particleVelocityY;
	float												m_particleVelocityZ;
	float												m_particleVelocityVariation;

	float												m_particleColorChangeR;
	float												m_particleColorChangeG;
	float												m_particleColorChangeB;
	float												m_particleColorVelocityVariation;

	float												m_startX;
	float												m_startY;
	float												m_startZ;

	float												m_particleSize; 
	float												m_particlesPerSecond;

	int													m_maxParticles;

	int													m_currentParticleCount;
	float												m_accumulatedTime;

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>	m_Texture;

	ParticleType*										m_particleList;
	//Particle*											m_particleList;

	int													m_vertexCount;
	int													m_indexCount;
	VertexType*											m_vertices;
	ID3D11Buffer*										m_vertexBuffer;
	ID3D11Buffer*										m_indexBuffer;
	Terrain*											m_terrain;

	bool												m_init;

};