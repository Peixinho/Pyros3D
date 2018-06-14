#pragma once

#include "../imgui/imgui.h"
#include "../imgui/examples/imgui_impl_sdl.h"
#include "../imgui/examples/imgui_impl_opengl3.h"

#include <SFML/Graphics.hpp>
#include <SFML/Window/Keyboard.hpp>
#include <SFML/Window/Event.hpp>
#include <SFML/Window/Window.hpp>
#include "../WindowManagers/SDL2/SDL2Context.h"

namespace p3d {

	class ImGuiContext : public SDL2Context {

		public:

			ImGuiContext(
					const uint32 width,
					const uint32 height,
					const std::string &title,
					const uint32 windowType = WindowType::Fullscreen
			);

			virtual void GetEvents();
			virtual void Draw();
			virtual ~ImGuiContext();

	};
};
