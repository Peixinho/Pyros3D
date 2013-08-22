//============================================================================
// Name        : PhysicsVehicle.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Physics Vehicle 
//============================================================================


#include "PhysicsVehicle.h"

namespace p3d {   

    PhysicsVehicle::PhysicsVehicle(IPhysics* engine, IPhysicsComponent* ChassisShape) : IPhysicsComponent(0,CollisionShapes::Vehicle,engine)
    {
        
        rightIndex = 0;
        upIndex = 1;
        forwardIndex = 2;

        maxProxies = 32766;
        maxOverlap = 65535;

        gEngineForce = 0.f;
        gBreakingForce = 0.f;

        maxEngineForce = 3000.f;
        maxBreakingForce = 300.f;

        gVehicleSteering = 0.f;
        steeringIncrement = 0.04f;
        steeringClamp = 0.3f;

        suspensionStiffness = 20.f;
        suspensionDamping = 2.3f;
        suspensionCompression = 4.4f;
        suspensionRestLength = 0.6f;
        
        // Save Chassis Shape
        this->chassisShape = ChassisShape;

    }

    void PhysicsVehicle::AddWheel(const Vec3& WheelDirection, const Vec3& WheelAxle, const f32& WheelRadius, const f32& WheelWidth, const f32& WheelFriction, const f32& WheelRollInfluence, const Vec3& Position, bool isFrontWheel)
    {
        
        VehicleWheel wheel;
        wheel.Direction = WheelDirection;
        wheel.Axle = WheelAxle;
        wheel.Radius = WheelRadius;
        wheel.Position = Position;
        wheel.Width = WheelWidth;
        wheel.Friction = WheelFriction;
        wheel.RollInfluence = WheelRollInfluence;
        wheel.IsFrontWheel = isFrontWheel;
        wheel.Transformation = Matrix();
        
        if (rigidBodyRegistered)
        {
            Wheels.push_back(wheel);
            InternalAddWheel(WheelDirection,WheelAxle,WheelRadius,WheelWidth,WheelFriction,WheelRollInfluence,Position,isFrontWheel);
            // Add on the Fly
        } else {
            // Add to a List, and then on Vehicle Creation, add Wheels
            Wheels.push_back(wheel);
        }
        
    }

    PhysicsVehicle::~PhysicsVehicle() {}

}
