// Micula / widgets.h
//
// The control vocabulary: Button, CheckBox, ToggleSwitch, Segmented, Slider,
// ScrollBar, DropDown, TextBox and ProgressBar.
//
// Kept small on purpose. Every control here is drawn by hand in four states, and a
// tenth one is not free -- it is another rest/hover/pressed/disabled quartet to get
// right in both themes, another Tab stop to wire, and another thing that can look
// subtly unlike Windows. When a page needs something this list does not have, the
// first answer is to express it with what is here; the second is a Widget subclass
// of the page's own, which is exactly what these are.
//
// Every geometry number below is in DIPs and comes from Microsoft's own control
// specs (a 32-DIP control height, a 20-DIP checkbox, a 40x20 switch with a 12-DIP
// knob). They are written as literals rather than named constants where they are
// used once, because a constant named kKnobInset that appears in one expression is
// harder to check against the spec than the number itself.

#pragma once

#include "window.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <string>
#include <vector>

namespace micula {

// ---------------------------------------------------------------- Button

enum class ButtonStyle {
    Accent,     // the one thing the page wants you to do. One per page, at most.
    Standard,   // everything else with a box around it
    Subtle,     // no box until hovered -- toolbar actions, "open log folder"
    Link,       // accent-coloured text, no box at all
};

struct Button : Widget {
    std::wstring label;
    std::wstring glyph;              // optional Segoe Fluent Icons code point, drawn left
    // Left-aligned rather than centred. For a navigation list, where the labels are
    // different lengths and centring each one in its own row makes the column look
    // ragged -- Windows' own Settings left-aligns them against the icon.
    bool         leftAlign = false;
    ButtonStyle  style = ButtonStyle::Standard;
    std::function<void()> onClick;

    Button(std::wstring text, ButtonStyle s, std::function<void()> f)
        : label(std::move(text)), style(s), onClick(std::move(f)) {}

    bool Focusable() const override { return true; }
    bool HandCursor() const override { return style == ButtonStyle::Link; }
    void OnClick() override { if (enabled && onClick) onClick(); }

    void Paint(const Painter &p) override {
        const Palette &c = *p.pal;
        D2D1_COLOR_F fg = enabled ? c.textPrimary : c.textDisabled;

        // **A button does not move.** The first version of this shrank it by 2% on press,
        // on the strength of PointerDownThemeAnimation -- which is real, and belongs to
        // ListViewItem and GridViewItem, not to Button. WinUI's Button template changes
        // exactly one thing over time, its background, and changes it with a
        // BrushTransition; the border and the label are setters. A button that shrinks
        // under the pointer is a button from another platform.
        const D2D1_RECT_F box = rect;

        if (style == ButtonStyle::Accent) {
            const D2D1_COLOR_F bg =
                !enabled ? c.controlBg
                         : Mix(Mix(c.accent, c.accentHover, hoverT), c.accentPressed, pressT);
            p.FillRound(box, metric::kRadiusControl, bg);
            fg = enabled ? c.accentText : c.textDisabled;
        } else if (style == ButtonStyle::Standard) {
            const D2D1_COLOR_F bg =
                !enabled ? c.controlBg
                         : Mix(Mix(c.controlBg, c.controlBgHover, hoverT),
                               c.controlBgPressed, pressT);
            p.FillRound(box, metric::kRadiusControl, bg);
            p.StrokeRound(box, metric::kRadiusControl, c.controlStroke);
            // Fluent's controls have a darker line along the bottom edge only. It is
            // one pixel and it is most of what makes a rectangle read as a button
            // rather than as a box; without it the standard button looks flat next to
            // Windows' own. It goes as the button is pressed, which is what the real
            // one does -- a pressed control is level with the page, not raised over it.
            if (enabled && pressT < 1.0f)
                p.Line(box.left + metric::kRadiusControl, box.bottom - 0.5f,
                       box.right - metric::kRadiusControl, box.bottom - 0.5f,
                       Fade(c.controlStrokeBottom, 1.0f - pressT));
        } else if (style == ButtonStyle::Subtle) {
            // Nothing at all at rest, so the fill fades in from transparent rather than
            // from a colour: its own alpha is what carries it.
            if (enabled && hoverT > 0.0f)
                p.FillRound(box, metric::kRadiusControl,
                            Fade(Mix(c.subtleHover, c.controlBgPressed, pressT), hoverT));
        } else {
            fg = !enabled ? c.textDisabled : Mix(c.accent, Shade(c.accent, -0.15f), pressT);
        }

        if (focus && owner && owner->showFocusRing) {
            // Fluent's focus ring is two strokes: a thick one in the text colour and
            // a thin one in the background colour inside it, so it stays visible
            // whatever the button is filled with. One stroke in a single colour
            // disappears against the accent button, which is the button people tab
            // to first.
            const D2D1_RECT_F o = { rect.left - 2, rect.top - 2, rect.right + 2, rect.bottom + 2 };
            p.StrokeRound(o, metric::kRadiusControl + 2, c.textPrimary, 2.0f);
            p.StrokeRound(rect, metric::kRadiusControl, c.windowBg, 1.0f);
        }

        D2D1_RECT_F text = box;
        if (!glyph.empty()) {
            const D2D1_RECT_F g = { box.left + 12, box.top, box.left + 32, box.bottom };
            p.Text(glyph, g, p.font->icon, fg);
            text.left += 32;
        }
        // Centred by measuring, not by a centring text format: the glyph above eats
        // into the box asymmetrically, and a format-centred label would sit off to
        // the right of the space that is actually left for it.
        if (leftAlign) {
            text.left += glyph.empty() ? 12.0f : 0.0f;
        } else {
            const float w = p.MeasureWidth(label, p.font->body);
            text.left += (std::max)(0.0f, (Width(text) - w) / 2);
        }
        p.Text(label, text, p.font->body, fg);
    }

    // The width this button wants: its label plus Fluent's 12-DIP side padding, and
    // never narrower than the 100-DIP minimum that keeps a row of buttons even.
    float PreferredWidth(const Painter &p) const {
        return (std::max)(metric::kButtonMinW,
                        p.MeasureWidth(label, p.font->body) + 24.0f + (glyph.empty() ? 0.0f : 32.0f));
    }
};

// ---------------------------------------------------------------- CheckBox

struct CheckBox : Widget {
    std::wstring label;
    std::wstring detail;             // second line, secondary colour, optional
    bool  checked = false;
    std::function<void(bool)> onChange;
    // The box filling and the tick arriving, rather than both appearing between two
    // frames. WinUI's checkbox animates its glyph in; this is that, one storyboard at
    // ControlFastAnimationDuration.
    motion::Track checkT;

    CheckBox(std::wstring text, bool on, std::function<void(bool)> f)
        : label(std::move(text)), checked(on), onChange(std::move(f)),
          checkT(on ? 1.0f : 0.0f) {}

    bool Focusable() const override { return true; }
    void OnClick() override {
        if (!enabled) return;
        checked = !checked;
        if (onChange) onChange(checked);
    }
    bool Animating() const override {
        return Widget::Animating() || checkT.Wants(checked ? 1.0f : 0.0f);
    }
    void Tick(float dt) override {
        Widget::Tick(dt);
        checkT.To(checked ? 1.0f : 0.0f);
        checkT.Step(dt, motion::kFast);
    }

    void Paint(const Painter &p) override {
        const Palette &c = *p.pal;
        const float side = 20.0f;
        const float y = rect.top + (Height(rect) - side) / 2;
        const D2D1_RECT_F box = { rect.left, y, rect.left + side, y + side };

        const D2D1_COLOR_F off = Mix(c.controlBg, c.controlBgHover, hoverT);
        const D2D1_COLOR_F on  = Mix(Mix(c.accent, c.accentHover, hoverT),
                                     c.accentPressed, pressT);
        const float k = checkT.value;
        p.FillRound(box, 3.0f, !enabled ? c.controlBg : Mix(off, on, k));
        // The empty box's border goes as the accent comes up behind it, or the two are
        // both visible halfway through and the box reads as having gained a second edge.
        if (k < 1.0f)
            p.StrokeRound(box, 3.0f, Fade(enabled ? c.controlStroke : c.controlBg, 1.0f - k));
        if (k > 0.0f)
            p.Text(glyph::kCheck, { box.left + 2, box.top, box.right, box.bottom },
                   p.font->icon, Fade(enabled ? c.accentText : c.textDisabled, k));
        if (focus && owner && owner->showFocusRing) {
            const D2D1_RECT_F o = { rect.left - 2, rect.top - 1, rect.right + 2, rect.bottom + 1 };
            p.StrokeRound(o, 5.0f, c.textPrimary, 2.0f);
        }

        const D2D1_COLOR_F fg = enabled ? c.textPrimary : c.textDisabled;
        const float tx = rect.left + side + 12;
        if (detail.empty()) {
            p.Text(label, { tx, rect.top, rect.right, rect.bottom }, p.font->body, fg);
        } else {
            const float half = Height(rect) / 2;
            p.Text(label, { tx, rect.top, rect.right, rect.top + half }, p.font->body, fg);
            p.Text(detail, { tx, rect.top + half, rect.right, rect.bottom },
                   p.font->caption, c.textSecondary);
        }
    }
};

// ---------------------------------------------------------------- ToggleSwitch

// The same boolean as a CheckBox, and the reason both exist is what the two say
// about *when* the change lands. Windows uses a switch for a setting that takes
// effect immediately and a checkbox for one that is collected now and applied on OK:
// a settings page is switches, an installer's "also do this during the install"
// options are checkboxes. Getting that round the wrong way is not a bug anybody
// reports, and it is the difference between a window that feels native and one that
// feels close.
struct ToggleSwitch : Widget {
    std::wstring label;
    std::wstring detail;
    bool on = false;
    std::function<void(bool)> onChange;
    // 0..1, animated. A switch that snaps is the single most obvious tell that a
    // control was drawn rather than inherited -- and it is one of the few things WinUI
    // really does animate, with a storyboard rather than a brush transition.
    motion::Track knob;

    ToggleSwitch(std::wstring text, bool value, std::function<void(bool)> f)
        : label(std::move(text)), on(value), onChange(std::move(f)),
          knob(value ? 1.0f : 0.0f) {}

    bool Focusable() const override { return true; }
    void OnClick() override {
        if (!enabled) return;
        on = !on;
        if (owner) StartAnimation(owner);
        if (onChange) onChange(on);
    }
    bool Animating() const override {
        return Widget::Animating() || knob.Wants(on ? 1.0f : 0.0f);
    }
    void Tick(float dt) override {
        Widget::Tick(dt);
        // Was `knob += 0.18f` per tick, which made the switch's speed a function of how
        // busy the message queue was. micula::motion says why that had to go.
        knob.To(on ? 1.0f : 0.0f);
        knob.Step(dt, motion::kFast);
    }

    void Paint(const Painter &p) override {
        const Palette &c = *p.pal;
        const float w = 40.0f, h = 20.0f;
        const float y = rect.top + (Height(rect) - h) / 2;
        // The switch sits on the right of the row, which is where Windows' own
        // settings put it -- label left, control right, aligned down the page.
        const D2D1_RECT_F track = { rect.right - w, y, rect.right, y + h };

        // The track crosses over with the knob rather than switching under it: `knob` is
        // the same 0..1 the knob slides on, so the colour arrives exactly as the knob
        // reaches the other end.
        const float k = knob.value;
        const D2D1_COLOR_F offBg = Mix(c.controlBg, c.controlBgHover, hoverT);
        const D2D1_COLOR_F onBg  = Mix(Mix(c.accent, c.accentHover, hoverT),
                                       c.accentPressed, pressT);
        p.FillRound(track, h / 2, !enabled ? c.controlBg : Mix(offBg, onBg, k));
        if (!enabled || k < 1.0f)
            p.StrokeRound(track, h / 2, Fade(c.controlStroke, enabled ? 1.0f - k : 1.0f));

        // Fluent's knob grows under the pointer and squashes while it is held -- wider
        // than it is tall, as if it were being pushed against the track. Two radii and
        // two lerps, which is the whole of it.
        const float rx = 6.0f + hoverT + pressT;
        const float ry = 6.0f + hoverT - pressT;
        const float cx = track.left + 10 + k * (w - 20);
        const D2D1_COLOR_F knobColor = !enabled ? c.textDisabled
                                                : Mix(c.textSecondary, c.accentText, k);
        p.rt->FillEllipse(D2D1::Ellipse(D2D1::Point2F(cx, y + h / 2), rx, ry),
                          p.Brush(knobColor));

        if (focus && owner && owner->showFocusRing) {
            const D2D1_RECT_F o = { track.left - 3, track.top - 3, track.right + 3, track.bottom + 3 };
            p.StrokeRound(o, h / 2 + 3, c.textPrimary, 2.0f);
        }

        // An empty label means the caller is drawing the words itself -- which is what
        // a settings card does, so that the title, the one-line description and the
        // control all sit on a grid the page owns rather than one each control invents.
        if (label.empty()) return;

        const D2D1_COLOR_F fg = enabled ? c.textPrimary : c.textDisabled;
        const float right = track.left - 16;
        if (detail.empty()) {
            p.Text(label, { rect.left, rect.top, right, rect.bottom }, p.font->body, fg);
        } else {
            const float half = Height(rect) / 2;
            p.Text(label, { rect.left, rect.top, right, rect.top + half }, p.font->body, fg);
            p.TextWrapped(detail, { rect.left, rect.top + half + 1, right, rect.bottom },
                          p.font->caption, c.textSecondary, false, true);
        }
    }
};

// ---------------------------------------------------------------- Segmented

// Three or four mutually exclusive words, side by side. Used where a dropdown would
// be heavier than the choice deserves -- a setting that is auto/on/off is a worse
// answer behind a click than with all three showing.
struct Segmented : Widget {
    std::vector<std::wstring> options;
    int selected = 0;
    std::function<void(int)> onChange;

    // The indicator, in cells: 0 is `rect.left`, 1 the far edge of the first option.
    //
    // Two values, because the block's two edges do not move together: `pillPos` is where the
    // move has got to and `pillLag` is a follower that trails behind it -- see Tick. At rest
    // both are `selected`, so a control that is not moving has nothing to remember and one
    // that Layout() has just built is right on its first paint.
    motion::Track pillPos;
    float pillLag = 0.0f;
    bool  dragging = false;

    // How far behind the leading edge the trailing one trails, in seconds. The block is
    // longer than a cell by about the distance covered in that time: 45 ms of a 167 ms move
    // is a quarter of a cell, which is as much as reads as deliberate.
    static constexpr float kPillLag = 0.045f;

    Segmented(std::vector<std::wstring> opts, int sel, std::function<void(int)> f)
        : options(std::move(opts)), selected(sel), onChange(std::move(f)),
          pillPos((float)sel), pillLag((float)sel) {}

    bool Focusable() const override { return true; }
    // The hovered cell follows the pointer across the control, and a drag follows it out of
    // the control altogether, so the window has to keep repainting either way.
    bool TracksPointer() const override { return hover || dragging; }
    float CellW() const { return Width(rect) / (float)(std::max)(size_t(1), options.size()); }

    // The cell under `x`, or -1 past either end. Asked of the cursor where it is needed
    // rather than cached in a field written by Paint: a cached one stays right only as long
    // as the control is repainted on every move, and anything reading it -- a click, an IME
    // position -- then depends on a frame having been drawn since the pointer last moved.
    int IndexAtX(float x) const {
        const int n = (int)options.size();
        if (n <= 0) return -1;
        const int i = (int)((x - rect.left) / CellW());
        return (i < 0 || i >= n) ? -1 : i;
    }

    // The same, kept between the ends -- what a drag wants, since dragging past the last
    // option means the last option.
    int ClampedIndexX(float x) const {
        const int n = (int)options.size();
        if (n <= 0) return -1;
        return std::clamp((int)((x - rect.left) / CellW()), 0, n - 1);
    }

    // The indicator's two edges, in cells. The leading one is where the move has got to and
    // the trailing one is behind it, so the block spans a cell plus however far apart they
    // are: that gap is the stretch, which opens as the move sets off and closes as it lands.
    void PillEdges(float *lo, float *hi) const {
        *lo = (std::min)(pillPos.value, pillLag);
        *hi = (std::max)(pillPos.value, pillLag) + 1.0f;
    }

    // Take the new index. Nothing starts here: the indicator is aimed at whatever `selected`
    // is, once a frame, in Tick. That is what makes a drag that crosses three cells inside one
    // frame one move rather than three, and crossing back again continuous rather than a
    // rewind.
    void Select(int index) {
        if (index < 0 || index >= (int)options.size() || index == selected) return;
        selected = index;
        if (onChange) onChange(selected);
    }

    // A press takes the option under the pointer and a drag carries the selection along;
    // the release only ends the gesture. The same shape as Slider, including following the
    // pointer out of the control -- and out of the window.
    void OnPress(float x, float /*y*/) override { dragging = true; Select(ClampedIndexX(x)); }
    void OnDrag(float x, float /*y*/) override { if (dragging) Select(ClampedIndexX(x)); }
    void OnRelease() override { dragging = false; }

    bool OnKey(WPARAM vk) override {
        if (vk != VK_LEFT && vk != VK_RIGHT) return false;
        const int n = (int)options.size();
        if (n <= 0) return false;
        // Wraps, and the indicator wraps with it: the one move that crosses the control.
        Select((selected + (vk == VK_RIGHT ? 1 : n - 1)) % n);
        return true;
    }

    bool Animating() const override {
        return Widget::Animating() || pillPos.Wants((float)selected) || pillLag != pillPos.value;
    }
    void Tick(float dt) override {
        Widget::Tick(dt);
        // Aimed every frame rather than at the press: a page that assigns `selected` itself is
        // followed too, and the target is always the last cell the pointer crossed.
        pillPos.To((float)selected);
        pillPos.Step(dt, motion::kFaster);
        // The trailing edge is a lag of the leading one. A follower is continuous through a
        // change of target by construction, which is the whole reason it is not a second
        // curve: a drag retargets this several times a frame, and a curve restarted each time
        // would jump to whichever cell it was last pointed at -- the block teleporting from
        // cell to cell while the pointer was dragged across them.
        pillLag += (pillPos.value - pillLag) * (1.0f - std::exp(-dt / kPillLag));
        // Snapped when it is close, or the two are never quite equal and the window animates
        // for the rest of its life, a thousandth of a cell out.
        if (std::fabs(pillPos.value - pillLag) < 0.002f) pillLag = pillPos.value;
    }

    void Paint(const Painter &p) override {
        const Palette &c = *p.pal;
        // Read here rather than in a mouse handler: this control never sees a move, only
        // its own hover flag, so the cell to light comes from the cursor at paint time --
        // which is also why TracksPointer is true. Nothing is stored: which cell the
        // pointer is over is a question about the pointer, not state this control keeps.
        const int hot = hover ? IndexAtX(Cursor().x) : -1;
        p.FillRound(rect, metric::kRadiusControl, c.controlBg);
        p.StrokeRound(rect, metric::kRadiusControl, c.controlStroke);
        // The hover fill goes under the indicator, so a block on its way to a cell the
        // pointer is already on is not covered by that cell's own highlight.
        if (hot >= 0 && enabled) {
            const float x = rect.left + CellW() * hot;
            p.FillRound({ x + 2, rect.top + 2, x + CellW() - 2, rect.bottom - 2 },
                        metric::kRadiusControl - 1, c.controlBgHover);
        }
        float lo = 0.0f, hi = 0.0f;
        PillEdges(&lo, &hi);
        const float w = CellW();
        const float blockLo = rect.left + lo * w, blockHi = rect.left + hi * w;
        p.FillRound({ blockLo + 2, rect.top + 2, blockHi - 2, rect.bottom - 2 },
                    metric::kRadiusControl - 1, enabled ? c.accent : c.controlBgHover);
        // **The label colour follows the block rather than the selection.** Each label is drawn
        // in the colour of what is behind it, and one the block is halfway across is drawn on
        // either side of the block's edge -- which cuts the glyph in two exactly where the fill
        // changes. Turning the whole word over the moment the option is chosen instead reads as
        // the text arriving before the block does, which is what it is.
        for (size_t i = 0; i < options.size(); i++) {
            const float x = rect.left + w * i;
            const D2D1_RECT_F cell = { x, rect.top, x + w, rect.bottom };
            const float tw = p.MeasureWidth(options[i], p.font->body);
            const D2D1_RECT_F text = { cell.left + (w - tw) / 2, cell.top, cell.right, cell.bottom };
            if (!enabled) { p.Text(options[i], text, p.font->body, c.textDisabled); continue; }
            // Where the block's edges fall inside this cell: three slices, of which the middle
            // one is empty whenever the block misses the cell entirely or covers all of it.
            const float cutLo = std::clamp(blockLo, cell.left, cell.right);
            const float cutHi = std::clamp(blockHi, cell.left, cell.right);
            Label(p, options[i], text, cell.left, cutLo, c.textPrimary);
            Label(p, options[i], text, cutLo, cutHi, c.accentText);
            Label(p, options[i], text, cutHi, cell.right, c.textPrimary);
        }
        if (focus && owner && owner->showFocusRing) {
            const D2D1_RECT_F o = { rect.left - 2, rect.top - 2, rect.right + 2, rect.bottom + 2 };
            p.StrokeRound(o, metric::kRadiusControl + 2, c.textPrimary, 2.0f);
        }
    }

    // One slice of a label: the text drawn through its own box, so that it stays where the
    // layout put it, and clipped to `[from, to)` across that box. The clip is never the text's
    // box -- narrowing that would move the glyphs instead of cutting them.
    static void Label(const Painter &p, const std::wstring &s, const D2D1_RECT_F &text,
                      float from, float to, const D2D1_COLOR_F &c) {
        if (to <= from) return;
        p.rt->PushAxisAlignedClip({ from, text.top, to, text.bottom },
                                  D2D1_ANTIALIAS_MODE_ALIASED);
        p.Text(s, text, p.font->body, c);
        p.rt->PopAxisAlignedClip();
    }
};

// ---------------------------------------------------------------- Slider

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

    Slider(float v, float a, float b, float s, std::function<void(float)> f)
        : value(v), lo(a), hi(b), step(s), onChange(std::move(f)) {}

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
        const float at = x0 + Frac() * (x1 - x0);
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

// ---------------------------------------------------------------- ScrollBar

// The two timers a scroll bar sets on its window, numbered clear of the window's caret
// (2). These are the page's bar's; a second bar in the same window -- an open
// drop-down's list -- takes two ids of its own (see DropDown), because a timer is
// offered to the controls in order and the first bar to recognise the id takes it.
//
// So Micula uses timer ids 2 and 4 to 7. A page's own timers should be numbered
// outside that range; they reach the page through Window::OnAppMessage.
constexpr UINT_PTR kScrollBarStateTimer  = 4;
constexpr UINT_PTR kScrollBarRepeatTimer = 5;

// A vertical scroll bar, and it is WinUI's rather than an impression of one: every number
// below is out of WinUI 2's ScrollBar template (ScrollBar_themeresources.xaml) or the
// ScrollViewer and RepeatButton code that drives it.
//
// Twelve DIPs wide (ScrollBarSize), in three states:
//
//   hidden     nothing. (NoIndicator)
//   indicator  a 2-DIP line and nothing else. It appears the moment the pointer moves
//              over the page or the page scrolls, and goes 2 s after both have stopped.
//              (MouseIndicator; ScrollViewerSeparatorContractDelay)
//   expanded   the track, the two arrows and a 6-DIP thumb. 400 ms after the pointer
//              comes onto the bar, and back to the line 500 ms after it leaves.
//              (ConsciousStates; ScrollBarExpandBeginTime, ScrollBarContractBeginTime)
//
// The line and the thumb are one shape. The template's Thumb is a Rectangle with a
// *transparent 6-DIP stroke* round its fill, 8 wide and shifted 2 right when collapsed,
// 12 wide and unshifted when expanded -- so what shows is 2 wide, then 6, growing out of
// a right edge that does not move. That is what Paint draws, rather than two widths.
//
// Unless "Always show scrollbars" is on (Settings > Accessibility > Visual effects):
// then WinUI's ScrollViewer is not "conscious", and the bar is simply always expanded.
//
// What it does is RepeatButton's and Thumb's. An arrow scrolls 16 DIPs
// (ScrollViewerLineDelta) and the track a viewport, once on press, again after 250 ms
// and every 50 ms after that *while the pointer is still on what was pressed* -- which
// is what stops a held track at the pointer instead of at the end of the page. The thumb
// moves the page in proportion to the room it has to travel.
struct ScrollBar : Widget {
    static constexpr float kSize     = 12.0f;   // ScrollBarSize
    static constexpr float kThumbMin = 30.0f;   // ScrollBarVerticalThumbMinHeight
    static constexpr float kLine     = 16.0f;   // ScrollViewerLineDelta
    static constexpr ULONGLONG kExpandDelay = 400, kContractDelay = 500, kHideDelay = 2000;

    enum class Part { None, Up, Down, PageUp, PageDown, Thumb };

    // What the page says on every layout, in DIPs.
    float viewport = 0.0f;   // how much of the page is visible
    float extent   = 0.0f;   // how tall the whole page is
    float value    = 0.0f;   // where it has been scrolled to
    float drawn    = 0.0f;   // where it is drawn, which trails `value` through a glide
    // The scrolling part of the window. The pointer moving anywhere over it shows the bar.
    // Handed to the window as this control's ExternalRegion, which is where a move over
    // the page is delivered from: not a hit-test area, so a click here is a click on the
    // page.
    D2D1_RECT_F area = {};
    // Asked to scroll the page to `to`. `glide` is false for the thumb only: a dragged
    // thumb has to stay under the pointer, not catch up with it.
    std::function<void(float to, bool glide)> onScroll;
    // Read once, when the bar is made.
    bool autoHide = SystemAutoHidesScrollBars();
    // Which timer ids are this bar's. See kScrollBarStateTimer.
    UINT_PTR stateTimer = kScrollBarStateTimer, repeatTimer = kScrollBarRepeatTimer;

    Part  grab = Part::None;             // what the button went down on
    float px = -1.0f, py = -1.0f;        // the pointer, from the last message that had it
    float grabY = 0.0f, grabValue = 0.0f;
    bool  repeating = false;             // past RepeatButton's Delay, on to its Interval
    bool  shown = false, expanded = false, held = false;
    ULONGLONG activeAt = 0, heldAt = 0, armedFor = 0;
    float showT = 0.0f;                  // the thumb's opacity
    float fadeT = 0.0f;                  // the track's and the arrows' opacity
    motion::Track sizeT;                 // 0 the line, 1 the expanded thumb

    explicit ScrollBar(std::function<void(float, bool)> f) : onScroll(std::move(f)) {}

    // The page scrolled, or the pointer moved over it: show the line, and start its 2 s
    // again.
    void Wake() { activeAt = GetTickCount64(); shown = true; }

    // --- geometry, in window DIPs ------------------------------------------------
    float MaxValue() const { return (std::max)(0.0f, extent - viewport); }
    float TrackTop() const { return rect.top + kSize; }
    float TrackLength() const { return (std::max)(0.0f, Height(rect) - 2 * kSize); }
    // A track too short for the smallest thumb has none, which is the template's answer too.
    bool  HasThumb() const { return TrackLength() >= kThumbMin; }
    float ThumbLength() const {
        const float t = TrackLength();
        const float l = extent > 0.0f ? t * viewport / extent : t;
        return std::clamp(l, kThumbMin, (std::max)(kThumbMin, t));
    }
    float ThumbTop(float at) const {
        const float m = MaxValue(), room = TrackLength() - ThumbLength();
        return TrackTop() + (m > 0.0f && room > 0.0f ? room * std::clamp(at / m, 0.0f, 1.0f)
                                                     : 0.0f);
    }
    // Against the thumb where it is drawn, because that is what the pointer is aimed at.
    Part PartAt(float y) const {
        if (y < rect.top + kSize) return Part::Up;
        if (y >= rect.bottom - kSize) return Part::Down;
        if (!HasThumb()) return y < TrackTop() + TrackLength() / 2 ? Part::PageUp : Part::PageDown;
        const float t = ThumbTop(drawn);
        if (y < t) return Part::PageUp;
        return y < t + ThumbLength() ? Part::Thumb : Part::PageDown;
    }

    // --- input --------------------------------------------------------------------
    bool TracksPointer() const override { return hover; }   // the arrow under it lights
    D2D1_RECT_F ExternalRegion() const override { return area; }

    void OnPointerMove(float x, float y) override {
        // Both places a move can arrive from -- over the bar itself and over the page it
        // scrolls -- are places the bar should be out for, so there is nothing to test.
        px = x; py = y;
        Wake();
        Poll();
    }
    void OnPress(float x, float y) override {
        px = x; py = y;
        grab = PartAt(y);
        Wake();
        if (grab == Part::Thumb) {
            grabY = y;
            grabValue = drawn;
        } else {
            repeating = false;
            Step(false);
            // RepeatButton.Delay's default. The template's Interval takes over in OnTimer.
            if (owner && owner->hwnd) SetTimer(owner->hwnd, repeatTimer, 250, nullptr);
        }
        Poll();
    }
    void OnDrag(float x, float y) override {
        px = x; py = y;
        const float room = TrackLength() - ThumbLength();
        if (grab != Part::Thumb || room <= 0.0f || !onScroll) return;
        onScroll(std::clamp(grabValue + (y - grabY) / room * MaxValue(), 0.0f, MaxValue()),
                 false);
    }
    void OnRelease() override {
        if (owner && owner->hwnd) KillTimer(owner->hwnd, repeatTimer);
        grab = Part::None;
        Poll();
    }
    bool OnTimer(UINT_PTR id) override {
        if (!owner || !owner->hwnd) return false;
        if (id == repeatTimer) {
            if (grab == Part::None || grab == Part::Thumb) {
                KillTimer(owner->hwnd, id);
                return true;
            }
            if (!repeating) {
                repeating = true;
                SetTimer(owner->hwnd, id, 50, nullptr);   // the template's Interval
            }
            Step(true);
            return true;
        }
        if (id == stateTimer) {
            KillTimer(owner->hwnd, id);
            armedFor = 0;
            Poll();
            owner->Invalidate();
            return true;
        }
        return false;
    }

    // One click of whatever was pressed. `repeat` is the timer's, and RepeatButton only
    // repeats while the pointer is on the button it went down on -- for the track, the
    // part of it still above or below the thumb, measured where the page is going.
    void Step(bool repeat) {
        if (!onScroll) return;
        const bool on = Inside(rect, px, py);
        float to = value;
        switch (grab) {
        case Part::Up:
            if (repeat && !(on && py < TrackTop())) return;
            to -= kLine;
            break;
        case Part::Down:
            if (repeat && !(on && py >= rect.bottom - kSize)) return;
            to += kLine;
            break;
        case Part::PageUp:
            if (repeat && !(on && py >= TrackTop() && py < ThumbTop(value))) return;
            to -= viewport;
            break;
        case Part::PageDown:
            if (repeat && !(on && py < rect.bottom - kSize &&
                            py >= ThumbTop(value) + ThumbLength())) return;
            to += viewport;
            break;
        default:
            return;
        }
        Wake();
        onScroll(std::clamp(to, 0.0f, MaxValue()), true);
    }

    // --- the three states ---------------------------------------------------------
    bool Held() const { return visible && (hover || grab != Part::None); }

    // Where the states stand now, and a timer for the next moment one of them is due to
    // change -- so a bar waiting out its 2 s costs one timer rather than two seconds of
    // frames.
    void Poll() {
        const ULONGLONG now = GetTickCount64();
        const bool h = Held();
        if (h != held) {
            held = h;
            heldAt = now;
            if (!h) activeAt = now;   // leaving the bar is the pointer moving over the page
        }
        ULONGLONG next = 0;
        if (!visible) {
            // Nothing to scroll. Put away at once, so that a page which does need the bar
            // does not arrive with this one's fading out on it.
            shown = expanded = false;
            showT = fadeT = 0.0f;
            sizeT.Set(0.0f);
        } else if (!autoHide) {
            if (!expanded) { showT = fadeT = 1.0f; sizeT.Set(1.0f); }
            shown = expanded = true;
        } else {
            auto due = [&](ULONGLONG at) {
                if (at <= now) return true;
                if (next == 0 || at < next) next = at;
                return false;
            };
            if (held) {
                shown = true;
                if (!expanded && due(heldAt + kExpandDelay)) expanded = true;
            } else {
                if (expanded && due(heldAt + kContractDelay)) expanded = false;
                if (shown && due(activeAt + kHideDelay)) shown = expanded = false;
            }
        }
        if (next != armedFor && owner && owner->hwnd) {
            armedFor = next;
            if (next == 0) KillTimer(owner->hwnd, stateTimer);
            else SetTimer(owner->hwnd, stateTimer, (UINT)(next - now), nullptr);
        }
    }

    bool Animating() const override {
        return Widget::Animating() || Held() != held ||
               showT != (shown ? 1.0f : 0.0f) || fadeT != (expanded ? 1.0f : 0.0f) ||
               sizeT.Wants(expanded ? 1.0f : 0.0f);
    }
    void Tick(float dt) override {
        Widget::Tick(dt);
        Poll();
        // Appearing is a setter in MouseIndicator; going is NoIndicator's 83 ms fade.
        if (shown) showT = 1.0f;
        else motion::Ramp(&showT, 0.0f, dt, motion::kFaster);
        // The track and the arrows fade over ScrollBarOpacityChangeDuration, linear. The
        // thumb widens over ScrollBarExpandDuration on KeySpline 0,0,0,1, which is Decel.
        motion::Ramp(&fadeT, expanded ? 1.0f : 0.0f, dt, motion::kFaster);
        sizeT.To(expanded ? 1.0f : 0.0f);
        sizeT.Step(dt, motion::kFast);
    }

    void Paint(const Painter &p) override {
        const Palette &c = *p.pal;
        if (fadeT > 0.0f) {
            // ScrollBarCornerRadius, doubled by the track's converter: a pill.
            p.FillRound(rect, 6.0f, Fade(c.acrylicInApp, fadeT));
            PaintArrow(p, Part::Up);
            PaintArrow(p, Part::Down);
        }
        if (showT <= 0.0f || !HasThumb()) return;
        const float s = sizeT.value;
        const float w = 8.0f + 4.0f * s;                              // Width 8 -> 12
        const float left = rect.left + (kSize - w) / 2 + 2.0f * (1.0f - s);   // X 2 -> 0
        const float top = ThumbTop(drawn);
        // Inside the transparent stroke.
        const D2D1_RECT_F fill = { left + 3, top + 3, left + w - 3, top + ThumbLength() - 3 };
        p.FillRound(fill, (std::min)(3.0f, Width(fill) / 2), Fade(c.controlStrong, showT));
    }

    void PaintArrow(const Painter &p, Part part) {
        const Palette &c = *p.pal;
        const bool up = part == Part::Up;
        // The button is the whole 12x12 end of the bar; the glyph sits in it with the
        // template's 4-DIP padding on the outer side.
        const D2D1_RECT_F box = up ? D2D1_RECT_F{ rect.left, rect.top + 4, rect.right, rect.top + kSize }
                                   : D2D1_RECT_F{ rect.left, rect.bottom - kSize, rect.right, rect.bottom - 4 };
        const bool over = hover && Inside(rect, px, py) &&
                          (up ? py < TrackTop() : py >= rect.bottom - kSize);
        const bool down = over && grab == part;
        const std::wstring g = up ? glyph::kCaretUp : glyph::kCaretDown;
        const float gw = p.MeasureWidth(g, p.font->iconTiny);
        const float cx = (box.left + box.right) / 2, cy = (box.top + box.bottom) / 2;
        // ScrollBarButtonArrowScalePressed, about the glyph's centre.
        D2D1_MATRIX_3X2_F was;
        if (down) {
            p.rt->GetTransform(&was);
            p.rt->SetTransform(D2D1::Matrix3x2F::Scale(0.875f, 0.875f, D2D1::Point2F(cx, cy)) * was);
        }
        p.Text(g, { cx - gw / 2, box.top, cx + gw / 2 + 1, box.bottom }, p.font->iconTiny,
               Fade(over ? c.textSecondary : c.controlStrong, fadeT));
        if (down) p.rt->SetTransform(was);
    }
};

// ---------------------------------------------------------------- DropDown

// The timers of an open list's scroll bar, clear of the page's (kScrollBarStateTimer).
// Only one list is ever open, so every drop-down can share the pair.
constexpr UINT_PTR kDropDownBarStateTimer  = 6;
constexpr UINT_PTR kDropDownBarRepeatTimer = 7;

struct DropDown : Widget {
    std::vector<std::wstring> options;
    int selected = 0;
    std::function<void(int)> onChange;
    bool open = false;
    float rowH = 32.0f;
    // How far the lid is open: 0 is the control's own row and nothing else, 1 is the whole
    // popup. Fluent expands a flyout out of the control it belongs to rather than blinking
    // it on, and on the way out it collapses back into it rather than vanishing -- which is
    // why the control stays raised while this runs down.
    //
    // Two curves, because Fluent has two: in on "Fast Out, Slow In", out on "Slow Out,
    // Fast In". A flyout arriving settles; a flyout leaving gets out of the way.
    motion::Track openT;

    DropDown(std::vector<std::wstring> opts, int sel, std::function<void(int)> f)
        : options(std::move(opts)), selected(sel), onChange(std::move(f)) {}
    // A list thrown away while open -- the page laid out again under it -- must not leave
    // its bar's timers firing at the window with nobody to answer them.
    ~DropDown() override {
        if (bar && owner && owner->hwnd) {
            if (bar->armedFor) KillTimer(owner->hwnd, bar->stateTimer);
            if (bar->grab != ScrollBar::Part::None) KillTimer(owner->hwnd, bar->repeatTimer);
        }
    }

    // The control's own row. While the list is open `rect` grows to cover the popup, so
    // the head cannot be derived from `rect` any more.
    D2D1_RECT_F head = {};
    // The rows' margin inside the popup, top and bottom. WinUI's
    // ComboBoxDropdownContentMargin, of which the 4 DIPs that show are the part that
    // matters here.
    static constexpr float kPad = 4.0f;
    // Where the popup comes to rest: worked out when it opens, from the chosen row, and
    // then left alone -- the popup does not move while it is being scrolled through, the
    // *rows* do. See Place().
    D2D1_RECT_F frame = {};
    // Where the panel is, in rows: 0 has the first option on the control's own row, 3 has
    // the fourth there. It trails `selected` so the list slides to a new choice rather than
    // jumping, and it is what the panel -- and every row in it -- is drawn from. The list
    // moves past the control as a whole; nothing scrolls inside the panel, and no marker
    // travels down a still one.
    float slid = 0.0f;
    // How long the slide takes to close most of its gap, in seconds. A follower rather than
    // a curve with a duration, like the page's scroll and the segmented indicator's block:
    // a spun wheel retargets this several times inside one frame, and a storyboard restarted
    // that often would stutter between the notches.
    static constexpr float kSlideLag = 0.05f;
    // Its scroll bar, the page's own control: WinUI's drop-down is a ScrollViewer, and
    // the bar in it is the one every other ScrollViewer has. Made the first time a list
    // needs one, not for every drop-down on every layout.
    std::unique_ptr<ScrollBar> bar;
    // The press went down on the bar, so letting go is not choosing a row.
    bool barGrab = false;

    bool Focusable() const override { return true; }
    bool TracksPointer() const override { return open; }
    D2D1_RECT_F Head() const { return open ? head : rect; }

    // The room a list may take: the page's visible strip, or on a window whose page does
    // not scroll, the window under its caption -- which is painted last, over everything.
    D2D1_RECT_F Bounds() const {
        if (!owner || !owner->hwnd) return D2D1_RECT_F{ 0, 0, 0, 0 };
        // In this control's own space rather than the window's, which on a scrolling page
        // are not the same thing: the strip the page shows moves with the scroll while the
        // control's rectangle stays where the layout put it. Measuring against the
        // window's rectangle instead made a control halfway down the page think it had the
        // room the top of the page had, and open downward off the bottom of it.
        const D2D1_RECT_F clip = VisibleArea();
        if (clip.bottom > clip.top) return clip;
        // No scrolling area: the window under its caption, in this control's space too,
        // because the page may still be drawn through a transform.
        float dy = 0.0f, op = 1.0f;
        if (scrolls) owner->ContentTransform(&dy, &op);
        return D2D1_RECT_F{ 0, kCaptionH - dy, owner->ClientW(), owner->ClientH() - dy };
    }
    float FullHeight() const { return rowH * (float)options.size() + 2 * kPad; }

    // The y a row has to be at to be over the control's own row: the two boxes centred on
    // each other, which for a row and a control of the same height is the two boxes on top
    // of each other -- and is why the popup reads as a lid closing over the control.
    float RowLine() const { return (Head().top + Head().bottom) / 2 - rowH / 2; }
    // Where a row is drawn. The panel's place in rows is `slid`, so every row moves with
    // it, which is the whole of the difference between this and a list that scrolls.
    float RowTop(int i) const { return RowLine() + rowH * ((float)i - slid); }

    // The panel: the whole list, laid out so that the chosen row is on the control's own
    // row, and moved as a whole when the choice moves.
    //
    // **This is what a Windows 11 combo box does**, and it is not "open below and flip when
    // short of room": the popup covers the control with the chosen item on it, so the two
    // names are in the same place and the choice reads as a swap rather than as a menu. The
    // rule is one line of ComboBox::GetNonPannablePopupLayout -- the chosen item is laid out
    // at `cbY + cbHeight/2 - itemHeight/2 - margin.Top` -- applied again after every step,
    // which is what makes the wheel feel like a dial under the control rather than a menu
    // being dragged about.
    //
    // One deliberate difference. WinUI clamps the popup into the window and shrinks it, so
    // the chosen row drifts off the control near the top and bottom of the screen. This lets
    // the panel hang off the strip and be clipped by the page instead, because the chosen
    // row being on the control is the whole reason the popup is over it: what goes out of
    // sight is the far end of the list, which is the end to lose.
    D2D1_RECT_F FrameAt(float k) const {
        const float top = RowLine() - kPad - rowH * k;
        return { Head().left, top, Head().right, top + FullHeight() };
    }
    D2D1_RECT_F Frame() const { return FrameAt(slid); }          // where it is drawn
    D2D1_RECT_F Rest() const { return FrameAt((float)selected); }  // where it belongs
    // Everything that follows from the chosen row: the area the mouse can reach and the bar.
    // The rows themselves need no telling -- RowTop reads `slid`.
    void Sync() {
        const D2D1_RECT_F f = Rest();
        rect = { head.left, (std::min)(head.top, f.top),
                 head.right, (std::max)(head.bottom, f.bottom) };
        SyncBar();
    }
    // The popup as it is drawn *now*: it grows out of the control's own row, each edge
    // that has somewhere to go travelling from that row to where it ends up. Up and down
    // both when the list reaches both ways, which is the ordinary case -- the chosen row
    // sits over the control and its neighbours arrive from under it.
    //
    // WinUI does this with SplitOpenThemeAnimation, whose ClosedLength, OpenedLength and
    // OffsetFromCenter are the three numbers ComboBoxTemplateSettings hands it for exactly
    // this purpose.
    D2D1_RECT_F Shown() const {
        const D2D1_RECT_F f = Frame();
        const float k = openT.value;
        const float top = f.top < head.top ? head.top + (f.top - head.top) * k : head.top;
        const float bot = f.bottom > head.bottom
                              ? head.bottom + (f.bottom - head.bottom) * k
                              : head.bottom;
        return { head.left, top, head.right, bot };
    }
    // Taller than the room it is seen through, which is the only thing the bar is for.
    bool Overflows() const { return FullHeight() > Height(Bounds()) + 0.5f; }

    // The chosen row, by the wheel, the keys, a click or the bar. The panel's place and the
    // bar come out of it -- see Sync -- so there is nothing here to keep in step by hand.
    void Select(int i) {
        const int n = (int)options.size();
        if (n <= 0) return;
        i = std::clamp(i, 0, n - 1);
        if (i != selected) {
            selected = i;
            Sync();
            if (onChange) onChange(selected);
        }
        if (BarShown()) { bar->Wake(); bar->Poll(); }
        if (owner) owner->Invalidate();
    }
    // The bar, which works in DIPs while the list works in rows. A step smaller than a row
    // still moves a row: an arrow that did nothing would be an arrow that is broken.
    void ScrollTo(float to, bool /*glide*/) {
        if (!open) return;
        Select(to < slid * rowH ? (int)std::floor(to / rowH) : (int)std::ceil(to / rowH));
    }
    // The bar's geometry. It belongs to the list, so it travels with the panel -- but its
    // track is only drawn where the page can show it, and what it reports is the chosen
    // row's place in the list, which is the only thing about this list that moves.
    void SyncBar() {
        if (!bar) return;
        const D2D1_RECT_F f = Frame();
        const D2D1_RECT_F b = Bounds();
        bar->rect = { f.right - 2 - ScrollBar::kSize, (std::max)(f.top, b.top) + 1,
                      f.right - 2, (std::min)(f.bottom, b.bottom) - 1 };
        bar->area = f;
        bar->viewport = Height(b);
        bar->extent = FullHeight();
        bar->value = (std::min)((std::max)(0.0f, slid * rowH),
                                (std::max)(0.0f, FullHeight() - Height(b)));
        bar->drawn = bar->value;
    }
    bool BarShown() const { return open && bar && bar->visible; }

    // Which option is at this point. -1 for anything outside the panel as it is drawn right
    // now -- a row the panel has not slid over yet is not there -- and for the bar.
    int RowAt(float x, float y) const {
        const D2D1_RECT_F s = Shown();
        if (y < s.top || y >= s.bottom) return -1;
        if (BarShown() && Inside(bar->rect, x, y)) return -1;
        const int i = (int)std::floor((y - RowLine()) / rowH + slid);
        return (i >= 0 && i < (int)options.size()) ? i : -1;
    }

    void SetOpen(bool o) {
        barGrab = false;
        if (o) {
            head = { rect.left, rect.top, rect.right, rect.top + metric::kControlH };
            open = true;
            z = 1;
            // Opened with the chosen row already on the control: nothing to slide.
            slid = (float)selected;
            const bool cut = Overflows();
            if (cut) {
                if (!bar) {
                    bar = std::make_unique<ScrollBar>(
                        [this](float to, bool glide) { ScrollTo(to, glide); });
                    bar->stateTimer = kDropDownBarStateTimer;
                    bar->repeatTimer = kDropDownBarRepeatTimer;
                }
                bar->owner = owner;
                bar->visible = true;
            }
            // The whole of it takes the mouse, or a click on an option falls through to
            // whatever is behind the list.
            Sync();
            if (cut) {
                // The line shows at once: the pointer is on the control, not over the
                // list, so nothing else would wake it.
                bar->Wake();
                bar->Poll();
            }
        } else {
            open = false;
            // Stays raised while the lid closes, and Tick drops it back to 0 when that is
            // done. Nothing is stolen by that: the rect has gone back to the control's own
            // row, so the hit test cannot reach the rows even though they are still being
            // drawn.
            z = 1;
            if (head.right > head.left) rect = head;
            // Its timers go with it; see kDropDownBarStateTimer.
            if (bar) {
                bar->OnRelease();
                bar->hover = false;
                bar->visible = false;
                bar->Poll();
            }
        }
    }
    void Dismiss() override { if (open) SetOpen(false); }
    bool Animating() const override {
        return Widget::Animating() || openT.Wants(open ? 1.0f : 0.0f) ||
               slid != (float)selected || (BarShown() && bar->Animating());
    }
    void Tick(float dt) override {
        Widget::Tick(dt);
        openT.To(open ? 1.0f : 0.0f);
        if (open) {
            openT.Step(dt, motion::kFast, motion::Decel);
        } else if (!openT.Step(dt, motion::kFaster, motion::Accel)) {
            z = 0;          // the lid has finished closing; stop keeping it raised
        }
        // What has a gap to close is the panel's own place: `slid` is where the list is
        // drawn and `selected` is where it belongs.
        const float want = (float)selected;
        if (slid != want) {
            slid += (want - slid) * (1.0f - std::exp(-dt / kSlideLag));
            if (std::fabs(want - slid) < 0.004f) slid = want;
            SyncBar();
        }
        if (BarShown()) {
            SyncBar();
            // The bar is not in the window's list, so nothing else tells it the pointer
            // has left: this control's own hover going is what runs this frame.
            if (!barGrab) {
                const D2D1_POINT_2F at = Cursor();
                bar->hover = hover && Inside(bar->rect, at.x, at.y);
            }
            bar->Tick(dt);
        }
    }

    void OnClick() override {
        if (!enabled) return;
        // A press on the list's bar scrolls it. It does not choose, and it does not close.
        if (barGrab) { barGrab = false; return; }
        if (!open) { SetOpen(true); return; }
        // Through the same function the drawing uses, so the row that was lit and the row
        // that is chosen cannot come apart.
        const D2D1_POINT_2F at = Cursor();
        const int i = RowAt(at.x, at.y);
        if (i >= 0) Select(i);
        SetOpen(false);
    }
    bool OnKey(WPARAM vk) override {
        // A bar drag that ended outside the control never reached OnClick; the key that
        // follows is not its release.
        barGrab = false;
        if (vk == VK_ESCAPE && open) { SetOpen(false); return true; }
        if (vk != VK_UP && vk != VK_DOWN) return false;
        const int n = (int)options.size();
        if (n <= 0) return false;
        // Open, a step goes through Select like the wheel's does, so the rows slide under
        // the control as the choice moves. Closed there is nothing to slide, and the only
        // thing a list can do that a scroll cannot is wrap.
        if (open) {
            Select(selected + (vk == VK_DOWN ? 1 : -1));
            return true;
        }
        Select((selected + (vk == VK_DOWN ? 1 : n - 1)) % n);
        return true;
    }

    // The list's bar, driven from here: it is not one of the window's controls, because
    // the window's list is rebuilt on every layout and this bar lives as long as the list.
    // No offset to take off anywhere: the popup is drawn where it was placed, and it is
    // the rows that move inside it rather than it moving over them.
    void OnPress(float x, float y) override {
        barGrab = false;
        if (!BarShown()) return;
        SyncBar();
        if (!Inside(bar->rect, x, y)) return;
        barGrab = true;
        bar->hover = true;
        bar->OnPress(x, y);
    }
    void OnDrag(float x, float y) override {
        if (barGrab && BarShown()) bar->OnDrag(x, y);
    }
    void OnRelease() override {
        if (barGrab && BarShown()) bar->OnRelease();
    }
    void OnPointerMove(float x, float y) override {
        if (!BarShown()) return;
        SyncBar();
        if (!barGrab) bar->hover = hover && Inside(bar->rect, x, y);
        bar->OnPointerMove(x, y);
    }
    bool OnTimer(UINT_PTR id) override { return BarShown() && bar->OnTimer(id); }
    bool OnWheel(float x, float y, float notches) override {
        if (!open || !Inside(Shown(), x, y)) return false;
        // One row a notch, and the row that arrives at the control's own row is the choice.
        // A notch is a step through the items here and not a distance: this is a list of
        // things to choose, and a wheel that moved it 66 DIPs, as the page's does, would
        // leave the chosen row somewhere other than under the control it was chosen from.
        //
        // Taken even at either end. Passed on, it would scroll the page out from under an
        // open list -- and on a page that lays itself out in response to a scroll, take the
        // list away with it.
        const int step = (int)std::lround(-notches);
        Select(selected + (step != 0 ? step : (notches > 0.0f ? -1 : 1)));
        if (BarShown()) { bar->Wake(); bar->Poll(); }
        return true;
    }

    void Paint(const Painter &p) override {
        const Palette &c = *p.pal;
        const D2D1_RECT_F h = Head();
        p.FillRound(h, metric::kRadiusControl,
                    !enabled ? c.controlBg
                             : Mix(c.controlBg, c.controlBgHover,
                                   open ? 1.0f : hoverT));
        p.StrokeRound(h, metric::kRadiusControl, c.controlStroke);
        const D2D1_COLOR_F fg = enabled ? c.textPrimary : c.textDisabled;
        if (selected >= 0 && selected < (int)options.size())
            p.Text(options[selected], { h.left + 11, h.top, h.right - 32, h.bottom },
                   p.font->body, fg);
        p.Text(glyph::kChevron, { h.right - 28, h.top, h.right, h.bottom }, p.font->icon,
               c.textSecondary);
        if (focus && owner && owner->showFocusRing) {
            const D2D1_RECT_F o = { h.left - 2, h.top - 2, h.right + 2, h.bottom + 2 };
            p.StrokeRound(o, metric::kRadiusControl + 2, c.textPrimary, 2.0f);
        }
        const float openF = openT.value;
        if (openF <= 0.0f) return;

        // Where the lid is *now*, and the rows where they rest. The two are separate, and
        // that separation is the whole of the animation: the frame grows out of the
        // control's own row while the rows stay exactly where the layout put them, so the
        // chosen row is over the control from the first frame to the last, and what opens
        // is a window onto a list rather than a list that slides into place.
        const D2D1_RECT_F s = Shown();
        if (s.bottom - s.top < 2.0f) return;

        // A flyout is not a card: it is over the page rather than part of it, so it gets
        // an opaque surface of its own and a shadow.
        //
        // The shadow is Fluent's, which is a blur, and there is no cheap real blur in
        // Direct2D without an effect and a layer per frame -- so this is twelve rounded
        // rectangles at 0.9 per cent, the largest and faintest outermost, which stack into
        // a falloff smooth enough to read as one. It was five at four per cent, and five at
        // four per cent is a black band with an edge on it: at the size of a flyout the
        // steps show as bands, and the four per cent at the outermost layer is not faint
        // enough to disappear. Four DIPs down and none up, because the light is above the
        // popup and the popup hangs off the control it belongs to.
        constexpr int kShadowLayers = 12;
        constexpr float kShadowReach = 14.0f;
        constexpr float kShadowDrop = 4.0f;
        for (int i = kShadowLayers; i >= 1; i--) {
            const float e = kShadowReach * (float)i / (float)kShadowLayers;
            p.FillRound({ s.left - e, s.top - e + kShadowDrop,
                          s.right + e, s.bottom + e + kShadowDrop },
                        8.0f + e, Fade(Rgb(0x000000, 0.009f), openF));
        }
        p.FillRound(s, 8.0f, Fade(c.flyoutBg, openF));
        p.StrokeRound(s, 8.0f, Fade(c.flyoutStroke, openF));

        // The rows, clipped to the lid: what it has not grown over yet is not drawn, and a
        // row the leading edge has reached half way through is cut there.
        p.rt->PushAxisAlignedClip({ s.left, s.top + 1, s.right, s.bottom - 1 },
                                  D2D1_ANTIALIAS_MODE_ALIASED);
        const D2D1_POINT_2F at = open ? Cursor() : D2D1::Point2F();
        const int hot = open ? RowAt(at.x, at.y) : -1;
        for (size_t i = 0; i < options.size(); i++) {
            const float top = RowTop((int)i);
            const D2D1_RECT_F row = { s.left + 4, top, s.right - 4, top + rowH };
            if (row.bottom <= s.top || row.top >= s.bottom) continue;
            const bool over = (int)i == hot;
            if (over) p.FillRound(row, metric::kRadiusControl, Fade(c.subtleHover, openF));
            if ((int)i == selected) {
                // Fluent marks the chosen row with a short accent bar on the left rather
                // than by filling it, which keeps the hover highlight legible on top of it.
                // The bar sits in the panel's own margin and the label beside it starts at
                // the same x as the control's own label, so the moment the panel settles the
                // chosen row *is* the control's row, down to the pixels.
                p.rt->FillRoundedRectangle(
                    D2D1::RoundedRect({ row.left - 3, row.top + 8, row.left, row.bottom - 8 },
                                      1.5f, 1.5f), p.Brush(Fade(c.accent, openF)));
            }
            p.Text(options[i], { row.left + 7, row.top, row.right, row.bottom },
                   p.font->body, Fade(c.textPrimary, openF));
        }
        // The bar once the list has arrived: its line is a setter, and at full strength
        // over a list still growing it would arrive first.
        if (BarShown() && openF >= 1.0f) {
            SyncBar();
            bar->Paint(p);
        }
        p.rt->PopAxisAlignedClip();
    }
};

// ---------------------------------------------------------------- TextBox

// A single-line text field, drawn.
//
// This was a real child EDIT control, and it had to stop being one: the window carries
// `WS_EX_NOREDIRECTIONBITMAP` so that Mica is visible behind it (window.h says why),
// and a window with no redirection surface has nowhere for USER32 to draw a child
// control. The choice was Mica or the EDIT, and the EDIT lost.
//
// So this reimplements the part of an edit control a settings field actually uses: a
// caret, a selection made with the keyboard or the mouse (press, drag, Shift+click,
// double-click), the six navigation keys, the four clipboard commands, and IME
// input. It does not reimplement undo, drag-and-drop, right-click, spell checking,
// multiple lines or accessibility, and it should not grow them -- a dialog that needs
// those wants a redirected window with a real edit control, not a bigger version of
// this.
//
// The layout object is kept rather than rebuilt per paint. Both things this needs --
// where the caret goes for a character index, and which character index a click landed
// on -- are `IDWriteTextLayout` queries, and creating a layout per query turns a
// 60-character path into 60 layouts on every mouse move.
struct TextBox : Widget {
    std::wstring text;
    size_t caret  = 0;      // index into `text`
    size_t anchor = 0;      // the other end of the selection; equal to caret when none
    float  scroll = 0.0f;   // how far the text is scrolled left, in DIPs
    std::wstring placeholder;
    // The field holds a file-system path. Paste then also drops the quotes that
    // Explorer's "Copy as path" puts round what it copies, and any trailing spaces --
    // neither is part of the path, and both are what somebody would have to delete by
    // hand. Off for ordinary text, where a quoted word is meant.
    bool pathField = false;
    std::function<void(const std::wstring &)> onChange;
    // Fired when the field is finished with -- Enter, or focus leaving it -- and not
    // on every keystroke. See Widget::OnBlur for why a text field needs both.
    std::function<void(const std::wstring &)> onCommit;

    // The layout is a cache of what the text, the format and the theme already say, so
    // it is mutable: the queries below are const and are called from places that have no
    // business rebuilding text layout, the IME among them. Built on demand, dropped by
    // Dirty().
    mutable IDWriteTextLayout *layout = nullptr;

    ~TextBox() override { if (layout) layout->Release(); }

    bool Focusable() const override { return true; }
    bool TextCursor() const override { return true; }
    void OnBlur() override { if (onCommit) onCommit(text); }

    // Fluent's text field padding: 11 DIPs each side.
    float InnerLeft() const  { return rect.left + 11.0f; }
    float InnerWidth() const { return Width(rect) - 22.0f; }

    void SetText(const std::wstring &s) {
        text = s;
        caret = anchor = text.size();
        Dirty();
    }
    void Dirty() {
        if (layout) { layout->Release(); layout = nullptr; }
    }
    void Ensure() const {
        if (layout || !owner || !owner->dw || !owner->fonts.body) return;
        owner->dw->CreateTextLayout(text.c_str(), (UINT32)text.size(), owner->fonts.body,
                                    100000.0f, 100.0f, &layout);
        // The shared body format is vertically centred, because every other call site
        // hands DirectWrite a rectangle the size of its control and wants the text in
        // the middle of it. A *layout* is different: it centres within its own
        // `maxHeight`, which is the 100 above, so the glyphs land forty pixels below
        // the origin they are drawn at -- outside a 32-DIP field, and clipped away.
        // The field came up empty and the box around it did not, which is a fault that
        // looks like the text was never set.
        if (layout) layout->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
    }
    float XOf(size_t index) const {
        Ensure();
        if (!layout) return 0.0f;
        FLOAT x = 0, y = 0;
        DWRITE_HIT_TEST_METRICS m = {};
        layout->HitTestTextPosition((UINT32)index, FALSE, &x, &y, &m);
        return x;
    }
    // The gap nearest `localX`, which is where a caret goes. `under`, if asked for, is
    // the character the point is actually over -- not the same thing in the right half
    // of a letter, and the one a double-click has to start from.
    size_t IndexAt(float localX, size_t *under = nullptr) const {
        Ensure();
        Ensure();
        if (under) *under = 0;
        if (!layout) return 0;
        BOOL trailing = FALSE, inside = FALSE;
        DWRITE_HIT_TEST_METRICS m = {};
        layout->HitTestPoint(localX, 0.0f, &trailing, &inside, &m);
        if (under) *under = (size_t)m.textPosition;
        return (size_t)m.textPosition + (trailing ? 1u : 0u);
    }

    bool HasSelection() const { return caret != anchor; }
    size_t SelLo() const { return caret < anchor ? caret : anchor; }
    size_t SelHi() const { return caret < anchor ? anchor : caret; }
    std::wstring Selected() const {
        return HasSelection() ? text.substr(SelLo(), SelHi() - SelLo()) : std::wstring();
    }
    void DeleteSelection() {
        if (!HasSelection()) return;
        text.erase(SelLo(), SelHi() - SelLo());
        caret = anchor = SelLo();
        Dirty();
    }
    void Changed() {
        Dirty();
        if (onChange) onChange(text);
    }

    // Keep the caret in view. Without this a path longer than the field types itself
    // off the right-hand edge and the person is editing something they cannot see.
    void ScrollToCaret() {
        const float x = XOf(caret);
        if (x - scroll > InnerWidth() - 2) scroll = x - InnerWidth() + 2;
        if (x - scroll < 0)                scroll = x;
        const float full = XOf(text.size());
        if (full - scroll < InnerWidth() && scroll > 0)
            scroll = full > InnerWidth() ? full - InnerWidth() : 0.0f;
        if (scroll < 0) scroll = 0;
    }

    // --- the mouse ---------------------------------------------------------------
    //
    // The caret moves on the press, not on the release. It used to move in OnClick,
    // which is the release, and that left a drag nowhere to start from: the field could
    // not select anything with the mouse, and a user reported it as a text box that was
    // only a picture of one.
    bool  selecting = false;       // between a press and its release
    // The window class has no CS_DBLCLKS -- with it, the second click of a quick pair
    // arrives as WM_LBUTTONDBLCLK, and every button in the window would miss it -- so a
    // double-click reaches this as two presses and is recognised here.
    DWORD lastPressTime = 0;
    float lastPressX = -1.0e6f;

    bool TracksPointer() const override { return selecting; }

    void OnPress(float x, float /*y*/) override {
        size_t under = 0;
        const size_t at = IndexAt(x - InnerLeft() + scroll, &under);
        const DWORD now = (DWORD)GetMessageTime();
        const float slop = (float)GetSystemMetrics(SM_CXDOUBLECLK) / 2.0f /
                           (owner ? owner->scale() : 1.0f);
        const bool twice = lastPressTime != 0 && now - lastPressTime <= GetDoubleClickTime() &&
                           std::fabs(x - lastPressX) <= slop;
        const bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        lastPressX = x;
        if (twice && !shift) {
            SelectPieceAt(under);
            lastPressTime = 0;     // a third click is a fresh one, not a second pair
            selecting = false;
        } else {
            lastPressTime = now;
            caret = at;
            if (!shift) anchor = at;
            selecting = true;
        }
        ScrollToCaret();
    }
    void OnDrag(float x, float /*y*/) override {
        if (!selecting) return;
        caret = IndexAt(x - InnerLeft() + scroll);
        ScrollToCaret();
    }
    void OnRelease() override { selecting = false; }

    // Nothing left to do: the press has placed the caret. This used to place it from
    // GetCursorPos, and Space reaches OnClick from the keyboard -- so every space typed
    // into `Program Files` moved the caret to wherever the pointer was resting and went
    // in there. OnKey now keeps Space from coming here at all; this is the other half.
    void OnClick() override {}

    // A double-click selects one piece of the text: the run between two separators,
    // where a separator is a backslash, a slash or a space. Spaces alone would select
    // `Files\Micula` out of `C:\Program Files\Micula`, and the piece somebody
    // double-clicks in a path to replace is one folder name.
    static bool Separator(wchar_t ch) {
        return ch == L'\\' || ch == L'/' || ch == L' ' || ch == L'\t';
    }
    void SelectPieceAt(size_t at) {
        if (text.empty()) { caret = anchor = 0; return; }
        if (at >= text.size()) at = text.size() - 1;
        if (Separator(text[at])) { anchor = at; caret = at + 1; return; }
        size_t lo = at, hi = at;
        while (lo > 0 && !Separator(text[lo - 1])) lo--;
        while (hi < text.size() && !Separator(text[hi])) hi++;
        anchor = lo;
        caret = hi;
    }

    // What of the clipboard this field keeps. One line: text copied out of a document
    // or a terminal routinely ends in a line break, and a single-line field that took it
    // would hold a character nobody can see or delete. A break in the middle ends the
    // paste there rather than being joined up, because joining guesses at what the lines
    // were meant to be.
    std::wstring Pasted(std::wstring in) const {
        const size_t br = in.find_first_of(L"\r\n");
        if (br != std::wstring::npos) in.erase(br);
        if (pathField) {
            while (!in.empty() && in.back() == L' ') in.pop_back();
            if (in.size() >= 2 && in.front() == L'"' && in.back() == L'"')
                in = in.substr(1, in.size() - 2);
        }
        return in;
    }

    bool OnChar(wchar_t ch) override {
        if (!enabled) return false;
        DeleteSelection();
        text.insert(caret, 1, ch);
        caret = anchor = caret + 1;
        Changed();
        ScrollToCaret();
        return true;
    }

    bool OnKey(WPARAM vk) override {
        if (!enabled) return false;
        const bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        const bool ctrl  = (GetKeyState(VK_CONTROL) & 0x8000) != 0;

        if (ctrl) {
            switch (vk) {
            case 'A': anchor = 0; caret = text.size(); return true;
            case 'C': if (HasSelection()) micula::SetClipboardText(owner->hwnd, Selected());
                      return true;
            case 'X': if (HasSelection()) {
                          micula::SetClipboardText(owner->hwnd, Selected());
                          DeleteSelection();
                          Changed();
                      }
                      return true;
            case 'V': {
                const std::wstring in = Pasted(micula::ClipboardText(owner->hwnd));
                if (in.empty()) return true;
                DeleteSelection();
                text.insert(caret, in);
                caret = anchor = caret + in.size();
                Changed();
                ScrollToCaret();
                return true;
            }
            default: return false;
            }
        }

        switch (vk) {
        case VK_RETURN:
            // Consumed rather than left to the window, which would otherwise read it
            // as the default button while the person was still in the field.
            if (onCommit) onCommit(text);
            return true;
        case VK_SPACE:
            // The space itself arrives as WM_CHAR. The key is consumed so the window does
            // not also read it as "operate the focused control" -- see OnClick.
            return true;
        case VK_LEFT:
            if (caret > 0) caret--;
            if (!shift) anchor = caret;
            break;
        case VK_RIGHT:
            if (caret < text.size()) caret++;
            if (!shift) anchor = caret;
            break;
        case VK_HOME:
            caret = 0;
            if (!shift) anchor = caret;
            break;
        case VK_END:
            caret = text.size();
            if (!shift) anchor = caret;
            break;
        case VK_BACK:
            if (HasSelection()) DeleteSelection();
            else if (caret > 0) { text.erase(caret - 1, 1); caret = anchor = caret - 1; }
            else return true;
            Changed();
            break;
        case VK_DELETE:
            if (HasSelection()) DeleteSelection();
            else if (caret < text.size()) text.erase(caret, 1);
            else return true;
            Changed();
            break;
        default:
            return false;
        }
        ScrollToCaret();
        return true;
    }

    // Where the IME should open: at the caret, in the field's own space.
    //
    // Measured now rather than read from a position cached by the last paint. This is
    // called from the message loop when a composition starts, which can arrive before
    // the WM_PAINT for a key that has just moved the caret, and the candidate window
    // then opened wherever the caret used to be. Nothing here depends on a paint having
    // happened: the layout is created on demand -- see Ensure.
    bool CaretPoint(D2D1_POINT_2F *out) const override {
        if (out) *out = D2D1::Point2F(InnerLeft() + XOf(caret) - scroll, rect.bottom);
        return true;
    }

    void Paint(const Painter &p) override {
        const Palette &c = *p.pal;
        const bool active = focus;

        // Three states crossing over: the resting fill, the hover fill, and the lighter
        // one Fluent gives a field with the caret in it (ControlFillColorInputActive,
        // which is opaque white in light and 70% #1E1E1E in dark). The focus fill wins
        // over hover, which is why it is the outer mix.
        p.FillRound(rect, metric::kRadiusControl,
                    !enabled ? c.controlBg
                             : Mix(Mix(c.controlBg, c.controlBgHover, hoverT),
                                   c.controlBgInput, focusT));
        p.StrokeRound(rect, metric::kRadiusControl, c.controlStroke);
        // The accent underline, **whole, the moment the field takes focus.**
        //
        // It used to grow out of the centre, which is Material Design's text field and
        // not Fluent's: WinUI changes `BorderThickness` to 0,0,0,2 in a visual-state
        // setter, and a setter has no duration. The fill behind it is the part that
        // crosses over, and it does that above.
        p.Line(rect.left + metric::kRadiusControl, rect.bottom - 1,
               rect.right - metric::kRadiusControl, rect.bottom - 1,
               active && enabled ? c.accent : c.controlStrokeBottom,
               active && enabled ? 2.0f : 1.0f);

        Ensure();

        const D2D1_RECT_F inner = { InnerLeft(), rect.top + 1, rect.right - 11, rect.bottom - 1 };
        p.rt->PushAxisAlignedClip(inner, D2D1_ANTIALIAS_MODE_ALIASED);

        if (text.empty() && !placeholder.empty()) {
            p.Text(placeholder, inner, p.font->body, c.textDisabled);
        } else if (layout) {
            if (HasSelection()) {
                const float a = XOf(SelLo()) - scroll, b = XOf(SelHi()) - scroll;
                // The accent at a third, which is what Fluent's selection highlight
                // is: the text stays its own colour and reads through it.
                p.Fill({ inner.left + a, rect.top + 5, inner.left + b, rect.bottom - 5 },
                       D2D1::ColorF(c.accent.r, c.accent.g, c.accent.b, 0.35f));
            }
            // Vertically centred by hand: the layout is drawn from its origin and its
            // own height is the line height, not the box's.
            DWRITE_TEXT_METRICS tm = {};
            layout->GetMetrics(&tm);
            const float ty = rect.top + (Height(rect) - tm.height) / 2;
            p.rt->DrawTextLayout(D2D1::Point2F(inner.left - scroll, ty), layout,
                                 p.Brush(enabled ? c.textPrimary : c.textDisabled),
                                 D2D1_DRAW_TEXT_OPTIONS_NONE);
        }

        if (active && owner && owner->caretOn && enabled) {
            const float x = inner.left + XOf(caret) - scroll;
            p.Line(x, rect.top + 6, x, rect.bottom - 6, c.textPrimary, 1.0f);
        }
        p.rt->PopAxisAlignedClip();
    }
};

// ---------------------------------------------------------------- ProgressBar

struct ProgressBar : Widget {
    float value = 0.0f;      // 0..1
    bool  indeterminate = false;

    // WinUI's indeterminate bar, to the numbers -- and it is not "a segment sweeps across".
    // It is **two** bars of different lengths crossing on a 2 s loop, the second staggered
    // three quarters of a second behind the first, so that for half of the cycle one is
    // leaving at the right while the other arrives at the left. That overlap is the whole
    // of the look, and the two lengths are why the bar is not the same shape on the way out
    // as on the way in.
    //
    // From ProgressBar.xaml's Indeterminate storyboard -- 2 s, RepeatBehavior Forever, two
    // TranslateX animations on KeySpline 0.4, 0, 0.6, 1 -- and the widths and positions
    // ProgressBar.cpp hands it through TemplateSettings:
    //
    //   the first   40% of the bar wide, -100% to 300% of its own width, 0 to 1.5 s* , held
    //   the second  60% of the bar wide, -150% to 166% of its own width, 0.75 to 2 s
    //
    // (*held to the end of the cycle, which is what the discrete key frame at 2 s is for.)
    // Multiplied out, they come in at -0.40 and 1.20 of the bar, and -0.90 and 0.996.
    static constexpr float kSweep = 2.0f;
    struct Sweep { float wide, in, out, begin, end; };
    static constexpr Sweep kSweeps[2] = {
        { 0.4f, -1.00f, 3.00f, 0.00f, 1.50f },
        { 0.6f, -1.50f, 1.66f, 0.75f, 2.00f },
    };

    // One bar's left edge at `t` seconds into the cycle, as a fraction of the control's
    // width: held at `in` until its turn comes round, then on cubic-bezier(0.4, 0, 0.6, 1)
    // to `out`.
    static float SweepLeft(const Sweep &s, float t) {
        const float u = std::clamp((t - s.begin) / (s.end - s.begin), 0.0f, 1.0f);
        return s.wide * (s.in + (s.out - s.in) * motion::InOut(u));
    }

    // Self-driving, because the alternative was a page remembering to advance it: the
    // indeterminate bar's sweep was a field nothing ever wrote, so the segment sat
    // still at the left-hand end wherever one was used.
    bool Animating() const override { return Widget::Animating() || indeterminate; }

    // Where in the cycle *now* is, in seconds.
    //
    // Read from the clock rather than counted across frames, and that is the whole of what
    // keeps the sweep from restarting. A ProgressBar is rebuilt for all sorts of reasons --
    // a resize, another control changing the shape of the page, a page that lays itself out
    // in response to a scroll -- and a phase accumulated per frame began the sweep again
    // from the left on each of them, while the operation it was reporting carried on. A
    // periodic animation has nothing to lose by this: the clock belongs to the window, not
    // to the control, and two bars on one page are in step rather than racing.
    static float Cycle() {
        return (float)std::fmod(micula::MonotonicSeconds(), (double)kSweep);
    }

    // No Tick: there is no phase to advance. Plain Widget::Tick runs the pointer fades.

    void Paint(const Painter &p) override {
        const Palette &c = *p.pal;
        const float cy = rect.top + Height(rect) / 2;
        const D2D1_RECT_F track = { rect.left, cy - 1.5f, rect.right, cy + 1.5f };
        p.rt->FillRoundedRectangle(D2D1::RoundedRect(track, 1.5f, 1.5f), p.Brush(c.controlStroke));
        if (indeterminate) {
            const float w = Width(rect);
            const float t = Cycle();
            for (const Sweep &s : kSweeps) {
                const float left = rect.left + SweepLeft(s, t) * w;
                // Cut at the ends of the track: most of each bar's travel is off it, and a
                // bar that is off the end must not be drawn past the corner.
                const D2D1_RECT_F seg = { (std::max)(rect.left, left), track.top,
                                          (std::min)(rect.right, left + s.wide * w), track.bottom };
                if (seg.right > seg.left)
                    p.rt->FillRoundedRectangle(D2D1::RoundedRect(seg, 1.5f, 1.5f), p.Brush(c.accent));
            }
        } else {
            const D2D1_RECT_F fill = { rect.left, track.top,
                                       rect.left + Width(rect) * std::clamp(value, 0.0f, 1.0f),
                                       track.bottom };
            if (fill.right > fill.left)
                p.rt->FillRoundedRectangle(D2D1::RoundedRect(fill, 1.5f, 1.5f), p.Brush(c.accent));
        }
    }
};

}  // namespace micula
