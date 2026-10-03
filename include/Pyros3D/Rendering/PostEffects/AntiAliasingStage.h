//============================================================================
// Name        : AntiAliasingStage.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : What PostEffectsManager runs for the anti-aliasing setting.
//============================================================================

#ifndef ANTIALIASINGSTAGE_H
#define ANTIALIASINGSTAGE_H

#include <Pyros3D/Rendering/PostEffects/AntiAliasing.h>
#include <Pyros3D/Rendering/PostEffects/Effects/IEffect.h>
#include <Pyros3D/Rendering/Device/IRenderDevice.h>
#include <vector>

namespace p3d {

	class FXAAEffect;
	class SMAAEdgeEffect;
	class SMAAWeightEffect;
	class SMAABlendEffect;
	class TAAResolveEffect;
	class MSDepthResolveEffect;

	// Not an entry in a scene's post-effect chain, because the modes act at
	// different points of the frame and none of them is a chain position:
	//
	//  - MSAA swaps the capture target for a multisample one and resolves it
	//    in EndCapture(), before any effect reads the frame.
	//  - TAA runs first, on the HDR frame, so bloom, depth of field and
	//    motion blur work on a stable image instead of a jittering one.
	//  - FXAA and SMAA run last, on the finished image.
	//
	// PostEffectsManager asks this for the pieces at each of those points.
	class PYROS3D_API AntiAliasingStage {

	public:

		AntiAliasingStage(const uint32 width, const uint32 height);
		~AntiAliasingStage();

		// Takes effect at the next Apply(). The effective mode is decided
		// here (see AntiAliasing::Resolve), so callers can report it at once.
		void Request(const AntiAliasingMode mode, const bool deferred);
		AntiAliasingMode GetRequested() const { return requested; }
		AntiAliasingMode GetEffective() const { return effective; }
		bool IsPending() const { return pending; }
		// Whether the pending or current mode reads the velocity map - the
		// manager has to own one before Apply() gets it.
		bool WantsVelocity() const { return effective == AntiAliasingMode::TAA; }

		// Builds what the effective mode needs and frees the rest. velocity
		// and velocityDepth are only read for TAA; captureDepth is the
		// single-sample depth an MSAA resolve writes into.
		void Apply(Texture* velocity, Texture* velocityDepth, Texture* captureDepth);
		void Resize(const uint32 width, const uint32 height);

		// The manager's capture FBO is told to preserve depth across binds
		// by its owner; the multisample one has to follow suit.
		void SetPreserveDepth(const bool preserve);

		bool IsMSAA() const { return msFBO != NULL; }
		// Passes that produce a new image (TAA, FXAA, SMAA). MSAA does not
		// count - its result is the capture's own colour texture.
		bool HasPasses() const;

		FrameBuffer* GetMultisampleFrameBuffer() { return msFBO; }
		// Resolves the multisample capture into `target` (the manager's
		// single-sample capture FBO). Depth only when something reads it.
		void ResolveMultisample(FrameBuffer* target, const bool withDepth, const DeviceHandle fullscreenVao);

		// This frame's TAA half with its parameters set, or NULL when TAA is
		// off. viewProjection is unjittered.
		IEffect* PrepareTAA(const Matrix &viewProjection, const bool haveViewProjection);
		// FXAA or SMAA's passes, in order. Their input is set separately
		// because it is only known once everything before them has run.
		void AppendFinalPasses(std::vector<IEffect*> &out);
		void SetFinalInput(Texture* input);
		// Copies LastRTT to the target - for when nothing else would.
		IEffect* GetCopyPass();

		// Projection offset for this frame, in NDC. Zero unless TAA is on.
		Vec2 GetJitter() const;
		// Advances the jitter and swaps the TAA halves. Once per frame,
		// after the passes ran.
		void EndFrame(const bool ranTAA);
		// The next TAA frame ignores its history. A scene load or camera cut
		// would otherwise ghost the old view into the new one.
		void ResetHistory() { historyValid = false; }

	private:

		void DestroyResources();
		void DrawDepthResolve(const DeviceHandle fullscreenVao);

		uint32 Width, Height;
		AntiAliasingMode requested, effective;
		bool deferred, pending, preserveDepth;

		// MSAA
		Texture *msColor, *msDepth;
		FrameBuffer* msFBO;
		MSDepthResolveEffect* depthResolve;

		// TAA
		TAAResolveEffect* taa[2];
		uint32 taaCurrent;
		bool historyValid;
		uint32 frameIndex;
		Matrix prevViewProjection;
		bool havePrevViewProjection;

		// FXAA / SMAA
		FXAAEffect* fxaa;
		SMAAEdgeEffect* smaaEdge;
		SMAAWeightEffect* smaaWeight;
		SMAABlendEffect* smaaBlend;
		Texture *areaTex, *searchTex;

		IEffect* copyPass;
	};

}

#endif /* ANTIALIASINGSTAGE_H */
