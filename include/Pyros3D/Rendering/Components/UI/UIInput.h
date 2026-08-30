//============================================================================
// Name        : UIInput.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : A single-line text field
//============================================================================

#ifndef UIINPUT_H
#define	UIINPUT_H

#include <Pyros3D/Rendering/Components/UI/UIWidget.h>
#include <string>
#include <vector>

namespace p3d {

	class PYROS3D_API UIInput : public UIWidget {

	public:

		UIInput();
		virtual ~UIInput();

		virtual void Register(SceneGraph* Scene);
		virtual void Init() {}
		virtual void Update(const f64 time = 0);
		virtual void Destroy() {}
		virtual void Unregister(SceneGraph* Scene);

		virtual uint32 GetComponentType() const { return ComponentType::UIInput; }

		static std::vector<IComponent*> &GetComponents();

		// The value. Setting it moves the caret to the end, which is what
		// assigning a field from code means everywhere else.
		void SetText(const std::string &t);
		const std::string &GetText() const { return text; }

		// Shown, in the placeholder element, while the field is empty.
		void SetPlaceholder(const std::string &t) { placeholder = t; dirty = true; }
		const std::string &GetPlaceholder() const { return placeholder; }

		// 0 is unlimited. Typing past the limit is ignored rather than
		// truncating what is already there.
		void SetMaxLength(const uint32 n) { maxLength = n; SetText(text); }
		uint32 GetMaxLength() const { return maxLength; }

		// Displays as `maskChar` repeated. The value itself is never
		// masked - only what the label is given.
		void SetPassword(const bool on) { password = on; dirty = true; }
		bool IsPassword() const { return password; }
		void SetMaskChar(const char c) { maskChar = c; dirty = true; }
		char GetMaskChar() const { return maskChar; }

		// Selectable and focusable, but not editable. Distinct from
		// non-interactable, which also stops the caret and the focus.
		void SetReadOnly(const bool on) { readOnly = on; }
		bool IsReadOnly() const { return readOnly; }

		// Anything not in this set is rejected as it is typed. Empty means
		// everything printable. A number field is SetFilter("0123456789.-").
		void SetFilter(const std::string &allowed) { filter = allowed; }
		const std::string &GetFilter() const { return filter; }

		// Fired on Enter. Like every handler here it is a name, looked up
		// by whoever owns scripting.
		void SetOnSubmit(const std::string &handler) { onSubmit = handler; }
		const std::string &GetOnSubmit() const { return onSubmit; }

		// The child elements this drives, by name: the label showing the
		// value, the one showing the placeholder, and the caret. Names
		// rather than pointers, for the same reason the slider's are.
		void SetTextElement(const std::string &name) { textName = name; dirty = true; }
		const std::string &GetTextElement() const { return textName; }
		void SetPlaceholderElement(const std::string &name) { placeholderName = name; dirty = true; }
		const std::string &GetPlaceholderElement() const { return placeholderName; }
		void SetCaretElement(const std::string &name) { caretName = name; dirty = true; }
		const std::string &GetCaretElement() const { return caretName; }

		// Where the caret sits, as a character index into the text.
		void SetCaret(const uint32 index);
		uint32 GetCaret() const { return caret; }

		// Seconds per blink half-cycle. 0 holds the caret solid, which is
		// what a test wants and what some styles want.
		void SetBlinkRate(const f32 seconds) { blinkRate = seconds > 0.f ? seconds : 0.f; }
		f32 GetBlinkRate() const { return blinkRate; }

		virtual uint32 OnPointer(const bool inside, const bool down,
			const Vec2 &point, const UIRectValue &rect);
		virtual uint32 OnText(const std::string &utf8);
		virtual uint32 OnKey(const uint32 key, bool &claimed);
		virtual void SetWidgetFocused(const bool on);
		// Typing has to go somewhere: clicking a field is how it gets there.
		virtual bool TakesFocusOnPress() const { return true; }

	private:

		void Apply();
		bool Allowed(const char c) const;

		std::string text, placeholder, filter, onSubmit;
		std::string textName, placeholderName, caretName;
		uint32 caret;
		uint32 maxLength;
		bool password;
		bool readOnly;
		char maskChar;
		bool dirty;
		f32 blinkRate;
		f64 blinkAt;
		bool blinkOn;
		// What the field held when it took focus, so Escape can put it back.
		std::string beforeEdit;

		static std::vector<IComponent*> Components;
	};

}

#endif	/* UIINPUT_H */
