//============================================================================
// Name        : JsApplication.h
// Description : Thin SDL2Context wrapper for JS-owned Embind clients.
//============================================================================

#ifndef JSAPPLICATION_H
#define JSAPPLICATION_H

#include "../../WindowManagers/SDL2/SDL2Context.h"

namespace p3d {

	// Window + GL for web; game logic stays in JavaScript.
	// Methods used from Embind are declared on this class (not only inherited)
	// so embind registers them reliably.
	class JsApplication : public SDL2Context {
	public:
		JsApplication(const uint32 width, const uint32 height, const std::string &title, const uint32 windowType);
		virtual ~JsApplication();

		virtual void Init() override;
		virtual void Update() override;
		virtual void Shutdown() override;

		void PollEvents() { GetEvents(); }
		void Present() { Draw(); }
		float GetTimeSeconds() { return (float)GetTime(); }
		uint32 GetWidth() const { return Width; }
		uint32 GetHeight() const { return Height; }
		bool Running() const { return IsRunning(); }
		void Quit() { Close(); }
		void ResizeWindow(const uint32 width, const uint32 height) { OnResize(width, height); }
	};

}

#endif /* JSAPPLICATION_H */
