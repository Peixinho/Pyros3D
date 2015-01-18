//============================================================================
// Name        : InputManager.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : Input Manager
//============================================================================

#ifndef INPUTMANAGER_H
#define	INPUTMANAGER_H

#include <Pyros3D/Ext/Signals/Signal.h>
#include <Pyros3D/Ext/Signals/Delegate.h>
#include <Pyros3D/Core/Math/Vec2.h>
#include <Pyros3D/Other/Export.h>
#include <map>
#include <vector>

namespace p3d {            
    
    namespace Event {
    
		namespace DataType {
			enum {
				UINT = 0,
				INT,
				FLOAT,
				VEC2,
				VEC3,
				VEC4,
				MATRIX
			};	
		};

		class InputData {
			
			public:
				operator const uint32() const
				{
					return *(int32*)&data[0];
				}
				operator const int32() const
				{
					return *(int32*)&data[0];
				}
				operator const f32() const
				{
					return *(f32*)&data[0];
				}
				operator const Vec2() const
				{
					return *(Vec2*)&data[0];
				}
				operator const Vec3() const
				{
					return *(Vec3*)&data[0];
				}
				operator const Vec4() const
				{
					return *(Vec4*)&data[0];
				}
				operator const Matrix() const
				{
					return *(Matrix*)&data[0];
				}
				void operator =( const uint32 &m )
				{
					Type = DataType::UINT;
					data.resize(sizeof(uint32));
					memcpy(&data[0],&m,sizeof(uint32));
				}
				void operator =( const int32 &m )
				{
					Type = DataType::INT;
					data.resize(sizeof(int32));
					memcpy(&data[0],&m,sizeof(int32));
				}
				void operator =( const f32 &m )
				{
					Type = DataType::FLOAT;
					data.resize(sizeof(f32));
					memcpy(&data[0],&m,sizeof(f32));
				}
				void operator =( const Vec2&m )
				{
					Type = DataType::VEC2;
					data.resize(sizeof(Vec2));
					memcpy(&data[0],&m,sizeof(Vec2));
				}
				void operator =( const Vec3 &m )
				{
					Type = DataType::VEC3;
					data.resize(sizeof(Vec3));
					memcpy(&data[0],&m,sizeof(Vec3));
				}
				void operator =( const Vec4 &m )
				{
					Type = DataType::VEC4;
					data.resize(sizeof(Vec4));
					memcpy(&data[0],&m,sizeof(Vec4));
				}
				void operator =( const Matrix &m )
				{
					Type = DataType::MATRIX;
					data.resize(sizeof(Matrix));
					memcpy(&data[0],&m,sizeof(Matrix));
				}

			private:
				std::vector<uchar> data;
				uint32 Type;
		};

        namespace Type {
            enum {
                OnPress = 0,
                OnRelease,
                OnMove, // mouse wheel and move
            };
        };

        namespace Input {

            // The Event Result
            struct Info {
                uint32 Type; // Event Type
                uint32 Input; // Event Input
				InputData Value;
            };

            namespace Mouse {
                enum  {
                    Left = 0x1000,
                    Middle,
                    Right,
                    Wheel,
                    Move
                };
            };
            namespace Keyboard {
                enum {
                        A,            ///< The A key
                        B,            ///< The B key
                        C,            ///< The C key
                        D,            ///< The D key
                        E,            ///< The E key
                        F,            ///< The F key
                        G,            ///< The G key
                        H,            ///< The H key
                        I,            ///< The I key
                        J,            ///< The J key
                        K,            ///< The K key
                        L,            ///< The L key
                        M,            ///< The M key
                        N,            ///< The N key
                        O,            ///< The O key
                        P,            ///< The P key
                        Q,            ///< The Q key
                        R,            ///< The R key
                        S,            ///< The S key
                        T,            ///< The T key
                        U,            ///< The U key
                        V,            ///< The V key
                        W,            ///< The W key
                        X,            ///< The X key
                        Y,            ///< The Y key
                        Z,            ///< The Z key
                        Num0,         ///< The 0 key
                        Num1,         ///< The 1 key
                        Num2,         ///< The 2 key
                        Num3,         ///< The 3 key
                        Num4,         ///< The 4 key
                        Num5,         ///< The 5 key
                        Num6,         ///< The 6 key
                        Num7,         ///< The 7 key
                        Num8,         ///< The 8 key
                        Num9,         ///< The 9 key
                        Escape,       ///< The Escape key
                        LControl,     ///< The left Control key
                        LShift,       ///< The left Shift key
                        LAlt,         ///< The left Alt key
                        LSystem,      ///< The left OS specific key: window (Windows and Linux), apple (MacOS X), ...
                        RControl,     ///< The right Control key
                        RShift,       ///< The right Shift key
                        RAlt,         ///< The right Alt key
                        RSystem,      ///< The right OS specific key: window (Windows and Linux), apple (MacOS X), ...
                        Menu,         ///< The Menu key
                        LBracket,     ///< The [ key
                        RBracket,     ///< The ] key
                        SemiColon,    ///< The ; key
                        Comma,        ///< The , key
                        Period,       ///< The . key
                        Quote,        ///< The ' key
                        Slash,        ///< The / key
                        BackSlash,    ///< The \ key
                        Tilde,        ///< The ~ key
                        Equal,        ///< The = key
                        Dash,         ///< The - key
                        Space,        ///< The Space key
                        Return,       ///< The Return key
                        Back,         ///< The Backspace key
                        Tab,          ///< The Tabulation key
                        PageUp,       ///< The Page up key
                        PageDown,     ///< The Page down key
                        End,          ///< The End key
                        Home,         ///< The Home key
                        Insert,       ///< The Insert key
                        Delete,       ///< The Delete key
                        Add,          ///< +
                        Subtract,     ///< -
                        Multiply,     ///< *
                        Divide,       ///< /
                        Left,         ///< Left arrow
                        Right,        ///< Right arrow
                        Up,           ///< Up arrow
                        Down,         ///< Down arrow
                        Numpad0,      ///< The numpad 0 key
                        Numpad1,      ///< The numpad 1 key
                        Numpad2,      ///< The numpad 2 key
                        Numpad3,      ///< The numpad 3 key
                        Numpad4,      ///< The numpad 4 key
                        Numpad5,      ///< The numpad 5 key
                        Numpad6,      ///< The numpad 6 key
                        Numpad7,      ///< The numpad 7 key
                        Numpad8,      ///< The numpad 8 key
                        Numpad9,      ///< The numpad 9 key
                        F1,           ///< The F1 key
                        F2,           ///< The F2 key
                        F3,           ///< The F3 key
                        F4,           ///< The F4 key
                        F5,           ///< The F5 key
                        F6,           ///< The F6 key
                        F7,           ///< The F7 key
                        F8,           ///< The F8 key
                        F9,           ///< The F8 key
                        F10,          ///< The F10 key
                        F11,          ///< The F11 key
                        F12,          ///< The F12 key
                        F13,          ///< The F13 key
                        F14,          ///< The F14 key
                        F15,          ///< The F15 key
                        Pause,        ///< The Pause key
                        
                        Count // count number of keyboard inputs
                };
                namespace OtherKeyboardEvents {
                    enum {
                        CharacterEnter = 0x2000
                    };
                };
            };
            namespace Joypad {
                namespace ID {
                    enum {
                        Joypad0 = 0,
                        Joypad1,
                        Joypad2,
                        Joypad3,
                        Joypad4,
                        Joypad5,
                        Joypad6,
                        Joypad7
                    };
                };
                namespace Button {
                    enum {
                        Button0 = 0,                        
                        Button1,
                        Button2,
                        Button3,
                        Button4,
                        Button5,
                        Button6,
                        Button7,
                        Button8,
                        Button9,
                        Button10,
                        Button11,
                        Button12,
                        Button13,
                        Button14,
                        Button15,
                        Button16,
                        Button17,
                        Button18,
                        Button19,
                        Button20,
                        Button21,
                        Button22,
                        Button23,
                        Button24,
                        Button25,
                        Button26,
                        Button27,
                        Button28,
                        Button29,
                        Button30,
                        Button31                        
                    };
                };
                namespace Axis {
                    enum {
                        X,    ///< The X axis
                        Y,    ///< The Y axis
                        Z,    ///< The Z axis
                        R,    ///< The R axis
                        U,    ///< The U axis
                        V,    ///< The V axis
                        PovX, ///< The X axis of the point-of-view hat
                        PovY  ///< The Y axis of the point-of-view hat
                    };
                };
            };            
        };
    };
    
    class PYROS3D_API InputManager {
        
        friend class Context;
        
        public:
            
            // Store Char Code
            static uchar Code;
            
            // Mouse Events
            static Vec2 GetMousePosition();
            
            // Add Events
            template< class X, class Y >
            static void AddEvent(const uint32 EventType, const uint32 Event, Y * obj, void (X::*func)(Event::Input::Info p))
            {                
                switch(EventType)
                {
                    case Event::Type::OnPress:
                            EventsMapPressed[Event].Connect(obj,func);
                        break;
                    case Event::Type::OnRelease:
                            EventsMapReleased[Event].Connect(obj,func);
                        break;
                    case Event::Type::OnMove:
                        switch(Event)
                        {
                            case Event::Input::Mouse::Move:
                                MouseMoveEvents.Connect(obj,func);
                                break;
                            case Event::Input::Mouse::Wheel:
                                MouseWheelEvents.Connect(obj,func);
                                break;
                        }                        
                        break;
                }
            }
            // Remove Events
            template< class X, class Y >
            static void RemoveEvent(const uint32 EventType, const uint32 Event, Y * obj, void (X::*func)(Event::Input::Info p))
            {
                switch(EventType)
                {
                    case Event::Type::OnPress:
                            EventsMapPressed[Event].Disconnect(obj,func);
                        break;
                    case Event::Type::OnRelease:
                            EventsMapReleased[Event].Disconnect(obj,func);
                        break;
                    case Event::Type::OnMove:
                        switch(Event)
                        {
                            case Event::Input::Mouse::Move:
                                MouseMoveEvents.Disconnect(obj,func);
                                break;
                            case Event::Input::Mouse::Wheel:
                                MouseWheelEvents.Disconnect(obj,func);
                                break;
                        }
                        
                        break;
                }
            }
            
            // Joypad Specific
            template< class X, class Y >
            static void AddJoypadEvent(const uint32 EventType, const uint32 Id, const uint32 Event, Y * obj, void (X::*func)(Event::Input::Info p))
            {                
                switch(EventType)
                {
                    case Event::Type::OnPress:
                        JoyPadPressed[Id][Event].Connect(obj,func);
                        break;
                    case Event::Type::OnRelease:
                        JoyPadReleased[Id][Event].Connect(obj,func);
                        break;
                    case Event::Type::OnMove:
                        JoyPadMove[Id][Event].Connect(obj,func);
                        break;
                }
            }
            template< class X, class Y >
            static void RemoveJoypadEvent(const uint32 EventType, const uint32 Id, const uint32 Event, Y * obj, void (X::*func)(Event::Input::Info p))
            {                
                switch(EventType)
                {
                    case Event::Type::OnPress:
                        JoyPadPressed[Id][Event].Disconnect(obj,func);
                        break;
                    case Event::Type::OnRelease:
                        JoyPadReleased[Id][Event].Disconnect(obj,func);
                        break;
                    case Event::Type::OnMove:
                        JoyPadMove[Id][Event].Disconnect(obj,func);
                        break;
                }
            }  
            
        protected:
            
            // Joypad
            static void JoypadButtonPressed(const uint32 joypadID, const uint32 button);
            static void JoypadButtonReleased(const uint32 joypadID, const uint32 button);
            static void JoypadMove(const uint32 joypadID, const uint32 axis, const f32 value);
            
            // Mouse Events only visible to Interface
            static void SetMousePosition(const uint32 mousex, const uint32 mousey);
            static void SetMouseWheel(const f32 delta);
            static void MousePressed(const  uint32 e);
            static void MouseReleased(const uint32 e);
            static uint32 mouseX;
			static uint32 mouseY;
            
            // Keyboard Events only visible to Interface
            static void KeyPressed(const uint32 e);
            static void KeyReleased(const uint32 e);
            static void CharEntered(const uint32 e);
            
            // Keyboard Events
            static std::map<uint32, Gallant::Signal1<Event::Input::Info> > EventsMapPressed;
            static std::map<uint32, Gallant::Signal1<Event::Input::Info> > EventsMapReleased;
            // Mouse Specific
            static Gallant::Signal1<Event::Input::Info> MouseWheelEvents;
            static Gallant::Signal1<Event::Input::Info> MouseMoveEvents;
            // Character Specific
            static Gallant::Signal1<Event::Input::Info> CharacterEnteredPress;
            static Gallant::Signal1<Event::Input::Info> CharacterEnteredRelease;
            // JoyPad Specific
            static std::map <uint32, std::map<uint32, Gallant::Signal1<Event::Input::Info> > > JoyPadPressed;
            static std::map <uint32, std::map<uint32, Gallant::Signal1<Event::Input::Info> > > JoyPadReleased;
            static std::map <uint32, std::map<uint32, Gallant::Signal1<Event::Input::Info> > > JoyPadMove;
    };

};
#endif	/* INPUTMANAGER_H */