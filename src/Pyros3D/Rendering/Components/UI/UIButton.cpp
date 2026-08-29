//============================================================================
// Name        : UIButton
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : An interactive canvas element with visual states
//============================================================================

#include <Pyros3D/Rendering/Components/UI/UIButton.h>
#include <Pyros3D/Rendering/Components/UI/UIImage.h>
#include <Pyros3D/Rendering/Components/UI/UIText.h>
#include <Pyros3D/GameObjects/GameObject.h>

namespace p3d {

	std::vector<IComponent*> UIButton::Components;

	UIButton::UIButton() : IComponent()
	{
		transition = 0.12f;
		current = UIState::Normal;
		interactable = true;
		pressedInside = false;
		clicked = false;
		blendInitialized = false;
		// True, so the very first frame observed can never look like a
		// press transition - a pointer already held down when the canvas
		// appears must not click whatever it happens to be over.
		wasDown = true;
		blendTint = Vec4(1.f, 1.f, 1.f, 1.f);
		blendTextColor = Vec4(1.f, 1.f, 1.f, 1.f);
		blendOffset = Vec2(0.f, 0.f);

		// A button with no styling set is still a button: Normal inherits
		// whatever the image and text already are (see ApplyState), and the
		// other states start unset, which means "same as Normal".
		states[UIState::Pressed].offset = Vec2(0.f, 2.f);
		states[UIState::Pressed].hasTint = false;
	}

	UIButton::~UIButton() {}

	void UIButton::Register(SceneGraph* Scene)
	{
		if (!Registered)
		{
			Components.push_back(this);
			Registered = true;
		}
	}

	void UIButton::Unregister(SceneGraph* Scene)
	{
		for (std::vector<IComponent*>::iterator i = Components.begin(); i != Components.end(); i++)
			if ((*i) == this) { Components.erase(i); break; }
		Registered = false;
	}

	std::vector<IComponent*> &UIButton::GetComponents()
	{
		return Components;
	}

	void UIButton::SetInteractable(bool on)
	{
		interactable = on;
		if (!on) { pressedInside = false; current = UIState::Disabled; }
		else if (current == UIState::Disabled) current = UIState::Normal;
	}

	bool UIButton::OnPointer(bool inside, bool down)
	{
		if (!interactable)
		{
			current = UIState::Disabled;
			pressedInside = false;
			wasDown = down;
			return false;
		}

		bool fired = false;
		if (down)
		{
			// Armed only on the transition from up to down while inside. The
			// first version armed on any frame where the pointer was down
			// and inside, so dragging onto a button with the button already
			// held made it clickable - which is exactly the accident a
			// press-and-drag-away is supposed to let you recover from.
			if (!wasDown && inside) pressedInside = true;
			current = pressedInside ? (inside ? UIState::Pressed : UIState::Normal)
				: (inside ? UIState::Hover : UIState::Normal);
		}
		else
		{
			// Released. Press and release both inside is a click; releasing
			// after dragging off cancels, which is what every other UI does
			// and what makes a mis-aimed press recoverable.
			if (pressedInside && inside) { fired = true; clicked = true; }
			pressedInside = false;
			current = inside ? UIState::Hover : UIState::Normal;
		}
		wasDown = down;
		return fired;
	}

	bool UIButton::ConsumeClicked()
	{
		const bool c = clicked;
		clicked = false;
		return c;
	}

	void UIButton::Update(const f64 time)
	{
		ApplyState(time);
	}

	void UIButton::ApplyState(const f64 time)
	{
		if (!GetOwner()) return;

		UIImage* image = NULL;
		UIText* text = NULL;
		UIRect* rect = NULL;
		const std::vector<std::shared_ptr<IComponent> > &cs = GetOwner()->GetComponents();
		for (size_t i = 0; i < cs.size(); i++)
		{
			if (!cs[i]) continue;
			if (cs[i]->GetComponentType() == ComponentType::UIImage) image = static_cast<UIImage*>(cs[i].get());
			else if (cs[i]->GetComponentType() == ComponentType::UIText) text = static_cast<UIText*>(cs[i].get());
			else if (cs[i]->GetComponentType() == ComponentType::UIRect) rect = static_cast<UIRect*>(cs[i].get());
		}
		// A label is usually a child of the button rather than a sibling,
		// since it needs its own rect to be padded inside.
		if (!text)
		{
			const std::vector<std::shared_ptr<GameObject> > &kids = GetOwner()->GetChildren();
			for (size_t i = 0; i < kids.size() && !text; i++)
			{
				if (!kids[i]) continue;
				const std::vector<std::shared_ptr<IComponent> > &kc = kids[i]->GetComponents();
				for (size_t j = 0; j < kc.size(); j++)
					if (kc[j] && kc[j]->GetComponentType() == ComponentType::UIText)
					{ text = static_cast<UIText*>(kc[j].get()); break; }
			}
		}

		// Normal is not a style so much as a baseline: whatever the element
		// was authored as. Reading it from the components means a button
		// added to an existing image does not blank it.
		const UIStateStyle &normal = states[UIState::Normal];
		Vec4 targetTint = normal.hasTint ? normal.tint : (image ? image->GetTint() : Vec4(1.f, 1.f, 1.f, 1.f));
		Vec4 targetText = normal.hasTextColor ? normal.textColor : (text ? text->GetColor() : Vec4(1.f, 1.f, 1.f, 1.f));
		Vec2 targetOffset = normal.offset;

		if (current != UIState::Normal)
		{
			const UIStateStyle &s = states[current];
			if (s.hasTint) targetTint = s.tint;
			if (s.hasTextColor) targetText = s.textColor;
			targetOffset = s.offset;
		}

		if (!blendInitialized)
		{
			blendTint = targetTint;
			blendTextColor = targetText;
			blendOffset = targetOffset;
			blendInitialized = true;
		}
		else
		{
			// Framerate-independent approach, not a fixed step per frame:
			// `transition` is a duration, and it has to mean the same thing
			// at 30fps and at 240.
			f32 k = 1.f;
			if (transition > 0.f)
			{
				const f32 dt = (f32)(time - lastTime);
				k = (dt > 0.f && dt < 1.f) ? (dt / transition) : 1.f;
				if (k > 1.f) k = 1.f;
			}
			blendTint = blendTint + (targetTint - blendTint) * k;
			blendTextColor = blendTextColor + (targetText - blendTextColor) * k;
			blendOffset = Vec2(blendOffset.x + (targetOffset.x - blendOffset.x) * k,
				blendOffset.y + (targetOffset.y - blendOffset.y) * k);
		}
		lastTime = time;

		if (image) image->SetTint(blendTint);
		if (text) text->SetColor(blendTextColor);
		if (rect)
		{
			// Applied on top of the authored offsets rather than into them,
			// so a press nudge never becomes part of the saved layout.
			rect->SetStateOffset(blendOffset);
		}
	}

};
