#include <Pyros3D/Other/PyrosGL.h>
#include <Pyros3D/Core/Context/Context.h>
#include "imgui_impl_sfml.h"
#include <Pyros3D/Utils/Thread/Thread.h>
#include <iostream>
#ifdef _WIN32
#undef APIENTRY
#endif

namespace ImGui {

	namespace SFML {

		static sf::Window*	g_Window;
		static bool         g_MousePressed[3] = { false, false, false };
		static bool         g_MouseReleased[3] = { true, true, true };
		static float        g_MouseWheel = 0.0f;
		static GLuint		g_FontTexture = 0;
		static int          g_ShaderHandle = 0, g_VertHandle = 0, g_FragHandle = 0;
		static int          g_AttribLocationTex = 0, g_AttribLocationProjMtx = 0;
		static int          g_AttribLocationPosition = 0, g_AttribLocationUV = 0, g_AttribLocationColor = 0;
		static unsigned int g_VboHandle = 0, g_VaoHandle = 0, g_ElementsHandle = 0;
		static sf::Clock	g_Time;
		static double		g_CurrentTime = 0;

		void ImGui_ImplSFML_RenderDrawLists(ImDrawData* draw_data)
		{

			if ((p3d::Context::GetGLMajorVersion()==3 && p3d::Context::GetGLMinorVersion()>=3) || p3d::Context::GetGLMajorVersion()>3)
			{

				// Backup GL state
				GLint last_program; glGetIntegerv(GL_CURRENT_PROGRAM, &last_program);
				GLint last_texture; glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);
				GLint last_array_buffer; glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &last_array_buffer);
				GLint last_element_array_buffer; glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &last_element_array_buffer);
				GLint last_vertex_array; glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &last_vertex_array);
				GLint last_blend_src; glGetIntegerv(GL_BLEND_SRC, &last_blend_src);
				GLint last_blend_dst; glGetIntegerv(GL_BLEND_DST, &last_blend_dst);
				GLint last_blend_equation_rgb; glGetIntegerv(GL_BLEND_EQUATION_RGB, &last_blend_equation_rgb);
				GLint last_blend_equation_alpha; glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &last_blend_equation_alpha);
				GLboolean last_enable_blend = glIsEnabled(GL_BLEND);
				GLboolean last_enable_cull_face = glIsEnabled(GL_CULL_FACE);
				GLboolean last_enable_depth_test = glIsEnabled(GL_DEPTH_TEST);
				GLboolean last_enable_scissor_test = glIsEnabled(GL_SCISSOR_TEST);

				// Setup render state: alpha-blending enabled, no face culling, no depth testing, scissor enabled
				glEnable(GL_BLEND);
				glBlendEquation(GL_FUNC_ADD);
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
				glDisable(GL_CULL_FACE);
				glDisable(GL_DEPTH_TEST);
				glEnable(GL_SCISSOR_TEST);
				glActiveTexture(GL_TEXTURE0);

				// Handle cases of screen coordinates != from framebuffer coordinates (e.g. retina displays)
				ImGuiIO& io = ImGui::GetIO();
				float fb_height = io.DisplaySize.y * io.DisplayFramebufferScale.y;
				draw_data->ScaleClipRects(io.DisplayFramebufferScale);
				
				// Setup orthographic projection matrix
				const float ortho_projection[4][4] =
				{
					{ 2.0f / io.DisplaySize.x, 0.0f,                   0.0f, 0.0f },
					{ 0.0f,                  2.0f / -io.DisplaySize.y, 0.0f, 0.0f },
					{ 0.0f,                  0.0f,                  -1.0f, 0.0f },
					{ -1.0f,                  1.0f,                   0.0f, 1.0f },
				};
				glUseProgram(g_ShaderHandle);
				glUniform1i(g_AttribLocationTex, 0);
				glUniformMatrix4fv(g_AttribLocationProjMtx, 1, GL_FALSE, &ortho_projection[0][0]);
				glBindVertexArray(g_VaoHandle);

				for (int n = 0; n < draw_data->CmdListsCount; n++)
				{
					const ImDrawList* cmd_list = draw_data->CmdLists[n];
					const ImDrawIdx* idx_buffer_offset = 0;

					glBindBuffer(GL_ARRAY_BUFFER, g_VboHandle);
					glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)cmd_list->VtxBuffer.size() * sizeof(ImDrawVert), (GLvoid*)&cmd_list->VtxBuffer.front(), GL_STREAM_DRAW);

					glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_ElementsHandle);
					glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)cmd_list->IdxBuffer.size() * sizeof(ImDrawIdx), (GLvoid*)&cmd_list->IdxBuffer.front(), GL_STREAM_DRAW);

					for (const ImDrawCmd* pcmd = cmd_list->CmdBuffer.begin(); pcmd != cmd_list->CmdBuffer.end(); pcmd++)
					{
						if (pcmd->UserCallback)
						{
							pcmd->UserCallback(cmd_list, pcmd);
						}
						else
						{
							glBindTexture(GL_TEXTURE_2D, (GLuint)(intptr_t)pcmd->TextureId);
							glScissor((int)pcmd->ClipRect.x, (int)(fb_height - pcmd->ClipRect.w), (int)(pcmd->ClipRect.z - pcmd->ClipRect.x), (int)(pcmd->ClipRect.w - pcmd->ClipRect.y));
							glDrawElements(GL_TRIANGLES, (GLsizei)pcmd->ElemCount, GL_UNSIGNED_SHORT, idx_buffer_offset);
						}
						idx_buffer_offset += pcmd->ElemCount;
					}
				}

				// Restore modified GL state
				glUseProgram(last_program);
				glBindTexture(GL_TEXTURE_2D, last_texture);
				glBindBuffer(GL_ARRAY_BUFFER, last_array_buffer);
				glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, last_element_array_buffer);
				glBindVertexArray(last_vertex_array);
				glBlendEquationSeparate(last_blend_equation_rgb, last_blend_equation_alpha);
				glBlendFunc(last_blend_src, last_blend_dst);
				if (last_enable_blend) glEnable(GL_BLEND); else glDisable(GL_BLEND);
				if (last_enable_cull_face) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
				if (last_enable_depth_test) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
				if (last_enable_scissor_test) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);
			
			} else {

				// We are using the OpenGL fixed pipeline to make the example code simpler to read!
			    // Setup render state: alpha-blending enabled, no face culling, no depth testing, scissor enabled, vertex/texcoord/color pointers.
			    GLint last_texture;
			    glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);
			    glPushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_TRANSFORM_BIT);
			    glEnable(GL_BLEND);
			    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
			    glDisable(GL_CULL_FACE);
			    glDisable(GL_DEPTH_TEST);
			    glEnable(GL_SCISSOR_TEST);
			    glEnableClientState(GL_VERTEX_ARRAY);
			    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
			    glEnableClientState(GL_COLOR_ARRAY);
			    glEnable(GL_TEXTURE_2D);
			    //glUseProgram(0); // You may want this if using this code in an OpenGL 3+ context

			    // Handle cases of screen coordinates != from framebuffer coordinates (e.g. retina displays)
			    ImGuiIO& io = ImGui::GetIO();
			    float fb_height = io.DisplaySize.y * io.DisplayFramebufferScale.y;
			    draw_data->ScaleClipRects(io.DisplayFramebufferScale);

			    // Setup orthographic projection matrix
			    glMatrixMode(GL_PROJECTION);
			    glPushMatrix();
			    glLoadIdentity();
			    glOrtho(0.0f, io.DisplaySize.x, io.DisplaySize.y, 0.0f, -1.0f, +1.0f);
			    glMatrixMode(GL_MODELVIEW);
			    glPushMatrix();
			    glLoadIdentity();

			    // Render command lists
			    #define OFFSETOF(TYPE, ELEMENT) ((size_t)&(((TYPE *)0)->ELEMENT))
			    for (int n = 0; n < draw_data->CmdListsCount; n++)
			    {
			        const ImDrawList* cmd_list = draw_data->CmdLists[n];
			        const unsigned char* vtx_buffer = (const unsigned char*)&cmd_list->VtxBuffer.front();
			        const ImDrawIdx* idx_buffer = &cmd_list->IdxBuffer.front();
			        glVertexPointer(2, GL_FLOAT, sizeof(ImDrawVert), (void*)(vtx_buffer + OFFSETOF(ImDrawVert, pos)));
			        glTexCoordPointer(2, GL_FLOAT, sizeof(ImDrawVert), (void*)(vtx_buffer + OFFSETOF(ImDrawVert, uv)));
			        glColorPointer(4, GL_UNSIGNED_BYTE, sizeof(ImDrawVert), (void*)(vtx_buffer + OFFSETOF(ImDrawVert, col)));

			        for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.size(); cmd_i++)
			        {
			            const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
			            if (pcmd->UserCallback)
			            {
			                pcmd->UserCallback(cmd_list, pcmd);
			            }
			            else
			            {
			                glBindTexture(GL_TEXTURE_2D, (GLuint)(intptr_t)pcmd->TextureId);
			                glScissor((int)pcmd->ClipRect.x, (int)(fb_height - pcmd->ClipRect.w), (int)(pcmd->ClipRect.z - pcmd->ClipRect.x), (int)(pcmd->ClipRect.w - pcmd->ClipRect.y));
			                glDrawElements(GL_TRIANGLES, (GLsizei)pcmd->ElemCount, GL_UNSIGNED_SHORT, idx_buffer);
			            }
			            idx_buffer += pcmd->ElemCount;
			        }
			    }
			    #undef OFFSETOF

			    // Restore modified state
			    glDisableClientState(GL_COLOR_ARRAY);
			    glDisableClientState(GL_TEXTURE_COORD_ARRAY);
			    glDisableClientState(GL_VERTEX_ARRAY);
			    glBindTexture(GL_TEXTURE_2D, last_texture);
			    glMatrixMode(GL_MODELVIEW);
			    glPopMatrix();
			    glMatrixMode(GL_PROJECTION);
			    glPopMatrix();
			    glPopAttrib();

			}
		}

		void ImGui_ImplSFML_CreateFontsTexture()
		{
			ImGuiIO& io = ImGui::GetIO();

			// Build texture atlas
			unsigned char* pixels;
			int width, height;
			io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);   // Load as RGBA 32-bits for OpenGL3 demo because it is more likely to be compatible with user's existing shader.
																	  // Create OpenGL texture
			glGenTextures(1, &g_FontTexture);
			glBindTexture(GL_TEXTURE_2D, g_FontTexture);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

			// Store our identifier
			io.Fonts->TexID = (void *)(intptr_t)g_FontTexture;

			// Cleanup (don't clear the input data if you want to append new fonts later)
			io.Fonts->ClearInputData();
			io.Fonts->ClearTexData();
		}

		bool ImGui_ImplSFML_CreateDeviceObjects()
		{
			
			GLint last_texture, last_array_buffer, last_vertex_array;

			if ((p3d::Context::GetGLMajorVersion()==3 && p3d::Context::GetGLMinorVersion()>=3) || p3d::Context::GetGLMajorVersion()>3)
			{
				// Backup GL state
				glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);
				glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &last_array_buffer);
				glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &last_vertex_array);

				const GLchar *vertex_shader =
					"#version 330\n"
					"uniform mat4 ProjMtx;\n"
					"in vec2 Position;\n"
					"in vec2 UV;\n"
					"in vec4 Color;\n"
					"out vec2 Frag_UV;\n"
					"out vec4 Frag_Color;\n"
					"void main()\n"
					"{\n"
					"	Frag_UV = UV;\n"
					"	Frag_Color = Color;\n"
					"	gl_Position = ProjMtx * vec4(Position.xy,0,1);\n"
					"}\n";

				const GLchar* fragment_shader =
					"#version 330\n"
					"uniform sampler2D Texture;\n"
					"in vec2 Frag_UV;\n"
					"in vec4 Frag_Color;\n"
					"out vec4 Out_Color;\n"
					"void main()\n"
					"{\n"
					"	Out_Color = Frag_Color * texture( Texture, Frag_UV.st);\n"
					"}\n";

				g_ShaderHandle = glCreateProgram();
				g_VertHandle = glCreateShader(GL_VERTEX_SHADER);
				g_FragHandle = glCreateShader(GL_FRAGMENT_SHADER);
				glShaderSource(g_VertHandle, 1, &vertex_shader, 0);
				glShaderSource(g_FragHandle, 1, &fragment_shader, 0);
				glCompileShader(g_VertHandle);
				glCompileShader(g_FragHandle);
				glAttachShader(g_ShaderHandle, g_VertHandle);
				glAttachShader(g_ShaderHandle, g_FragHandle);
				glLinkProgram(g_ShaderHandle);

				g_AttribLocationTex = glGetUniformLocation(g_ShaderHandle, "Texture");
				g_AttribLocationProjMtx = glGetUniformLocation(g_ShaderHandle, "ProjMtx");
				g_AttribLocationPosition = glGetAttribLocation(g_ShaderHandle, "Position");
				g_AttribLocationUV = glGetAttribLocation(g_ShaderHandle, "UV");
				g_AttribLocationColor = glGetAttribLocation(g_ShaderHandle, "Color");

				glGenBuffers(1, &g_VboHandle);
				glGenBuffers(1, &g_ElementsHandle);

				glGenVertexArrays(1, &g_VaoHandle);
				glBindVertexArray(g_VaoHandle);
				glBindBuffer(GL_ARRAY_BUFFER, g_VboHandle);
				glEnableVertexAttribArray(g_AttribLocationPosition);
				glEnableVertexAttribArray(g_AttribLocationUV);
				glEnableVertexAttribArray(g_AttribLocationColor);

#define OFFSETOF(TYPE, ELEMENT) ((size_t)&(((TYPE *)0)->ELEMENT))
				glVertexAttribPointer(g_AttribLocationPosition, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), (GLvoid*)OFFSETOF(ImDrawVert, pos));
				glVertexAttribPointer(g_AttribLocationUV, 2, GL_FLOAT, GL_FALSE, sizeof(ImDrawVert), (GLvoid*)OFFSETOF(ImDrawVert, uv));
				glVertexAttribPointer(g_AttribLocationColor, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(ImDrawVert), (GLvoid*)OFFSETOF(ImDrawVert, col));
#undef OFFSETOF
			}

			ImGui_ImplSFML_CreateFontsTexture();
			glBindTexture(GL_TEXTURE_2D, last_texture);

			if ((p3d::Context::GetGLMajorVersion()==3 && p3d::Context::GetGLMinorVersion()>=3) || p3d::Context::GetGLMajorVersion()>3)
			{
				// Restore modified GL state
				glBindBuffer(GL_ARRAY_BUFFER, last_array_buffer);
				glBindVertexArray(last_vertex_array);
			}

			return true;
		}

		bool ImGui_ImplSFML_Init(sf::Window *window)
		{
			g_Window = window;

			ImGuiIO& io = ImGui::GetIO();
			io.KeyMap[ImGuiKey_Tab] = sf::Keyboard::Tab;                         // Keyboard mapping. ImGui will use those indices to peek into the io.KeyDown[] array.
			io.KeyMap[ImGuiKey_LeftArrow] = sf::Keyboard::Left;
			io.KeyMap[ImGuiKey_RightArrow] = sf::Keyboard::Right;
			io.KeyMap[ImGuiKey_UpArrow] = sf::Keyboard::Up;
			io.KeyMap[ImGuiKey_DownArrow] = sf::Keyboard::Down;
			io.KeyMap[ImGuiKey_PageUp] = sf::Keyboard::PageUp;
			io.KeyMap[ImGuiKey_PageDown] = sf::Keyboard::PageDown;
			io.KeyMap[ImGuiKey_Home] = sf::Keyboard::Home;
			io.KeyMap[ImGuiKey_End] = sf::Keyboard::End;
			io.KeyMap[ImGuiKey_Delete] = sf::Keyboard::Delete;
			io.KeyMap[ImGuiKey_Backspace] = sf::Keyboard::BackSpace;
			io.KeyMap[ImGuiKey_Enter] = sf::Keyboard::Return;
			io.KeyMap[ImGuiKey_Escape] = sf::Keyboard::Escape;
			io.KeyMap[ImGuiKey_A] = sf::Keyboard::A;
			io.KeyMap[ImGuiKey_C] = sf::Keyboard::C;
			io.KeyMap[ImGuiKey_V] = sf::Keyboard::V;
			io.KeyMap[ImGuiKey_X] = sf::Keyboard::X;
			io.KeyMap[ImGuiKey_Y] = sf::Keyboard::Y;
			io.KeyMap[ImGuiKey_Z] = sf::Keyboard::Z;

			io.RenderDrawListsFn = ImGui_ImplSFML_RenderDrawLists;       // Alternatively you can set this to NULL and call ImGui::GetDrawData() after ImGui::Render() to get the same ImDrawData pointer.
			/*io.SetClipboardTextFn = ImGui_ImplSFML_SetClipboardText;*/
			/*io.GetClipboardTextFn = ImGui_ImplSFML_GetClipboardText;*/
#ifdef _WIN32
			io.ImeWindowHandle = g_Window->getSystemHandle();
#endif
			return true;
		}

		void ImGui_ImplSFML_Shutdown()
		{
			if (g_VaoHandle) glDeleteVertexArrays(1, &g_VaoHandle);
			if (g_VboHandle) glDeleteBuffers(1, &g_VboHandle);
			if (g_ElementsHandle) glDeleteBuffers(1, &g_ElementsHandle);
			g_VaoHandle = g_VboHandle = g_ElementsHandle = 0;

			glDetachShader(g_ShaderHandle, g_VertHandle);
			glDeleteShader(g_VertHandle);
			g_VertHandle = 0;

			glDetachShader(g_ShaderHandle, g_FragHandle);
			glDeleteShader(g_FragHandle);
			g_FragHandle = 0;

			glDeleteProgram(g_ShaderHandle);
			g_ShaderHandle = 0;

			if (g_FontTexture)
			{
				glDeleteTextures(1, &g_FontTexture);
				ImGui::GetIO().Fonts->TexID = 0;
				g_FontTexture = 0;
			}
			ImGui::Shutdown();
		}

		void ImGui_ImplSFML_NewFrame()
		{
			if (!g_FontTexture)
				ImGui_ImplSFML_CreateDeviceObjects();

			ImGuiIO& io = ImGui::GetIO();

			// Setup display size (every frame to accommodate for window resizing)
			io.DisplaySize = ImVec2((float)g_Window->getSize().x-1, (float)g_Window->getSize().y);

			// Setup time step
			io.DeltaTime = g_Time.getElapsedTime().asSeconds() > 0.0 ? (float)(g_Time.getElapsedTime().asSeconds()-g_CurrentTime) : (float)(1.0f / 60.0f);
			g_CurrentTime = g_Time.getElapsedTime().asSeconds();

			// Setup inputs
			// (we already got mouse wheel, keyboard keys & characters from glfw callbacks polled in glfwPollEvents())
			
			if (g_Window->hasFocus())
			{
				io.MousePos = ImVec2((float)sf::Mouse::getPosition(*g_Window).x, (float)sf::Mouse::getPosition(*g_Window).y);   // Mouse position in screen coordinates (set to -1,-1 if no mouse / on another screen, etc.)
			}
			else
			{
				io.MousePos = ImVec2(-1, -1);
			}

			for (int i = 0; i < 3; i++)
			{
				io.MouseDown[i] = g_MousePressed[i] && !g_MouseReleased[i];    // If a mouse press event came, always pass it as "mouse held this frame", so we don't miss click-release events that are shorter than 1 frame.
			}

			io.MouseWheel = g_MouseWheel;
			g_MouseWheel = 0.0f;

			// Hide OS mouse cursor if ImGui is drawing it
			g_Window->setMouseCursorVisible(!io.MouseDrawCursor || io.MousePos.x<0.0 || io.MousePos.x>io.DisplaySize.x || io.MousePos.y<0.0 || io.MousePos.y>io.DisplaySize.y);

			// Start the frame
			ImGui::NewFrame();
		}

		void ImGui_ImplSFML_Events(sf::Event e)
		{
			switch (e.type)
			{
			case sf::Event::MouseButtonPressed:
			{
				g_MousePressed[e.mouseButton.button] = true;
				g_MouseReleased[e.mouseButton.button] = false;
				break;
			}
			case sf::Event::MouseButtonReleased:
			{
				g_MousePressed[e.mouseButton.button] = false;
				g_MouseReleased[e.mouseButton.button] = true;
				break;
			}
			case sf::Event::MouseWheelMoved:
			{
				g_MouseWheel = (float)e.mouseWheel.delta;
				break;
			}
			case sf::Event::KeyPressed:
			{
				ImGuiIO& io = ImGui::GetIO();
				io.KeysDown[e.key.code] = true;
				io.KeyCtrl = e.key.control;
				io.KeyShift = e.key.shift;
				break;
			}
			case sf::Event::KeyReleased:
			{
				ImGuiIO& io = ImGui::GetIO();
				io.KeysDown[e.key.code] = false;
				io.KeyCtrl = e.key.control;
				io.KeyShift = e.key.shift;
				break;
			}
			case sf::Event::TextEntered:
			{
				if (e.text.unicode > 0 && e.text.unicode < 0x10000)
					ImGui::GetIO().AddInputCharacter(e.text.unicode);
				break;
			}
			default: break;
			}
		}

		void ImGui_ImplSFML_Render(const unsigned int width, const unsigned int height, const ImVec4 &clear_color)
		{
			glViewport(0, 0, width, height);
			glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
			ImGui::Render();
		}

	}
}

namespace p3d {

	void ImGuiContext::GetEvents()
	{
		while (rview.pollEvent(event)) 
		{
			ImGui::SFML::ImGui_ImplSFML_Events(event);

			if (event.type == sf::Event::Closed)
			{
				Close();
			}

			if (event.type == sf::Event::KeyPressed)
				KeyPressed(event.key.code);

			if (event.type == sf::Event::KeyReleased)
				KeyReleased(event.key.code);

			if (event.type == sf::Event::TextEntered)
			{
				TextEntered(event.text.unicode);
			}

			if (event.type == sf::Event::MouseButtonPressed)
				MouseButtonPressed(event.mouseButton.button);

			if (event.type == sf::Event::MouseButtonReleased)
				MouseButtonReleased(event.mouseButton.button);

			if (event.type == sf::Event::MouseMoved)
				MouseMove(sf::Mouse::getPosition(rview).x, sf::Mouse::getPosition(rview).y);

			if (event.type == sf::Event::MouseWheelMoved)
				MouseWheel(event.mouseWheel.delta);

			// Joypad
			if (event.type == sf::Event::JoystickButtonPressed)
				JoypadButtonPressed(event.joystickButton.joystickId, event.joystickButton.button);

			if (event.type == sf::Event::JoystickButtonReleased)
				JoypadButtonReleased(event.joystickButton.joystickId, event.joystickButton.button);

			if (event.type == sf::Event::JoystickMoved)
			{
				JoypadMove(event.joystickButton.joystickId, event.joystickMove.axis, event.joystickMove.position);
			}

			// Adjust the viewport when the window is resized
			if (event.type == sf::Event::Resized)
				OnResize(event.size.width, event.size.height);
		}

		SetTime(clock.getElapsedTime().asSeconds());
		fps.setFPS(clock.getElapsedTime().asSeconds());
		Thread::CheckThreads();
	}

}