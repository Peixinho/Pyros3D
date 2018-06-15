#include <Pyros3D/Other/PyrosGL.h>
#include <Pyros3D/Core/Context/Context.h>
#include "imgui_sdl2_context.h"
#include <Pyros3D/Utils/Thread/Thread.h>
#include <iostream>
#ifdef _WIN32
#undef APIENTRY
#endif

namespace p3d {

	ImGuiContext::ImGuiContext(
		const uint32 width,
		const uint32 height,
		const std::string &title,
		const uint32 windowType
	)
	: SDL2Context(width, height, title, windowType)
	{
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO(); (void)io;

		ImGui_ImplSDL2_InitForOpenGL(rview, mainGLContext);
		ImGui_ImplOpenGL3_Init();

		// Setup style
		ImGui::StyleColorsDark();
	}

	void ImGuiContext::GetEvents()
	{
		SDL_Event sdl_event;
		while (SDL_PollEvent(&sdl_event)>0)
		{
			if (sdl_event.type == SDL_QUIT)
				Close();

			if (sdl_event.type == SDL_KEYDOWN)
				KeyPressed(sdl_event.key.keysym.sym);

			if (sdl_event.type == SDL_KEYUP)
				KeyReleased(sdl_event.key.keysym.sym);

			if (sdl_event.type == SDL_MOUSEBUTTONDOWN)
				MouseButtonPressed(sdl_event.button.button);

			if (sdl_event.type == SDL_MOUSEBUTTONUP)
				MouseButtonReleased(sdl_event.button.button);

			if (sdl_event.type == SDL_MOUSEMOTION)
				MouseMove(sdl_event.motion.x, sdl_event.motion.y);

			if (sdl_event.type == SDL_MOUSEWHEEL)
				MouseWheel(sdl_event.wheel.y);

			if (sdl_event.window.event == SDL_WINDOWEVENT_RESIZED)
				OnResize(sdl_event.window.data1, sdl_event.window.data2);

			ImGui_ImplSDL2_ProcessEvent(&sdl_event);

		}
        SetTime(SDL_GetTicks());
        fps.setFPS(SDL_GetTicks());

        // Start the ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame(rview);
        ImGui::NewFrame();
	}

	ImGuiContext::~ImGuiContext()
	{
		ImGui_ImplOpenGL3_Shutdown();
		ImGui_ImplSDL2_Shutdown();
		ImGui::DestroyContext();

		this->SDL2Context::~SDL2Context();
	}

	void ImGuiContext::Draw()
	{
		ImGui::Render();
		ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
		SDL_GL_SwapWindow(rview);
	}
}

