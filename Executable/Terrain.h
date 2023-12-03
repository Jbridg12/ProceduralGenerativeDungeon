#pragma once
#define WALL_HEIGHT 100.0f
#define FLOOR_HEIGHT -1.0f
#define COLLECTIBLE_COUNT 10
#define COLLECTIBLE_LEEWAY 2.0f

using namespace DirectX;

class Terrain
{
private:
	struct VertexType
	{
		DirectX::SimpleMath::Vector3 position;
		DirectX::SimpleMath::Vector2 texture;
		DirectX::SimpleMath::Vector3 normal;
	};
	struct HeightMapType
	{
		float x, y, z;
		float nx, ny, nz;
		float u, v;
	};
public:
	Terrain();
	~Terrain();

	bool Initialize(ID3D11Device*, int terrainWidth, int terrainHeight);
	void Render(ID3D11DeviceContext*);
	bool GenerateHeightMap(ID3D11Device*, DirectX::SimpleMath::Vector3);
	bool RandomHeightMap();
	bool NoiseHeightMap();
	bool SmoothHeight();
	bool RandomParticleDeposition();
	bool ParticleDepositionAtPoint(int index);
	bool Update();

	float* GetWavelength();
	float* GetAmplitude();

	int* GetPCGIterations();
	int* GetPCGThreshold();
	float* GetPCGSeedChance();

	bool GenerateDungeonHeightMap();
	bool PCGDungeonMap(DirectX::SimpleMath::Vector3);


	bool PlaceCollectibles();
	DirectX::SimpleMath::Vector3* getCollectibles();


	bool CollideWithCollectible(DirectX::SimpleMath::Vector3);
	DirectX::SimpleMath::Vector3 CollideWithWall(DirectX::SimpleMath::Vector3, DirectX::SimpleMath::Vector3);
	DirectX::SimpleMath::Vector3 CollideWithNeighborhood(DirectX::SimpleMath::Vector3, DirectX::SimpleMath::Vector3);

private:
	bool CalculateNormals();
	void Shutdown();
	void ShutdownBuffers();
	bool InitializeBuffers(ID3D11Device*);
	void RenderBuffers(ID3D11DeviceContext*);
	

private:
	bool m_terrainGeneratedToggle;
	int m_terrainWidth, m_terrainHeight;
	ID3D11Buffer * m_vertexBuffer, *m_indexBuffer;
	int m_vertexCount, m_indexCount;
	float m_frequency, m_amplitude, m_wavelength;
	HeightMapType* m_heightMap;
	ClassicNoise m_perlNoise;

	//Collectibles
	DirectX::SimpleMath::Vector3* m_collectibles;

	// PCG Dungeon Parameters
	int m_iterations;
	int m_threshold;
	int m_neightborhood;
	float m_seedChance;


	//arrays for our generated objects Made by directX
	std::vector<VertexPositionNormalTexture> preFabVertices;
	std::vector<uint16_t> preFabIndices;
};

