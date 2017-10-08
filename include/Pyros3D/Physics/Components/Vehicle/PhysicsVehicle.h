//============================================================================
// Name        : PhysicsVehicle.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Physics Vehicle 
//============================================================================

#ifndef PHYSICSVEHICLE_H
#define	PHYSICSVEHICLE_H

#include <Pyros3D/Physics/Components/IPhysicsComponent.h>
#include <vector>

namespace p3d {

	// Wheel Structure
	struct VehicleWheel {

		Vec3 Direction;
		Vec3 Axle;
		f32 Radius;
		f32 Width;
		f32 Friction;
		f32 RollInfluence;
		Vec3 Position;
		bool IsFrontWheel;
		Matrix Transformation;

	};

	class PYROS3D_API PhysicsVehicle : public IPhysicsComponent {

	public:

		PhysicsVehicle(IPhysics* engine, IPhysicsComponent* ChassisShape, bool ghost);

		virtual ~PhysicsVehicle();

		int GetRightIndex() { return rightIndex; }
		int GetUpIndex() { return upIndex; }
		int GetForwardIndex() { return forwardIndex; }
		int GetMaxProxies() { return maxProxies; }
		int GetMaxOverlap() { return maxOverlap; }
		f32 GetEngineForce() { return gEngineForce; }
		f32 GetBreakingForce() { return gBreakingForce; }
		f32 GetMaxEngineForce() { return maxEngineForce; }
		f32 GetMaxBreakingForce() { return maxBreakingForce; }
		f32 GetVehicleSteering() { return gVehicleSteering; }
		f32 GetSteeringIncrement() { return steeringIncrement; }
		f32 GetSteeringClamp() { return steeringClamp; }
		f32 GetSuspensionStiffness() { return suspensionStiffness; }
		f32 GetSuspensionDamping() { return suspensionDamping; }
		f32 GetSuspensionCompression() { return suspensionCompression; }
		f32 GetSuspensionRestLength() { return suspensionRestLength; }

		IPhysicsComponent* GetChassis() { return this->chassisShape; }

		// Setters
		void SetMaxProxies(const uint32 maxProxies) { this->maxProxies = maxProxies; }
		void SetMaxOverlap(const uint32 maxOverlap) { this->maxOverlap = maxOverlap; }
		void SetEngineForce(const f32 engineForce) { this->gEngineForce = engineForce; }
		void SetBreakingForce(const f32 breakingForce) { this->gBreakingForce = breakingForce; }
		void SetMaxEngineForce(const f32 maxEngineForce) { this->maxEngineForce = maxEngineForce; }
		void SetMaxBreakingForce(const f32 maxBreakingForce) { this->maxBreakingForce = maxBreakingForce; }
		void SetVehicleSteering(const f32 vehicleSteering) { this->gVehicleSteering = vehicleSteering; }
		void SetSteeringIncrement(const f32 steeringIncrement) { this->steeringIncrement = steeringIncrement; }
		void SetSteeringClamp(const f32 steeringClamp) { this->steeringClamp = steeringClamp; }
		void SetSuspensionStiffness(const f32 suspensionStiffness) { this->suspensionStiffness = suspensionStiffness; }
		void SetSuspensionDamping(const f32 suspensionDamping) { this->suspensionDamping = suspensionDamping; }
		void SetSuspensionCompression(const f32 suspensiomCompression) { this->suspensionCompression = suspensiomCompression; }
		void SetSuspensionRestLength(const f32 suspensionRestLength) { this->suspensionRestLength = suspensionRestLength; }
		// Add Wheel to Vehicle
		void AddWheel(const Vec3 &WheelDirection, const Vec3 &WheelAxle, const f32 WheelRadius, const f32 WheelWidth, const f32 WheelFriction, const f32 WheelRollInfluence, const Vec3 &Position, bool isFrontWheel);
		// Get Wheels of the Vehicle
		std::vector<VehicleWheel> &GetWheels() { return Wheels; }

	protected:

		int rightIndex;
		int upIndex;
		int forwardIndex;
		int maxProxies;
		int maxOverlap;
		f32 gEngineForce;
		f32 gBreakingForce;
		f32 maxEngineForce;
		f32 maxBreakingForce;
		f32 gVehicleSteering;
		f32 steeringIncrement;
		f32 steeringClamp;
		f32 suspensionStiffness;
		f32 suspensionDamping;
		f32 suspensionCompression;
		f32 suspensionRestLength;

		// Save Chassis Shape of the Vehicle
		IPhysicsComponent* chassisShape;

		// List of Wheels in the Vehicle
		std::vector<VehicleWheel> Wheels;
	};

}

#endif	/* PHYSICSVEHICLE_H */

