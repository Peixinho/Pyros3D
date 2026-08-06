//============================================================================
// Name        : FrameProfiler.cpp
//============================================================================

#include <Pyros3D/Utils/Profiler/FrameProfiler.h>
#include "imgui.h"
#include <algorithm>
#include <cstdio>

namespace p3d {

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
		if (!enabled_) return;
		stack_.clear();
		recordingScopeCount_ = 0;
		frameStart_ = Clock::now();
	}

	void FrameProfiler::Begin(const char *name)
	{
		if (!enabled_) return;
		OpenScope s;
		CopyName(s.name, name);
		s.start = Clock::now();
		s.depth = (uint32)stack_.size();
		stack_.push_back(s);
	}

	void FrameProfiler::End()
	{
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

		ImGui::End();
	}

}
