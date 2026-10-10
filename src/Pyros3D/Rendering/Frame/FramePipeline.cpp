//============================================================================
// Name        : FramePipeline.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : How a frame is spread over threads, for any application
//============================================================================

#include <Pyros3D/Rendering/Frame/FramePipeline.h>
#include <Pyros3D/Rendering/Device/IRenderDevice.h>
#include <Pyros3D/Rendering/Device/ThreadedRenderDevice.h>
#include <Pyros3D/GameObjects/GameObject.h>
#include <Pyros3D/SceneGraph/SceneGraph.h>
#include <Pyros3D/Assets/Renderable/Terrains/TerrainEditor.h>
#include <Pyros3D/Utils/Jobs/JobSystem.h>
#include <Pyros3D/Utils/Profiler/FrameProfiler.h>
#include <cstdlib>
#include <memory>

namespace p3d {

	namespace {
		std::shared_ptr<IRenderDevice> g_deviceItself;
		std::shared_ptr<ThreadedRenderDevice> g_deviceThread;
		bool g_splitWanted = false, g_splitOn = false, g_inFlight = false;
		void* g_logicStream = NULL;
		JobCounter* g_done = NULL;
		std::function<void()> g_after;
	}

	bool FramePipeline::StartDeviceThread()
	{
		if (g_deviceThread) return true;
		const char* asked = std::getenv("PYROS_DEVICE_THREAD");
		if (asked != NULL && asked[0] == '0') return false;
		if (!IsActiveRenderDeviceSet() || !GetActiveRenderDevice().IsVulkan()) return false;
		g_deviceItself = BorrowActiveRenderDevice();
		if (!g_deviceItself) return false;
		g_deviceThread = std::make_shared<ThreadedRenderDevice>(g_deviceItself);
		SetActiveRenderDevice(std::static_pointer_cast<IRenderDevice>(g_deviceThread));
		return true;
	}

	void FramePipeline::StopDeviceThread()
	{
		Finish();
		if (!g_deviceThread) return;
		g_deviceThread->Finish();
		SetActiveRenderDevice(g_deviceItself);
		g_deviceThread.reset();
		g_deviceItself.reset();
	}

	ThreadedRenderDevice* FramePipeline::DeviceThread() { return g_deviceThread.get(); }

	void FramePipeline::SetSplit(const bool wanted) { g_splitWanted = wanted; }
	bool FramePipeline::GetSplit() { return g_splitWanted; }
	bool FramePipeline::InFlight() { return g_inFlight; }

	// The frame another thread has been recording: waited for, and then what
	// follows its scene is done by this thread - before anything is taken for
	// the next.
	void FramePipeline::Finish()
	{
		if (!g_inFlight) return;
		{
			PYROS_PROFILE_SCOPE("Frame.Join");
			JobSystem::Instance().Wait(*g_done);
		}
		GetActiveRenderDevice().LeaveParallelStream(g_logicStream);
		g_logicStream = NULL;
		g_inFlight = false;
		GameObject::SetKeeping(false);
		GameObject::ReleaseKept();
		FrameProfiler::Instance().Counter("Frame.Split", 1.0);
		if (g_after)
		{
			const std::function<void()> after = g_after;
			g_after = nullptr;
			after();
		}
	}

	void FramePipeline::HandOver()
	{
		Finish();
		// (no frame is in flight here: the one place the setting may change)
		static const char* forced = std::getenv("PYROS_FRAME_SPLIT");
		const bool on = forced != NULL ? forced[0] == '2' : g_splitWanted;
		const bool copies = forced != NULL ? (forced[0] == '1' || forced[0] == '2') : g_splitWanted;
		// (switched on: the copies first, for one frame - nothing has been noted
		// for them yet - and only then the other thread)
		if (copies && !GameObject::DrawCopies()) { GameObject::SetDrawCopies(true); g_splitOn = false; }
		else
		{
			g_splitOn = on && copies;
			if (!copies) GameObject::SetDrawCopies(false);
		}
	}

	bool FramePipeline::Submit(SceneGraph* scene, const std::function<void()> &scenePass, const std::function<void()> &afterScene, const bool mayGoToAnotherThread)
	{
		if (g_splitOn && mayGoToAnotherThread && g_deviceThread && !g_inFlight && JobSystem::Instance().WorkerCount() > 0)
		{
			IRenderDevice* dev = &GetActiveRenderDevice();
			void* logic = dev->NewDetachedStream();
			if (logic != NULL)
			{
				if (g_done == NULL) g_done = new JobCounter();
				g_inFlight = true;
				g_logicStream = logic;
				g_after = afterScene;
				// (what the other thread reads of the scene, as it is now; and
				// whatever leaves the scene meanwhile is kept until the join)
				if (scene != NULL) TerrainEditor::TakeTilesForDraw(scene);
				GameObject::SetKeeping(true);
				JobSystem::Instance().Run([scenePass, dev, logic]() {
					TerrainEditor::SetDrawSide(true);
					scenePass();
					TerrainEditor::SetDrawSide(false);
					// (what logic asked of the device meanwhile is carried out here: after this frame's scene)
					dev->PlaceStream(logic);
				}, *g_done);
				dev->EnterParallelStream(logic);
				return true;
			}
		}
		scenePass();
		afterScene();
		return false;
	}

}
