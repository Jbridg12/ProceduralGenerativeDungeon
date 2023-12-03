#pragma once

#include "Terrain.h"

// Define global parameters for physics engine
#define AIR_FRICTION	 0.02f
#define FLOOR_FRICTION	 0.5f
#define GRAVITY			 9.8f
#define KICK_RANGE		 5.0f

// Ball and Block Physics
class Physics
{
private:
	struct PhysicsObject
	{
		DirectX::SimpleMath::Vector3	 position;
		DirectX::SimpleMath::Vector3	 velocity;
		DirectX::SimpleMath::Vector3	 acceleration;
		DirectX::SimpleMath::Quaternion  orientation;

		float radius;
		float mass;
		int modelID;
	};
public:
	// ImGUI accessor functions
	float*		GravityGUI();
	float*		FrictionGUI();
	float*		ElasticityGUI();
	float*		BallMassGUI();
	float*		KickStrengthGUI();

	void		Initialize(Terrain);
	bool		Update(float);
	
	// Detect collisions at position +- radius for all axes
	
	// Detect Horizontal collisions with walls
	// (target, new position, out string specifying the collision axis)
	bool		FacesCollisionXZ(PhysicsObject, DirectX::SimpleMath::Vector3, char**);

	// Detect Horizontal collisions with Floor
	// (target, new position)
	bool		FacesCollisionY(PhysicsObject, DirectX::SimpleMath::Vector3);

	// Other collision detection
	DirectX::SimpleMath::Vector3	CollideWithBall(DirectX::SimpleMath::Vector3, DirectX::SimpleMath::Vector3, DirectX::SimpleMath::Vector3);
	bool							CalculateCollision(PhysicsObject, DirectX::SimpleMath::Vector3*, DirectX::SimpleMath::Vector3*, DirectX::SimpleMath::Vector3*);
	bool							FloorBounce(PhysicsObject, DirectX::SimpleMath::Vector3*, DirectX::SimpleMath::Vector3*);
	bool							WallBounce(PhysicsObject, const char*, DirectX::SimpleMath::Vector3*, DirectX::SimpleMath::Vector3*);

	DirectX::SimpleMath::Vector3		ApplyForceOn(PhysicsObject, DirectX::SimpleMath::Vector3, float);

	// Apply force if an object is nearby
	// Vector3 playerPosition, Vector3 playerDirection
	void								ApplyForceOnObjectInRange(DirectX::SimpleMath::Vector3, DirectX::SimpleMath::Vector3);
	DirectX::SimpleMath::Vector3		ApplyFriction(PhysicsObject, DirectX::SimpleMath::Vector3, DirectX::SimpleMath::Vector3*, float);

	//Spawn physics objects ( Location, Mass, Radius )
	bool		SpawnBall(DirectX::SimpleMath::Vector3, float);
	bool		SpawnBox(DirectX::SimpleMath::Vector3, float, float);


	// Rendering Getters
	DirectX::SimpleMath::Vector3	 GetActivePosition();
	DirectX::SimpleMath::Vector3	 GetActiveVelocity();
	DirectX::SimpleMath::Quaternion  GetActiveRotation();
	void							 SetActiveRotation(DirectX::SimpleMath::Quaternion);


private:

	//Global modifiers to allow GUI to modify physics
	float		m_friction;
	float		m_gravity;
	float		m_elastic;

	// Modifiers to allow player control over next ball spawned
	float		m_mass;

	// Control power of kick
	float		m_kickStrength;

	// Force camera applies to ball
	float		m_pushStrength;

	// Future functionality for multiple objects at once
	//PhysicsObject* m_actveObjects;

	PhysicsObject	m_activeObject;

	// Needs to know the level
	Terrain			m_level;

};

