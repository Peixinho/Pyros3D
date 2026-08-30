//============================================================================
// Name        : UISlider.h
// Author      : Duarte Peixinho
// Version     :
// Copyright   : ;)
// Description : A value dragged along a track
//============================================================================

#ifndef UISLIDER_H
#define	UISLIDER_H

#include <Pyros3D/Rendering/Components/UI/UIWidget.h>
#include <string>
#include <vector>

namespace p3d {

	class PYROS3D_API UISlider : public UIWidget {

	public:

		UISlider();
		virtual ~UISlider();

		virtual void Register(SceneGraph* Scene);
		virtual void Init() {}
		virtual void Update(const f64 time = 0);
		virtual void Destroy() {}
		virtual void Unregister(SceneGraph* Scene);

		virtual uint32 GetComponentType() const { return ComponentType::UISlider; }

		static std::vector<IComponent*> &GetComponents();

		// Clamped to the range, and snapped to `step` when that is non-zero.
		void SetValue(const f32 v);
		f32 GetValue() const { return value; }
		// 0..1 across the range, which is what the fill and handle are
		// positioned by and what a style or a script usually wants.
		f32 GetNormalized() const;

		void SetRange(const f32 minimum, const f32 maximum);
		f32 GetMin() const { return minValue; }
		f32 GetMax() const { return maxValue; }

		// 0 is continuous. Otherwise the value snaps to multiples of this
		// from the minimum, so a volume slider can move in tenths.
		void SetStep(const f32 s) { step = s > 0.f ? s : 0.f; SetValue(value); }
		f32 GetStep() const { return step; }

		// Vertical sliders fill upwards: the minimum is at the bottom of
		// the track, which is what every volume fader does, even though
		// canvas y grows downwards.
		void SetVertical(const bool on) { vertical = on; dirty = true; }
		bool IsVertical() const { return vertical; }

		// The child elements this drives, by name - a name rather than a
		// pointer because that is what survives a save, a prefab and an
		// undo. The slider only ever writes their anchors, never their
		// offsets, so whatever padding or size they were authored with is
		// left alone.
		void SetFillElement(const std::string &name) { fillName = name; dirty = true; }
		const std::string &GetFillElement() const { return fillName; }
		void SetHandleElement(const std::string &name) { handleName = name; dirty = true; }
		const std::string &GetHandleElement() const { return handleName; }

		virtual uint32 OnPointer(const bool inside, const bool down,
			const Vec2 &point, const UIRectValue &rect);
		virtual uint32 OnKey(const uint32 key, bool &claimed);
		// Dragging then reaching for the arrow keys is the expected way to
		// fine-tune a slider, so it keeps the focus it was clicked with.
		virtual bool TakesFocusOnPress() const { return true; }

		bool IsDragging() const { return dragging; }

	private:

		void Apply();
		f32 Snap(const f32 v) const;

		f32 value, minValue, maxValue, step;
		bool vertical;
		bool dragging;
		bool wasDown;
		bool dirty;
		std::string fillName, handleName;

		static std::vector<IComponent*> Components;
	};

}

#endif	/* UISLIDER_H */
