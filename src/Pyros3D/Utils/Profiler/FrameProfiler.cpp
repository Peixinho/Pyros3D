//============================================================================
// Name        : FrameProfiler.cpp
//============================================================================

#include <cstring>
#include <string>
#include <map>
#include <Pyros3D/Utils/Profiler/FrameProfiler.h>
#include "imgui.h"
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <thread>

namespace p3d {

	// The scope stack is one per profiler, not per thread, so only the thread
	// that runs the frame may push onto it. Scopes opened from job workers
	// are dropped rather than interleaved into the frame's stack.
	static std::atomic<std::thread::id> gFrameThread;

	static bool OnFrameThread()
	{
		return std::this_thread::get_id() == gFrameThread.load(std::memory_order_relaxed);
	}

	// PYROS_PROFILE_LOG=<path> appends the frame breakdown every 30 frames,
	// for apps with no UI or socket to read it from (DemoLauncher, a built
	// game). "-" writes to stderr.
	static FILE* ProfileLogFile()
	{
		static bool opened = false;
		static FILE* f = NULL;
		if (!opened)
		{
			opened = true;
			if (const char* path = std::getenv("PYROS_PROFILE_LOG"))
				f = (path[0] == '-' && path[1] == 0) ? stderr : std::fopen(path, "a");
		}
		return f;
	}

	// Slow frames, one line each, for a build on a machine nobody can attach to:
	// see FrameProfiler::LogSlowFrames.
	static FILE* gSlowLog = NULL;
	static f64 gSlowMs = 0.0;

	void FrameProfiler::LogSlowFrames(const char* path, const f64 thresholdMs)
	{
		if (gSlowLog && gSlowLog != stderr) std::fclose(gSlowLog);
		gSlowLog = NULL;
		gSlowMs = thresholdMs;
		if (!path || !path[0] || thresholdMs <= 0.0) return;
		gSlowLog = std::fopen(path, "w");
		if (gSlowLog)
		{
			std::fprintf(gSlowLog, "# frames slower than %.0f ms: time since start, the wall time the frame took, then every scope of 1 ms or more\n", thresholdMs);
			std::fflush(gSlowLog);
		}
	}

	namespace {
		// (microseconds, so atomics of an integer: the device may tell from its own thread)
		std::atomic<int64_t> g_displayWaitUs(0);
		// (the waits of any thread but the frame's - gFrameThread - are the device thread's)
		std::atomic<int64_t> g_otherWaitUs(0), g_deviceWorkMaxUs(-1);
		std::atomic<int64_t> g_presentedLastUs(0), g_presentedMaxUs(0), g_presentedCount(0);
	}
	void FrameProfiler::NoteDisplayWait(const f64 ms)
	{
		if (ms <= 0.0) return;
		if (std::this_thread::get_id() == gFrameThread.load(std::memory_order_relaxed)) g_displayWaitUs.fetch_add((int64_t)(ms * 1000.0));
		else g_otherWaitUs.fetch_add((int64_t)(ms * 1000.0));
	}
	void FrameProfiler::NoteDeviceFrame(const f64 ms)
	{
		int64_t us = (int64_t)(ms * 1000.0) - g_otherWaitUs.exchange(0);
		if (us < 0) us = 0;
		int64_t m = g_deviceWorkMaxUs.load();
		while (us > m && !g_deviceWorkMaxUs.compare_exchange_weak(m, us)) {}
	}
	void FrameProfiler::NotePresented(const f64 intervalMs)
	{
		const int64_t us = (int64_t)(intervalMs * 1000.0);
		g_presentedLastUs.store(us);
		int64_t m = g_presentedMaxUs.load();
		while (us > m && !g_presentedMaxUs.compare_exchange_weak(m, us)) {}
		g_presentedCount.fetch_add(1);
	}

	FrameProfiler &FrameProfiler::Instance()
	{
		static FrameProfiler inst;
		return inst;
	}

	FrameProfiler::FrameProfiler()
		: enabled_(true), windowOpen_(true), recordingScopeCount_(0), recordingFrameMs_(0.0),
		  displayScopeCount_(0), displayFrameMs_(0.0),
		  historyWrite_(0), historyCount_(0),
		  avgFrameMs_(0.0), minFrameMs_(0.0), maxFrameMs_(0.0)
	{
		for (uint32 i = 0; i < kHistorySize; i++)
			historyMs_[i] = 0.0;
	}

	void FrameProfiler::CopyName(char *dst, const char *src)
	{
		if (!src) src = "";
		std::snprintf(dst, kMaxNameLen, "%s", src);
	}

	void FrameProfiler::BeginFrame()
	{
		gFrameThread.store(std::this_thread::get_id(), std::memory_order_relaxed);
		// PYROS_SLOW_FRAMES=<ms>[:file]: any application logs its slow frames (as logSlowFrames does
		// from a script) - stderr unless a file is named.
		static bool slowFromEnv = false;
		if (!slowFromEnv)
		{
			slowFromEnv = true;
			if (const char* e = std::getenv("PYROS_SLOW_FRAMES"))
			{
				const f64 ms = std::atof(e);
				const char* colon = std::strchr(e, ':');
				if (ms > 0.0) { if (colon && colon[1]) LogSlowFrames(colon + 1, ms); else { gSlowMs = ms; gSlowLog = stderr; } }
			}
		}
		if (ProfileLogFile() || gSlowLog) enabled_ = true;
		if (!enabled_) return;
		stack_.clear();
		recordingScopeCount_ = 0;
		frameStart_ = Clock::now();
	}

	namespace {
		// Scopes opened by a thread that is not the frame's own (a frame's scene
		// recorded by another thread): not timed here, but named - the GPU's time
		// is told apart by the scope that was open when its work was recorded.
		thread_local const char* t_otherScopes[16];
		thread_local uint32 t_otherDepth = 0;
	}
	void FrameProfiler::Begin(const char *name)
	{
		if (!OnFrameThread()) { if (t_otherDepth < 16) t_otherScopes[t_otherDepth] = name; t_otherDepth++; return; }
		if (!enabled_) return;
		OpenScope s;
		CopyName(s.name, name);
		s.start = Clock::now();
		s.depth = (uint32)stack_.size();
		stack_.push_back(s);
	}

	const char *FrameProfiler::CurrentScopeName() const
	{
		if (!OnFrameThread()) return (t_otherDepth > 0 && t_otherDepth <= 16) ? t_otherScopes[t_otherDepth - 1] : "";
		if (!enabled_ || stack_.empty()) return "";
		return stack_.back().name;
	}

	void FrameProfiler::SetGpuTimings(const ScopeRecord *records, uint32 count)
	{
		if (count > kMaxScopes) count = kMaxScopes;
		for (uint32 i = 0; i < count; i++)
			gpu_[i] = records[i];
		gpuCount_ = count;
	}

	void FrameProfiler::Counter(const char *name, f64 value)
	{
		if (!OnFrameThread() || !enabled_) return;
		for (uint32 i = 0; i < counterCount_; i++)
			if (std::strncmp(counters_[i].name, name, kMaxNameLen) == 0) { counters_[i].ms = value; return; }
		if (counterCount_ < kMaxCounters)
		{
			CopyName(counters_[counterCount_].name, name);
			counters_[counterCount_++].ms = value;
		}
	}

	void FrameProfiler::End()
	{
		if (!OnFrameThread()) { if (t_otherDepth > 0) t_otherDepth--; return; }
		if (!enabled_ || stack_.empty()) return;
		OpenScope s = stack_.back();
		stack_.pop_back();
		const f64 ms = std::chrono::duration<f64, std::milli>(Clock::now() - s.start).count();
		if (recordingScopeCount_ < kMaxScopes)
		{
			ScopeRecord &r = recordingScopes_[recordingScopeCount_++];
			CopyName(r.name, s.name);
			r.ms = ms;
			r.depth = s.depth;
		}
	}

	void FrameProfiler::EndFrame()
	{
		if (!enabled_) return;
		while (!stack_.empty())
			End();

		recordingFrameMs_ = std::chrono::duration<f64, std::milli>(Clock::now() - frameStart_).count();

		// Publish a stable snapshot for DrawImGui (often called mid next frame).
		displayFrameMs_ = recordingFrameMs_;
		displayScopeCount_ = recordingScopeCount_;
		for (uint32 i = 0; i < displayScopeCount_; i++)
			displayScopes_[i] = recordingScopes_[i];

		// The frame's work: its time less what it spent waiting for the display.
		const f64 waited = (f64)g_displayWaitUs.exchange(0) / 1000.0;
		displayWorkMs_ = displayFrameMs_ - waited > 0.0 ? displayFrameMs_ - waited : 0.0;
		// (and the device thread's, the most of any frame it finished since: -1, none)
		{
			const int64_t d = g_deviceWorkMaxUs.exchange(-1);
			displayDeviceWorkMs_ = d >= 0 ? (f64)d / 1000.0 : 0.0;
		}
		// The graph's work line is whichever thread's was longer: a hitch on either is seen.
		workHistoryMs_[historyWrite_] = displayWorkMs_ > displayDeviceWorkMs_ ? displayWorkMs_ : displayDeviceWorkMs_;
		historyMs_[historyWrite_] = displayFrameMs_;
		historyWrite_ = (historyWrite_ + 1) % kHistorySize;
		if (historyCount_ < kHistorySize) historyCount_++;

		f64 sum = 0.0;
		minFrameMs_ = displayFrameMs_;
		maxFrameMs_ = displayFrameMs_;
		for (uint32 i = 0; i < historyCount_; i++)
		{
			const f64 v = historyMs_[i];
			sum += v;
			if (v < minFrameMs_) minFrameMs_ = v;
			if (v > maxFrameMs_) maxFrameMs_ = v;
		}
		avgFrameMs_ = historyCount_ > 0 ? sum / (f64)historyCount_ : 0.0;
		{
			f64 ws = 0.0; maxWorkMs_ = 0.0;
			for (uint32 i = 0; i < historyCount_; i++) { ws += workHistoryMs_[i]; if (workHistoryMs_[i] > maxWorkMs_) maxWorkMs_ = workHistoryMs_[i]; }
			avgWorkMs_ = historyCount_ > 0 ? ws / (f64)historyCount_ : 0.0;
		}

		if (gSlowLog)
		{
			// The wall time from one frame's end to the next: what the player
			// feels, and it holds whatever no scope covers (the present, the
			// event pump, a stall in the driver).
			static Clock::time_point start = Clock::now(), last = Clock::now(), lastSummary = Clock::now();
			static uint32 frames = 0, slow = 0;
			static f64 worst = 0.0;
			const Clock::time_point now = Clock::now();
			const f64 wall = std::chrono::duration<f64, std::milli>(now - last).count();
			last = now;
			frames++;
			// (slow by its work, not by its wait for the display: on Metal every frame that waited
			// a second refresh with frames already queued was logged, and nothing was seen on screen)
			const f64 mainWork = wall - waited > 0.0 ? wall - waited : 0.0;
			const f64 work = mainWork > displayDeviceWorkMs_ ? mainWork : displayDeviceWorkMs_;
			if (work > worst) worst = work;
			// every frame's scopes and counters, summed: the summary gives their
			// averages, which is what says where a steady low frame rate goes
			static std::map<std::string, f64> sums;
			for (uint32 i = 0; i < displayScopeCount_; i++) sums[displayScopes_[i].name] += displayScopes_[i].ms;
			for (uint32 i = 0; i < counterCount_; i++) sums[std::string("#") + counters_[i].name] += counters_[i].ms;
			// (and what the GPU took for each pass, where the device times them)
			for (uint32 i = 0; i < gpuCount_; i++) sums[std::string("gpu:") + gpu_[i].name] += gpu_[i].ms;
			// (all but the very first frame, timed from before anything ran - not the first of every
			// summary's ten seconds, which kept a 205 ms frame out of the log and in the summary)
			static bool first = true;
			const bool wasFirst = first; first = false;
			if (work >= gSlowMs && !wasFirst)
			{
				slow++;
				std::fprintf(gSlowLog, "t=%.2f wall=%.1f waited=%.1f work=%.1f deviceWork=%.1f scoped=%.1f", std::chrono::duration<f64>(now - start).count(), wall, waited, mainWork, displayDeviceWorkMs_, displayFrameMs_);
				for (uint32 i = 0; i < displayScopeCount_; i++)
					if (displayScopes_[i].ms >= 1.0) std::fprintf(gSlowLog, " %s=%.1f", displayScopes_[i].name, displayScopes_[i].ms);
				for (uint32 i = 0; i < counterCount_; i++)
					std::fprintf(gSlowLog, " %s=%.0f", counters_[i].name, counters_[i].ms);
				std::fprintf(gSlowLog, "\n");
			}
			const f64 since = std::chrono::duration<f64>(now - lastSummary).count();
			if (since >= 10.0)
			{
				std::fprintf(gSlowLog, "# t=%.0f: %u frames in %.0f s (%.0f fps), %u slow, worst work %.1f ms\n",
					std::chrono::duration<f64>(now - start).count(), frames, since, (f64)frames / since, slow, worst);
				std::fprintf(gSlowLog, "#   average a frame (ms; # is a counter):");
				for (std::map<std::string, f64>::const_iterator i = sums.begin(); i != sums.end(); ++i)
					if (i->second / (f64)frames >= 0.2) std::fprintf(gSlowLog, " %s=%.2f", i->first.c_str(), i->second / (f64)frames);
				std::fprintf(gSlowLog, "\n");
				sums.clear();
				std::fflush(gSlowLog);
				lastSummary = now; frames = 0; slow = 0; worst = 0.0;
			}
		}

		if (FILE* log = ProfileLogFile())
		{
			static uint32 frame = 0;
			static const Clock::time_point start = Clock::now();
			if (++frame % 30 == 0)
			{
				std::fprintf(log, "frame=%u t=%.2f ms=%.3f", frame,
					std::chrono::duration<f64>(Clock::now() - start).count(), displayFrameMs_);
				for (uint32 i = 0; i < displayScopeCount_; i++)
					std::fprintf(log, " %s=%.3f", displayScopes_[i].name, displayScopes_[i].ms);
				for (uint32 i = 0; i < counterCount_; i++)
					std::fprintf(log, " %s=%.0f", counters_[i].name, counters_[i].ms);
				for (uint32 i = 0; i < gpuCount_; i++)
					std::fprintf(log, " gpu:%s=%.3f", gpu_[i].name, gpu_[i].ms);
				std::fprintf(log, "\n");
				std::fflush(log);
			}
		}
	}

	void FrameProfiler::DrawImGui(bool *p_open)
	{
		if (ImGui::GetCurrentContext() == NULL)
			return;

		if (p_open && !*p_open)
			return;

		bool *openPtr = p_open ? p_open : &windowOpen_;
		if (!ImGui::Begin("Profiler", openPtr))
		{
			ImGui::End();
			return;
		}

		ImGui::Checkbox("Enabled", &enabled_);
		ImGui::SameLine();
		if (ImGui::Button("Clear history"))
		{
			historyCount_ = 0;
			historyWrite_ = 0;
		}

		ImGui::Separator();
		ImGui::Text("FPS  %.1f", AverageFps());
		ImGui::Text("Frame  %.3f ms  (avg %.3f  min %.3f  max %.3f)",
			displayFrameMs_, avgFrameMs_, minFrameMs_, maxFrameMs_);
		// What is not waiting for the display: the number to watch for hitches. (A frame that
		// waited a second refresh shows as a frame spike above and nothing here.)
		ImGui::Text("Work   %.3f ms  (avg %.3f  max %.3f)", displayWorkMs_, avgWorkMs_, maxWorkMs_);
		if (displayDeviceWorkMs_ > 0.0) { ImGui::SameLine(); ImGui::TextDisabled("device thread %.3f ms", displayDeviceWorkMs_); }
		if (g_presentedCount.load() > 0)
		{
			ImGui::Text("On screen  %.2f ms between frames  (worst %.2f)", (f64)g_presentedLastUs.load() / 1000.0, (f64)g_presentedMaxUs.load() / 1000.0);
			ImGui::SameLine();
			if (ImGui::SmallButton("reset##presented")) g_presentedMaxUs.store(0);
		}
		ImGui::TextDisabled("Scopes = previous completed frame");

		if (historyCount_ > 1)
		{
			float samples[kHistorySize];
			f32 peak = 1.f;
			for (uint32 i = 0; i < historyCount_; i++)
			{
				const uint32 idx = (historyWrite_ + kHistorySize - historyCount_ + i) % kHistorySize;
				samples[i] = (float)historyMs_[idx];
				if (samples[i] > peak) peak = samples[i];
			}
			char overlay[64];
			std::snprintf(overlay, sizeof(overlay), "%.2f ms", displayFrameMs_);
			ImGui::PlotLines("##frame_hist", samples, (int)historyCount_, 0, overlay, 0.f, peak * 1.1f, ImVec2(-1, 60));
			for (uint32 i = 0; i < historyCount_; i++) samples[i] = (float)workHistoryMs_[(historyWrite_ + kHistorySize - historyCount_ + i) % kHistorySize];
			std::snprintf(overlay, sizeof(overlay), "work %.2f ms", displayWorkMs_);
			ImGui::PlotLines("##work_hist", samples, (int)historyCount_, 0, overlay, 0.f, peak * 1.1f, ImVec2(-1, 60));
		}

		ImGui::Separator();
		ImGui::TextUnformatted("Scopes");
		ImGui::TextDisabled("ms = inclusive  |  self / %% = exclusive (children removed)");
		if (ImGui::BeginTable("scopes", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
		{
			ImGui::TableSetupColumn("Name");
			ImGui::TableSetupColumn("ms", ImGuiTableColumnFlags_WidthFixed, 64.f);
			ImGui::TableSetupColumn("self", ImGuiTableColumnFlags_WidthFixed, 64.f);
			ImGui::TableSetupColumn("%", ImGuiTableColumnFlags_WidthFixed, 44.f);
			ImGui::TableHeadersRow();

			// Scopes are recorded on End() → children appear before parents
			// (post-order). Exclusive = inclusive − sum of direct children.
			const f64 denom = displayFrameMs_ > 0.0 ? displayFrameMs_ : 1.0;
			for (uint32 i = 0; i < displayScopeCount_; i++)
			{
				const ScopeRecord &r = displayScopes_[i];
				f64 childSum = 0.0;
				for (int j = (int)i - 1; j >= 0 && displayScopes_[j].depth > r.depth; --j)
				{
					if (displayScopes_[j].depth == r.depth + 1)
						childSum += displayScopes_[j].ms;
				}
				f64 selfMs = r.ms - childSum;
				if (selfMs < 0.0) selfMs = 0.0;

				ImGui::TableNextRow();
				ImGui::TableSetColumnIndex(0);
				if (r.depth > 0)
				{
					ImGui::Dummy(ImVec2((float)r.depth * 10.f, 0.f));
					ImGui::SameLine();
				}
				ImGui::TextUnformatted(r.name);
				ImGui::TableSetColumnIndex(1);
				ImGui::Text("%.3f", r.ms);
				ImGui::TableSetColumnIndex(2);
				ImGui::Text("%.3f", selfMs);
				ImGui::TableSetColumnIndex(3);
				ImGui::Text("%.0f%%", (selfMs / denom) * 100.0);
			}
			ImGui::EndTable();
		}

		if (gpuCount_ > 0)
		{
			ImGui::Separator();
			ImGui::TextUnformatted("GPU");
			ImGui::TextDisabled("render passes, summed per CPU scope that began them (a few frames old)");
			if (ImGui::BeginTable("gpu", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp))
			{
				ImGui::TableSetupColumn("Pass");
				ImGui::TableSetupColumn("ms", ImGuiTableColumnFlags_WidthFixed, 64.f);
				ImGui::TableHeadersRow();
				for (uint32 i = 0; i < gpuCount_; i++)
				{
					ImGui::TableNextRow();
					ImGui::TableSetColumnIndex(0);
					ImGui::TextUnformatted(gpu_[i].name);
					ImGui::TableSetColumnIndex(1);
					ImGui::Text("%.3f", gpu_[i].ms);
				}
				ImGui::EndTable();
			}
		}

		ImGui::End();
	}

}
