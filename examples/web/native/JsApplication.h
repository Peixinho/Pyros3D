//============================================================================
// Name        : JsApplication.h
// Description : Thin SDL2Context wrapper for JS-owned Embind clients.
//============================================================================

#ifndef JSAPPLICATION_H
#define JSAPPLICATION_H

#include "../../WindowManagers/SDL2/SDL2Context.h"
#include <Pyros3D/Utils/Bindings/PyrosEmbind.h>
#include <memory>

namespace p3d {

	// The canvas and its graphics device (WebGPU) for the page; game logic stays in JavaScript.
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
		// A renderer drawing onto this canvas, at its size (Renderer.resize after the canvas changes)
		std::shared_ptr<EmbindRenderer> CreateRenderer() { return GetDevice() ? std::make_shared<EmbindRenderer>(GetDevice(), Width, Height) : std::shared_ptr<EmbindRenderer>(); }
	};

}

#endif /* JSAPPLICATION_H */
