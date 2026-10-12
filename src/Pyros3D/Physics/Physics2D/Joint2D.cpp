//============================================================================
// Name        : Joint2D
// Description : See the header.
//============================================================================

#include <Pyros3D/Physics/Physics2D/Joint2D.h>
#include <box2d/box2d.h>
#include <atomic>

namespace p3d {

	static std::atomic<int> gLiveJoint2D(0);
	uint32 Joint2D::LiveCount() { const int n = gLiveJoint2D.load(); return n > 0 ? (uint32)n : 0; }

	Joint2D::Joint2D(const uint32 t)
		: type(t), anchor(0.f, 0.f), connectedAnchor(0.f, 0.f), haveConnectedAnchor(false),
		axis(1.f, 0.f), collideConnected(false), limit(false), lower(0.f), upper(0.f),
		motor(false), motorSpeed(0.f), maxMotorForce(100.f), spring(false), hertz(4.f), damping(0.7f),
		length(0.f), haveReference(false), referenceAnchor(0.f, 0.f), referenceAngleA(0.f), referenceAngleB(0.f),
		dirty(true), haveJoint(false), jointIndex(0), jointWorld(0), jointGeneration(0)
	{
		gLiveJoint2D++;
	}

	Joint2D::~Joint2D() { gLiveJoint2D--; }

	void Joint2D::SetLimits(const bool enabled, const f32 lowerLimit, const f32 upperLimit)
	{
		// Given either way round: a limit of (90, -90) means the same range.
		const f32 lo = lowerLimit < upperLimit ? lowerLimit : upperLimit;
		const f32 hi = lowerLimit < upperLimit ? upperLimit : lowerLimit;
		if (enabled == limit && lo == lower && hi == upper) return;
		limit = enabled;
		lower = lo;
		upper = hi;
		dirty = true;
	}

	void Joint2D::SetMotor(const bool enabled, const f32 speed, const f32 maxForce)
	{
		if (enabled == motor && speed == motorSpeed && maxForce == maxMotorForce) return;
		motor = enabled;
		motorSpeed = speed;
		maxMotorForce = maxForce;
		dirty = true;
	}

	void Joint2D::SetSpring(const bool enabled, const f32 springHertz, const f32 dampingRatio)
	{
		if (enabled == spring && springHertz == hertz && dampingRatio == damping) return;
		spring = enabled;
		hertz = springHertz;
		damping = dampingRatio;
		dirty = true;
	}

	void Joint2D::SetTarget(const Vec2 &t)
	{
		const bool first = !haveTarget;
		if (!first && t.x == target.x && t.y == target.y) return;
		target = t;
		haveTarget = true;
		if (type != Joint2DType::Target) return;
		// Only a joint to the world reads it; one to another body follows
		// that body's anchor.
		if (!haveJoint || !connected.empty() || first) { dirty = true; return; }
		b2JointId id;
		id.index1 = jointIndex;
		id.world0 = jointWorld;
		id.generation = jointGeneration;
		if (!b2Joint_IsValid(id)) { dirty = true; return; }
		b2Transform frame = b2Joint_GetLocalFrameA(id);
		frame.p.x = t.x;
		frame.p.y = t.y;
		b2Joint_SetLocalFrameA(id, frame);
		b2Joint_WakeBodies(id);
	}

	void Joint2D::SetMotorSpeed(const f32 speed)
	{
		if (speed == motorSpeed) return;
		motorSpeed = speed;
		if (!haveJoint) { dirty = true; return; }
		b2JointId id;
		id.index1 = jointIndex;
		id.world0 = jointWorld;
		id.generation = jointGeneration;
		if (!b2Joint_IsValid(id)) { dirty = true; return; }
		switch (type)
		{
		case Joint2DType::Hinge: b2RevoluteJoint_SetMotorSpeed(id, speed); break;
		case Joint2DType::Slider: b2PrismaticJoint_SetMotorSpeed(id, speed); break;
		case Joint2DType::Wheel: b2WheelJoint_SetMotorSpeed(id, speed); break;
		case Joint2DType::Distance: b2DistanceJoint_SetMotorSpeed(id, speed); break;
		default: break;
		}
		b2Joint_WakeBodies(id);
	}

	void Joint2D::SetJointHandle(const int32 index, const uint16 world, const uint16 generation)
	{
		jointIndex = index;
		jointWorld = world;
		jointGeneration = generation;
		haveJoint = true;
	}

};
