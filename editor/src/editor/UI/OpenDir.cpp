//============================================================================
// Name        : OPENDIR.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : OPENDIR
//============================================================================

#include "OpenDir.h"
#include <misc/cpp/imgui_stdlib.h>

namespace ImGui {

	namespace _priv {

		std::string selectedPath;
		ReadDirectory readDirectory;
		std::vector<_FileInfo> dir;
		std::vector<const char*> listbox_items;
		std::string Name;
		std::string path;
		std::string extension;
		std::string lastVisitedPath;
		int selectedDir;

		#ifdef _WIN32
			int driveSelected;
			std::vector<const char*> drives;
		#endif

		void OpenLocation(const std::string &path, const std::string &extension, bool* Open) {
			#ifdef _WIN32
				_priv::drives = AvailableDrives();
				_priv::driveSelected = -1;
			#endif

			*Open = true;

			if (path.size() == 0 && _priv::lastVisitedPath.size() != 0)
				_priv::path = _priv::lastVisitedPath;
			else if (_priv::path.size()==0)
				_priv::path = ExePath();
			else _priv::path = path;

			_priv::extension = extension;
			_priv::dir = _priv::readDirectory.Open(_priv::path, _priv::extension);
			_priv::listbox_items.clear();
			_priv::selectedPath.clear();
			for (std::vector<_FileInfo>::iterator i = _priv::dir.begin(); i != _priv::dir.end(); i++)
			{
				if ((*i).NamePrefix.size()>0)
					_priv::listbox_items.push_back((*i).NamePrefix.c_str());
				else
					_priv::listbox_items.push_back("[D]..");
			}
		}
	};

	bool FilePath(const std::string &label, const std::string &path, const std::string &extension, std::string* buffer, const uint32 bufferSize, bool* Open)
	{
		// imgui_stdlib overload: takes the std::string directly. The old
		// form wrote into &(*buffer)[0] for bufferSize bytes, which forced
		// every caller to resize() the string to that capacity first - and
		// a string full of NULs then reads as non-empty everywhere else.
		(void)bufferSize;
		ImGui::InputText(label.size() ? label.c_str() : "##path", buffer, ImGuiInputTextFlags_ReadOnly);
		ImGui::SameLine();
		if (ImGui::Button(std::string(std::string("...##") + label).c_str())) {
			_priv::OpenLocation(path, extension, Open);
		}

		if (*Open)
		{
			ImGui::OpenPopup("Open");
			if (ImGui::BeginPopupModal("Open", Open, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
			{
				ImGui::Text("Path: %s", _priv::path.c_str());
				// An empty (or unreadable) directory yields no entries, and
				// &vector[0] on an empty vector is undefined.
				if (_priv::listbox_items.empty())
					ImGui::TextDisabled("(no matching files here)");
				else if (ImGui::ListBox("##files", &_priv::selectedDir, &_priv::listbox_items[0], (int)_priv::listbox_items.size(), 8))
				{
					if (_priv::dir[_priv::selectedDir].isFolder)
					{
						if (_priv::dir[_priv::selectedDir].Name == "..")
						{
							uint32 pos = _priv::path.rfind(std::string("/"));
							if (_priv::path.size() - 1 == pos)
							{
								_priv::path = _priv::path.substr(0, pos);
								_priv::path = _priv::path.substr(0, _priv::path.rfind(std::string("/")));
							}
							else
								_priv::path = _priv::path.substr(0, pos);

							#ifndef _WIN32
									if (_priv::path.size()==0) {
										_priv::path = "/";
										_priv::OpenLocation(_priv::path, _priv::extension, Open);
									}
									else _priv::OpenLocation(_priv::path + std::string("/"), _priv::extension, Open);
							#else
								_priv::OpenLocation(_priv::path + std::string("/"), _priv::extension, Open);
							#endif
						}
						else
							_priv::OpenLocation(_priv::path + _priv::dir[_priv::selectedDir].Name + std::string("/"), _priv::extension, Open);
					}
					else _priv::selectedPath = _priv::path + _priv::dir[_priv::selectedDir].Name;
				}
				ImGui::Text("[D] Directory");
				ImGui::Text("[F] File");
		#ifdef _WIN32
				if (ImGui::Combo("Drives Available", &_priv::driveSelected, &_priv::drives[0], _priv::drives.size()))
				{
					_priv::OpenLocation(std::string(_priv::drives[_priv::driveSelected]) + std::string("/"), _priv::extension, Open);
				}
		#endif
				if (ImGui::Button("Open")) {
					if (_priv::selectedPath.size() > 0)
					{
						*Open = false;
						ImGui::CloseCurrentPopup();
						(*buffer) = _priv::selectedPath;
						_priv::lastVisitedPath = _priv::path;
						ImGui::Spacing();
						ImGui::EndPopup();
						return true;
					}
				}
				ImGui::SameLine();
				if (ImGui::Button("Cancel")) { *Open = false; ImGui::CloseCurrentPopup(); _priv::path.clear(); }
				ImGui::Spacing();
				ImGui::EndPopup();
			}
		}
		return false;
	}
};
