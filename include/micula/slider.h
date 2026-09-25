// Micula / slider.h

#pragma once

#include "window.h"

#include <algorithm>
#include <cmath>
#include <cwctype>
#include <functional>
#include <string>
#include <vector>

namespace micula {


struct Slider : Widget {
    float value = 0.0f, lo = -1.0f, hi = 1.0f, step = 0.01f;
    // Two callbacks, because a slider produces two different kinds of event and the
    // caller wants different things from them. `onChange` fires on every pixel of the
    // drag -- that is the readout following the knob. `onCommit` fires once, when the
    // gesture ends, and that is the one to write a file from: saving on every change
    // rewrites the file a hundred times across one drag.
    //
    // A page that listens only to the first and saves nothing ends up with a value that
    // is correct on screen, correct in memory and absent from the disk -- so it comes
    // back wrong the next time the page is built.
    std::function<void(float)> onChange;
    std::function<void(float)> onCommit;
    // Between a press and its release, wherever the pointer has got to since.
    bool dragging = false;
    // Where the knob is drawn, as a fraction, which trails a value that snaps from one step
    // to the next. A step is the one thing about a stepped slider that is visible, and
    // answering it by jumping the knob says the knob and the value are the same thing. They
    // are not: the value is what the page is told and it is exact, the knob is where the
    // result is drawn -- so the knob eases to each new step while the value is already there.
    motion::Track drawn;

    Slider(float v, float a, float b, float s, std::function<void(float)> f)
        : value(v), lo(a), hi(b), step(s), onChange(std::move(f)), drawn(Frac()) {}

    bool Focusable() const override { return true; }
    // While it is being dragged: the knob follows the cursor, and the cursor moving is
    // the only thing that happens. `dragging` rather than `pressed`, because the window
    // clears `pressed` the moment the pointer leaves the rectangle -- which is what
    // makes a button cancellable by dragging off it, and which a knob dragged past its
    // own end must not do.
    bool TracksPointer() const override { return dragging; }
    // Held for the whole drag, not only while the pointer is inside the rectangle: the
    // thumb is the one thing on screen saying the gesture has not ended yet.
    bool PressedVisual() const override { return pressed || dragging; }
    float Frac() const { return (value - lo) / (hi - lo); }

    bool Animating() const override { return Widget::Animating() || drawn.Wants(Frac()); }
    void Tick(float dt) override {
        Widget::Tick(dt);
        // A curve rather than a follower, because a step is a place rather than a direction:
        // the knob arrives at it and stops, instead of closing a gap that a hand could keep
        // opening. Retargeting means a fast drag restarts this once a step, which is a move
        // the knob can make continuously.
        drawn.To(Frac());
        drawn.Step(dt, motion::kFaster);
    }

    void SetFromX(float x) {
        const float t = std::clamp((x - rect.left - 8) / (std::max)(1.0f, Width(rect) - 16), 0.0f, 1.0f);
        // Snapped to the step, so a slider that writes 0.02 into a settings file
        // cannot land on 0.019999999. Files like that are read by people.
        const float raw = lo + t * (hi - lo);
        const float snapped = std::round(raw / step) * step;
        // The drag reads the cursor at paint time, so this runs once a frame for as long
        // as the mouse is down. A frame that lands on the step the knob is already on
        // has nothing to report, and reporting it anyway would have onChange write the
        // settings file sixty times across one gesture.
        if (snapped == value) return;
        value = snapped;
        if (onChange) onChange(value);
    }
    void OnClick() override { /* handled by the press below */ }
    // The press takes the knob to where it landed rather than waiting for a frame: a
    // click short enough to be over before the window repaints is still a click.
    void OnPress(float x, float /*y*/) override { dragging = true; SetFromX(x); }
    void OnRelease() override { dragging = false; if (onCommit) onCommit(value); }
    bool OnKey(WPARAM vk) override {
        if (vk == VK_LEFT || vk == VK_DOWN)  { value = (std::max)(lo, value - step); }
        else if (vk == VK_RIGHT || vk == VK_UP) { value = (std::min)(hi, value + step); }
        else if (vk == VK_HOME) value = lo;
        else if (vk == VK_END)  value = hi;
        else return false;
        if (onChange) onChange(value);
        // A keypress is a whole gesture on its own -- there is no release to wait for.
        if (onCommit) onCommit(value);
        return true;
    }

    void Paint(const Painter &p) override {
        const Palette &c = *p.pal;
        // The widget is dragged by reading the cursor while the mouse is down: the window
        // holds capture and gave the widget its `dragging` flag at the press, so this is
        // the whole of the drag, with no per-widget mouse-move plumbing. It is also what
        // keeps the knob under the cursor once the drag has left the control's rectangle
        // -- or the window -- entirely.
        if (dragging && owner) SetFromX(Cursor().x);
        const float cy = rect.top + Height(rect) / 2;
        const float x0 = rect.left + 8, x1 = rect.right - 8;
        const float at = x0 + std::clamp(drawn.value, 0.0f, 1.0f) * (x1 - x0);
        p.rt->FillRoundedRectangle(D2D1::RoundedRect({ x0, cy - 2, x1, cy + 2 }, 2, 2),
                                   p.Brush(c.controlStroke));
        p.rt->FillRoundedRectangle(D2D1::RoundedRect({ x0, cy - 2, at, cy + 2 }, 2, 2),
                                   p.Brush(enabled ? c.accent : c.textDisabled));
        // Fluent's thumb is an accent ring with a filled centre that shrinks when
        // grabbed. Drawn as three circles because that is exactly what it is.
        p.rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(at, cy), 10, 10), p.Brush(c.controlBg));
        p.rt->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(at, cy), 9.5f, 9.5f),
                          p.Brush(c.controlStroke), 1.0f);
        // 6 at rest, 7 under the pointer, 4 while it is being dragged -- Fluent's own
        // three sizes, now passed through rather than jumped between.
        const float r = 6.0f + hoverT - 3.0f * pressT;
        p.rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(at, cy), r, r),
                          p.Brush(enabled ? c.accent : c.textDisabled));
        if (focus && owner && owner->showFocusRing)
            p.rt->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(at, cy), 12, 12),
                              p.Brush(c.textPrimary), 2.0f);
    }
};

}  // namespace micula
