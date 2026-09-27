# opane examples

Example programs for [opane](https://github.com/cresmarmat-an/opane), the
application, window, input, and interface library for C++20. Each program
links opane only.

The sounds and images the programs use are generated when they start, so the
repository contains no binary files.

Documentation: [opane](https://cresmarmat-an.github.io/opane/).

## Building and running

```bash
cmake -S . -B build
```

```bash
cmake --build build --config Release --parallel
```

The programs are written to `build/bin`, with `SDL3.dll` copied next to them.

The first configure downloads opane 0.0.1 along with SDL3, stb, and
miniaudio, which takes a few minutes. Later configures reuse the download. To
build against your own copy of opane, add
`-DFETCHCONTENT_SOURCE_DIR_OPANE=path/to/opane` when configuring.

You need CMake 3.22 or later, a C++20 compiler, and the Windows SDK, whose
`dxc` compiles the shaders for Direct3D 12. If the
[Vulkan SDK](https://vulkan.lunarg.com) is installed, the shaders are also
compiled for Vulkan.

## The programs

| | |
| --- | --- |
| [01-interface](#01-interface) | Widgets, theming, a custom shader, a post-process chain, and a custom element |
| [02-text](#02-text) | Text from 8 to 200 pixels, a text material, and editable fields |
| [03-custom-ui](#03-custom-ui) | A frameless window, frosted glass, and widgets skinned from a theme file |
| [04-ui-checks](#04-ui-checks) | Automated checks that drive the interface with simulated input |

## 01-interface

The element tree, a vertical stack layout, the built-in widgets, switching
themes at runtime, and audio with a volume group. One panel uses a custom
shader, `aurora.hlsl`, which is compiled at startup and recompiled when you
save it.

The sidebar uses a post-process chain. It draws itself and its widgets into a
texture, `ripple.hlsl` runs over that, and `crt.hlsl` runs over the result.
The buttons and slider inside still work normally. The "Post-process sidebar"
checkbox turns the effect off.

In the bottom-right corner, a PNG written at startup is loaded by file name
from an added asset root and shown at 160, 64, and 24 pixels. Its fine rings
show the mip maps at work: the smallest copy blends to an even tone instead of
flickering. Next to it, an image that does not exist shows the magenta and
black placeholder, and the log lists every path that was tried.

`RadialGauge`, at the top of `main.cpp`, is a custom element. It implements
its own measuring, painting, hit testing (a circle, so clicks on its corners
pass through), and event handling, and it takes part in layout, input, and
focus like a built-in widget. It uses only the public API.

## 02-text

Text from 8 to 200 pixels in one window. The column on the left is one font
at eleven sizes. Below it are characters from several scripts, a line whose
words have their own sizes and colours, and a paragraph wrapped to its width.

On the right, the heading is drawn with a text material that adds a gradient
and an outline; the outline widens when the pointer is over it. Under the
heading are three text fields: a plain one, one limited to eight characters,
and a password field. They support selection and the usual editing shortcuts.

| Key | Action |
| --- | --- |
| Click, drag | Focus a field and select text |
| Shift + arrows | Extend the selection |
| Ctrl + A, C, X, V | Select all, copy, cut, paste |
| Home, End | Move to the start or end of the line |
| Wheel | Scroll the column of sizes |
| Escape | Quit |

## 03-custom-ui

An interface styled by `03-custom-ui/theme.json`. The file is watched, so
editing and saving it while the program runs changes the look.

The window has no system frame. The title bar is drawn by the interface: drag
it to move the window, double-click it to maximize, and use its minimize,
maximize, and close buttons. The window's edges resize it. The background is
an image generated at startup, and the sidebar and control panel are frosted
glass that blurs it. The cards use a named style from the theme and lift and
glow under the pointer. There is also a nine-slice frame, a tiled pattern with
an inset shadow, a glowing orb, and a wave drawn with a custom painter. The
controls on the right are built-in widgets skinned by the theme's `widgets`
section.

| Key | Action |
| --- | --- |
| Escape | Quit |

## 04-ui-checks

Automated checks for the interface. The program opens a hidden window and
sends simulated mouse and keyboard input through SDL's event queue, the same
path real input takes, so the checks test what a user would experience.

It covers layout, removing elements, keyboard focus and Tab order, every
control, text editing and undo, popups, menus and shortcuts, drop-downs,
modal dialogs, the splitter, tabs, windows, docking (tabs, splits, floating
windows, closing, and saved layouts), lists and trees, and repeatedly
building and removing a large screen.

```bash
build/bin/04-ui-checks --scale 1.5
```

`--scale` runs every check with the interface scaled, to confirm that sizes,
positions, and input agree at that scale.

There is nothing to press. The program prints its results and exits with a
non-zero code if a check failed.

## License

MIT, copyright (c) 2026 Cresmar Mat-an. See [LICENSE](LICENSE).
