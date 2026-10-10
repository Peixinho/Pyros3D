//============================================================================
// Name        : FramePipeline.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : How a frame is spread over threads, for any application
//============================================================================

#ifndef PYROS_FRAMEPIPELINE_H
#define	PYROS_FRAMEPIPELINE_H

#include <Pyros3D/Other/Export.h>
#include <Pyros3D/Other/Global.h>
#include <functional>

namespace p3d {

	class SceneGraph;
	class ThreadedRenderDevice;

	// What makes a frame cost less than the sum of its parts, in one place and
	// for whoever has a frame loop - a game's player, a demo, a tool:
	//
	//   * The render device on a thread of its own (StartDeviceThread): the
	//     application says what is to be drawn and goes on; that thread makes the
	//     graphics API's calls. It is also what lets a pass be recorded on every
	//     core (IRenderer::DrawPassOnEveryCore).
	//
	//   * The frame split (SetSplit): a frame's scene is recorded by another
	//     thread while the application is already running the next frame's
	//     logic. The loop then reads:
	//
	//         logic of this frame (events, physics, the scene's update, scripts)
	//         FramePipeline::HandOver();        // the frame before is finished here
	//         open the frame, PreRender the renderers
	//         FramePipeline::Submit(scene, scenePass, afterScene);
	//
	//     scenePass is what may run on the other thread (the renderer's
	//     RenderScene); afterScene is everything that follows it and closes the
	//     frame (effects, UI, EndFrame) - it runs on the calling thread, at once
	//     when the split is off and at the next HandOver/Finish when it is on.
	//     Finish() must also be called before anything that cannot bear a frame
	//     in flight: a resize, a scene being unloaded, shutting down.
	//
	// PYROS_DEVICE_THREAD=0 and PYROS_FRAME_SPLIT=0/1/2 (never, copies only, on)
	// override what the application asks for.
	class PYROS3D_API FramePipeline {
	public:
		static bool StartDeviceThread();
		static void StopDeviceThread();
		static ThreadedRenderDevice* DeviceThread();

		static void SetSplit(const bool wanted);
		static bool GetSplit();

		static bool InFlight();
		static void Finish();
		static void HandOver();
		// True if the scene's passes were given to another thread (afterScene is then still to come).
		static bool Submit(SceneGraph* scene, const std::function<void()> &scenePass, const std::function<void()> &afterScene, const bool mayGoToAnotherThread = true);
	};

}

#endif	/* PYROS_FRAMEPIPELINE_H */
