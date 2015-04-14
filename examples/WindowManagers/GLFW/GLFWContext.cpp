//============================================================================
// Name        : GLFWContext.cpp
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Game Template
//============================================================================

#include <Pyros3D/Other/PyrosGL.h>
#include "GLFWContext.h"

namespace p3d {
    
    GLFWContext::GLFWContext(const uint32 width, const uint32 height, const std::string &title, const uint32 windowType) : Context(width,height) 
    {

        glfwInit();

        rview = glfwCreateWindow(width, height, title.c_str(), NULL, NULL);
        glfwMakeContextCurrent(rview);

        glfwSwapInterval(1);
        //glfwSetKeyCallback(rview, key_callback);


#if !defined(GLES2)
        // Initialize GLew
        glewInit();
#endif
		
    }
    GLFWContext::~GLFWContext() 
    {
        glfwTerminate();
        glfwDestroyWindow(rview);
    }
    
    void GLFWContext::OnResize(const uint32 width, const uint32 height)
    {
        Width = width;
        Height = height;
    }
    bool GLFWContext::IsRunning() const
    {
        // return if application is running
        return !glfwWindowShouldClose(rview) && Initialized == true;
    }
    void GLFWContext::GetEvents()
    {

        // Shit Needed


        SetTime(glfwGetTime());
        fps.setFPS(glfwGetTime());
    }

    void GLFWContext::Draw() 
    {
        glfwSwapBuffers(rview);
        glfwPollEvents();
    }
    
    void GLFWContext::HideMouse()
    {
        glfwSetInputMode(rview, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);
    }
    void GLFWContext::ShowMouse()
    {
        glfwSetInputMode(rview, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
    
    // Buttons and Mouse
    void GLFWContext::KeyPressed(const uint32 key)
    {
        // Key Pressed
        SetKeyPressed(key);
    }
    void GLFWContext::KeyReleased(const uint32 key)
    {
        // Key Released
        SetKeyReleased(key);
    }
    void GLFWContext::TextEntered(const uint32 Code)
    {
        SetCharEntered(Code);
    }
    void GLFWContext::MouseButtonPressed(const uint32 button)
    {
        // // Mouse Button Pressed
        // switch(button)
        // {
        //     case sf::Mouse::Left:
        //         SetMouseButtonPressed(Event::Input::Mouse::Left);
        //         break;
        //     case sf::Mouse::Middle:
        //         SetMouseButtonPressed(Event::Input::Mouse::Middle);
        //         break;
        //     case sf::Mouse::Right:
        //         SetMouseButtonPressed(Event::Input::Mouse::Right);
        //         break;
        // }
    }
    void GLFWContext::MouseButtonReleased(const uint32 button)
    {
        // // Mouse Button Released
        // switch(button)
        // {
        //     case sf::Mouse::Left:
        //         SetMouseButtonReleased(Event::Input::Mouse::Left);
        //         break;
        //     case sf::Mouse::Middle:
        //         SetMouseButtonReleased(Event::Input::Mouse::Middle);
        //         break;
        //     case sf::Mouse::Right:
        //         SetMouseButtonReleased(Event::Input::Mouse::Right);
        //         break;
        // }
    }
    void GLFWContext::MouseMove(const f32 mousex, const f32 mousey)
    {
        SetMouseMove(mousex,mousey);
    }
    void GLFWContext::MouseWheel(const f32 delta)
    {
        SetMouseWheel(delta);
    }
    void GLFWContext::SetMousePosition(const uint32 mouseX, const uint32 mouseY)
    {
        glfwSetCursorPos(rview,mouseX,mouseY);
    }
    const Vec2 GLFWContext::GetMousePosition() const
    {
        Vec2 coord;
        glfwGetCursorPos(rview,(double*)&coord.x,(double*)&coord.y);
        return coord;
    }
    void GLFWContext::JoypadButtonPressed(const uint32 JoypadID, const uint32 Button)
    {
        SetJoypadButtonPressed(JoypadID, Button);
    }
    void GLFWContext::JoypadButtonReleased(const uint32 JoypadID, const uint32 Button)
    {
        SetJoypadButtonReleased(JoypadID, Button);
    }
    void GLFWContext::JoypadMove(const uint32 JoypadID, const uint32 Axis, const f32 Value)
    {
        SetJoypadMove(JoypadID, Axis, Value);
    }
    // virtuals methods
    void GLFWContext::Init() {}
    void GLFWContext::Update() {}
    void GLFWContext::Shutdown() {}
    void GLFWContext::Close()
    {
        Initialized = false;
    }
   
}
