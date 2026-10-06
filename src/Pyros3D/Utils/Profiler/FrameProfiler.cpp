//============================================================================
// Name        : FrameProfiler.cpp
//============================================================================

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
		if (ProfileLogFile() || gSlowLog) enabled_ = true;
		if (!enabled_) return;
		stack_.clear();
		recordingScopeCount_ = 0;
		frameStart_ = Clock::now();
	}

	void FrameProfiler::Begin(const char *name)
	{
		if (!enabled_ || !OnFrameThread()) return;
		OpenScope s;
		CopyName(s.name, name);
		s.start = Clock::now();
		s.depth = (uint32)stack_.size();
		stack_.push_back(s);
	}

	const char *FrameProfiler::CurrentScopeName() const
	{
		if (!enabled_ || stack_.empty() || !OnFrameThread()) return "";
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
		if (!enabled_ || !OnFrameThread()) return;
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
		if (!enabled_ || stack_.empty() || !OnFrameThread()) return;
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
			if (wall > worst) worst = wall;
			// every frame's scopes and counters, summed: the summary gives their
			// averages, which is what says where a steady low frame rate goes
			static std::map<std::string, f64> sums;
			for (uint32 i = 0; i < displayScopeCount_; i++) sums[displayScopes_[i].name] += displayScopes_[i].ms;
			for (uint32 i = 0; i < counterCount_; i++) sums[std::string("#") + counters_[i].name] += counters_[i].ms;
			// (and what the GPU took for each pass, where the device times them)
			for (uint32 i = 0; i < gpuCount_; i++) sums[std::string("gpu:") + gpu_[i].name] += gpu_[i].ms;
			if (wall >= gSlowMs && frames > 1)
			{
				slow++;
				std::fprintf(gSlowLog, "t=%.2f wall=%.1f scoped=%.1f", std::chrono::duration<f64>(now - start).count(), wall, displayFrameMs_);
				for (uint32 i = 0; i < displayScopeCount_; i++)
					if (displayScopes_[i].ms >= 1.0) std::fprintf(gSlowLog, " %s=%.1f", displayScopes_[i].name, displayScopes_[i].ms);
				for (uint32 i = 0; i < counterCount_; i++)
					std::fprintf(gSlowLog, " %s=%.0f", counters_[i].name, counters_[i].ms);
				std::fprintf(gSlowLog, "\n");
			}
			const f64 since = std::chrono::duration<f64>(now - lastSummary).count();
			if (since >= 10.0)
			{
				std::fprintf(gSlowLog, "# t=%.0f: %u frames in %.0f s (%.0f fps), %u slow, worst %.1f ms\n",
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
