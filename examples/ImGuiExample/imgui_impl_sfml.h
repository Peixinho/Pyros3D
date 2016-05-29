#pragma once

#include "imgui.h"
#include <SFML/Graphics.hpp>
#include <SFML/Window/Keyboard.hpp>
#include <SFML/Window/Event.hpp>
#include <SFML/Window/Window.hpp>
#include "../WindowManagers/SFML/SFMLContext.h"

namespace ImGui {

	namespace SFML {

		IMGUI_API bool        ImGui_ImplSFML_Init(sf::Window* window);
		IMGUI_API void        ImGui_ImplSFML_Shutdown();
		IMGUI_API void        ImGui_ImplSFML_NewFrame();
		IMGUI_API void        ImGui_ImplSFML_Events(sf::Event e);
		IMGUI_API void		  ImGui_ImplSFML_Render(const unsigned int width, const unsigned int height, const ImVec4 &clear_color);

		// Use if you want to reset your rendering device without losing ImGui state.
		// IMGUI_API void        ImGui_ImplSFML_InvalidateDeviceObjects();
		IMGUI_API bool        ImGui_ImplSFML_CreateDeviceObjects();
	}
};

namespace p3d {

	class imguiContext : public SFMLContext {

		public:

			imguiContext(const uint32 width, const uint32 height, const std::string &title, const uint32 windowType = WindowType::Fullscreen) : SFMLContext(width, height, title, windowType) {}

			virtual void GetEvents();

	};
};