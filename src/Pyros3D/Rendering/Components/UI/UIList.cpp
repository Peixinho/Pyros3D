//============================================================================
// Name        : UIList
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : A scrolling list of values over a fixed set of rows
//============================================================================

#include <Pyros3D/Rendering/Components/UI/UIList.h>
#include <Pyros3D/Rendering/Components/UI/UIRect.h>
#include <Pyros3D/Rendering/Components/UI/UIText.h>
#include <Pyros3D/GameObjects/GameObject.h>

namespace p3d {

	std::vector<IComponent*> UIList::Components;

	namespace {
		const std::string kNoItem;

		// A named element inside a row, which is usually a child of it but
		// may be the row itself - a row whose whole background is the
		// highlight is a reasonable thing to author.
		GameObject* FindIn(GameObject* root, const std::string &name)
		{
			if (!root || name.empty()) return NULL;
			if (root->GetName() == name) return root;
			const std::vector<std::shared_ptr<GameObject> > &kids = root->GetChildren();
			for (size_t i = 0; i < kids.size(); i++)
			{
				if (!kids[i]) continue;
				if (GameObject* found = FindIn(kids[i].get(), name)) return found;
			}
			return NULL;
		}

		UIRect* RectOn(GameObject* go)
		{
			if (!go) return NULL;
			const std::vector<std::shared_ptr<IComponent> > &cs = go->GetComponents();
			for (size_t i = 0; i < cs.size(); i++)
				if (cs[i] && cs[i]->GetComponentType() == ComponentType::UIRect)
					return static_cast<UIRect*>(cs[i].get());
			return NULL;
		}

		UIText* TextIn(GameObject* go)
		{
			if (!go) return NULL;
			const std::vector<std::shared_ptr<IComponent> > &cs = go->GetComponents();
			for (size_t i = 0; i < cs.size(); i++)
				if (cs[i] && cs[i]->GetComponentType() == ComponentType::UIText)
					return static_cast<UIText*>(cs[i].get());
			return NULL;
		}
	}

	UIList::UIList() : UIWidget()
	{
		selected = -1;
		itemHeight = 24.f;
		scroll = 0.f;
		dirty = true;
		wasDown = true;
		visibleRows = 0;
		rowPrefix = "Row";
		labelName = "Label";
		highlightName = "Highlight";
	}

	UIList::~UIList() {}

	void UIList::Register(SceneGraph* Scene)
	{
		if (!Registered)
		{
			Components.push_back(this);
			Registered = true;
		}
	}

	void UIList::Unregister(SceneGraph* Scene)
	{
		for (std::vector<IComponent*>::iterator i = Components.begin(); i != Components.end(); i++)
			if ((*i) == this) { Components.erase(i); break; }
		Registered = false;
	}

	std::vector<IComponent*> &UIList::GetComponents()
	{
		return Components;
	}

	void UIList::SetItems(const std::vector<std::string> &v)
	{
		items = v;
		// A selection that no longer exists is worse than none: it would
		// highlight whatever moved into that index.
		if (selected >= (int32)items.size()) selected = -1;
		SetScroll(scroll);
		dirty = true;
	}

	void UIList::AddItem(const std::string &item)
	{
		items.push_back(item);
		dirty = true;
	}

	void UIList::ClearItems()
	{
		items.clear();
		selected = -1;
		scroll = 0.f;
		dirty = true;
	}

	void UIList::SetSelected(const int32 index)
	{
		const int32 clamped = (index < 0 || index >= (int32)items.size()) ? -1 : index;
		if (clamped == selected) return;
		selected = clamped;
		dirty = true;
	}

	const std::string &UIList::GetSelectedItem() const
	{
		if (selected < 0 || selected >= (int32)items.size()) return kNoItem;
		return items[selected];
	}

	f32 UIList::GetMaxScroll() const
	{
		const f32 content = (f32)items.size() * itemHeight;
		const f32 view = lastRect.height;
		return content > view ? content - view : 0.f;
	}

	void UIList::SetScroll(const f32 offset)
	{
		const f32 maximum = GetMaxScroll();
		f32 c = offset < 0.f ? 0.f : offset;
		if (c > maximum) c = maximum;
		if (c == scroll) return;
		scroll = c;
		dirty = true;
	}

	int32 UIList::RowAt(const Vec2 &point, const UIRectValue &rect) const
	{
		if (!rect.Contains(point)) return -1;
		const f32 local = point.y - rect.y + scroll;
		const int32 index = (int32)(local / itemHeight);
		if (index < 0 || index >= (int32)items.size()) return -1;
		return index;
	}

	uint32 UIList::OnPointer(const bool inside, const bool down,
		const Vec2 &point, const UIRectValue &rect)
	{
		lastRect = rect;
		if (!interactable) { wasDown = down; return UIEventFlag::None; }

		// On the press, like everything else here: a drag that ends over a
		// list must not pick whatever it lands on.
		const bool pressed = down && !wasDown && inside;
		wasDown = down;
		if (!pressed) return UIEventFlag::None;

		const int32 row = RowAt(point, rect);
		if (row < 0) return UIEventFlag::None;

		// A second press on the row that is already selected activates it,
		// which is the double-click every file list has - without needing a
		// clock to decide what counts as double.
		if (row == selected) return UIEventFlag::Submitted;

		SetSelected(row);
		return UIEventFlag::Changed;
	}

	uint32 UIList::OnScroll(const f32 delta)
	{
		if (!interactable) return UIEventFlag::None;
		const f32 before = scroll;
		// A notch is a row. Positive is away from the user, which moves the
		// content down, which means scrolling towards the start.
		SetScroll(scroll - delta * itemHeight);
		// Unclaimed at the ends, so a list scrolled to its bottom inside a
		// scrolling panel hands the rest of the gesture outwards.
		return scroll != before ? UIEventFlag::Changed : UIEventFlag::None;
	}

	uint32 UIList::OnKey(const uint32 key, bool &claimed)
	{
		claimed = false;
		if (!interactable || items.empty()) return UIEventFlag::None;

		int32 target = selected;
		if (key == UIKey::Up) { claimed = true; target = selected <= 0 ? 0 : selected - 1; }
		else if (key == UIKey::Down) { claimed = true; target = selected < 0 ? 0 : selected + 1; }
		else if (key == UIKey::Home) { claimed = true; target = 0; }
		else if (key == UIKey::End) { claimed = true; target = (int32)items.size() - 1; }
		else if (key == UIKey::Enter) { claimed = true; return selected >= 0 ? UIEventFlag::Submitted : UIEventFlag::None; }
		else return UIEventFlag::None;

		if (target >= (int32)items.size()) target = (int32)items.size() - 1;
		if (target == selected) return UIEventFlag::None;
		SetSelected(target);

		// Follow the selection: a keyboard walk that left the selected row
		// off-screen would be worse than no keyboard support.
		const f32 top = (f32)target * itemHeight;
		if (top < scroll) SetScroll(top);
		else if (top + itemHeight > scroll + lastRect.height) SetScroll(top + itemHeight - lastRect.height);
		return UIEventFlag::Changed;
	}

	uint32 UIList::Activate()
	{
		if (!interactable || selected < 0) return UIEventFlag::None;
		return UIEventFlag::Submitted;
	}

	void UIList::Update(const f64 time)
	{
		if (dirty) Apply();
	}

	void UIList::Apply()
	{
		if (!GetOwner()) return;

		// A list always clips: rows scrolled past its edge have to stop
		// existing visually rather than draw over what is around them.
		if (UIRect* own = RectOn(GetOwner())) own->SetClipChildren(true);

		// Rows in order, by name. The first one visible is whichever the
		// scroll has reached; the rest follow it, and any left over are
		// hidden rather than left showing a stale value.
		const int32 first = (int32)(scroll / itemHeight);
		const f32 fraction = scroll - (f32)first * itemHeight;

		const std::vector<std::shared_ptr<GameObject> > &kids = GetOwner()->GetChildren();
		uint32 rowIndex = 0;
		visibleRows = 0;
		for (size_t i = 0; i < kids.size(); i++)
		{
			if (!kids[i]) continue;
			const std::string &name = kids[i]->GetName();
			if (name.compare(0, rowPrefix.size(), rowPrefix) != 0) continue;

			GameObject* row = kids[i].get();
			UIRect* rect = RectOn(row);
			if (!rect) continue;

			const int32 item = first + (int32)rowIndex;
			const bool used = item >= 0 && item < (int32)items.size();
			rect->SetVisible(used);
			if (used)
			{
				// Pinned to the top edge and moved down: the row keeps
				// whatever width its anchors give it, so a list that
				// stretches still has full-width rows.
				const f32 y = (f32)rowIndex * itemHeight - fraction;
				const Vec2 aMin = rect->GetAnchorMin();
				const Vec2 aMax = rect->GetAnchorMax();
				rect->SetAnchors(Vec2(aMin.x, 0.f), Vec2(aMax.x, 0.f));
				rect->SetOffsets(Vec2(rect->GetOffsetMin().x, y),
					Vec2(rect->GetOffsetMax().x, y + itemHeight));

				if (UIText* label = TextIn(FindIn(row, labelName))) label->SetText(items[item]);
				if (UIRect* highlight = RectOn(FindIn(row, highlightName)))
					highlight->SetVisible(item == selected);
				visibleRows++;
			}
			rowIndex++;
		}
		dirty = false;
	}

};
