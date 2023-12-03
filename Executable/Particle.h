#pragma once

using namespace DirectX;

class Particle
{
private:
	struct VertexType
	{
		DirectX::SimpleMath::Vector3 position;
		DirectX::SimpleMath::Vector2 texture;
		DirectX::SimpleMath::Vector3 normal;
	};


public:
	bool Initialize(ID3D11Device*, DirectX::SimpleMath::Vector3 pos);
	void Render(ID3D11DeviceContext*);
	bool GroundCollisionBasic();
	bool Update();

};

