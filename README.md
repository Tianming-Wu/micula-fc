# Micula

A small Fluent control set for plain Win32 programs — the settings window of a tool,
an installer, a configurator — in three headers and about 85 KB of code.

*Micula* is Latin for "a small crumb". *Mica* is the crumb it is a diminutive of, and
also the material the window is drawn on.

- **Mica behind the window.** Direct2D on a DirectComposition swap chain with real
  alpha, `WS_EX_NOREDIRECTIONBITMAP`, and DWM's system backdrop. Windows 10 and early
  Windows 11 get an opaque background instead; the window works the same.
- **A caption drawn in the same pass as the page**, so the backdrop reaches the top edge
  — while hover on the maximise button still opens Windows 11's snap layouts, and
  dragging, double-click and the window menu are still Windows' own.
- **Numbers taken from WinUI rather than guessed.** The palette is Fluent's published
  tokens, alpha included. The accent is read from the machine, in the shade Windows
  itself uses for each theme. Control sizes, timings and easing curves come from WinUI's
  control templates, and the comments say which one.
- **Nothing to install.** Header-only, Windows SDK only, no WinRT, no runtime. A program
  built with `/MT` is one `.exe` that runs on a machine that has installed nothing.

## Controls

| Control | Notes |
|---|---|
| `Button` | Accent, Standard, Subtle and Link styles; optional icon glyph |
| `CheckBox` | Optional second line of detail |
| `ToggleSwitch` | Animated knob; draws only the switch when given no label |
| `Segmented` | Three or four mutually exclusive options side by side |
| `Slider` | `onChange` while dragging, `onCommit` once when the gesture ends |
| `ScrollBar` | WinUI's three states (hidden, indicator, expanded), repeat buttons, "Always show scrollbars" honoured |
| `DropDown` | Flyout that flips upwards when there is no room below, and scrolls when there is no room either way |
| `TextBox` | Single line: caret, mouse and keyboard selection, clipboard, IME; `pathField` for paths |
| `ProgressBar` | Determinate and indeterminate |

Anything else is a `Widget` subclass of your own: a rectangle, a `Paint`, and whichever
input hooks it needs.

## A window

```cpp
#include <micula/micula.h>

struct Hello : micula::Window {
    bool on = false;   // state lives in the page, not in the controls

    const wchar_t *ClassName() const override { return L"Hello"; }
    const wchar_t *Title() const override { return L"Hello"; }

    // Builds every control from the page's state. Runs on resize, and whenever
    // the page calls it.
    void Layout() override {
        ClearWidgets();
        auto *sw = Add(new micula::ToggleSwitch(L"Enabled", on, [this](bool v) { on = v; }));
        sw->rect = micula::Rect(24, 56, 280, 32);
        auto *ok = Add(new micula::Button(L"Close", micula::ButtonStyle::Accent,
                                          [this] { PostMessageW(hwnd, WM_CLOSE, 0, 0); }));
        ok->rect = micula::Rect(24, 104, 120, 32);
    }
};

int WINAPI wWinMain(HINSTANCE, HINSTANCE, wchar_t *, int) {
    micula::EnablePerMonitorDpi();
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);   // WIC, for icons and pictures
    Hello w;
    return w.Create(340, 170, false, nullptr) ? w.Run() : 1;
}
```

`examples/gallery` puts every control on one scrolling page, and is the reference for the
parts this sample leaves out: a fixed header over scrolling cards, a persistent scroll
bar, text drawn in `PaintPage`, and rebuilding the page from inside a callback.

## Building

Header-only, C++17, MSVC. The headers name every library they need with
`#pragma comment`, so a bare compiler line is enough:

```bat
cl /std:c++17 /EHsc /O1 /MT /I include app.cpp /link /SUBSYSTEM:WINDOWS
```

With CMake:

```cmake
add_subdirectory(micula)            # or FetchContent
target_link_libraries(app PRIVATE micula::micula)
```

`cmake -S . -B build` on this repository builds the gallery. The headers are plain
ASCII and call the `W` functions throughout, so they compile the same with or without
`UNICODE` and `/utf-8`, and after a `<windows.h>` that was included without `NOMINMAX`.

Measured with MSVC 14.50, x64, `/O1 /GL`, all at the same flags: a probe that places all
nine controls is 204 KB with the CRT linked statically (`/MT`) and 101 KB against the DLL
CRT (`/MD`). A bare Win32 window using the same standard containers is 117 KB and 17 KB,
so Micula itself is 83 to 88 KB. The gallery is 248 KB and 114 KB.

## What a program provides

- **COM** initialised on the UI thread, apartment-threaded, before `Window::Create`.
  The caption icon and `Window::Image` go through WIC; without COM both are silently
  missing.
- **Per-monitor DPI awareness**, ideally in the manifest. `EnablePerMonitorDpi()` also
  sets it from code.
- **Timer ids** outside 2 and 4–7, which Micula uses. A page's own timers and any
  message Micula does not handle reach `Window::OnAppMessage`.

## How a page works

- `Layout()` throws the controls away and builds them again from the page's own fields.
  A control's callback writes the field it shows. Do not keep a pointer to a control
  across a `Layout()` unless you set `persistent` on it.
- `Layout()` may be called from inside a control's callback — a "Next" button that
  replaces its own page. The window keeps the old controls alive until that message
  returns.
- Everything that is not a control is drawn in `PaintPage`. Coordinates are DIPs; the
  window's DPI is on the render target.
- `ClipRect()` names the scrolling part of the page, and controls with `scrolls` set are
  clipped to it. `ContentTransform()` offsets and fades that part while it is drawn, and
  hit-testing subtracts the same offset, which is how a page glides or arrives without
  being laid out every frame.
- Idle windows block in `GetMessage`. While anything animates, frames are paced by the
  compositor's clock where Windows has one (Windows 11) and by a high-resolution timer
  where it does not.

## Limitations

These are known, and each is a reason not to use Micula for something:

- **No accessibility.** Nothing publishes a UI Automation provider and high-contrast
  themes are not followed, so a screen reader sees an empty window. This is the largest
  gap. A program that must be usable without sight needs another way to do the same
  things — a command line, a config file.
- **No layout system.** Controls are placed in DIP rectangles that the page computes.
  There are no panels, no measure/arrange and no data binding.
- **`TextBox` is single-line**, through IMM32 rather than TSF, with no undo, context
  menu, drag-and-drop or right-to-left text.
- **The caption icon is drawn as a monochrome mask** in the caption's text colour. That
  suits a white mark on either theme and turns a colour icon into its silhouette.
- **Acrylic is not drawn.** Flyouts use the solid colour WinUI falls back to when
  transparency effects are off.

## License

MIT. See [LICENSE](LICENSE).

Micula is not affiliated with or endorsed by Microsoft. WinUI, Fluent and Windows are
Microsoft's names, used here only to say what this imitates.
