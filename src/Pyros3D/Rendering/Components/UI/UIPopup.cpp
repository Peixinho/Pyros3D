//============================================================================
// Name        : UIPopup
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : A panel that opens over everything, optionally blocking it
//============================================================================

#include <Pyros3D/Rendering/Components/UI/UIPopup.h>
#include <Pyros3D/Rendering/Components/UI/UIRect.h>
#include <Pyros3D/GameObjects/GameObject.h>

namespace p3d {

	std::vector<IComponent*> UIPopup::Components;

	namespace {
		UIRect* RectOn(GameObject* go)
		{
			if (!go) return NULL;
			const std::vector<std::shared_ptr<IComponent> > &cs = go->GetComponents();
			for (size_t i = 0; i < cs.size(); i++)
				if (cs[i] && cs[i]->GetComponentType() == ComponentType::UIRect)
					return static_cast<UIRect*>(cs[i].get());
			return NULL;
		}

		GameObject* FindNamed(GameObject* root, const std::string &name)
		{
			if (!root || name.empty()) return NULL;
			const std::vector<std::shared_ptr<GameObject> > &kids = root->GetChildren();
			for (size_t i = 0; i < kids.size(); i++)
			{
				if (!kids[i]) continue;
				if (UINameMatches(kids[i]->GetName(), name)) return kids[i].get();
				if (GameObject* found = FindNamed(kids[i].get(), name)) return found;
			}
			return NULL;
		}
	}

	UIPopup::UIPopup() : UIWidget()
	{
		open = false;
		modal = true;
		closeOnEscape = true;
		closeOnOutside = true;
		applied = false;
		// True, so a pointer already held when a dialog opens cannot be read
		// as a press on the scrim and close it again immediately.
		wasDown = true;
		dialogName = "Dialog";
	}

	UIPopup::~UIPopup() {}

	void UIPopup::Register(SceneGraph* Scene)
	{
		if (!Registered)
		{
			Components.push_back(this);
			Registered = true;
		}
	}

	void UIPopup::Unregister(SceneGraph* Scene)
	{
		for (std::vector<IComponent*>::iterator i = Components.begin(); i != Components.end(); i++)
			if ((*i) == this) { Components.erase(i); break; }
		Registered = false;
	}

	std::vector<IComponent*> &UIPopup::GetComponents()
	{
		return Components;
	}

	void UIPopup::SetOpen(const bool on)
	{
		if (open == on && applied) return;
		open = on;
		applied = false;
		// A dialog that opens under a held pointer must not take that press
		// as a click on itself when the button comes up.
		wasDown = true;
	}

	void UIPopup::Update(const f64 time)
	{
		// Every frame until it lands: the rect may not exist yet when a
		// scene is still being assembled.
		if (!applied) Apply();
	}

	void UIPopup::Apply()
	{
		if (!GetOwner()) return;
		if (UIRect* r = RectOn(GetOwner()))
		{
			r->SetVisible(open);
			applied = true;
		}
	}

	uint32 UIPopup::OnPointer(const bool inside, const bool down,
		const Vec2 &point, const UIRectValue &rect)
	{
		const bool pressed = down && !wasDown;
		wasDown = down;
		if (!open || !pressed || !closeOnOutside) return UIEventFlag::None;

		// "Outside" means outside the dialog, not outside this component:
		// the popup's own rect covers the canvas, so a press on the scrim
		// is inside it and is exactly the press meant to dismiss it.
		const UIRectValue &dialog = dialogName.empty() ? rect
			: (RectOn(FindNamed(GetOwner(), dialogName)) ? RectOn(FindNamed(GetOwner(), dialogName))->GetRect() : rect);
		if (dialog.Contains(point)) return UIEventFlag::None;

		SetOpen(false);
		return UIEventFlag::Submitted;
	}

	uint32 UIPopup::OnKey(const uint32 key, bool &claimed)
	{
		claimed = false;
		if (!open || key != UIKey::Escape || !closeOnEscape) return UIEventFlag::None;
		claimed = true;
		SetOpen(false);
		return UIEventFlag::Submitted;
	}

};
