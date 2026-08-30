//============================================================================
// Name        : UIInput
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : A single-line text field
//============================================================================

#include <Pyros3D/Rendering/Components/UI/UIInput.h>
#include <Pyros3D/Rendering/Components/UI/UIRect.h>
#include <Pyros3D/Rendering/Components/UI/UIText.h>
#include <Pyros3D/GameObjects/GameObject.h>

namespace p3d {

	std::vector<IComponent*> UIInput::Components;

	UIInput::UIInput() : UIWidget()
	{
		caret = 0;
		maxLength = 0;
		password = false;
		readOnly = false;
		maskChar = '*';
		dirty = true;
		blinkRate = 0.5f;
		blinkAt = 0.0;
		blinkOn = true;
		textName = "Text";
		placeholderName = "Placeholder";
		caretName = "Caret";
	}

	UIInput::~UIInput() {}

	void UIInput::Register(SceneGraph* Scene)
	{
		if (!Registered)
		{
			Components.push_back(this);
			Registered = true;
		}
	}

	void UIInput::Unregister(SceneGraph* Scene)
	{
		for (std::vector<IComponent*>::iterator i = Components.begin(); i != Components.end(); i++)
			if ((*i) == this) { Components.erase(i); break; }
		Registered = false;
	}

	std::vector<IComponent*> &UIInput::GetComponents()
	{
		return Components;
	}

	void UIInput::SetText(const std::string &t)
	{
		text = t;
		if (maxLength > 0 && text.size() > maxLength) text.resize(maxLength);
		caret = (uint32)text.size();
		dirty = true;
	}

	void UIInput::SetCaret(const uint32 index)
	{
		caret = index > (uint32)text.size() ? (uint32)text.size() : index;
		dirty = true;
	}

	bool UIInput::Allowed(const char c) const
	{
		// Control characters never belong in a single-line field, whatever
		// the filter says - a pasted newline would otherwise end up in the
		// value and draw as a missing glyph.
		if ((unsigned char)c < 0x20) return false;
		if (filter.empty()) return true;
		return filter.find(c) != std::string::npos;
	}

	uint32 UIInput::OnText(const std::string &utf8)
	{
		if (!interactable || readOnly) return UIEventFlag::None;

		std::string accepted;
		for (size_t i = 0; i < utf8.size(); i++)
			if (Allowed(utf8[i])) accepted += utf8[i];
		if (accepted.empty()) return UIEventFlag::None;

		// Over the limit inserts nothing rather than a partial run: typing
		// into a full field should feel like nothing happened, not like
		// half of what you typed arrived.
		if (maxLength > 0 && text.size() + accepted.size() > maxLength) return UIEventFlag::None;

		text.insert(caret, accepted);
		caret += (uint32)accepted.size();
		dirty = true;
		// Typing puts the caret back on: a blink that happened to be off
		// when a key landed would look like the field ignored it.
		blinkOn = true;
		return UIEventFlag::Changed;
	}

	uint32 UIInput::OnKey(const uint32 key, bool &claimed)
	{
		claimed = false;
		if (!interactable) return UIEventFlag::None;

		blinkOn = true;
		switch (key)
		{
		case UIKey::Left:
			// Claimed at the ends too: a caret at zero that let Left walk
			// the focus out of the field would make editing the start of a
			// value impossible to do twice.
			claimed = true;
			if (caret > 0) { caret--; dirty = true; }
			return UIEventFlag::None;
		case UIKey::Right:
			claimed = true;
			if (caret < (uint32)text.size()) { caret++; dirty = true; }
			return UIEventFlag::None;
		case UIKey::Home:
			claimed = true;
			caret = 0; dirty = true;
			return UIEventFlag::None;
		case UIKey::End:
			claimed = true;
			caret = (uint32)text.size(); dirty = true;
			return UIEventFlag::None;
		case UIKey::Backspace:
			claimed = true;
			if (readOnly || caret == 0) return UIEventFlag::None;
			text.erase(caret - 1, 1);
			caret--;
			dirty = true;
			return UIEventFlag::Changed;
		case UIKey::Delete:
			claimed = true;
			if (readOnly || caret >= (uint32)text.size()) return UIEventFlag::None;
			text.erase(caret, 1);
			dirty = true;
			return UIEventFlag::Changed;
		case UIKey::Enter:
			claimed = true;
			beforeEdit = text;
			return UIEventFlag::Submitted;
		case UIKey::Escape:
			// Back to what it held when it was focused. Claimed only if it
			// actually undid something, so Escape still closes the menu
			// around an untouched field.
			if (text == beforeEdit) return UIEventFlag::None;
			claimed = true;
			text = beforeEdit;
			caret = (uint32)text.size();
			dirty = true;
			return UIEventFlag::Changed;
		default:
			return UIEventFlag::None;
		}
	}

	void UIInput::SetWidgetFocused(const bool on)
	{
		UIWidget::SetWidgetFocused(on);
		// The revert point is taken on focus rather than on the first
		// keystroke: Escape means "put it back to what it was when I
		// started", and starting is when the caret arrived.
		if (on) beforeEdit = text;
		blinkOn = true;
		dirty = true;
	}

	uint32 UIInput::OnPointer(const bool inside, const bool down,
		const Vec2 &point, const UIRectValue &rect)
	{
		if (!interactable || !inside || !down) return UIEventFlag::None;

		// Click to place the caret: walk the prefixes and take the closest
		// gap, so clicking the right half of a character puts the caret
		// after it the way every other field does.
		UIText* label = NULL;
		f32 labelLeft = rect.x;
		if (GetOwner())
		{
			const std::vector<std::shared_ptr<GameObject> > &kids = GetOwner()->GetChildren();
			for (size_t i = 0; i < kids.size() && !label; i++)
			{
				if (!kids[i] || !UINameMatches(kids[i]->GetName(), textName)) continue;
				const std::vector<std::shared_ptr<IComponent> > &cs = kids[i]->GetComponents();
				for (size_t j = 0; j < cs.size(); j++)
				{
					if (!cs[j]) continue;
					if (cs[j]->GetComponentType() == ComponentType::UIText) label = static_cast<UIText*>(cs[j].get());
					else if (cs[j]->GetComponentType() == ComponentType::UIRect)
						labelLeft = static_cast<UIRect*>(cs[j].get())->GetRect().x;
				}
			}
		}
		if (!label || !label->GetFont()) return UIEventFlag::None;

		const f32 scale = label->GetSize() / label->GetFont()->GetFontSize();
		const f32 want = point.x - labelLeft;
		uint32 best = 0;
		f32 bestDistance = -1.f;
		for (uint32 i = 0; i <= (uint32)text.size(); i++)
		{
			const f32 at = label->GetFont()->MeasureAdvance(text.substr(0, i)) * scale;
			const f32 d = at > want ? at - want : want - at;
			if (bestDistance < 0.f || d < bestDistance) { bestDistance = d; best = i; }
		}
		if (best != caret) { caret = best; dirty = true; }
		blinkOn = true;
		return UIEventFlag::None;
	}

	void UIInput::Update(const f64 time)
	{
		// The caret blinks only while the field has focus, and holds solid
		// while the rate is zero - which is what a test wants, and what a
		// style that draws no caret at all wants.
		if (widgetFocused && blinkRate > 0.f)
		{
			if (blinkAt == 0.0) blinkAt = time;
			if (time - blinkAt >= (f64)blinkRate)
			{
				blinkAt = time;
				blinkOn = !blinkOn;
				dirty = true;
			}
		}
		else if (blinkAt != 0.0) { blinkAt = 0.0; blinkOn = true; dirty = true; }

		if (dirty) Apply();
	}

	void UIInput::Apply()
	{
		if (!GetOwner()) return;

		const std::string shown = password ? std::string(text.size(), maskChar) : text;

		UIText* label = NULL;
		UIRect* labelRect = NULL;
		UIRect* caretRect = NULL;
		UIRect* placeholderRect = NULL;
		UIText* placeholderLabel = NULL;

		const std::vector<std::shared_ptr<GameObject> > &kids = GetOwner()->GetChildren();
		for (size_t i = 0; i < kids.size(); i++)
		{
			if (!kids[i]) continue;
			const bool isText = UINameMatches(kids[i]->GetName(), textName);
			const bool isPlaceholder = UINameMatches(kids[i]->GetName(), placeholderName);
			const bool isCaret = UINameMatches(kids[i]->GetName(), caretName);
			if (!isText && !isPlaceholder && !isCaret) continue;

			const std::vector<std::shared_ptr<IComponent> > &cs = kids[i]->GetComponents();
			for (size_t j = 0; j < cs.size(); j++)
			{
				if (!cs[j]) continue;
				const uint32 t = cs[j]->GetComponentType();
				if (t == ComponentType::UIText)
				{
					if (isText) label = static_cast<UIText*>(cs[j].get());
					else if (isPlaceholder) placeholderLabel = static_cast<UIText*>(cs[j].get());
				}
				else if (t == ComponentType::UIRect)
				{
					if (isText) labelRect = static_cast<UIRect*>(cs[j].get());
					else if (isPlaceholder) placeholderRect = static_cast<UIRect*>(cs[j].get());
					else if (isCaret) caretRect = static_cast<UIRect*>(cs[j].get());
				}
			}
		}

		if (label) label->SetText(shown);
		if (placeholderLabel) placeholderLabel->SetText(placeholder);
		// The placeholder is a separate element rather than a substituted
		// string, so it can be styled differently - which is the only
		// reason anyone has a placeholder rather than a default value.
		if (placeholderRect) placeholderRect->SetVisible(text.empty());

		if (caretRect)
		{
			// Hidden unless focused, so an unfocused field does not sit
			// there with a caret in it.
			caretRect->SetVisible(widgetFocused && blinkOn && !readOnly);
			if (label && label->GetFont())
			{
				const f32 scale = label->GetSize() / label->GetFont()->GetFontSize();
				const f32 at = label->GetFont()->MeasureAdvance(shown.substr(0, caret)) * scale;
				// Pinned to the label's left edge and offset along it, so
				// the caret tracks the text rather than the field: a
				// centred or right-aligned label is a different x for the
				// same caret index.
				const Vec2 aMin = caretRect->GetAnchorMin();
				const Vec2 aMax = caretRect->GetAnchorMax();
				const Vec2 offMin = caretRect->GetOffsetMin();
				const Vec2 offMax = caretRect->GetOffsetMax();
				const f32 width = offMax.x - offMin.x;
				const f32 base = labelRect ? (labelRect->GetOffsetMin().x) : 0.f;
				caretRect->SetAnchors(Vec2(aMin.x, aMin.y), Vec2(aMin.x, aMax.y));
				caretRect->SetOffsets(Vec2(base + at, offMin.y), Vec2(base + at + width, offMax.y));
			}
		}

		dirty = false;
	}

};
