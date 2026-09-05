//============================================================================
// Name        : FrameProfiler.h
// Author      : Duarte Peixinho
// Description : Lightweight CPU frame profiler with optional ImGui UI.
//               Usable from any app that links PyrosEngine (ImGui is already
//               part of the engine). Mark scopes with PYROS_PROFILE_SCOPE /
//               Begin/End, call BeginFrame/EndFrame once per frame, and
//               DrawImGui() during your ImGui pass.
//============================================================================

#ifndef FRAMEPROFILER_H
#define FRAMEPROFILER_H

#include <Pyros3D/Other/Export.h>
#include <Pyros3D/Other/Global.h>
#include <Pyros3D/Core/Math/Math.h>
#include <chrono>
#include <cstring>
#include <string>
#include <vector>

namespace p3d {

	class PYROS3D_API FrameProfiler
	{
	public:
		static const uint32 kHistorySize = 240;
		static const uint32 kMaxScopes = 64;
		static const uint32 kMaxNameLen = 48;

		struct ScopeRecord
		{
			char name[kMaxNameLen];
			f64 ms;
			uint32 depth;
			ScopeRecord() : ms(0.0), depth(0) { name[0] = 0; }
		};

		static FrameProfiler &Instance();

		void SetEnabled(const bool enabled) { enabled_ = enabled; }
		bool IsEnabled() const { return enabled_; }
		void Toggle() { enabled_ = !enabled_; }

		// Call once at the start / end of each frame (outside nested scopes).
		void BeginFrame();
		void EndFrame();

		void Begin(const char *name);
		void End();

		// RAII helper — safe no-op when disabled.
		struct Scope
		{
			explicit Scope(const char *name) { FrameProfiler::Instance().Begin(name); }
			~Scope() { FrameProfiler::Instance().End(); }
		};

		// The published snapshot, in recording order (children before parents,
		// see EndFrame). Same data DrawImGui() renders - exposed so a caller
		// with no ImGui (an out-of-process agent asking the editor what the
		// last frame cost) can read the breakdown too, following the
		// Count()/At() shape LOG::_LOG already uses for its ring buffer.
		uint32 ScopeCount() const { return displayScopeCount_; }
		const ScopeRecord &ScopeAt(const uint32 i) const { return displayScopes_[i]; }

		f64 LastFrameMs() const { return displayFrameMs_; }
		f64 AverageFrameMs() const { return avgFrameMs_; }
		f32 AverageFps() const { return avgFrameMs_ > 0.0 ? (f32)(1000.0 / avgFrameMs_) : 0.f; }

		// Draws the *previous completed* frame (safe to call mid-frame,
		// e.g. during ImGui prepare before RenderScene runs).
		void DrawImGui(bool *p_open = NULL);

	private:
		FrameProfiler();

		typedef std::chrono::steady_clock Clock;
		typedef Clock::time_point TimePoint;

		struct OpenScope
		{
			char name[kMaxNameLen];
			TimePoint start;
			uint32 depth;
		};

		bool enabled_;
		bool windowOpen_;
		TimePoint frameStart_;
		std::vector<OpenScope> stack_;

		// Written during the current frame; published to display* in EndFrame.
		ScopeRecord recordingScopes_[kMaxScopes];
		uint32 recordingScopeCount_;
		f64 recordingFrameMs_;

		ScopeRecord displayScopes_[kMaxScopes];
		uint32 displayScopeCount_;
		f64 displayFrameMs_;

		f64 historyMs_[kHistorySize];
		uint32 historyWrite_;
		uint32 historyCount_;
		f64 avgFrameMs_;
		f64 minFrameMs_;
		f64 maxFrameMs_;

		static void CopyName(char *dst, const char *src);
	};

}

#define PYROS_PROFILE_SCOPE(name) ::p3d::FrameProfiler::Scope _pyros_profile_scope_##__LINE__(name)

#endif /* FRAMEPROFILER_H */
