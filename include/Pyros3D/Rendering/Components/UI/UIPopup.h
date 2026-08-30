//============================================================================
// Name        : UIPopup.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : A panel that opens over everything, optionally blocking it
//============================================================================

#ifndef UIPOPUP_H
#define	UIPOPUP_H

#include <Pyros3D/Rendering/Components/UI/UIWidget.h>
#include <string>
#include <vector>

namespace p3d {

	// Sits on the root of a dialog - a full-canvas node holding a scrim and
	// the panel itself - and shows or hides the lot. What it adds over
	// simply toggling a rect's visibility is modality: while it is open,
	// nothing underneath it can be clicked, hovered or focused, which is
	// the entire difference between a dialog and a floating panel.
	class PYROS3D_API UIPopup : public UIWidget {

	public:

		UIPopup();
		virtual ~UIPopup();

		virtual void Register(SceneGraph* Scene);
		virtual void Init() {}
		virtual void Update(const f64 time = 0);
		virtual void Destroy() {}
		virtual void Unregister(SceneGraph* Scene);

		virtual uint32 GetComponentType() const { return ComponentType::UIPopup; }

		static std::vector<IComponent*> &GetComponents();

		void SetOpen(const bool on);
		bool IsOpen() const { return open; }
		void Open() { SetOpen(true); }
		void Close() { SetOpen(false); }

		// Modal blocks everything under it. Off makes this a plain floating
		// panel that happens to be shown and hidden as one.
		void SetModal(const bool on) { modal = on; }
		bool IsModalPopup() const { return modal; }

		void SetCloseOnEscape(const bool on) { closeOnEscape = on; }
		bool ClosesOnEscape() const { return closeOnEscape; }
		// A press outside the dialog element - on the scrim, in other words.
		// Off for anything the user has to answer rather than dismiss.
		void SetCloseOnOutside(const bool on) { closeOnOutside = on; }
		bool ClosesOnOutside() const { return closeOnOutside; }

		// The child that counts as the dialog for "clicked outside": the
		// popup's own rect covers the whole canvas, so it cannot be the
		// thing being clicked outside of. Empty means the popup's own rect,
		// for a popup with no scrim.
		void SetDialogElement(const std::string &name) { dialogName = name; }
		const std::string &GetDialogElement() const { return dialogName; }

		void SetOnClose(const std::string &handler) { onClose = handler; }
		const std::string &GetOnClose() const { return onClose; }

		virtual uint32 OnPointer(const bool inside, const bool down,
			const Vec2 &point, const UIRectValue &rect);
		virtual uint32 OnKey(const uint32 key, bool &claimed);
		// While open: Escape has to reach the dialog rather than whatever
		// had focus before it opened.
		virtual bool WantsKeysWhileUnfocused() const { return open && closeOnEscape; }
		// What makes everything underneath inert - see UICanvas::UpdateInput.
		virtual bool IsModalActive() const { return open && modal; }
		// The popup root is a container, not something to land on.
		virtual bool IsFocusable() const { return false; }

	private:

		void Apply();

		bool open;
		bool modal;
		bool closeOnEscape;
		bool closeOnOutside;
		bool applied;
		bool wasDown;
		std::string dialogName;
		std::string onClose;

		static std::vector<IComponent*> Components;
	};

}

#endif	/* UIPOPUP_H */
