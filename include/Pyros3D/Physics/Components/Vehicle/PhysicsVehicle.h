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
#include <memory>

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

		// Where the wheel is now, kept by the physics engine: how far the hub
		// has moved up its suspension from Position, how far it has turned on
		// its axle and about its steering axis, and whether it is on anything.
		f32 Travel;
		f32 Spin;
		f32 SpinSpeed;
		f32 Steer;
		bool InContact;

	};

	// Which wheels the engine force turns.
	namespace VehicleDrive {
		enum {
			Rear = 0,
			Front,
			All
		};
	}

	class PYROS3D_API PhysicsVehicle : public IPhysicsComponent {

	public:

		PhysicsVehicle(IPhysics* engine, const std::shared_ptr<IPhysicsComponent> &ChassisShape, bool ghost);

		virtual ~PhysicsVehicle();

		// Overrides the shared IPhysicsComponent::GetComponentType() -
		// Vehicle needs its own tag distinct from ComponentType::Physics
		// since it has a whole extra shape (chassis) and wheel list a
		// generic physics-shape serializer can't handle.
		virtual uint32 GetComponentType() const { return ComponentType::Vehicle; }

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
		uint32 GetDriveWheels() { return driveWheels; }
		f32 GetHandBrakeForce() { return gHandBrakeForce; }
		f32 GetSuspensionLowerLimit() { return suspensionLower; }
		f32 GetSuspensionUpperLimit() { return suspensionUpper; }
		bool HasCenterOfMass() { return hasCenterOfMass; }
		const Vec3 &GetCenterOfMass() { return centerOfMass; }

		IPhysicsComponent* GetChassis() { return this->chassisShape.get(); }

		// Legacy/optional extra storage (same void* pattern as
		// IPhysicsComponent::rigidBodyPTR). Unused by the Box3D backend
		// (wheel bodies/joints live in Box3DBodyHandles via
		// SaveRigidBodyPTR); kept for API compatibility / engine extras.
		void SaveVehicleRaycasterPTR(void* ptr) { vehicleRaycasterPTR = ptr; }
		void* GetVehicleRaycasterPTR() { return vehicleRaycasterPTR; }

		// Setters
		//
		// A wheel is a ray down its suspension, not a body: where the ray
		// meets the ground a spring holds the chassis up and the tyre pushes
		// on it. A wheel's Position is its hub with the vehicle at rest.
		//
		// The engine force is the push asked of each driven wheel where it
		// meets the ground, in newtons: positive drives the vehicle toward
		// cross(axle, up), negative reverses it. The braking force is the most
		// each wheel may hold back with, and wins over the engine. The hand
		// brake is a braking force on the rear wheels alone, and locked rear
		// wheels slide. All of it is limited by the wheel's friction times the
		// weight on it.
		//
		// Suspension stiffness is the ride frequency in hertz (a car is 1.5
		// to 2.5) and damping the share of critical damping (0.3 to 1).
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
		void SetDriveWheels(const uint32 drive) { this->driveWheels = drive; }
		void SetHandBrakeForce(const f32 force) { this->gHandBrakeForce = force; }
		// How far a hub travels from its Position, along the chassis' up:
		// lower is the droop (negative), upper the compression.
		void SetSuspensionLimits(const f32 lower, const f32 upper) { this->suspensionLower = lower; this->suspensionUpper = upper; }
		// Where the weight sits, in chassis space. Left unset it is the middle
		// of the chassis shape, which for a car is far too high: it rolls over
		// in the first fast corner. Read when the vehicle is registered.
		void SetCenterOfMass(const Vec3 &center) { this->centerOfMass = center; this->hasCenterOfMass = true; }
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
		uint32 driveWheels;
		f32 gHandBrakeForce;
		f32 suspensionLower;
		f32 suspensionUpper;
		bool hasCenterOfMass;
		Vec3 centerOfMass;

		// Save Chassis Shape of the Vehicle (shared_ptr - orphan shape not
		// attached to a GameObject, kept alive for the vehicle's lifetime).
		std::shared_ptr<IPhysicsComponent> chassisShape;

		// List of Wheels in the Vehicle
		std::vector<VehicleWheel> Wheels;

		// See SaveVehicleRaycasterPTR() above.
		void* vehicleRaycasterPTR = NULL;
	};

}

#endif	/* PHYSICSVEHICLE_H */

