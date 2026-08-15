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
			if (ImGui::Button("Copy All"))
			{
				std::string buf;
				const unsigned int count = p3d::LOG::_LOG::MessageCount();
				for (unsigned int i = 0; i < count; i++)
				{
					const p3d::LOG::Entry &e = p3d::LOG::_LOG::MessageAt(i);
					if (logErrorsOnly && !e.error) continue;
					buf += e.text + "\n";
				}
				ImGui::SetClipboardText(buf.c_str());
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
				ImGui::PushID((int)i);
				ImGui::Selectable(e.text.c_str(), false, ImGuiSelectableFlags_AllowDoubleClick);
				ImGui::PopID();
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
