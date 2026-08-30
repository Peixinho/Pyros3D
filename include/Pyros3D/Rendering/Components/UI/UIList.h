//============================================================================
// Name        : UIList.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : A scrolling list of values over a fixed set of rows
//============================================================================

#ifndef UILIST_H
#define	UILIST_H

#include <Pyros3D/Rendering/Components/UI/UIWidget.h>
#include <string>
#include <vector>

namespace p3d {

	// The rows are authored, not generated: every child named with the row
	// prefix is one, and the list recycles them as it scrolls. Enough rows
	// to cover the viewport is enough rows for a list of any length, which
	// is what a virtualized list does anyway - and it means a row is a
	// normal element that can be styled, nested and skinned like everything
	// else, with no second subtree-duplicating mechanism next to prefabs.
	class PYROS3D_API UIList : public UIWidget {

	public:

		UIList();
		virtual ~UIList();

		virtual void Register(SceneGraph* Scene);
		virtual void Init() {}
		virtual void Update(const f64 time = 0);
		virtual void Destroy() {}
		virtual void Unregister(SceneGraph* Scene);

		virtual uint32 GetComponentType() const { return ComponentType::UIList; }

		static std::vector<IComponent*> &GetComponents();

		void SetItems(const std::vector<std::string> &items);
		const std::vector<std::string> &GetItems() const { return items; }
		void AddItem(const std::string &item);
		void ClearItems();

		// -1 for nothing selected, which is a real state: a list of things
		// to pick from starts with none of them picked.
		void SetSelected(const int32 index);
		int32 GetSelected() const { return selected; }
		const std::string &GetSelectedItem() const;

		// Row height in canvas units, including the gap after it. What the
		// scroll is measured in and what decides how many rows fit.
		void SetItemHeight(const f32 h) { itemHeight = h > 1.f ? h : 1.f; dirty = true; }
		f32 GetItemHeight() const { return itemHeight; }

		// Canvas units from the top of the list, clamped so the last item
		// can reach the bottom edge and no further.
		void SetScroll(const f32 offset);
		f32 GetScroll() const { return scroll; }
		f32 GetMaxScroll() const;

		// Children whose name starts with this are rows, in order.
		void SetRowPrefix(const std::string &prefix) { rowPrefix = prefix; dirty = true; }
		const std::string &GetRowPrefix() const { return rowPrefix; }

		// Inside a row: the label that shows the item, and the element that
		// is shown only while that row is the selected one.
		void SetLabelElement(const std::string &name) { labelName = name; dirty = true; }
		const std::string &GetLabelElement() const { return labelName; }
		void SetHighlightElement(const std::string &name) { highlightName = name; dirty = true; }
		const std::string &GetHighlightElement() const { return highlightName; }

		// Fired when a row is activated rather than merely selected - a
		// second click on the selected row, or Enter.
		void SetOnSubmit(const std::string &handler) { onSubmit = handler; }
		const std::string &GetOnSubmit() const { return onSubmit; }

		// How many rows the last layout actually filled. Below the number
		// of row elements when the list is shorter than its viewport - the
		// spare rows are hidden rather than left showing stale values.
		uint32 GetVisibleRows() const { return visibleRows; }

		virtual uint32 OnPointer(const bool inside, const bool down,
			const Vec2 &point, const UIRectValue &rect);
		virtual uint32 OnScroll(const f32 delta);
		virtual uint32 OnKey(const uint32 key, bool &claimed);
		virtual uint32 Activate();
		// Arrow keys walk the rows once it has been clicked, rather than
		// walking away from the list.
		virtual bool TakesFocusOnPress() const { return true; }

		// Row index under a canvas point, or -1. `rect` is the list's own
		// solved rect - the same one the canvas hands OnPointer.
		int32 RowAt(const Vec2 &point, const UIRectValue &rect) const;

	private:

		void Apply();

		std::vector<std::string> items;
		int32 selected;
		f32 itemHeight;
		f32 scroll;
		bool dirty;
		bool wasDown;
		uint32 visibleRows;
		std::string rowPrefix, labelName, highlightName, onSubmit;
		// The rect the last layout ran against, so scrolling with the wheel
		// can clamp without waiting for the next pointer event.
		UIRectValue lastRect;

		static std::vector<IComponent*> Components;
	};

}

#endif	/* UILIST_H */
