//============================================================================
// Name        : UISlider
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : A value dragged along a track
//============================================================================

#include <Pyros3D/Rendering/Components/UI/UISlider.h>
#include <Pyros3D/Rendering/Components/UI/UIRect.h>
#include <Pyros3D/GameObjects/GameObject.h>
#include <cmath>

namespace p3d {

	std::vector<IComponent*> UISlider::Components;

	UISlider::UISlider() : UIWidget()
	{
		value = 0.f;
		minValue = 0.f;
		maxValue = 1.f;
		step = 0.f;
		vertical = false;
		dragging = false;
		dirty = true;
		// True for the same reason UIButton's is: a pointer already held
		// when the slider appears must not be read as a press on it.
		wasDown = true;
		fillName = "Fill";
		handleName = "Handle";
	}

	UISlider::~UISlider() {}

	void UISlider::Register(SceneGraph* Scene)
	{
		if (!Registered)
		{
			Components.push_back(this);
			Registered = true;
		}
	}

	void UISlider::Unregister(SceneGraph* Scene)
	{
		for (std::vector<IComponent*>::iterator i = Components.begin(); i != Components.end(); i++)
			if ((*i) == this) { Components.erase(i); break; }
		Registered = false;
	}

	std::vector<IComponent*> &UISlider::GetComponents()
	{
		return Components;
	}

	f32 UISlider::Snap(const f32 v) const
	{
		f32 c = v < minValue ? minValue : (v > maxValue ? maxValue : v);
		if (step > 0.f)
		{
			const f32 steps = floorf((c - minValue) / step + 0.5f);
			c = minValue + steps * step;
			if (c > maxValue) c = maxValue;
		}
		return c;
	}

	void UISlider::SetValue(const f32 v)
	{
		const f32 snapped = Snap(v);
		if (snapped == value && !dirty) return;
		value = snapped;
		dirty = true;
	}

	void UISlider::SetRange(const f32 minimum, const f32 maximum)
	{
		minValue = minimum;
		// A zero-width range would divide by zero everywhere below, and is
		// never what anyone meant.
		maxValue = maximum > minimum ? maximum : minimum + 0.0001f;
		SetValue(value);
		dirty = true;
	}

	f32 UISlider::GetNormalized() const
	{
		return (value - minValue) / (maxValue - minValue);
	}

	void UISlider::Update(const f64 time)
	{
		if (dirty) Apply();
	}

	uint32 UISlider::OnPointer(const bool inside, const bool down,
		const Vec2 &point, const UIRectValue &rect)
	{
		if (!interactable) { dragging = false; wasDown = down; return UIEventFlag::None; }

		// Armed on the press, exactly as a button is: the drag has to have
		// started on the track, or dragging across the screen with the
		// button held would grab every slider it passed over.
		if (down && !wasDown && inside) dragging = true;
		else if (!down) dragging = false;
		wasDown = down;

		if (!dragging) return UIEventFlag::None;

		// Off the ends is a legitimate place to drag to - the pointer
		// leaving the track while held should pin the value to that end
		// rather than stop responding.
		f32 t;
		if (vertical)
			t = rect.height > 0.f ? 1.f - (point.y - rect.y) / rect.height : 0.f;
		else
			t = rect.width > 0.f ? (point.x - rect.x) / rect.width : 0.f;
		if (t < 0.f) t = 0.f;
		if (t > 1.f) t = 1.f;

		const f32 before = value;
		SetValue(minValue + t * (maxValue - minValue));
		return value != before ? UIEventFlag::Changed : UIEventFlag::None;
	}

	uint32 UISlider::OnKey(const uint32 key, bool &claimed)
	{
		claimed = false;
		if (!interactable) return UIEventFlag::None;

		// A step of nothing still has to move: a hundredth of the range is
		// the usual granularity for a continuous slider on the keyboard.
		const f32 delta = step > 0.f ? step : (maxValue - minValue) * 0.01f;
		f32 target = value;
		if (vertical)
		{
			if (key == UIKey::Up) target = value + delta;
			else if (key == UIKey::Down) target = value - delta;
			else return UIEventFlag::None;
		}
		else
		{
			if (key == UIKey::Right) target = value + delta;
			else if (key == UIKey::Left) target = value - delta;
			else return UIEventFlag::None;
		}

		// Claimed either way: an arrow along the slider's own axis belongs
		// to the slider even at the end of its range, or holding left at
		// zero would jump the focus to whatever is beside it.
		claimed = true;
		const f32 before = value;
		SetValue(target);
		return value != before ? UIEventFlag::Changed : UIEventFlag::None;
	}

	void UISlider::Apply()
	{
		if (!GetOwner()) return;
		const f32 t = GetNormalized();

		const std::vector<std::shared_ptr<GameObject> > &kids = GetOwner()->GetChildren();
		for (size_t i = 0; i < kids.size(); i++)
		{
			if (!kids[i]) continue;
			const bool isFill = kids[i]->GetName() == fillName;
			const bool isHandle = kids[i]->GetName() == handleName;
			if (!isFill && !isHandle) continue;

			const std::vector<std::shared_ptr<IComponent> > &cs = kids[i]->GetComponents();
			for (size_t j = 0; j < cs.size(); j++)
			{
				if (!cs[j] || cs[j]->GetComponentType() != ComponentType::UIRect) continue;
				UIRect* r = static_cast<UIRect*>(cs[j].get());
				Vec2 aMin = r->GetAnchorMin();
				Vec2 aMax = r->GetAnchorMax();

				// Anchors only. Offsets are the author's - a fill inset by
				// two pixels inside its track stays inset at every value.
				if (isFill)
				{
					// Vertical fills upwards, so it is the minimum edge
					// that moves: canvas y grows downwards.
					if (vertical) { aMin.y = 1.f - t; aMax.y = 1.f; }
					else { aMin.x = 0.f; aMax.x = t; }
				}
				else
				{
					// Pinned, so the handle keeps whatever size its offsets
					// give it and simply travels.
					if (vertical) { aMin.y = aMax.y = 1.f - t; }
					else { aMin.x = aMax.x = t; }
				}
				r->SetAnchors(aMin, aMax);
			}
		}
		dirty = false;
	}

};
