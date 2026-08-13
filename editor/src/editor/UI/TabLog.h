//============================================================================
// Name        : TABLOG.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : TABLOG
//============================================================================

#ifndef TABLOG_H
#define	TABLOG_H

#include "IUInterface.h"
#include <Pyros3D/Core/Logs/Log.h>
#include <string>

class TabLog: public IUInterface {
public:

	TabLog(const std::string &name, bool* open)
		: Name(name), Open(open), logSeen(0), logErrorsOnly(false) {}

	virtual void Init(const uint32 width, const uint32 height) {}
	virtual void OnResize(const uint32 width, const uint32 height) {}
	virtual void Update(const f64 time) {}
	virtual void Shutdown() {}

	virtual void Show()
	{
		// Same ring-buffer reader as DemoLauncher::DrawLogWindow — engine
		// records into LOG::_LOG; this panel only displays it.
		if (!Open || !*Open)
		{
			logSeen = p3d::LOG::_LOG::MessageCount();
			return;
		}

		if (ImGui::Begin(Name.c_str(), Open))
		{
			if (ImGui::Button("Clear"))
			{
				p3d::LOG::_LOG::ClearMessages();
				logSeen = 0;
			}
			ImGui::SameLine();
			ImGui::Checkbox("Errors only", &logErrorsOnly);
			ImGui::SameLine();
			{
				int level = p3d::LOG::_LOG::GetLevel();
				ImGui::SetNextItemWidth(150.0f);
				if (ImGui::Combo("Level", &level, "Errors\0+ Warnings\0+ Success\0Everything\0"))
					p3d::LOG::_LOG::SetLevel(level);
			}
			ImGui::SameLine();
			ImGui::Text("%u line(s)", p3d::LOG::_LOG::MessageCount());
			ImGui::Separator();

			ImGui::BeginChild("##loglines", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
			const unsigned int count = p3d::LOG::_LOG::MessageCount();
			for (unsigned int i = 0; i < count; i++)
			{
				const p3d::LOG::Entry &e = p3d::LOG::_LOG::MessageAt(i);
				if (logErrorsOnly && !e.error) continue;
				if (e.level == p3d::LOG::Level::Error)
					ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f), "%s", e.text.c_str());
				else if (e.level == p3d::LOG::Level::Warning)
					ImGui::TextColored(ImVec4(1.0f, 0.75f, 0.25f, 1.0f), "%s", e.text.c_str());
				else if (e.level == p3d::LOG::Level::Success)
					ImGui::TextColored(ImVec4(0.45f, 0.9f, 0.5f, 1.0f), "%s", e.text.c_str());
				else
					ImGui::TextUnformatted(e.text.c_str());
			}
			if (count != logSeen && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
				ImGui::SetScrollHereY(1.0f);
			ImGui::EndChild();
			logSeen = count;
		}
		ImGui::End();
	}

protected:

	std::string Name;
	bool* Open;
	unsigned int logSeen;
	bool logErrorsOnly;

};

#endif	/* TABLOG_H */
