#include "pch.h"
#include "Physics.h"

using namespace DirectX::SimpleMath;

void Physics::Initialize(Terrain level)
{
	m_gravity = 0.5;
	m_elastic = 0.3;
	m_friction = 0.5;
	m_mass = 10.0f;
	m_kickStrength = 50.0f;
	m_pushStrength = 10.0f;

	m_level = level;

	SpawnBall(Vector3(10.f, 1.f, 10.f), 10.f);
}

bool Physics::Update(float dTime)
{
	bool result;

	// calculate new values in position, vel, and acceleration
	// add for loop to expand to multiple objects


	// Constant Gravity
	//Vector3 gravity = ApplyForceOn(m_activeObject, Vector3(0, -1, 0), m_gravity * GRAVITY * dTime);
	float gravityAccel = -1 * (m_gravity * GRAVITY) / m_activeObject.mass;

	Vector3 newAccel = Vector3(0, gravityAccel, 0);
	Vector3 newVel = m_activeObject.velocity + m_activeObject.acceleration;
	Vector3 newPos = m_activeObject.position + m_activeObject.velocity * dTime;

	result = CalculateCollision(m_activeObject, &newPos, &newVel, &newAccel);
	if (!result)
	{
		return false;
	}

	// Update Values
	m_activeObject.position		= newPos;
	m_activeObject.velocity		= newVel;
	m_activeObject.acceleration = newAccel;
	
	return false;
}

bool Physics::FacesCollisionXZ(PhysicsObject target, Vector3 newPos, char** out)
{
	bool result = false;

	//result = result || (m_level.CollideWithWall(newPos, target.position) == target.position);

	// wanted to make it cleaner but the need to identify the corresponding axis forced a large conditional
	if (m_level.CollideWithWall(Vector3(newPos.x - target.radius, newPos.y, newPos.z), target.position) == target.position)
	{
		result = true;
		*out = "X";
	}
	else if (m_level.CollideWithWall(Vector3(newPos.x + target.radius, newPos.y, newPos.z), target.position) == target.position)
	{
		result = true;
		*out = "X";
	}
	else if (m_level.CollideWithWall(Vector3(newPos.x, newPos.y, newPos.z - target.radius), target.position) == target.position)
	{
		result = true;
		*out = "Z";
	}
	else if (m_level.CollideWithWall(Vector3(newPos.x, newPos.y, newPos.z + target.radius), target.position) == target.position)
	{
		result = true;
		*out = "Z";
	}

	return result;
}

bool Physics::FacesCollisionY(PhysicsObject target, DirectX::SimpleMath::Vector3 newPos)
{
	if (newPos.y - target.radius <= FLOOR_HEIGHT - 0.6)
		return true;

	return false;
}

DirectX::SimpleMath::Vector3 Physics::CollideWithBall(DirectX::SimpleMath::Vector3 newPos, DirectX::SimpleMath::Vector3 oldPos, DirectX::SimpleMath::Vector3 forward)
{
	if (newPos.x <= (m_activeObject.position.x + m_activeObject.radius*2) && newPos.x >= (m_activeObject.position.x - m_activeObject.radius * 2))
	{
		if (newPos.z <= (m_activeObject.position.z + m_activeObject.radius*2) && newPos.z >= (m_activeObject.position.z - m_activeObject.radius * 2))
		{
			m_activeObject.acceleration += ApplyForceOn(m_activeObject, forward, m_pushStrength);
			return oldPos;
		}
	}
	
	return newPos;
}

bool Physics::CalculateCollision(PhysicsObject active, Vector3* nextPos, Vector3* nextVel, Vector3* nextAcc)
{

	bool onWall, onFloor = false;
	char* axis = "A";

	onWall = FacesCollisionXZ(active, *nextPos, &axis);
	onFloor = FacesCollisionY(active, *nextPos);

	if (onWall)
	{
		// Handle Wall Bounce
		WallBounce(active, axis, nextVel, nextAcc);
		
		if (axis == "X")
		{
			nextPos->x = m_activeObject.position.x;
		}
		else if (axis == "Z")
		{
			nextPos->z = m_activeObject.position.z;
		}

	}
	else if (onFloor)
	{
		// Handle Floor Bounce
		FloorBounce(active, nextVel, nextAcc);
		nextPos->y = m_activeObject.position.y;
	}
	else
	{
		// No collision? In air perform air resistance
		Vector3 frictionDirection = active.velocity * -1;
		frictionDirection.Normalize();
		*nextAcc += ApplyFriction(m_activeObject, frictionDirection, nextVel, m_friction * AIR_FRICTION);
	}
	

	return true;
}

bool Physics::FloorBounce(PhysicsObject active, DirectX::SimpleMath::Vector3* vel, DirectX::SimpleMath::Vector3* acc)
{
	Vector3 frictionDirection = active.velocity * -1;
	
	// No friction in the perpendicular axis
	frictionDirection.y = 0;
	frictionDirection.Normalize();

	float normalForce = m_activeObject.mass * m_gravity * GRAVITY;
	*acc += ApplyFriction(m_activeObject, frictionDirection, vel, m_friction * FLOOR_FRICTION * normalForce);

	vel->y *= -1 * m_elastic;

	return true;
}

bool Physics::WallBounce(PhysicsObject active, const char* axis, DirectX::SimpleMath::Vector3* vel, DirectX::SimpleMath::Vector3* acc)
{
	// Get Opposite direction
	Vector3 frictionDirection = active.velocity * -1;
	frictionDirection.Normalize();

	float normalForce = 0.f;

	// momentum conserved so m*v (mass is same so just v)
	if (axis == "X")
	{
		vel->x *= -1 * m_elastic;

		// No friction in the perpendicular axis
		frictionDirection.x = 0;

		//Normal from the wall
  		normalForce = acc->x * m_activeObject.mass;
	}
	else if (axis == "Z")
	{
		vel->z *= -1 * m_elastic;

		// No friction in the perpendicular axis
		frictionDirection.z = 0;

		//Normal from the wall
		normalForce = acc->z * m_activeObject.mass;
	}
	else
	{
		return false;
	}

	// f = coeff * NormalF = coeff * mass * -gravity
	*acc += ApplyFriction(m_activeObject, frictionDirection, vel, m_friction * FLOOR_FRICTION * normalForce);
	return true;
}

// F = m*a
DirectX::SimpleMath::Vector3 Physics::ApplyForceOn(PhysicsObject target, Vector3 direction, float force)
{
	// Force only applies to accelerations therefore just add change to vector
	// First scale by mass

	Vector3 newAccel;
	float deltaAccel = force / target.mass;

	newAccel.x = (deltaAccel * direction.x);
	newAccel.y = (deltaAccel * direction.y);
	newAccel.z = (deltaAccel * direction.z);

	return newAccel;
}

void Physics::ApplyForceOnObjectInRange(DirectX::SimpleMath::Vector3 playerPosition, DirectX::SimpleMath::Vector3 playerDirection)
{
	//if(m_activeObject.position.x <= )

	Vector3 adjustedForward = Vector3(playerDirection.x, playerDirection.y + 0.5f, playerDirection.z);
	m_activeObject.acceleration += ApplyForceOn(m_activeObject, adjustedForward, m_kickStrength);
	return;
}

DirectX::SimpleMath::Vector3 Physics::ApplyFriction(PhysicsObject target, Vector3 direction, Vector3* nextVel, float force)
{
	Vector3 result = ApplyForceOn(m_activeObject, direction, force);
	Vector3 resultVel = *nextVel + result;


	if (resultVel.x * nextVel->x < 0)
	{
		result.x = 0;
		nextVel->x = 0;
	}
	if (resultVel.z * nextVel->z < 0)
	{
		result.z = 0;
		nextVel->z = 0;
	}
	if (resultVel.y * nextVel->y < 0)
	{
		result.y = 0;
		nextVel->y = 0;
	}

	return result;
}

bool Physics::SpawnBall(DirectX::SimpleMath::Vector3 location, float mass)
{
	PhysicsObject newPO;
	
	newPO.position		= location;
	newPO.velocity		= Vector3(1.f, 0.f, 1.f);
	newPO.acceleration	= Vector3(0.f, 0.f, 0.f);
	newPO.orientation	= Quaternion::Identity;


	newPO.mass			= mass;
	newPO.radius		= 0.5;
	newPO.modelID		= 0;


	m_activeObject = newPO;
	return true;
}

bool Physics::SpawnBox(DirectX::SimpleMath::Vector3 location, float mass, float radius)
{
	return false;
}

DirectX::SimpleMath::Vector3 Physics::GetActivePosition()
{
	return m_activeObject.position;
}

DirectX::SimpleMath::Vector3 Physics::GetActiveVelocity()
{
	return m_activeObject.velocity;
}

DirectX::SimpleMath::Quaternion Physics::GetActiveRotation()
{
	return m_activeObject.orientation;
}

void Physics::SetActiveRotation(Quaternion newRotation)
{
	m_activeObject.orientation = newRotation;
}

// IMGUI Getters
float* Physics::GravityGUI()
{
	return &m_gravity;
}

float* Physics::FrictionGUI()
{
	return &m_friction;
}

float* Physics::ElasticityGUI()
{
	return &m_elastic;
}

float* Physics::BallMassGUI()
{
	return &m_mass;
}

float* Physics::KickStrengthGUI()
{
	return &m_kickStrength;
}
