//============================================================================
// Name        : VehicleSim.cpp
// Description : See VehicleSim.h.
//============================================================================

#include <Pyros3D/Physics/Vehicle/VehicleSim.h>
#include <cmath>
#include <algorithm>

namespace p3d {

	namespace {
		const f32 kGravity = 9.81f;
		const f32 kPi = 3.14159265358979f;
		inline f32 Clamp(const f32 v, const f32 lo, const f32 hi) { return v < lo ? lo : (v > hi ? hi : v); }
		inline f32 Sign(const f32 v) { return v < 0.f ? -1.f : 1.f; }
		inline f32 Lerp(const f32 a, const f32 b, const f32 t) { return a + (b - a) * t; }
		inline f32 RadPerSecToRpm(const f32 w) { return w * 60.f / (2.f * kPi); }
		inline f32 RpmToRadPerSec(const f32 r) { return r * 2.f * kPi / 60.f; }

		// Magic-Formula-shaped curve on a normalised slip: 0 at 0, peak 1 at 1,
		// falling towards sin(C pi/2) far past it.
		inline f32 SlipCurve(const f32 rho, const f32 C)
		{
			const f32 B = tanf(kPi / (2.f * C));
			return sinf(C * atanf(B * rho));
		}
	}

	VehicleSim::VehicleSim(IPhysics* physics, const std::shared_ptr<IPhysicsComponent> &chassis, const VehicleConfig &config)
		: physics(physics), chassis(chassis), cfg(config)
	{
		// corners: FL FR RL RR
		for (int i = 0; i < 4; i++)
		{
			Wheel &w = wheels[i];
			w.mount = cfg.wheels[i];
			w.front = i < 2;
			w.left = (i % 2) == 0;
			w.tempSurface = w.tempCore = Tyre(w).startTemperature;
		}

		// Each spring carries its corner's share of the weight at z = 0, so
		// the car sits at the ride height it was modelled at.
		const f32 zf = 0.5f * (cfg.wheels[0].z + cfg.wheels[1].z);
		const f32 zr = 0.5f * (cfg.wheels[2].z + cfg.wheels[3].z);
		f32 frontShare = (zr - cfg.centreOfMass.z) / (zr - zf);
		frontShare = Clamp(frontShare, 0.2f, 0.8f);
		const f32 W = cfg.mass * kGravity;
		for (int i = 0; i < 4; i++)
			wheels[i].preload = W * (wheels[i].front ? frontShare : 1.f - frontShare) * 0.5f;

		if (physics && chassis)
		{
			physics->SetMassProperties(chassis.get(), cfg.mass, cfg.centreOfMass, cfg.inertia);
			// (the world's speed cap is for crate piles: a car does 300 km/h)
			if (physics->GetMaximumLinearSpeed() < 150.f) physics->SetMaximumLinearSpeed(150.f);
			physics->Activate(chassis.get());
			callback = physics->AddStepCallback([this](f32 dt) { Step(dt); });
		}
		engineOmega = RpmToRadPerSec(cfg.engine.idleRpm);
	}

	VehicleSim::~VehicleSim()
	{
		if (physics && callback) physics->RemoveStepCallback(callback);
	}

	void VehicleSim::SetInputs(const f32 steer, const f32 throttle, const f32 brake, const f32 clutch)
	{
		inSteer = Clamp(steer, -1.f, 1.f);
		steerIsAngle = false;
		inThrottle = Clamp(throttle, 0.f, 1.f);
		inBrake = Clamp(brake, 0.f, 1.f);
		inClutch = Clamp(clutch, 0.f, 1.f);
	}

	void VehicleSim::SetSteeringWheelAngle(const f32 radians)
	{
		inSteerAngle = radians;
		steerIsAngle = true;
	}

	f32 VehicleSim::GearRatio(const int g) const
	{
		if (g > 0 && g <= (int)cfg.drivetrain.gears.size()) return cfg.drivetrain.gears[(size_t)g - 1] * cfg.drivetrain.finalDrive;
		if (g < 0) return -cfg.drivetrain.reverse * cfg.drivetrain.finalDrive;
		return 0.f;
	}

	void VehicleSim::ShiftUp()
	{
		if (shiftTimer > 0.f) return;
		const int target = (shiftTimer > 0.f ? nextGear : gear) + 1;
		if (target > (int)cfg.drivetrain.gears.size()) return;
		nextGear = target;
		shiftTimer = (gear <= 0) ? 0.12f : cfg.drivetrain.shiftUpTime;
		shiftingDown = false;
		gear = 0;
	}

	void VehicleSim::ShiftDown()
	{
		if (shiftTimer > 0.f) return;
		const int target = gear - 1;
		if (target < -1) return;
		// reverse only when (nearly) stopped
		if (target == -1 && fabsf(telemetry.speed) > 1.5f) return;
		// the gearbox refuses a downshift that would over-rev the engine
		if (target > 0)
		{
			const int driven0 = cfg.front.driven ? 0 : 2;
			const f32 wheelOmega = 0.5f * (wheels[driven0].omega + wheels[driven0 + 1].omega);
			const f32 rpm = RadPerSecToRpm(fabsf(wheelOmega * GearRatio(target)));
			if (rpm > cfg.engine.limiterRpm + 250.f) return;
		}
		nextGear = target;
		shiftTimer = (target <= 0) ? 0.12f : cfg.drivetrain.shiftDownTime;
		shiftingDown = target > 0;
		gear = 0;
	}

	void VehicleSim::SetGear(const int g)
	{
		gear = std::max(-1, std::min(g, (int)cfg.drivetrain.gears.size()));
		nextGear = gear;
		shiftTimer = 0.f;
	}

	void VehicleSim::SetEngineRunning(const bool on)
	{
		engineRunning = on;
		if (on && engineOmega < RpmToRadPerSec(cfg.engine.idleRpm)) engineOmega = RpmToRadPerSec(cfg.engine.idleRpm);
	}

	void VehicleSim::SetSurface(IPhysicsComponent* body, const VehicleSurface &surface)
	{
		for (size_t i = 0; i < surfaces.size(); i++)
			if (surfaces[i].body == body) { surfaces[i].surface = surface; return; }
		SurfaceEntry e;
		e.body = body;
		e.surface = surface;
		surfaces.push_back(e);
	}

	void VehicleSim::Reset()
	{
		for (int i = 0; i < 4; i++)
		{
			Wheel &w = wheels[i];
			w.z = w.zPrev = w.zVel = 0.f;
			w.groundZ = -1e9f;
			w.omega = 0.f;
			w.sx = w.sy = 0.f;
		}
		engineOmega = RpmToRadPerSec(cfg.engine.idleRpm);
		engineRunning = true;
		shiftTimer = 0.f;
		tcFactor = 1.f;
		hasLastVelocity = false;
		ffbSmoothed = 0.f;
	}

	void VehicleSim::SetTyreTemperature(const f32 t)
	{
		for (int i = 0; i < 4; i++) wheels[i].tempSurface = wheels[i].tempCore = t;
	}

	void VehicleSim::ResetRolling(const f32 speed, const int gear)
	{
		Reset();
		SetGear(gear);
		for (int i = 0; i < 4; i++) { Wheel &w = wheels[i]; w.omega = speed / std::max(0.05f, Tyre(w).radius); }
		const f32 rr = Tyre(wheels[2]).radius;
		const f32 eo = speed / std::max(0.05f, rr) * GearRatio(gear);
		engineOmega = std::max(engineOmega, std::min(eo, RpmToRadPerSec(cfg.engine.limiterRpm)));
	}

	f32 VehicleSim::EngineTorque(const f32 rpm, const f32 throttle) const
	{
		const std::vector<f32> &r = cfg.engine.torqueRpm;
		const std::vector<f32> &t = cfg.engine.torqueNm;
		f32 full = 0.f;
		if (!r.empty() && r.size() == t.size())
		{
			if (rpm <= r.front()) full = t.front() * Clamp(rpm / std::max(1.f, r.front()), 0.f, 1.f);
			else if (rpm >= r.back()) full = t.back();
			else
				for (size_t i = 1; i < r.size(); i++)
					if (rpm <= r[i]) { full = Lerp(t[i - 1], t[i], (rpm - r[i - 1]) / std::max(1.f, r[i] - r[i - 1])); break; }
		}
		const f32 friction = cfg.engine.frictionTorque + cfg.engine.frictionPerRpm * std::max(0.f, rpm);
		return throttle * full * enginePower - (1.f - throttle) * friction;
	}

	void VehicleSim::FindGround(Wheel &w, const Vec3 &pos, const Quaternion &rot, const Vec3 &up, const Vec3 &forward)
	{
		const VehicleAxle &ax = Axle(w);
		const f32 r = Tyre(w).radius;
		const Vec3 mountWorld = pos + rot * w.mount;
		const f32 top = ax.bumpTravel + r;
		const f32 length = ax.bumpTravel + ax.droopTravel + 2.f * r + 0.05f;

		// Rays across the tyre's profile, and two across its tread: the wheel rests on whichever lifts it most. (A
		// tyre is a hand wide: the slit between a kerb and the gravel beside it, which one ray fell into, it rolls over.)
		static const f32 offsets[7] = { 0.f, -0.35f, 0.35f, -0.7f, 0.7f, 0.f, 0.f };
		static const f32 across[7] = { 0.f, 0.f, 0.f, 0.f, 0.f, -0.4f, 0.4f };
		const Vec3 side = up.cross(forward).normalize();
		const f32 tread = Tyre(w).width;
		f32 best = -1e9f;
		RayCastHit bestHit;
		for (int k = 0; k < 7; k++)
		{
			const f32 dx = offsets[k] * r;
			const Vec3 origin = mountWorld + up * top + forward * dx + side * (across[k] * tread);
			const RayCastHit hit = groundOnlySurfaces
				? physics->RayCastFiltered(origin, origin - up * length, [this](IPhysicsComponent* c) {
					for (size_t s = 0; s < surfaces.size(); s++) if (surfaces[s].body == c) return true;
					return false; })
				: physics->RayCastIgnoring(origin, origin - up * length, chassis.get());
			if (!hit.hasHit) continue;
			// (a steep face - a wall - is not something to roll up)
			if (hit.normal.dotProduct(up) < 0.35f) continue;
			const f32 zc = top - hit.distance + sqrtf(std::max(0.f, r * r - dx * dx));
			if (zc > best) { best = zc; bestHit = hit; }
		}

		w.contact = bestHit.hasHit && best >= -ax.droopTravel;
		w.surface = &defaultSurface;
		if (bestHit.hasHit)
			for (size_t i = 0; i < surfaces.size(); i++)
				if (surfaces[i].body == bestHit.component) { w.surface = &surfaces[i].surface; break; }
		if (w.contact)
		{
			const VehicleSurface &s = *w.surface;
			// The tyre's forces act at the middle of its contact patch, under the wheel's centre - not where the ray
			// that lifted it most happened to land. (That ray is the tread's left edge or its right one as the car
			// rolls a fraction of a degree on the road, a hand fore or aft over a ripple: under full braking 28 kN
			// stepped 24 cm sideways and back, twice a second - 3.4 kN m of yaw one way then the other. The car
			// weaved and rocked through every braking, the wheel held straight.)
			w.groundPoint = mountWorld + up * (best - r);
			if (s.bumpAmplitude > 0.f) best += s.bumpAmplitude * 0.5f * (1.f + sinf(w.bumpPhase));
			w.groundNormal = bestHit.normal;
			// Where the undeformed tyre would sit (suspension travel). Ground far above the bump stop - a wall's
			// top, a barrier - is the chassis's to hit: the tyre squashes at most a few cm, not a spring's throw.
			w.groundZ = std::min(best, ax.bumpTravel + 0.06f);
		}
		else
			w.groundZ = -1e9f;
	}

	void VehicleSim::Step(const f32 dt)
	{
		if (!physics || !chassis || dt <= 0.f) return;
		const IPhysics::BodyState st = physics->GetBodyState(chassis.get());
		if (!st.valid) return;

		const Quaternion rot = st.rotation;
		const Vec3 right = rot * Vec3(1.f, 0.f, 0.f);
		const Vec3 up = rot * Vec3(0.f, 1.f, 0.f);
		const Vec3 fwd = rot * Vec3(0.f, 0.f, -1.f);
		const Vec3 vCom = st.linearVelocity;
		const Vec3 omegaBody = st.angularVelocity;
		const Vec3 com = st.com;
		auto velocityAt = [&](const Vec3 &p) { return vCom + omegaBody.cross(p - com); };
		const f32 speed = vCom.dotProduct(fwd);

		// ---- steering --------------------------------------------------------
		const f32 halfWheelLock = 0.5f * std::max(0.1f, cfg.steeringWheelLock);
		const f32 swAngle = steerIsAngle ? Clamp(inSteerAngle, -halfWheelLock, halfWheelLock) : inSteer * halfWheelLock;
		const f32 delta = swAngle / halfWheelLock * cfg.steeringLock;
		f32 steerAngle[4] = { 0.f, 0.f, 0.f, 0.f };
		{
			const f32 L = fabsf(0.5f * (cfg.wheels[2].z + cfg.wheels[3].z) - 0.5f * (cfg.wheels[0].z + cfg.wheels[1].z));
			const f32 T = fabsf(cfg.wheels[1].x - cfg.wheels[0].x);
			const f32 a = fabsf(delta);
			f32 inner = a, outer = a;
			if (a > 1e-4f && L > 0.1f)
			{
				const f32 R = L / tanf(a);
				inner = Lerp(a, atanf(L / std::max(0.1f, R - 0.5f * T)), cfg.ackermann);
				outer = Lerp(a, atanf(L / (R + 0.5f * T)), cfg.ackermann);
			}
			const int steeredBase = cfg.front.steered || !cfg.rear.steered ? 0 : 2;
			// turning right (delta > 0): the right wheel is the inner one
			steerAngle[steeredBase + 0] = Sign(delta) * (delta > 0.f ? outer : inner);
			steerAngle[steeredBase + 1] = Sign(delta) * (delta > 0.f ? inner : outer);
			if (a <= 1e-4f) steerAngle[steeredBase] = steerAngle[steeredBase + 1] = delta;
			for (int i = 0; i < 4; i++) steerAngle[i] += wheels[i].dmgToe;     // (a bent arm)
		}

		// ---- ground under each wheel (the hub's own motion is in the sub-steps) ----
		f32 load[4] = { 0, 0, 0, 0 }, groundRate[4];
		for (int i = 0; i < 4; i++)
		{
			Wheel &w = wheels[i];
			w.bumpPhase += fabsf(speed) * dt / std::max(0.05f, w.surface ? w.surface->bumpWavelength : 1.f) * 2.f * kPi;
			if (w.bumpPhase > 1000.f) w.bumpPhase = fmodf(w.bumpPhase, 2.f * kPi);
			if (w.surface && w.surface->ffbRumble > 0.f)
				w.rumblePhase = fmodf(w.rumblePhase + std::min(fabsf(speed) / std::max(0.05f, w.surface->ffbRumbleWavelength), cfg.ffbRumbleMaxHz) * dt * 2.f * kPi, 2.f * kPi);
			const f32 before = w.groundZ;
			FindGround(w, st.position, rot, up, fwd);
			// (the steering geometry: the turned wheel's ground met that much higher or lower - the spring then lifts or
			// lowers that corner of the body, at a standstill as at speed)
			{
				const VehicleAxle &axj = Axle(w);
				if (w.contact && fabsf(steerAngle[i]) > 1e-4f && (axj.casterJack != 0.f || axj.kpiJack != 0.f))
					w.groundZ += axj.casterJack * sinf(steerAngle[i]) * (w.left ? 1.f : -1.f) + axj.kpiJack * (1.f - cosf(steerAngle[i]));
			}
			if (w.detached) w.contact = false;     // (gone: the corner rests on its hub - the body's own hull)
			groundRate[i] = (w.contact && before > -1e8f) ? Clamp((w.groundZ - before) / dt, -3.f, 3.f) : 0.f;
		}
		// the suspension's force on the body, from the hub's travel z (+ up) and speed
		auto suspensionForce = [&](const Wheel &w) {
			const VehicleAxle &ax = Axle(w);
			f32 f = w.preload + ax.springRate * w.z;
			const f32 stopAt = ax.bumpTravel - ax.bumpStopGap;
			if (w.z > stopAt) f += ax.bumpStopRate * (w.z - stopAt) * (1.f + 20.f * (w.z - stopAt)); // progressive
			const f32 v = w.zVel;
			const f32 slow = v > 0.f ? ax.bumpDamping : ax.reboundDamping;
			const f32 fast = v > 0.f ? ax.fastBumpDamping : ax.fastReboundDamping;
			const f32 av = fabsf(v);
			const f32 damp = av < ax.damperKnee ? slow * av : slow * ax.damperKnee + fast * (av - ax.damperKnee);
			f += Sign(v) * damp;
			f += ax.antiRollRate * (w.z - wheels[(&w - wheels) ^ 1].z);   // anti-roll bar
			return std::max(0.f, f);
		};

		// ---- driver aids, gearbox, clutch -------------------------------------
		f32 throttle = inThrottle;
		const f32 brake = inBrake;
		const f32 rpm = RadPerSecToRpm(engineOmega);
		bool downshifting = false;
		if (shiftTimer > 0.f)
		{
			shiftTimer -= dt;
			downshifting = shiftingDown;
			if (shiftTimer <= 0.f) { gear = nextGear; shiftTimer = 0.f; }
		}
		const bool shifting = shiftTimer > 0.f;
		const int drivenBase = cfg.front.driven && !cfg.rear.driven ? 0 : 2;
		Wheel &dl = wheels[drivenBase], &dr = wheels[drivenBase + 1];
		const f32 axleOmega = 0.5f * (dl.omega + dr.omega);

		if (shifting)
		{
			// up: ignition cut; down: blip to the rpm the next gear will want
			const f32 target = fabsf(axleOmega * GearRatio(nextGear));
			if (downshifting && autoBlip && engineOmega < target) throttle = 1.f;
			else if (!downshifting) throttle = 0.f;
		}
		if (rpm > cfg.engine.limiterRpm) limiterCut = true;
		else if (rpm < cfg.engine.limiterRpm - 120.f) limiterCut = false;
		telemetry.limiter = limiterCut;
		if (limiterCut) throttle = 0.f;
		if (engineRunning && rpm < cfg.engine.idleRpm)
			throttle = std::max(throttle, Clamp((cfg.engine.idleRpm - rpm) / 400.f, 0.f, 0.35f));

		// traction control: driven wheels spinning past the peak
		bool tcActive = false;
		if (cfg.tcLevel > 0 && gear != 0)
		{
			// (the level, 1 to 10: how much spin it lets go before it acts, how hard, and how far down - level 1 only a wheel
			// spinning away, 3.4 times the tyre's peak slip, never under 45 % of the throttle and cut slowly; level 10 a
			// little past the peak, 1.2 times, to the car's own floor at its own rate. They were 1.82 to 1.1 times, the
			// same floor and rate for all: "even on low TC it cuts too much")
			const f32 lv = (f32)(std::min(std::max(cfg.tcLevel, 1), 10) - 1) / 9.f;     // (0 at level 1, 1 at 10)
			const f32 limit = Tyre(dl).peakSlipRatio * (3.4f - 2.2f * lv);
			const f32 levelFloor = std::max(cfg.tcFloor, 0.45f - (0.45f - cfg.tcFloor) * lv);
			const f32 levelCut = cfg.tcCutRate * (0.35f + 0.65f * lv);
			// (the slip ratio read as the wheels' slip speed over 8 m/s below that: off the line it is huge for a little
			// wheelspin - (wr - v) / v at 2 m/s - and the control cut the engine to 5 % the moment the car rolled: no start)
			const f32 lowK = std::min(1.f, fabsf(speed) / std::max(0.1f, cfg.tcLowSpeed));
			const f32 slip = std::max(dl.contact ? dl.sx : 0.f, dr.contact ? dr.sx : 0.f) * lowK;
			// (cut by how far past the limit: a wheel a little over it a little, one spinning away at the full rate - cut at
			// the full rate for any slip past it, the car off a wet line had next to no power, again and again: "TC very
			// noticeable on start, the car has real problems to accelerate")
			if (slip > limit && fabsf(speed) > 1.f)
			{
				const f32 over = Clamp((slip - limit) / std::max(0.5f * limit, 1e-3f), 0.15f, 1.f);
				tcFactor = std::max(levelFloor, tcFactor - dt * levelCut * over); tcActive = true;
			}
			else tcFactor = std::min(1.f, tcFactor + dt * cfg.tcRecoverRate);
		}
		else tcFactor = 1.f;
		if (inThrottle > 0.f) throttle = std::min(throttle, std::max(throttle * tcFactor, 0.f));
		if (!ignition || !engineRunning) throttle = 0.f;

		const f32 G = GearRatio(gear);
		f32 engage = 1.f - inClutch;
		if (gear == 0 || shifting) engage = 0.f;
		else if (autoClutch)
		{
			// a standstill: the clutch slips in as the revs rise, never stalls it
			// (the wheels' speed as the gear sees it: under zero when the car rolls against its gear - backwards out of
			// a spin, in first. Taken as a size, that was "fast enough": the clutch went home, the wheels turned the
			// engine backwards to a stop and it stayed there, dead under a full throttle, until the car had rolled
			// to a halt - 25 s after a spin at Interlagos. Against the gear the clutch slips, as from a standstill)
			const f32 wheelSideRpm = RadPerSecToRpm(axleOmega * G);
			const f32 idle = cfg.engine.idleRpm;
			if (wheelSideRpm < idle * 1.1f)
			{
				const f32 bite = Clamp((rpm - idle * 0.92f) / std::max(100.f, cfg.drivetrain.launchRpm - idle * 0.92f), 0.f, 1.f);
				engage = std::min(engage, bite * bite);
				launching = true;
			}
			// leaving the line: a foot down in first, the wheels still turning slower than the revs the car leaves
			// from - the clutch slips as a driver's does, holding the engine in its torque until the car has caught
			// it up. (Let in whole at walking pace it dragged the engine to 2500 rpm, far under its torque: 0-100
			// in five seconds from a standing start)
			// (only on the way up from a standstill: a hairpin taken in first is driven with the clutch home)
			else if (launching && (gear == 1 || gear == -1) && inThrottle > 0.5f && wheelSideRpm < cfg.drivetrain.launchRpm)
			{
				const f32 L = cfg.drivetrain.launchRpm;
				engage = std::min(engage, Clamp((rpm - 0.75f * L) / (0.25f * L), 0.12f, 1.f));
			}
			else if (wheelSideRpm >= cfg.drivetrain.launchRpm || (gear != 1 && gear != -1)) launching = false;
		}
		clutchEngage = engage;
		const f32 clutchCap = cfg.drivetrain.clutchTorque * engage;

		// brakes and ABS
		// (the pressure in the lines follows the pedal in a few hundredths of a second, as the fluid and the pads
		// take it: a pedal stamped from nothing does not clamp the discs before the car's weight has come forward
		// onto the front tyres - which locked them for the first tenth of every braking)
		if (cfg.brakeRise > 0.f) brakeLine += (brake - brakeLine) * (1.f - expf(-dt / (brake > brakeLine ? cfg.brakeRise : 0.5f * cfg.brakeRise)));
		else brakeLine = brake;
		const f32 totalBrake = 2.f * (cfg.front.brakeTorque + cfg.rear.brakeTorque);
		f32 brakeTorque[4];
		for (int i = 0; i < 4; i++)
		{
			Wheel &w = wheels[i];
			const f32 share = w.front ? cfg.brakeBias : 1.f - cfg.brakeBias;
			brakeTorque[i] = brakeLine * totalBrake * share * 0.5f;
		}

		// ---- tyres, wheels and drivetrain, in sub-steps ------------------------
		struct Frame { Vec3 f, l, n, point; f32 vx, vy, mu, r, sigmaX, sigmaY; bool on; };
		Frame fr[4];
		for (int i = 0; i < 4; i++)
		{
			Wheel &w = wheels[i];
			const VehicleTyre &ty = Tyre(w);
			Frame &F = fr[i];
			F.on = w.contact;
			const f32 c = cosf(steerAngle[i]), s = sinf(steerAngle[i]);
			const Vec3 heading = fwd * c + right * s;
			F.n = w.contact ? w.groundNormal : up;
			Vec3 f = heading - F.n * F.n.dotProduct(heading);
			const f32 fl = f.magnitude();
			F.f = fl > 1e-5f ? f / fl : fwd;
			F.l = F.f.cross(F.n); // right
			F.point = w.contact ? w.groundPoint : (st.position + rot * w.mount - up * ty.radius);
			const Vec3 v = velocityAt(F.point);
			F.vx = v.dotProduct(F.f);
			F.vy = v.dotProduct(F.l);
			F.r = ty.radius;
			// grip: temperature x surface (x load sensitivity, per sub-step)
			const f32 lf = 1.f;
			const f32 T = 0.5f * (w.tempSurface + w.tempCore);
			// (over the optimum the grip goes more slowly than under it - 1.6 times the window: a few slides took the tyres past
			// 100 C and lost them as much as cold ones, "like butter")
			const f32 win = std::max(1.f, ty.temperatureWindow) * (T > ty.optimumTemperature ? ty.hotWindowFactor : 1.f);
			const f32 x = (T - ty.optimumTemperature) / win;
			// (x 0.96 at the optimum: the cars and the AI's speeds were tuned with the tyres held ~20 C under it in a race by a
			// heat balance that kept them cold - ~0.96 of their grip. Now warm, they keep that grip there, and lose it when
			// cold or too hot. 0.90 left the AI 3.5 s a lap slower at Spa, 0.93 1.5 s, 0.95 1 s)
			const f32 tf = ty.peakGripFactor * (1.f - ty.temperatureGripLoss * std::min(x * x, 2.5f));
			const f32 sg = w.surface ? w.surface->grip : 1.f;
			F.mu = ty.friction * lf * tf * sg * w.dmgGrip;
			w.state.grip = lf * tf * sg;
			F.sigmaX = ty.relaxationLongitudinal;
			F.sigmaY = ty.relaxationLateral;
		}

		const int N = std::max(1, cfg.substeps);
		const f32 h = dt / (f32)N;
		f32 sumFx[4] = { 0, 0, 0, 0 }, sumFy[4] = { 0, 0, 0, 0 }, sumMz[4] = { 0, 0, 0, 0 }, slipPower[4] = { 0, 0, 0, 0 };
		f32 sumSusp[4] = { 0, 0, 0, 0 }, sumLoad[4] = { 0, 0, 0, 0 };
		f32 engineTorqueSum = 0.f;
		bool absActive = false;
		for (int it = 0; it < N; it++)
		{
			// the wheel's own mass between its spring and its tyre: a kerb lifts the wheel, the
			// tyre squashes, the body feels it through the spring - never a step straight into the car
			f32 tyreLoad[4], susp[4], omega0[4], rolling[4];
			for (int i = 0; i < 4; i++) { susp[i] = suspensionForce(wheels[i]); omega0[i] = wheels[i].omega; rolling[i] = Tyre(wheels[i]).radius; }
			for (int i = 0; i < 4; i++)
			{
				Wheel &w = wheels[i];
				const VehicleAxle &ax = Axle(w);
				const VehicleTyre &ty = Tyre(w);
				const f32 groundZ = w.contact ? w.groundZ + groundRate[i] * h * (f32)it : -1e9f;
				const f32 squash = groundZ - w.z;
				f32 ft = 0.f;
				if (squash > 0.f) ft = std::max(0.f, ty.verticalStiffness * squash + ty.verticalDamping * (groundRate[i] - w.zVel));
				const f32 mu = std::max(5.f, ax.unsprungMass);
				// (its weight is in the body's mass already: here only its inertia, how it follows the road)
				w.zVel += h * (ft - susp[i]) / mu;
				w.zVel = Clamp(w.zVel, -6.f, 6.f);
				w.z += h * w.zVel;
				if (w.z < -ax.droopTravel) { w.z = -ax.droopTravel; if (w.zVel < 0.f) w.zVel = 0.f; }
				if (w.z > ax.bumpTravel + 0.01f) { w.z = ax.bumpTravel + 0.01f; if (w.zVel > 0.f) w.zVel = 0.f; }   // (solid: the chassis takes it)
				tyreLoad[i] = Clamp(ft, 0.f, 60000.f);
				sumSusp[i] += susp[i];
				sumLoad[i] += tyreLoad[i];
			}
			for (int i = 0; i < 4; i++)
			{
				Wheel &w = wheels[i];
				const VehicleTyre &ty = Tyre(w);
				const Frame &F = fr[i];
				const f32 Fz = tyreLoad[i];
				if (!F.on || Fz <= 0.f)
				{
					// in the air: the slip states unwind, the wheel spins on
					w.sx *= 0.9f;
					w.sy *= 0.9f;
					w.omega -= w.omega * 0.2f * h;
					continue;
				}
				const f32 avx = fabsf(F.vx);
				const f32 rLoaded = std::max(0.1f, F.r - Fz / std::max(1.f, ty.verticalStiffness));
				const f32 slipVx = w.omega * rLoaded - F.vx;
				rolling[i] = rLoaded;
				const f32 slipVy = F.vy;
				// transient slip (first-order relaxation, implicit in |vx|)
				w.sx = (w.sx + h * slipVx / F.sigmaX) / (1.f + h * avx / F.sigmaX);
				w.sy = (w.sy + h * slipVy / F.sigmaY) / (1.f + h * avx / F.sigmaY);
				w.sx = Clamp(w.sx, -3.f, 3.f);
				w.sy = Clamp(w.sy, -3.f, 3.f);

				const f32 nx = w.sx / ty.peakSlipRatio;
				const f32 ny = w.sy / tanf(ty.peakSlipAngle);
				const f32 rho = sqrtf(nx * nx + ny * ny);
				f32 Fx = 0.f, Fy = 0.f;
				const f32 lsens = Clamp(1.f - ty.loadSensitivity * (Fz / ty.nominalLoad - 1.f), 0.6f, 1.25f);
				const f32 Fmax = F.mu * lsens * Fz;
				if (rho > 1e-6f)
				{
					const f32 C = (ty.shapeLongitudinal * nx * nx + ty.shapeLateral * ny * ny) / (rho * rho);
					const f32 curve = SlipCurve(rho, C);
					Fx = Fmax * ty.longitudinalFriction * curve * nx / rho;
					Fy = -Fmax * curve * ny / rho;
				}
				// (what heats the tyre: the friction force sliding, not the damping below)
				const f32 slidingPower = fabsf(Fx * slipVx) + fabsf(Fy * slipVy);
				// low-speed damping: the relaxation states are springs at a standstill
				const f32 ls = Clamp(1.f - avx / 4.f, 0.f, 1.f);
				if (ls > 0.f)
				{
					// (critical-ish for the car on four tyre "springs": ~2.5 N s/m per N of load)
					const f32 c = 2.5f * Fz * ls;
					Fy -= c * slipVy;
					Fx += c * slipVx;
					// (and never more than the tyre's friction: "low speed" is the wheel's speed along its own heading,
					// and a car sliding sideways - a spin, at any speed - has none. The damping was then 2.5 times the
					// load for each m/s it slid: 25 times the car's weight at 10 m/s. It tripped over its own wheels,
					// was thrown up and rolled. All of the peak while the contact barely moves - the car held on a
					// slope - and what a sliding tyre gives once it slides)
					const f32 slide = sqrtf(slipVx * slipVx + slipVy * slipVy);
					const f32 lim = Fmax * Lerp(1.f, sinf(ty.shapeLateral * kPi * 0.5f), Clamp((slide - 0.5f) / 2.5f, 0.f, 1.f));
					const f32 Fm = sqrtf(Fx * Fx + Fy * Fy);
					if (Fm > lim) { Fx *= lim / Fm; Fy *= lim / Fm; }
				}
				sumFx[i] += Fx;
				sumFy[i] += Fy;
				// aligning moment: the lateral force behind the contact centre (pneumatic trail)
				const f32 trail = ty.pneumaticTrail * Clamp(1.f - fabsf(ny), -0.15f, 1.f);
				sumMz[i] += -trail * Fy;
				slipPower[i] += slidingPower;

				// the tyre's force turns the wheel back
				w.omega += h * (-Fx * rLoaded) / ty.inertia;
			}

			for (int i = 0; i < 4; i++) wheels[i].omega = Clamp(wheels[i].omega, -450.f, 450.f);
			// engine
			const f32 eRpm = RadPerSecToRpm(engineOmega);
			const f32 Te = (engineRunning && ignition) ? EngineTorque(eRpm, throttle) : -(cfg.engine.frictionTorque + cfg.engine.frictionPerRpm * eRpm);
			engineTorqueSum += Te;
			engineOmega += h * Te / cfg.engine.inertia;
			engineOmega = Clamp(engineOmega, 0.f, RpmToRadPerSec(cfg.engine.limiterRpm * 1.3f));

			// clutch: a friction impulse between the engine and the driven axle
			f32 torqueIn = 0.f;
			if (clutchCap > 0.f && G != 0.f)
			{
				const VehicleTyre &dty = Tyre(dl);
				const f32 Ia = 2.f * dty.inertia;
				const f32 aOmega = 0.5f * (dl.omega + dr.omega);
				const f32 slip = engineOmega - G * aOmega;
				const f32 m = 1.f / (1.f / cfg.engine.inertia + G * G / Ia);
				const f32 J = Clamp(slip * m, -clutchCap * h, clutchCap * h);
				engineOmega -= J / cfg.engine.inertia;
				const f32 Jw = G * J * (J * G > 0.f ? cfg.drivetrain.efficiency : 1.f);
				dl.omega += 0.5f * Jw / dty.inertia;
				dr.omega += 0.5f * Jw / dty.inertia;
				torqueIn = Jw / h;
			}
			// limited-slip differential: friction between the two driven wheels
			{
				const VehicleTyre &dty = Tyre(dl);
				const f32 lock = cfg.drivetrain.diffPreload + (torqueIn >= 0.f ? cfg.drivetrain.diffPowerRamp : cfg.drivetrain.diffCoastRamp) * fabsf(torqueIn);
				const f32 slip = dl.omega - dr.omega;
				const f32 m = 0.5f * dty.inertia;
				const f32 J = Clamp(slip * m, -lock * h, lock * h);
				dl.omega -= J / dty.inertia;
				dr.omega += J / dty.inertia;
			}
			// brakes: friction impulses towards a stopped wheel. ABS: a wheel the pedal would take past the slip
			// where its tyre grips most is held at that slip (VehicleConfig properties - tune from Lua)
			for (int i = 0; i < 4; i++)
			{
				Wheel &w = wheels[i];
				const VehicleTyre &ty = Tyre(w);
				const f32 I = ty.inertia;
				f32 bt = brakeTorque[i];
				if (cfg.absLevel > 0 && fabsf(speed) > cfg.absMinSpeed && brake > 0.05f && fr[i].on)
				{
					// (the torque that holds the wheel at the target is known: what turned it this sub-step - its tyre,
					// the drivetrain - less what brings it to the target's speed in absResponse s. A pressure that
					// was dumped and re-applied by the slip's error swung round the target several times a second,
					// each wheel on its own: the car shook under braking and braked on the average)
					const f32 target = ty.peakSlipRatio * (cfg.absSlipBase - cfg.absSlipPerLevel * (f32)std::min(cfg.absLevel, 10));
					const f32 dir = fr[i].vx < 0.f ? -1.f : 1.f;
					const f32 omegaT = fr[i].vx * (1.f - target) / rolling[i];
					const f32 turned = I * (w.omega - omega0[i]) / h;
					const f32 hold = dir * (turned - I * (omegaT - w.omega) / std::max(h, cfg.absResponse));
					const f32 cap = std::max(hold, bt * std::max(0.f, cfg.absMinFactor));
					if (cap < bt) { bt = cap; absActive = true; }
				}
				const f32 J = Clamp(w.omega * I, -bt * h, bt * h);
				w.omega -= J / I;
			}
		}

		// ---- forces on the chassis ---------------------------------------------
		f32 kingpin = 0.f;
		const f32 invN = 1.f / (f32)N;
		for (int i = 0; i < 4; i++)
		{
			Wheel &w = wheels[i];
			const VehicleAxle &ax = Axle(w);
			const VehicleTyre &ty = Tyre(w);
			const Frame &F = fr[i];
			const f32 Fx = sumFx[i] * invN, Fy = sumFy[i] * invN, Mz = sumMz[i] * invN;
			load[i] = sumLoad[i] * invN;
			const f32 suspensionAvg = sumSusp[i] * invN;
			VehicleWheelState &ws = w.state;
			ws.suspension = w.z;
			ws.steer = steerAngle[i];
			ws.angularVelocity = w.omega;
			ws.spin += w.omega * dt;
			if (ws.spin > 1000.f || ws.spin < -1000.f) ws.spin = fmodf(ws.spin, 2.f * kPi);
			ws.load = load[i];
			ws.contact = F.on && load[i] > 0.f;
			ws.surface = w.surface ? w.surface->id : 0;
			ws.contactPoint = F.point;
			ws.brakeTorque = brakeTorque[i];
			ws.slipRatio = w.sx;
			ws.slipAngle = atanf(w.sy);
			ws.longitudinalForce = Fx;
			ws.lateralForce = Fy;

			// suspension pushes the body up at the wheel
			const Vec3 centre = st.position + rot * w.mount + up * w.z;
			if (suspensionAvg > 0.f) physics->ApplyForceAtPoint(chassis.get(), up * suspensionAvg, centre);
			if (F.on)
			{
				Vec3 force = F.f * Fx + F.l * Fy;
				// rolling resistance and what a soft surface drags
				const VehicleSurface &s = w.surface ? *w.surface : defaultSurface;
				const f32 rr = (ty.rollingResistance + s.rollingResistance) * load[i] * Clamp(F.vx / 0.5f, -1.f, 1.f);
				force -= F.f * rr;
				// a soft surface (gravel, grass) digs in: a resistance like rolling resistance - a share of the
				// load, the same at any speed past a crawl (in proportion to speed it was ~9 g at 240 km/h)
				if (s.drag > 0.f)
				{
					const f32 vh = sqrtf(F.vx * F.vx + F.vy * F.vy);
					// (in full from 15 m/s, a share of it under that: from 3 m/s on, a GT3 in the gravel had more drag than its
					// rear tyres could push - 0.52 g against 0.25 - and never moved again: "like 0.0005 km/h". A car ploughing in
					// fast is stopped hard; one crawling can drive out)
					if (vh > 0.01f) force -= (F.f * F.vx + F.l * F.vy * 0.7f) * (s.drag * load[i] * std::min(1.f, vh / 15.f) / vh);
				}
				physics->ApplyForceAtPoint(chassis.get(), force, F.point + up * ax.rollCentreHeight);
				if (w.front) kingpin += Mz - (ax.mechanicalTrail) * Fy;
			}

			// tyre temperature: sliding heats the surface, the core follows, air and track cool
			{
				const f32 P = slipPower[i] * invN;
				const f32 airCool = (6.f + 1.4f * fabsf(F.vx)) * (w.tempSurface - ty.ambientTemperature);
				const f32 trackCool = F.on ? 8.f * (w.tempSurface - ty.trackTemperature) : 0.f;
				const f32 toCore = 60.f * (w.tempSurface - w.tempCore);
				// rolling flex (carcass) warms the core with load and speed
				const f32 flex = F.on ? 0.0045f * load[i] * fabsf(F.vx) : 0.f;
				// (0.45 of the sliding's work into the surface - 0.30 held a race at Spa at ~67 C mean, its optimum 90: the air
				// took it all; 0.70 ran the fronts' cores to 132 C in a race, grip gone, the laps 10 s slower)
				// (0.36 since: at 0.45 a few slides took them past 100 C - Duarte: "really easy to get them hot")
				w.tempSurface += dt * (ty.heatShare * P - airCool - trackCool - toCore) / std::max(1.f, ty.surfaceCapacity);
				// (the core's capacity 5000, not 14000: from cold - 30 C, no blankets - it took 10 minutes to reach 40; now about
				// two laps to its working heat. Where it settles is the same)
				w.tempCore += dt * (toCore + flex - 2.f * (w.tempCore - ty.ambientTemperature)) / std::max(1.f, ty.coreCapacity);
				ws.surfaceTemperature = w.tempSurface;
				ws.coreTemperature = w.tempCore;
			}
		}

		// ---- aero ---------------------------------------------------------------
		{
			const VehicleAero &a = cfg.aero;
			const f32 v2 = vCom.magnitudeSQR();
			const f32 u = std::max(0.f, speed);
			const f32 q = 0.5f * a.airDensity * u * u;
			const f32 down = q * a.liftArea;
			const Vec3 frontAxle = st.position + rot * ((cfg.wheels[0] + cfg.wheels[1]) * 0.5f);
			const Vec3 rearAxle = st.position + rot * ((cfg.wheels[2] + cfg.wheels[3]) * 0.5f);
			if (down > 0.f)
			{
				physics->ApplyForceAtPoint(chassis.get(), up * (-down * a.frontShare), frontAxle);
				physics->ApplyForceAtPoint(chassis.get(), up * (-down * (1.f - a.frontShare)), rearAxle);
			}
			if (v2 > 0.01f)
			{
				const f32 vm = sqrtf(v2);
				const Vec3 drag = vCom * (-0.5f * a.airDensity * a.dragArea * vm);
				physics->ApplyForceAtPoint(chassis.get(), drag, st.position + rot * Vec3(0.f, a.centreHeight, cfg.centreOfMass.z));
				telemetry.drag = drag.magnitude();
			}
			else telemetry.drag = 0.f;
			telemetry.downforce = down;
		}

		// ---- telemetry and force feedback ----------------------------------------
		telemetry.speed = speed;
		telemetry.rpm = RadPerSecToRpm(engineOmega);
		telemetry.gear = shifting ? nextGear : gear;
		telemetry.shifting = shifting;
		telemetry.throttle = throttle;
		telemetry.brake = brake;
		telemetry.clutch = 1.f - clutchEngage;
		telemetry.steerInput = swAngle / halfWheelLock;
		telemetry.steeringWheelAngle = swAngle;
		telemetry.absActive = absActive;
		telemetry.tcActive = tcActive;
		telemetry.engineTorque = engineTorqueSum * invN;
		telemetry.fuelCut = 1.f - tcFactor;
		if (hasLastVelocity)
		{
			const Vec3 acc = (vCom - lastVelocity) / dt;
			const Quaternion inv = Quaternion(rot.w, -rot.x, -rot.y, -rot.z);
			const Vec3 local = inv * acc;
			telemetry.acceleration = telemetry.acceleration.Lerp(local, Clamp(dt * 30.f, 0.f, 1.f));
		}
		lastVelocity = vCom;
		hasLastVelocity = true;
		telemetry.aligningTorque = kingpin;
		// road feel: one front wheel kicked up and not the other twists the rack
		const f32 road = Clamp(cfg.ffbRoadFeel * (wheels[0].zVel - wheels[1].zVel), -0.35f, 0.35f);
		// the surface's rumble under each front wheel that touches it (kerbs: what the hands feel of them)
		f32 rumble = 0.f;
		for (int i = 0; i < 2; i++)
		{
			const Wheel &w = wheels[i];
			if (!w.contact || !w.surface || w.surface->ffbRumble <= 0.f) continue;
			rumble += w.surface->ffbRumble * std::min(1.f, fabsf(speed) / 10.f) * sinf(w.rumblePhase);
		}
		rumble = Clamp(rumble, -0.5f, 0.5f);
		telemetry.ffbRumble = rumble;
		const f32 ffb = Clamp(kingpin / std::max(1.f, cfg.ffbFullTorque) + road + rumble, -1.5f, 1.5f);
		ffbSmoothed = Lerp(ffbSmoothed, ffb, Clamp(dt * 400.f, 0.f, 1.f));     // (light: the rumble passes)
		telemetry.ffb = Clamp(ffbSmoothed, -1.f, 1.f);
	}

}
