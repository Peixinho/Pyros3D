//============================================================================
// Name        : VehicleSim.h
// Description : A car simulated the way racing sims do it, on top of the
//               physics world's rigid body: the chassis is a Box3D body (so it
//               hits walls and other things), and every fixed physics step
//               (run it at 300-500 Hz: IPhysics::SetStepRate) this works out
//               what its four corners do and pushes it with the result:
//
//   - suspension: springs (with preload so the car sits at its modelled ride
//     height), dampers with separate bump/rebound and a fast-speed knee,
//     progressive bump stops, anti-roll bars; the wheel finds the road with
//     several rays across its profile, so a kerb lifts it before its centre
//     gets there
//   - tyres: transient slip (relaxation lengths - stable down to standstill,
//     where they hold the car like the rubber does), a Magic-Formula-shaped
//     curve on normalised combined slip (a friction ellipse that falls off
//     past the peak), load sensitivity, surface/core temperature with a grip
//     window, pneumatic trail for the aligning moment
//   - drivetrain: engine torque curve, inertia, friction/engine braking, rev
//     limiter, idle control; a clutch and a limited-slip differential solved
//     as velocity impulses (stable at any stiffness); sequential gearbox with
//     shift time, ignition cut and rev-matched downshifts; auto-clutch
//   - brakes with bias; ABS and traction control with levels
//   - aero: drag and front/rear downforce
//   - surfaces per physics body (asphalt, kerb, grass, gravel...)
//   - force feedback: the steering column torque from the front tyres'
//     aligning moments and lateral forces about the mechanical trail
//
//               Axes: the car's own frame is +X right, +Y up, -Z forward
//               (the engine's camera convention), metres, kg, N, rad.
//============================================================================

#ifndef PYROS_VEHICLESIM_H
#define PYROS_VEHICLESIM_H

#include <Pyros3D/Core/Math/Math.h>
#include <Pyros3D/Physics/PhysicsEngines/IPhysics.h>
#include <Pyros3D/Other/Export.h>
#include <memory>
#include <vector>
#include <string>

namespace p3d {

	struct PYROS3D_API VehicleSurface {
		f32 grip = 1.f;            // multiplies the tyre's friction
		f32 rollingResistance = 0.f; // extra rolling resistance coefficient
		f32 drag = 0.f;            // resistance as a share of the load (like rolling resistance): gravel/grass sinking in
		f32 bumpAmplitude = 0.f;   // m, a ripple the wheel rides (kerbs' rumble, grass ruts)
		f32 bumpWavelength = 1.f;  // m
		// the rumble the steering wheel shakes with on it (0 none, 1 full force; kerbs ~0.25), every that many metres -
		// at most VehicleConfig::ffbRumbleMaxHz a second (what a game sending the force once a frame can carry)
		f32 ffbRumble = 0.f;
		f32 ffbRumbleWavelength = 0.6f;
		uint32 id = 0;             // the game's own number, given back in telemetry
	};

	struct PYROS3D_API VehicleAxle {
		f32 springRate = 150000.f;     // N/m at the wheel
		f32 bumpDamping = 6000.f;      // N/(m/s), slow
		f32 reboundDamping = 9000.f;
		f32 fastBumpDamping = 3000.f;  // past the knee
		f32 fastReboundDamping = 4500.f;
		f32 damperKnee = 0.12f;        // m/s
		f32 bumpTravel = 0.04f;        // m the wheel can rise from its static position
		f32 droopTravel = 0.07f;       // m it can fall
		f32 bumpStopRate = 400000.f;   // N/m once in the bump stop
		f32 bumpStopGap = 0.008f;      // m of bump travel left when the stop begins
		f32 antiRollRate = 80000.f;    // N/m between the two wheels' travels
		f32 rollCentreHeight = 0.05f;  // m: lateral force acts this high above the road
		f32 brakeTorque = 3200.f;      // N m per wheel at full pedal (before bias)
		f32 mechanicalTrail = 0.03f;   // m (caster): lateral force's lever on the steering
		// The steering's geometry lifting the car: turned, a steered wheel's contact patch moves down against the hub - the
		// caster by casterJack * sin(angle), one side up and the other down (the body rolls); the kingpin's inclination
		// by kpiJack * (1 - cos(angle)), both up (the body rises, and its weight turns the wheel back to the middle)
		f32 casterJack = 0.03f;        // m
		f32 kpiJack = 0.03f;           // m
		f32 unsprungMass = 46.f;       // kg per corner: wheel, tyre, upright, brake (moves with the road)
		bool driven = false;
		bool steered = false;
	};

	struct PYROS3D_API VehicleTyre {
		f32 radius = 0.34f;
		f32 width = 0.31f;
		f32 inertia = 1.3f;            // wheel+tyre+hub+disc about the axle, kg m^2
		f32 friction = 1.75f;          // peak mu at nominal load, optimum temperature
		f32 nominalLoad = 4000.f;      // N
		f32 loadSensitivity = 0.13f;   // mu falls this share per doubling-of-load fraction (dmu/mu per dFz/Fz0)
		f32 peakSlipAngle = 0.12f;     // rad (~7 deg)
		f32 peakSlipRatio = 0.10f;
		f32 shapeLateral = 1.45f;      // Magic Formula C: how far it falls past the peak
		f32 shapeLongitudinal = 1.55f;
		f32 longitudinalFriction = 1.02f; // mu_x / mu_y
		f32 relaxationLateral = 0.30f; // m
		f32 relaxationLongitudinal = 0.12f; // m
		f32 rollingResistance = 0.012f;
		f32 pneumaticTrail = 0.035f;   // m at small slip, gone past the peak
		f32 verticalStiffness = 300000.f; // N/m: the tyre is a spring under the wheel (a slick: ~300 N/mm)
		f32 verticalDamping = 500.f;   // N/(m/s)
		// temperature
		f32 optimumTemperature = 90.f; // deg C
		f32 temperatureWindow = 30.f;  // +- this much loses `temperatureGripLoss` of the grip
		f32 temperatureGripLoss = 0.12f;
		f32 startTemperature = 85.f;    // (warmed: an out lap or blankets - 70 started a race on cold tyres)
		f32 ambientTemperature = 22.f;
		f32 trackTemperature = 30.f;
		// (the heat model's numbers - all set from the car's data, these the defaults)
		f32 heatShare = 0.36f;          // of the sliding's work into the surface
		f32 surfaceCapacity = 2200.f;   // J/C: how fast the surface heats and cools
		f32 coreCapacity = 5000.f;      // J/C: how fast the core follows (from cold ~2 laps)
		f32 hotWindowFactor = 1.6f;     // over the optimum the window is this much wider - the grip goes more slowly
		f32 peakGripFactor = 0.96f;     // the grip at the optimum, of `friction`
	};

	struct PYROS3D_API VehicleEngine {
		std::vector<f32> torqueRpm = { 1000.f, 3000.f, 5000.f, 6500.f, 7500.f, 8500.f, 9400.f, 9800.f };
		std::vector<f32> torqueNm = { 250.f, 380.f, 440.f, 470.f, 465.f, 440.f, 410.f, 0.f };
		f32 inertia = 0.18f;           // kg m^2 (engine + flywheel + clutch)
		f32 idleRpm = 2500.f;
		f32 limiterRpm = 9400.f;
		f32 stallRpm = 900.f;
		f32 frictionTorque = 25.f;     // N m at zero rpm when off throttle
		f32 frictionPerRpm = 0.009f;   // N m per rpm (engine braking grows with rpm)
	};

	struct PYROS3D_API VehicleDrivetrain {
		std::vector<f32> gears = { 3.08f, 2.24f, 1.80f, 1.50f, 1.28f, 1.12f }; // 1st.. top
		f32 reverse = 3.0f;
		f32 finalDrive = 3.44f;
		f32 efficiency = 0.92f;
		f32 clutchTorque = 950.f;      // N m the clutch can carry
		f32 shiftUpTime = 0.045f;      // s with no drive while it changes
		f32 shiftDownTime = 0.07f;
		f32 diffPreload = 120.f;       // N m locking with no torque through it
		f32 diffPowerRamp = 0.45f;     // extra locking per N m going in, on power
		f32 diffCoastRamp = 0.25f;     // ... and off power
		f32 launchRpm = 5500.f;        // auto-clutch bites here from a standstill
	};

	struct PYROS3D_API VehicleAero {
		f32 dragArea = 0.92f;          // Cd * A, m^2
		f32 liftArea = 2.3f;           // Cl * A (downforce), m^2
		f32 frontShare = 0.40f;        // of the downforce, on the front axle
		f32 airDensity = 1.2f;
		f32 centreHeight = 0.45f;      // m above the floor where drag acts (car frame)
	};

	struct PYROS3D_API VehicleConfig {
		f32 mass = 1350.f;             // kg with driver and fuel
		Vec3 centreOfMass = Vec3(0.f, 0.45f, 0.f); // car frame
		Vec3 inertia = Vec3(2100.f, 2400.f, 550.f); // principal moments about the CoM: x (pitch), y (yaw), z (roll)
		// wheel centres in the car frame at static ride height: FL, FR, RL, RR
		Vec3 wheels[4] = { Vec3(-0.82f, 0.34f, -1.30f), Vec3(0.82f, 0.34f, -1.30f), Vec3(-0.80f, 0.34f, 1.15f), Vec3(0.80f, 0.34f, 1.15f) };
		VehicleAxle front, rear;
		VehicleTyre frontTyre, rearTyre;
		VehicleEngine engine;
		VehicleDrivetrain drivetrain;
		VehicleAero aero;
		f32 steeringLock = 0.42f;      // rad: road wheel angle at full lock (straight-ahead wheel, no Ackermann)
		f32 steeringWheelLock = 4.71f; // rad: the car's steering wheel rotation lock-to-lock (270 deg each way = 540)
		f32 ackermann = 0.35f;         // 0 parallel .. 1 full Ackermann
		f32 brakeBias = 0.58f;         // front share
		f32 brakeRise = 0.05f;         // s: how soon the brakes' pressure follows the pedal (0 = at once)
		int absLevel = 3;              // 0 off .. 10
		// ABS (Lua properties): a wheel braked past the slip where its tyre grips most is held at
		// target slip = peakSlipRatio * (absSlipBase - absSlipPerLevel * level); under it the pedal is the driver's
		f32 absMinFactor = 0.02f;      // lowest share of the pedal's brake torque ABS will leave (0..1)
		f32 absResponse = 0.02f;       // s: how soon a wheel past the target is brought back to it
		f32 absSlipBase = 1.0f;        // slip target scale at level 0 (1 = the tyre's peak)
		f32 absSlipPerLevel = 0.03f;   // each ABS level tightens the target by this
		f32 absMinSpeed = 1.5f;        // m/s below which ABS is off
		int tcLevel = 3;               // 0 off .. 10
		f32 tcLowSpeed = 8.f;          // m/s: under this the slip is read as slip speed over it (no cut off the line)
		f32 tcFloor = 0.25f;           // the least of the throttle traction control leaves
		f32 tcCutRate = 12.f;          // its cut, per second
		f32 tcRecoverRate = 6.f;       // its recovery, per second
		f32 ffbFullTorque = 900.f;     // N m at the kingpins (both) that is full force feedback
		f32 ffbRoadFeel = 0.10f;       // force feedback share per m/s of the front wheels' differing bump speed (kerbs, ripples)
		f32 ffbRumbleMaxHz = 25.f;     // the surfaces' rumble no faster than this (the force is sent once a frame)
		int substeps = 4;              // tyre/drivetrain iterations per physics step
	};

	struct PYROS3D_API VehicleWheelState {
		f32 suspension = 0.f;   // m from static: + up (compressed)
		f32 steer = 0.f;        // rad, + right
		f32 spin = 0.f;         // rad, accumulated
		f32 angularVelocity = 0.f; // rad/s
		f32 load = 0.f;         // N
		f32 slipRatio = 0.f;
		f32 slipAngle = 0.f;    // rad
		f32 longitudinalForce = 0.f, lateralForce = 0.f;
		f32 surfaceTemperature = 0.f, coreTemperature = 0.f;
		f32 grip = 1.f;         // temperature x load x surface factor actually used
		bool contact = false;
		uint32 surface = 0;
		Vec3 contactPoint;      // world
		f32 brakeTorque = 0.f;
	};

	struct PYROS3D_API VehicleTelemetry {
		f32 speed = 0.f;        // m/s, forward
		f32 rpm = 0.f;
		int gear = 0;           // -1 R, 0 N, 1..
		bool shifting = false;
		f32 throttle = 0.f, brake = 0.f, clutch = 0.f; // what reaches the car (after assists)
		f32 steerInput = 0.f;   // -1..1
		f32 steeringWheelAngle = 0.f; // rad
		Vec3 acceleration;      // car frame, m/s^2 (x right, y up, z back)
		bool absActive = false, tcActive = false, limiter = false;
		f32 ffb = 0.f;          // -1..1 force feedback (+ turns the wheel right)
		f32 ffbRumble = 0.f;    // the surfaces' share of it
		f32 aligningTorque = 0.f; // N m at the kingpins
		f32 downforce = 0.f, drag = 0.f;
		f32 engineTorque = 0.f;
		f32 fuelCut = 0.f;
	};

	class PYROS3D_API VehicleSim {

	public:

		VehicleSim(IPhysics* physics, const std::shared_ptr<IPhysicsComponent> &chassis, const VehicleConfig &config);
		~VehicleSim();

		// Driver's inputs, any time (the steps read the latest). steer -1..1
		// (fraction of the car's steering-wheel lock, + right), pedals 0..1.
		void SetInputs(const f32 steer, const f32 throttle, const f32 brake, const f32 clutch);
		// The steering wheel angle directly (rad, + right): a real wheel turned
		// 1:1 - clamped to the car's lock.
		void SetSteeringWheelAngle(const f32 radians);
		void ShiftUp();
		void ShiftDown();
		void SetGear(const int gear);
		void SetAutoClutch(const bool on) { autoClutch = on; }
		void SetAutoBlip(const bool on) { autoBlip = on; }
		void SetABS(const int level) { cfg.absLevel = level; }
		void SetTC(const int level) { cfg.tcLevel = level; }
		void SetBrakeBias(const f32 front) { cfg.brakeBias = front; }
		void SetIgnition(const bool on) { ignition = on; }
		void SetEngineRunning(const bool on);

		// A physics body the wheels can drive on, and what it is like.
		void SetSurface(IPhysicsComponent* body, const VehicleSurface &surface);
		void SetDefaultSurface(const VehicleSurface &surface) { defaultSurface = surface; }
		// Wheels stand only on bodies given a surface (SetSurface): walls, barriers, other
		// cars and props are left to the chassis to hit. Off: any body is ground.
		void SetGroundOnlySurfaces(const bool on) { groundOnlySurfaces = on; }

		// Stops it (wheels, engine at idle in neutral) - after a teleport.
		void Reset();
		// Reset() as a car already rolling at speed (m/s) in that gear: the wheels spinning with the road, the engine at the
		// gear's revs (a car put down mid-lap - a test that re-runs one piece of a lap)
		void ResetRolling(const f32 speed, const int gear);
		// every tyre's surface and core at this temperature (C): a session begun on fresh tyres - warmed in blankets or not.
		// Reset() leaves them as they are: a retried race kept the last one's heat
		void SetTyreTemperature(const f32 t);

		// Damage (a game's own model decides it): a wheel's grip share (a flat-spotted or cut tyre, 1 whole), its toe in
		// radians (a bent arm: + to the right), and whether it is gone - no force, the corner on its hub; the engine's
		// share of its torque (a radiator, an oil leak). Reset() leaves them as they are; SetWheelDamage(i, 1, 0, false)
		// mends a wheel.
		void SetWheelDamage(const uint32 i, const f32 grip, const f32 toe, const bool detached)
		{ if (i < 4) { wheels[i].dmgGrip = grip; wheels[i].dmgToe = toe; wheels[i].detached = detached; } }
		void SetEnginePower(const f32 share) { enginePower = share; }

		const VehicleWheelState &GetWheel(const uint32 i) const { return wheels[i < 4 ? i : 0].state; }
		const VehicleTelemetry &GetTelemetry() const { return telemetry; }
		const VehicleConfig &GetConfig() const { return cfg; }
		VehicleConfig &EditConfig() { return cfg; }
		IPhysicsComponent* GetChassis() const { return chassis.get(); }

	private:

		struct Wheel {
			Vec3 mount;          // static centre, car frame
			bool front = false, left = false;
			f32 z = 0.f, zPrev = 0.f, zVel = 0.f; // the hub's suspension travel (+ compressed) and its speed
			f32 groundZ = -1e9f;             // where the undeformed tyre would put it
			f32 preload = 0.f;   // spring force at z = 0
			f32 omega = 0.f;     // rad/s
			f32 sx = 0.f, sy = 0.f; // transient slip states (ratio, tan angle)
			f32 tempSurface = 70.f, tempCore = 70.f;
			f32 bumpPhase = 0.f, rumblePhase = 0.f;
			// this step
			bool contact = false;
			Vec3 groundPoint, groundNormal;
			const VehicleSurface* surface = NULL;
			VehicleWheelState state;
			f32 dmgGrip = 1.f, dmgToe = 0.f;     // (damage: SetWheelDamage)
			bool detached = false;
		};

		void Step(const f32 dt);
		void FindGround(Wheel &w, const Vec3 &pos, const Quaternion &rot, const Vec3 &up, const Vec3 &forward);
		f32 EngineTorque(const f32 rpm, const f32 throttle) const;
		f32 GearRatio(const int gear) const;
		const VehicleAxle &Axle(const Wheel &w) const { return w.front ? cfg.front : cfg.rear; }
		const VehicleTyre &Tyre(const Wheel &w) const { return w.front ? cfg.frontTyre : cfg.rearTyre; }

		IPhysics* physics;
		std::shared_ptr<IPhysicsComponent> chassis;
		VehicleConfig cfg;
		uint32 callback = 0;
		Wheel wheels[4];

		// inputs
		f32 inSteer = 0.f, inThrottle = 0.f, inBrake = 0.f, inClutch = 0.f;
		f32 brakeLine = 0.f;           // the brakes' pressure, 0..1 of the pedal's most
		f32 enginePower = 1.f;
		bool steerIsAngle = false;
		f32 inSteerAngle = 0.f;
		bool autoClutch = true, autoBlip = true, ignition = true, engineRunning = true;
		bool launching = false;        // on the way up from a standstill: the automatic clutch slips until the wheels have caught the revs

		// drivetrain
		f32 engineOmega = 0.f;
		int gear = 0, nextGear = 0;
		f32 shiftTimer = 0.f;
		bool shiftingDown = false;
		bool limiterCut = false;
		f32 tcFactor = 1.f;
		f32 clutchEngage = 0.f;

		Vec3 lastVelocity;
		bool hasLastVelocity = false;
		f32 ffbSmoothed = 0.f;

		struct SurfaceEntry { IPhysicsComponent* body; VehicleSurface surface; };
		std::vector<SurfaceEntry> surfaces;
		VehicleSurface defaultSurface;
		bool groundOnlySurfaces = false;

		VehicleTelemetry telemetry;
	};

}

#endif /* PYROS_VEHICLESIM_H */
