#include "pch.h"
#include "Terrain.h"


Terrain::Terrain()
{
	m_terrainGeneratedToggle = false;
}


Terrain::~Terrain()
{
}

bool Terrain::Initialize(ID3D11Device* device, int terrainWidth, int terrainHeight)
{
	int index;
	float height = 0.0;
	bool result;

	// Save the dimensions of the terrain.
	m_terrainWidth = terrainWidth;
	m_terrainHeight = terrainHeight;

	m_frequency = m_terrainWidth / 20;
	m_amplitude = 3.0;
	m_wavelength = 1;

	//Initialize PCG parameters
	m_threshold = 5;
	m_seedChance = 0.4;
	m_iterations = 5;

	//Init Collectibels
	
	m_collectibles = new DirectX::SimpleMath::Vector3[COLLECTIBLE_COUNT];

	// Create the structure to hold the terrain data.
	m_heightMap = new HeightMapType[m_terrainWidth * m_terrainHeight];
	if (!m_heightMap)
	{
		return false;
	}

	//this is how we calculate the texture coordinates first calculate the step size there will be between vertices. 
	float textureCoordinatesStep = 5.0f / m_terrainWidth;  //tile 5 times across the terrain. 
	// Initialise the data in the height map (flat).
	
	for (int j = 0; j<m_terrainHeight; j++)
	{
		for (int i = 0; i<m_terrainWidth; i++)
		{
			index = (m_terrainHeight * j) + i;

			m_heightMap[index].x = (float)i;
			m_heightMap[index].y = (float)height;
			m_heightMap[index].z = (float)j;

			//and use this step to calculate the texture coordinates for this point on the terrain.
			m_heightMap[index].u = (float)i * textureCoordinatesStep;
			m_heightMap[index].v = (float)j * textureCoordinatesStep;

		}
	}
	

	// Randomly Initialize
	/*result = RandomHeightMap();
	if (!result)
	{
		return false;
	}*/


	//Start in a flat world
	result = GenerateDungeonHeightMap();
	if (!result)
	{
		return false;
	}

	/*result = PCGDungeonMap(DirectX::SimpleMath::Vector3(20.0f, 0.0f, 20.0f));
	if (!result)
	{
		return false;
	}*/

	// Place collectibles so they aren't automatically collected
	result = PlaceCollectibles();
	if (!result)
	{
		return false;
	}

	//even though we are generating a flat terrain, we still need to normalise it. 
	// Calculate the normals for the terrain data.
	result = CalculateNormals();
	if (!result)
	{
		return false;
	}

	// Initialize the vertex and index buffer that hold the geometry for the terrain.
	result = InitializeBuffers(device);
	if (!result)
	{
		return false;
	}

	
	return true;
}

void Terrain::Render(ID3D11DeviceContext * deviceContext)
{
	// Put the vertex and index buffers on the graphics pipeline to prepare them for drawing.
	RenderBuffers(deviceContext);
	deviceContext->DrawIndexed(m_indexCount, 0, 0);

	return;
}

bool Terrain::CalculateNormals()
{
	int i, j, index1, index2, index3, index, count;
	float vertex1[3], vertex2[3], vertex3[3], vector1[3], vector2[3], sum[3], length;
	DirectX::SimpleMath::Vector3* normals;
	

	// Create a temporary array to hold the un-normalized normal vectors.
	normals = new DirectX::SimpleMath::Vector3[(m_terrainHeight - 1) * (m_terrainWidth - 1)];
	if (!normals)
	{
		return false;
	}

	// Go through all the faces in the mesh and calculate their normals.
	for (j = 0; j<(m_terrainHeight - 1); j++)
	{
		for (i = 0; i<(m_terrainWidth - 1); i++)
		{
			index1 = (j * m_terrainHeight) + i;
			index2 = (j * m_terrainHeight) + (i + 1);
			index3 = ((j + 1) * m_terrainHeight) + i;

			// Get three vertices from the face.
			vertex1[0] = m_heightMap[index1].x;
			vertex1[1] = m_heightMap[index1].y;
			vertex1[2] = m_heightMap[index1].z;

			vertex2[0] = m_heightMap[index2].x;
			vertex2[1] = m_heightMap[index2].y;
			vertex2[2] = m_heightMap[index2].z;

			vertex3[0] = m_heightMap[index3].x;
			vertex3[1] = m_heightMap[index3].y;
			vertex3[2] = m_heightMap[index3].z;

			// Calculate the two vectors for this face.
			vector1[0] = vertex1[0] - vertex3[0];
			vector1[1] = vertex1[1] - vertex3[1];
			vector1[2] = vertex1[2] - vertex3[2];
			vector2[0] = vertex3[0] - vertex2[0];
			vector2[1] = vertex3[1] - vertex2[1];
			vector2[2] = vertex3[2] - vertex2[2];

			index = (j * (m_terrainHeight - 1)) + i;

			// Calculate the cross product of those two vectors to get the un-normalized value for this face normal.
			normals[index].x = (vector1[1] * vector2[2]) - (vector1[2] * vector2[1]);
			normals[index].y = (vector1[2] * vector2[0]) - (vector1[0] * vector2[2]);
			normals[index].z = (vector1[0] * vector2[1]) - (vector1[1] * vector2[0]);
		}
	}

	// Now go through all the vertices and take an average of each face normal 	
	// that the vertex touches to get the averaged normal for that vertex.
	for (j = 0; j<m_terrainHeight; j++)
	{
		for (i = 0; i<m_terrainWidth; i++)
		{
			// Initialize the sum.
			sum[0] = 0.0f;
			sum[1] = 0.0f;
			sum[2] = 0.0f;

			// Initialize the count.
			count = 0;

			// Bottom left face.
			if (((i - 1) >= 0) && ((j - 1) >= 0))
			{
				index = ((j - 1) * (m_terrainHeight - 1)) + (i - 1);

				sum[0] += normals[index].x;
				sum[1] += normals[index].y;
				sum[2] += normals[index].z;
				count++;
			}

			// Bottom right face.
			if ((i < (m_terrainWidth - 1)) && ((j - 1) >= 0))
			{
				index = ((j - 1) * (m_terrainHeight - 1)) + i;

				sum[0] += normals[index].x;
				sum[1] += normals[index].y;
				sum[2] += normals[index].z;
				count++;
			}

			// Upper left face.
			if (((i - 1) >= 0) && (j < (m_terrainHeight - 1)))
			{
				index = (j * (m_terrainHeight - 1)) + (i - 1);

				sum[0] += normals[index].x;
				sum[1] += normals[index].y;
				sum[2] += normals[index].z;
				count++;
			}

			// Upper right face.
			if ((i < (m_terrainWidth - 1)) && (j < (m_terrainHeight - 1)))
			{
				index = (j * (m_terrainHeight - 1)) + i;

				sum[0] += normals[index].x;
				sum[1] += normals[index].y;
				sum[2] += normals[index].z;
				count++;
			}

			// Take the average of the faces touching this vertex.
			sum[0] = (sum[0] / (float)count);
			sum[1] = (sum[1] / (float)count);
			sum[2] = (sum[2] / (float)count);

			// Calculate the length of this normal.
			length = sqrt((sum[0] * sum[0]) + (sum[1] * sum[1]) + (sum[2] * sum[2]));

			// Get an index to the vertex location in the height map array.
			index = (j * m_terrainHeight) + i;

			// Normalize the final shared normal for this vertex and store it in the height map array.
			m_heightMap[index].nx = (sum[0] / length);
			m_heightMap[index].ny = (sum[1] / length);
			m_heightMap[index].nz = (sum[2] / length);
		}
	}

	// Release the temporary normals.
	delete[] normals;
	normals = 0;

	return true;
}

void Terrain::Shutdown()
{
	// Release the index buffer.
	if (m_indexBuffer)
	{
		m_indexBuffer->Release();
		m_indexBuffer = 0;
	}

	// Release the vertex buffer.
	if (m_vertexBuffer)
	{
		m_vertexBuffer->Release();
		m_vertexBuffer = 0;
	}

	return;
}

bool Terrain::InitializeBuffers(ID3D11Device * device )
{
	VertexType* vertices;
	unsigned long* indices;
	D3D11_BUFFER_DESC vertexBufferDesc, indexBufferDesc;
	D3D11_SUBRESOURCE_DATA vertexData, indexData;
	HRESULT result;
	int index, i, j;
	int index1, index2, index3, index4; //geometric indices. 

	// Calculate the number of vertices in the terrain mesh.
	m_vertexCount = (m_terrainWidth - 1) * (m_terrainHeight - 1) * 6;

	// Set the index count to the same as the vertex count.
	m_indexCount = m_vertexCount;

	// Create the vertex array.
	vertices = new VertexType[m_vertexCount];
	if (!vertices)
	{
		return false;
	}

	// Create the index array.
	indices = new unsigned long[m_indexCount];
	if (!indices)
	{
		return false;
	}

	// Initialize the index to the vertex buffer.
	index = 0;

	for (j = 0; j<(m_terrainHeight - 1); j++)
	{
		for (i = 0; i<(m_terrainWidth - 1); i++)
		{
			index1 = (m_terrainHeight * j) + i;          // Bottom left.
			index2 = (m_terrainHeight * j) + (i + 1);      // Bottom right.
			index3 = (m_terrainHeight * (j + 1)) + i;      // Upper left.
			index4 = (m_terrainHeight * (j + 1)) + (i + 1);  // Upper right.

			if (i % 2 == 1)
			{
				if (j % 2 == 1)
				{
					// Upper left.
					vertices[index].position = DirectX::SimpleMath::Vector3(m_heightMap[index3].x, m_heightMap[index3].y, m_heightMap[index3].z);
					vertices[index].normal = DirectX::SimpleMath::Vector3(m_heightMap[index3].nx, m_heightMap[index3].ny, m_heightMap[index3].nz);
					vertices[index].texture = DirectX::SimpleMath::Vector2(m_heightMap[index3].u, m_heightMap[index3].v);
					indices[index] = index;
					index++;

					// Upper right.
					vertices[index].position = DirectX::SimpleMath::Vector3(m_heightMap[index4].x, m_heightMap[index4].y, m_heightMap[index4].z);
					vertices[index].normal = DirectX::SimpleMath::Vector3(m_heightMap[index4].nx, m_heightMap[index4].ny, m_heightMap[index4].nz);
					vertices[index].texture = DirectX::SimpleMath::Vector2(m_heightMap[index4].u, m_heightMap[index4].v);
					indices[index] = index;
					index++;

					// Bottom left.
					vertices[index].position = DirectX::SimpleMath::Vector3(m_heightMap[index1].x, m_heightMap[index1].y, m_heightMap[index1].z);
					vertices[index].normal = DirectX::SimpleMath::Vector3(m_heightMap[index1].nx, m_heightMap[index1].ny, m_heightMap[index1].nz);
					vertices[index].texture = DirectX::SimpleMath::Vector2(m_heightMap[index1].u, m_heightMap[index1].v);
					indices[index] = index;
					index++;

					// Bottom left.
					vertices[index].position = DirectX::SimpleMath::Vector3(m_heightMap[index1].x, m_heightMap[index1].y, m_heightMap[index1].z);
					vertices[index].normal = DirectX::SimpleMath::Vector3(m_heightMap[index1].nx, m_heightMap[index1].ny, m_heightMap[index1].nz);
					vertices[index].texture = DirectX::SimpleMath::Vector2(m_heightMap[index1].u, m_heightMap[index1].v);
					indices[index] = index;
					index++;

					// Upper right.
					vertices[index].position = DirectX::SimpleMath::Vector3(m_heightMap[index4].x, m_heightMap[index4].y, m_heightMap[index4].z);
					vertices[index].normal = DirectX::SimpleMath::Vector3(m_heightMap[index4].nx, m_heightMap[index4].ny, m_heightMap[index4].nz);
					vertices[index].texture = DirectX::SimpleMath::Vector2(m_heightMap[index4].u, m_heightMap[index4].v);
					indices[index] = index;
					index++;

					// Bottom right.
					vertices[index].position = DirectX::SimpleMath::Vector3(m_heightMap[index2].x, m_heightMap[index2].y, m_heightMap[index2].z);
					vertices[index].normal = DirectX::SimpleMath::Vector3(m_heightMap[index2].nx, m_heightMap[index2].ny, m_heightMap[index2].nz);
					vertices[index].texture = DirectX::SimpleMath::Vector2(m_heightMap[index2].u, m_heightMap[index2].v);
					indices[index] = index;
					index++;
				}
				else
				{
					// Upper left.
					vertices[index].position = DirectX::SimpleMath::Vector3(m_heightMap[index3].x, m_heightMap[index3].y, m_heightMap[index3].z);
					vertices[index].normal = DirectX::SimpleMath::Vector3(m_heightMap[index3].nx, m_heightMap[index3].ny, m_heightMap[index3].nz);
					vertices[index].texture = DirectX::SimpleMath::Vector2(m_heightMap[index3].u, m_heightMap[index3].v);
					indices[index] = index;
					index++;

					// Bottom right.
					vertices[index].position = DirectX::SimpleMath::Vector3(m_heightMap[index2].x, m_heightMap[index2].y, m_heightMap[index2].z);
					vertices[index].normal = DirectX::SimpleMath::Vector3(m_heightMap[index2].nx, m_heightMap[index2].ny, m_heightMap[index2].nz);
					vertices[index].texture = DirectX::SimpleMath::Vector2(m_heightMap[index2].u, m_heightMap[index2].v);
					indices[index] = index;
					index++;

					// Bottom left.
					vertices[index].position = DirectX::SimpleMath::Vector3(m_heightMap[index1].x, m_heightMap[index1].y, m_heightMap[index1].z);
					vertices[index].normal = DirectX::SimpleMath::Vector3(m_heightMap[index1].nx, m_heightMap[index1].ny, m_heightMap[index1].nz);
					vertices[index].texture = DirectX::SimpleMath::Vector2(m_heightMap[index1].u, m_heightMap[index1].v);
					indices[index] = index;
					index++;

					// Upper left.
					vertices[index].position = DirectX::SimpleMath::Vector3(m_heightMap[index3].x, m_heightMap[index3].y, m_heightMap[index3].z);
					vertices[index].normal = DirectX::SimpleMath::Vector3(m_heightMap[index3].nx, m_heightMap[index3].ny, m_heightMap[index3].nz);
					vertices[index].texture = DirectX::SimpleMath::Vector2(m_heightMap[index3].u, m_heightMap[index3].v);
					indices[index] = index;
					index++;

					// Upper right.
					vertices[index].position = DirectX::SimpleMath::Vector3(m_heightMap[index4].x, m_heightMap[index4].y, m_heightMap[index4].z);
					vertices[index].normal = DirectX::SimpleMath::Vector3(m_heightMap[index4].nx, m_heightMap[index4].ny, m_heightMap[index4].nz);
					vertices[index].texture = DirectX::SimpleMath::Vector2(m_heightMap[index4].u, m_heightMap[index4].v);
					indices[index] = index;
					index++;

					// Bottom right.
					vertices[index].position = DirectX::SimpleMath::Vector3(m_heightMap[index2].x, m_heightMap[index2].y, m_heightMap[index2].z);
					vertices[index].normal = DirectX::SimpleMath::Vector3(m_heightMap[index2].nx, m_heightMap[index2].ny, m_heightMap[index2].nz);
					vertices[index].texture = DirectX::SimpleMath::Vector2(m_heightMap[index2].u, m_heightMap[index2].v);
					indices[index] = index;
					index++;
				}
			}
			else
			{
				if (j % 2 == 1)
				{
					// Upper left.
					vertices[index].position = DirectX::SimpleMath::Vector3(m_heightMap[index3].x, m_heightMap[index3].y, m_heightMap[index3].z);
					vertices[index].normal = DirectX::SimpleMath::Vector3(m_heightMap[index3].nx, m_heightMap[index3].ny, m_heightMap[index3].nz);
					vertices[index].texture = DirectX::SimpleMath::Vector2(m_heightMap[index3].u, m_heightMap[index3].v);
					indices[index] = index;
					index++;

					// Bottom right.
					vertices[index].position = DirectX::SimpleMath::Vector3(m_heightMap[index2].x, m_heightMap[index2].y, m_heightMap[index2].z);
					vertices[index].normal = DirectX::SimpleMath::Vector3(m_heightMap[index2].nx, m_heightMap[index2].ny, m_heightMap[index2].nz);
					vertices[index].texture = DirectX::SimpleMath::Vector2(m_heightMap[index2].u, m_heightMap[index2].v);
					indices[index] = index;
					index++;

					// Bottom left.
					vertices[index].position = DirectX::SimpleMath::Vector3(m_heightMap[index1].x, m_heightMap[index1].y, m_heightMap[index1].z);
					vertices[index].normal = DirectX::SimpleMath::Vector3(m_heightMap[index1].nx, m_heightMap[index1].ny, m_heightMap[index1].nz);
					vertices[index].texture = DirectX::SimpleMath::Vector2(m_heightMap[index1].u, m_heightMap[index1].v);
					indices[index] = index;
					index++;

					// Upper left.
					vertices[index].position = DirectX::SimpleMath::Vector3(m_heightMap[index3].x, m_heightMap[index3].y, m_heightMap[index3].z);
					vertices[index].normal = DirectX::SimpleMath::Vector3(m_heightMap[index3].nx, m_heightMap[index3].ny, m_heightMap[index3].nz);
					vertices[index].texture = DirectX::SimpleMath::Vector2(m_heightMap[index3].u, m_heightMap[index3].v);
					indices[index] = index;
					index++;

					// Upper right.
					vertices[index].position = DirectX::SimpleMath::Vector3(m_heightMap[index4].x, m_heightMap[index4].y, m_heightMap[index4].z);
					vertices[index].normal = DirectX::SimpleMath::Vector3(m_heightMap[index4].nx, m_heightMap[index4].ny, m_heightMap[index4].nz);
					vertices[index].texture = DirectX::SimpleMath::Vector2(m_heightMap[index4].u, m_heightMap[index4].v);
					indices[index] = index;
					index++;

					// Bottom right.
					vertices[index].position = DirectX::SimpleMath::Vector3(m_heightMap[index2].x, m_heightMap[index2].y, m_heightMap[index2].z);
					vertices[index].normal = DirectX::SimpleMath::Vector3(m_heightMap[index2].nx, m_heightMap[index2].ny, m_heightMap[index2].nz);
					vertices[index].texture = DirectX::SimpleMath::Vector2(m_heightMap[index2].u, m_heightMap[index2].v);
					indices[index] = index;
					index++;
				}
				else
				{
					// Upper left.
					vertices[index].position = DirectX::SimpleMath::Vector3(m_heightMap[index3].x, m_heightMap[index3].y, m_heightMap[index3].z);
					vertices[index].normal = DirectX::SimpleMath::Vector3(m_heightMap[index3].nx, m_heightMap[index3].ny, m_heightMap[index3].nz);
					vertices[index].texture = DirectX::SimpleMath::Vector2(m_heightMap[index3].u, m_heightMap[index3].v);
					indices[index] = index;
					index++;

					// Upper right.
					vertices[index].position = DirectX::SimpleMath::Vector3(m_heightMap[index4].x, m_heightMap[index4].y, m_heightMap[index4].z);
					vertices[index].normal = DirectX::SimpleMath::Vector3(m_heightMap[index4].nx, m_heightMap[index4].ny, m_heightMap[index4].nz);
					vertices[index].texture = DirectX::SimpleMath::Vector2(m_heightMap[index4].u, m_heightMap[index4].v);
					indices[index] = index;
					index++;

					// Bottom left.
					vertices[index].position = DirectX::SimpleMath::Vector3(m_heightMap[index1].x, m_heightMap[index1].y, m_heightMap[index1].z);
					vertices[index].normal = DirectX::SimpleMath::Vector3(m_heightMap[index1].nx, m_heightMap[index1].ny, m_heightMap[index1].nz);
					vertices[index].texture = DirectX::SimpleMath::Vector2(m_heightMap[index1].u, m_heightMap[index1].v);
					indices[index] = index;
					index++;

					// Bottom left.
					vertices[index].position = DirectX::SimpleMath::Vector3(m_heightMap[index1].x, m_heightMap[index1].y, m_heightMap[index1].z);
					vertices[index].normal = DirectX::SimpleMath::Vector3(m_heightMap[index1].nx, m_heightMap[index1].ny, m_heightMap[index1].nz);
					vertices[index].texture = DirectX::SimpleMath::Vector2(m_heightMap[index1].u, m_heightMap[index1].v);
					indices[index] = index;
					index++;

					// Upper right.
					vertices[index].position = DirectX::SimpleMath::Vector3(m_heightMap[index4].x, m_heightMap[index4].y, m_heightMap[index4].z);
					vertices[index].normal = DirectX::SimpleMath::Vector3(m_heightMap[index4].nx, m_heightMap[index4].ny, m_heightMap[index4].nz);
					vertices[index].texture = DirectX::SimpleMath::Vector2(m_heightMap[index4].u, m_heightMap[index4].v);
					indices[index] = index;
					index++;

					// Bottom right.
					vertices[index].position = DirectX::SimpleMath::Vector3(m_heightMap[index2].x, m_heightMap[index2].y, m_heightMap[index2].z);
					vertices[index].normal = DirectX::SimpleMath::Vector3(m_heightMap[index2].nx, m_heightMap[index2].ny, m_heightMap[index2].nz);
					vertices[index].texture = DirectX::SimpleMath::Vector2(m_heightMap[index2].u, m_heightMap[index2].v);
					indices[index] = index;
					index++;
				}
				
			}
			
		}
	}

	// Set up the description of the static vertex buffer.
	vertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	vertexBufferDesc.ByteWidth = sizeof(VertexType) * m_vertexCount;
	vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vertexBufferDesc.CPUAccessFlags = 0;
	vertexBufferDesc.MiscFlags = 0;
	vertexBufferDesc.StructureByteStride = 0;

	// Give the subresource structure a pointer to the vertex data.
	vertexData.pSysMem = vertices;
	vertexData.SysMemPitch = 0;
	vertexData.SysMemSlicePitch = 0;

	// Now create the vertex buffer.
	result = device->CreateBuffer(&vertexBufferDesc, &vertexData, &m_vertexBuffer);
	if (FAILED(result))
	{
		return false;
	}

	// Set up the description of the static index buffer.
	indexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	indexBufferDesc.ByteWidth = sizeof(unsigned long) * m_indexCount;
	indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
	indexBufferDesc.CPUAccessFlags = 0;
	indexBufferDesc.MiscFlags = 0;
	indexBufferDesc.StructureByteStride = 0;

	// Give the subresource structure a pointer to the index data.
	indexData.pSysMem = indices;
	indexData.SysMemPitch = 0;
	indexData.SysMemSlicePitch = 0;

	// Create the index buffer.
	result = device->CreateBuffer(&indexBufferDesc, &indexData, &m_indexBuffer);
	if (FAILED(result))
	{
		return false;
	}

	// Release the arrays now that the vertex and index buffers have been created and loaded.
	delete[] vertices;
	vertices = 0;

	delete[] indices;
	indices = 0;

	return true;
}

void Terrain::RenderBuffers(ID3D11DeviceContext * deviceContext)
{
	unsigned int stride;
	unsigned int offset;

	// Set vertex buffer stride and offset.
	stride = sizeof(VertexType);
	offset = 0;

	// Set the vertex buffer to active in the input assembler so it can be rendered.
	deviceContext->IASetVertexBuffers(0, 1, &m_vertexBuffer, &stride, &offset);

	// Set the index buffer to active in the input assembler so it can be rendered.
	deviceContext->IASetIndexBuffer(m_indexBuffer, DXGI_FORMAT_R32_UINT, 0);

	// Set the type of primitive that should be rendered from this vertex buffer, in this case triangles.
	deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	return;
}

bool Terrain::GenerateHeightMap(ID3D11Device* device, DirectX::SimpleMath::Vector3 playerStart)
{
	bool result;

	int index;
	float height = 0.0;

	m_frequency = (XM_2PI/m_terrainHeight) / m_wavelength; //we want a wavelength of 1 to be a single wave over the whole terrain.  A single wave is 2 pi which is about 6.283

	//loop through the terrain and set the hieghts how we want. This is where we generate the terrain
	//in this case I will run a sin-wave through the terrain in one axis.
	/*
	for (int j = 0; j<m_terrainHeight; j++)
	{
		for (int i = 0; i<m_terrainWidth; i++)
		{
			index = (m_terrainHeight * j) + i;

			
			m_heightMap[index].x = (float)i;
			m_heightMap[index].y = (float)(sin((float)i * (m_frequency)) * m_amplitude) + (float)(sin((float)j * (m_frequency)) * m_amplitude);
			m_heightMap[index].z = (float)j;
		}
	}
	

	*/

	result = PCGDungeonMap(playerStart);
	if (!result)
	{
		return false;
	}

	result = PlaceCollectibles();
	if (!result)
	{
		return false;
	}

	result = CalculateNormals();
	if (!result)
	{
		return false;
	}

	result = InitializeBuffers(device);
	if (!result)
	{
		return false;
	}
}

bool Terrain::RandomHeightMap()
{
	bool result;
	int index;
	float height = 0.0;

	for (int j = 0; j < m_terrainHeight; j++)
	{
		for (int i = 0; i < m_terrainWidth; i++)
		{
			index = (m_terrainHeight * j) + i;

			height = (float) (rand() % 200);
			height = (height - 100.f) / 100.f;
			height *= m_amplitude;
			height *= m_perlNoise.noise((float)i / m_terrainWidth, (float)j / m_terrainHeight, 0);
			//height *= m_perlNoise.noise(0.5, 0.1, 0.8);

			m_heightMap[index].x = (float)i;
			m_heightMap[index].y = height;
			m_heightMap[index].z = (float)j;
		}
	}

	return true;
}

bool Terrain::NoiseHeightMap()
{
	bool result;
	int index;
	float height = 100.0;

	for (int j = 0; j < m_terrainHeight; j++)
	{
		for (int i = 0; i < m_terrainWidth; i++)
		{
			index = (m_terrainHeight * j) + i;


			m_heightMap[index].x = (float)i;
			m_heightMap[index].y = m_perlNoise.noise((float)i / m_terrainWidth , 0, (float)j / m_terrainHeight) * height;
			m_heightMap[index].z = (float)j;
		}
	}

	return true;
}

// Function to take Dungeon Map and imprint it onto the terrain
bool Terrain::GenerateDungeonHeightMap()
{
	int index;
	float height = 100.0;

	for (int j = 0; j < m_terrainHeight; j++)
	{
		for (int i = 0; i < m_terrainWidth; i++)
		{
			index = (m_terrainHeight * j) + i;


			m_heightMap[index].x = (float)i;

			if (i == 0 || j == 0)
			{
				m_heightMap[index].y = height;
			}
			else if (i == m_terrainWidth-1 || j == m_terrainHeight-1)
			{
				m_heightMap[index].y = height;
			}
			else
			{
				m_heightMap[index].y = -1.0;
			}

			//m_heightMap[index].y = m_perlNoise.noise((float)i / m_terrainWidth, 0, (float)j / m_terrainHeight) * height;
			m_heightMap[index].z = (float)j;
		}
	}

	return true;
}


// Using Cellular Automata, Construct a cave map by expanding traversable zones from a seed
bool Terrain::PCGDungeonMap(DirectX::SimpleMath::Vector3 playerStart)
{
	int index;
	int valid_neighbors;

	// Seeding Map
	for (int j = 0; j < m_terrainHeight; j++)
	{
		for (int i = 0; i < m_terrainWidth; i++)
		{
			index = (m_terrainHeight * j) + i;


			m_heightMap[index].x = (float)i;
			m_heightMap[index].z = (float)j;

			

			// Walls enforced on edge of map
			if (i == 0 || j == 0)
			{
				m_heightMap[index].y = WALL_HEIGHT;
			}
			else if (i == m_terrainWidth - 1 || j == m_terrainHeight - 1)
			{
				m_heightMap[index].y = WALL_HEIGHT;
			}
			else
			{
				float random = (float)rand() / RAND_MAX;
				if (random <= m_seedChance)
				{
					m_heightMap[index].y = FLOOR_HEIGHT;
				}
				else
				{
					m_heightMap[index].y = WALL_HEIGHT;
				}
			}
		}
	}

	// Always Seed Camera Location as an empty area
	int temp = ((int)playerStart.z * m_terrainHeight + (int)playerStart.x);
	m_heightMap[temp].y = FLOOR_HEIGHT;

	// Cellular Automata
	for (int iter = 0; iter < m_iterations; iter++)
	{
		for (int j = 1; j < m_terrainHeight-1; j++)
		{
			for (int i = 1; i < m_terrainWidth-1; i++)
			{
				index = (m_terrainHeight * j) + i;


				m_heightMap[index].x = (float)i;
				m_heightMap[index].z = (float)j;

				valid_neighbors = 0;

				if (j > 1)
				{
					// Top Left
					if (i > 1)
					{
						if (m_heightMap[(m_terrainHeight * (j - 1)) + (i - 1)].y < 0)
							valid_neighbors++;
					}

					//Top Right
					if (i < (m_terrainWidth - 2))
					{
						if (m_heightMap[(m_terrainHeight * (j - 1)) + (i + 1)].y < 0)
							valid_neighbors++;
					}

					//Top Middle
					if (m_heightMap[(m_terrainHeight * (j - 1)) + (i)].y < 0)
						valid_neighbors++;

				}

				if (j < (m_terrainHeight - 2))
				{
					// Bottom Left
					if (i > 1)
					{
						if (m_heightMap[(m_terrainHeight * (j + 1)) + (i - 1)].y < 0)
							valid_neighbors++;
					}

					// Bottom Right
					if (i < (m_terrainWidth - 2))
					{
						if (m_heightMap[(m_terrainHeight * (j + 1)) + (i + 1)].y < 0)
							valid_neighbors++;
					}

					// Bottom Middle
					if (m_heightMap[(m_terrainHeight * (j + 1)) + (i)].y < 0)
						valid_neighbors++;

				}

				// Inline Left
				if (i > 1)
				{
					if (m_heightMap[(m_terrainHeight * (j)) + (i - 1)].y < 0)
						valid_neighbors++;
				}

				// Inline Right
				if (i < (m_terrainWidth - 2))
				{
					if (m_heightMap[(m_terrainHeight * (j)) + (i + 1)].y < 0)
						valid_neighbors++;
				}


				// If threshold of neighbors reached
				if(valid_neighbors >= m_threshold)
					m_heightMap[index].y = FLOOR_HEIGHT;
			}
		}
	}

	return true;
}

bool Terrain::PlaceCollectibles()
{
	int placed = 0;

	while (placed < COLLECTIBLE_COUNT)
	{
		int index = rand() % (m_terrainHeight * m_terrainWidth);

		if (m_heightMap[index].y == FLOOR_HEIGHT)
		{
			m_collectibles[placed] = DirectX::SimpleMath::Vector3(m_heightMap[index].x, m_heightMap[index].y, m_heightMap[index].z);
			placed++;
		}
	}

	return true;
}

DirectX::SimpleMath::Vector3* Terrain::getCollectibles()
{
	return m_collectibles;
}

// Check collision of collectible with player cam
bool Terrain::CollideWithCollectible(DirectX::SimpleMath::Vector3 other)
{
	for (int i = 0; i < COLLECTIBLE_COUNT; i++)
	{
		// Provide a small leeway to allow impercise movement
		if (other.x <= m_collectibles[i].x + COLLECTIBLE_LEEWAY && other.x >= m_collectibles[i].x - COLLECTIBLE_LEEWAY)
		{
			if (other.z <= m_collectibles[i].z + COLLECTIBLE_LEEWAY && other.z >= m_collectibles[i].z - COLLECTIBLE_LEEWAY)
			{
				m_collectibles[i].x = m_terrainWidth + 10;
				return true;
			}
			
		}
	}
	return false;
}

// Check if location collides with the wall
DirectX::SimpleMath::Vector3 Terrain::CollideWithWall(DirectX::SimpleMath::Vector3 other, DirectX::SimpleMath::Vector3 lastPos)
{
	int index = ((int)other.z * m_terrainHeight + (int)other.x);

	if (other.x < 0 || other.z < 0)
		return lastPos;

	if (other.x > m_terrainWidth || other.z > m_terrainHeight)
		return lastPos;

	if (m_heightMap[index].y == WALL_HEIGHT)
		return lastPos;

	return CollideWithNeighborhood(other, lastPos);
}

DirectX::SimpleMath::Vector3 Terrain::CollideWithNeighborhood(DirectX::SimpleMath::Vector3 other, DirectX::SimpleMath::Vector3 lastPos)
{
	int index = (((int)other.z + 1) * m_terrainHeight + (int)other.x);
	if (m_heightMap[index].y == WALL_HEIGHT)
		return lastPos;
	
	index = (((int)other.z - 1) * m_terrainHeight + (int)other.x);
	if (m_heightMap[index].y == WALL_HEIGHT)
		return lastPos;
	
	index = ((int)other.z  * m_terrainHeight + ((int)other.x + 1));
	if (m_heightMap[index].y == WALL_HEIGHT)
		return lastPos;
	
	index = ((int)other.z * m_terrainHeight + ((int)other.x - 1));
	if (m_heightMap[index].y == WALL_HEIGHT)
		return lastPos;

	return other;
}

bool Terrain::SmoothHeight()
{
	bool result;
	int index;
	float heightSum;
	int averageCount;

	for (int j = 0; j < m_terrainHeight; j++)
	{
		for (int i = 0; i < m_terrainWidth; i++)
		{
			heightSum = 0.f;
			averageCount = 0;
			index = (m_terrainHeight * j) + i;

			if (j > 0)
			{
				// Top Left
				if (i > 0)
				{
					averageCount++;
					heightSum += m_heightMap[(m_terrainHeight * (j - 1)) + (i - 1)].y;
				}

				//Top Right
				if(i < (m_terrainWidth - 1))
				{
					averageCount++;
					heightSum += m_heightMap[(m_terrainHeight * (j - 1)) + (i + 1)].y;
				}
				
				//Top Middle
				averageCount++;
				heightSum += m_heightMap[(m_terrainHeight * (j - 1)) + (i)].y;

			}

			if (j < (m_terrainHeight - 1))
			{
				// Bottom Left
				if (i > 0)
				{
					averageCount++;
					heightSum += m_heightMap[(m_terrainHeight * (j + 1)) + (i - 1)].y;
				}

				// Bottom Right
				if (i < (m_terrainWidth - 1))
				{
					averageCount++;
					heightSum += m_heightMap[(m_terrainHeight * (j + 1)) + (i + 1)].y;
				}

				// Bottom Middle
				averageCount++;
				heightSum += m_heightMap[(m_terrainHeight * (j + 1)) + (i)].y;

			}

			// Inline Left
			if (i > 0)
			{
				averageCount++;
				heightSum += m_heightMap[(m_terrainHeight * (j)) + (i - 1)].y;
			}

			// Inline Right
			if (i < (m_terrainWidth - 1))
			{
				averageCount++;
				heightSum += m_heightMap[(m_terrainHeight * (j)) + (i + 1)].y;
			}

			// Ours
			averageCount++;
			heightSum += m_heightMap[index].y;
			
			m_heightMap[index].y = heightSum / averageCount;
		}
	}
	return true;
}

bool Terrain::RandomParticleDeposition()
{
	//return ParticleDepositionAtPoint(rand() % (m_terrainHeight * m_terrainWidth));
	return ParticleDepositionAtPoint(127);
}

bool Terrain::ParticleDepositionAtPoint(int index)
{
	int landingSite = index;
	int i, j;

	int nextSite = landingSite;

	float tempHeight;
	float particleHeight = m_amplitude * 0.5;

	bool onFlatSurface = false;

	while (!onFlatSurface)
	{
		onFlatSurface = true;
		i = nextSite % m_terrainHeight;
		j = (nextSite) / m_terrainHeight;
		tempHeight = m_heightMap[nextSite].y + particleHeight;

		if (j > 0)
		{
			// Top Left
			if (i > 0)
			{
				if (m_heightMap[(m_terrainHeight * (j - 1)) + (i - 1)].y < m_heightMap[nextSite].y)
				{
					onFlatSurface = false;
					nextSite = (m_terrainHeight * (j - 1)) + (i - 1);
				}
			}

			//Top Right
			if (i < (m_terrainWidth - 1))
			{
				if (m_heightMap[(m_terrainHeight * (j - 1)) + (i + 1)].y < m_heightMap[nextSite].y)
				{
					onFlatSurface = false;
					nextSite = (m_terrainHeight * (j - 1)) + (i + 1);
				}
			}

			//Top Middle
			if (m_heightMap[(m_terrainHeight * (j - 1)) + (i)].y < m_heightMap[nextSite].y)
			{
				onFlatSurface = false;
				nextSite = (m_terrainHeight * (j - 1)) + (i);
			}

		}

		if (j < (m_terrainHeight - 1))
		{
			// Bottom Left
			if (i > 0)
			{
				if (m_heightMap[(m_terrainHeight * (j + 1)) + (i - 1)].y < m_heightMap[nextSite].y)
				{
					onFlatSurface = false;
					nextSite = (m_terrainHeight * (j + 1)) + (i - 1);
				}
			}

			// Bottom Right
			if (i < (m_terrainWidth - 1))
			{
				if (m_heightMap[(m_terrainHeight * (j + 1)) + (i + 1)].y < m_heightMap[nextSite].y)
				{
					onFlatSurface = false;
					nextSite = (m_terrainHeight * (j + 1)) + (i + 1);
				}
			}

			// Bottom Middle
			if (m_heightMap[(m_terrainHeight * (j + 1)) + (i)].y < m_heightMap[nextSite].y)
			{
				onFlatSurface = false;
				nextSite = (m_terrainHeight * (j + 1)) + (i);
			}

		}

		// Inline Left
		if (i > 0)
		{
			if (m_heightMap[(m_terrainHeight * (j)) + (i - 1)].y < m_heightMap[nextSite].y)
			{
				onFlatSurface = false;
				nextSite = (m_terrainHeight * (j)) + (i - 1);
			}
		}
		  

		// Inline Right
		if (i < (m_terrainWidth - 1))
		{
			if (m_heightMap[(m_terrainHeight * (j)) + (i + 1)].y < m_heightMap[nextSite].y)
			{
				onFlatSurface = false;
				nextSite = (m_terrainHeight * (j)) + (i + 1);
			}
		}
	}

	m_heightMap[nextSite].y += particleHeight;
	return true;
}

bool Terrain::Update()
{
	return true; 
}

float* Terrain::GetWavelength()
{
	return &m_wavelength;
}

float* Terrain::GetAmplitude()
{
	return &m_amplitude;
}

int* Terrain::GetPCGIterations()
{
	return &m_iterations;
}

int* Terrain::GetPCGThreshold()
{
	return &m_threshold;
}

float* Terrain::GetPCGSeedChance()
{
	return &m_seedChance;
}

