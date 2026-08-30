//============================================================================
// Name        : UIToggle
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : A checkbox: a button that remembers what it was set to
//============================================================================

#include <Pyros3D/Rendering/Components/UI/UIToggle.h>
#include <Pyros3D/GameObjects/GameObject.h>
#include <Pyros3D/Rendering/Components/UI/UIRect.h>

namespace p3d {

	std::vector<IComponent*> UIToggle::Components;

	UIToggle::UIToggle() : UIButton()
	{
		value = false;
		applied = false;
		checkName = "Check";
	}

	UIToggle::~UIToggle() {}

	void UIToggle::Register(SceneGraph* Scene)
	{
		if (!Registered)
		{
			Components.push_back(this);
			Registered = true;
		}
	}

	void UIToggle::Unregister(SceneGraph* Scene)
	{
		for (std::vector<IComponent*>::iterator i = Components.begin(); i != Components.end(); i++)
			if ((*i) == this) { Components.erase(i); break; }
		Registered = false;
	}

	std::vector<IComponent*> &UIToggle::GetComponents()
	{
		return Components;
	}

	void UIToggle::SetValue(const bool on)
	{
		if (value == on && applied) return;
		value = on;
		applied = false;
		if (value) ClearGroupSiblings();
	}

	void UIToggle::Update(const f64 time)
	{
		UIButton::Update(time);
		// Every frame rather than only on change: the check element may not
		// exist yet when the value is set (a scene still loading, a prefab
		// still being assembled), and `applied` only latches once it has
		// actually been found and written.
		if (!applied) ApplyValue();
	}

	uint32 UIToggle::OnPointer(const bool inside, const bool down,
		const Vec2 &point, const UIRectValue &rect)
	{
		if (!UIButton::OnPointer(inside, down)) return UIEventFlag::None;
		SetValue(!value);
		// Clicked as well as Changed: a host that only listens for clicks
		// still hears the checkbox, and one that listens for changes does
		// not have to know a pointer was involved.
		return UIEventFlag::Changed | UIEventFlag::Clicked;
	}

	uint32 UIToggle::Activate()
	{
		if (!interactable) return UIEventFlag::None;
		SetValue(!value);
		return UIEventFlag::Changed | UIEventFlag::Clicked;
	}

	void UIToggle::ApplyValue()
	{
		if (!GetOwner() || checkName.empty()) { applied = true; return; }

		const std::vector<std::shared_ptr<GameObject> > &kids = GetOwner()->GetChildren();
		for (size_t i = 0; i < kids.size(); i++)
		{
			if (!kids[i] || !UINameMatches(kids[i]->GetName(), checkName)) continue;
			// Through the rect's own visibility, so a tick mark that is a
			// small tree of its own hides as a whole - and so the canvas
			// skips it outright rather than solving something invisible.
			const std::vector<std::shared_ptr<IComponent> > &cs = kids[i]->GetComponents();
			for (size_t j = 0; j < cs.size(); j++)
			{
				if (!cs[j] || cs[j]->GetComponentType() != ComponentType::UIRect) continue;
				static_cast<UIRect*>(cs[j].get())->SetVisible(value);
				applied = true;
			}
			return;
		}
	}

	void UIToggle::ClearGroupSiblings()
	{
		if (group.empty() || !GetOwner() || !GetOwner()->GetParent()) return;

		// Siblings only. A group that reached across the whole canvas would
		// make two unrelated sets of radio buttons that happened to share a
		// name silently fight each other.
		const std::vector<std::shared_ptr<GameObject> > &kids = GetOwner()->GetParent()->GetChildren();
		for (size_t i = 0; i < kids.size(); i++)
		{
			if (!kids[i] || kids[i].get() == GetOwner()) continue;
			const std::vector<std::shared_ptr<IComponent> > &cs = kids[i]->GetComponents();
			for (size_t j = 0; j < cs.size(); j++)
			{
				if (!cs[j] || cs[j]->GetComponentType() != ComponentType::UIToggle) continue;
				UIToggle* other = static_cast<UIToggle*>(cs[j].get());
				if (other->GetGroup() != group) continue;
				other->value = false;
				other->applied = false;
			}
		}
	}

};
