// Example 04: automated interface checks.
//
// A hidden window receives simulated input through SDL's event queue, the same
// path real mouse and keyboard input takes. Nothing calls a widget's event
// handler directly, so a passing check means the feature works for a user.
//
// It covers layout, removing elements, keyboard focus, every built-in control,
// popups and menus, the dock space (tabs, splits, floating windows, docking
// back, closing, and saving and restoring a layout), and custom looks: states,
// transforms, theme files, and drawing.

#include <opane/opane.h>

#include "../common/CrashReport.h"

#include <SDL3/SDL.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace
{

int g_Failures = 0;
int g_Passes = 0;

void Check(bool condition, const char* description)
{
    std::printf("  [%s] %s\n", condition ? "pass" : "FAIL", description);
    if (condition)
    {
        ++g_Passes;
    }
    else
    {
        ++g_Failures;
    }
}

bool Near(float a, float b, float tolerance = 0.5f)
{
    return std::abs(a - b) <= tolerance;
}

opane::Vec2 Middle(const opane::Rect& rect)
{
    return opane::Vec2{ rect.X + rect.Width * 0.5f, rect.Y + rect.Height * 0.5f };
}

opane::Vec2 Middle(const opane::Element* element)
{
    return Middle(element->GetBounds());
}

// Everything a test does to the window goes through here.
class Driver
{
public:
    explicit Driver(opane::App app) : m_App(app)
    {
    }

    // Tests speak in interface units; SDL speaks in window units, which at a
    // scale of 1.5 are one and a half times as many. Everything pushed is
    // converted, so the same tests hold at any scale.
    float ToWindow(float units) const
    {
        SDL_Window* window = m_App.GetWindow();
        const float density = window != nullptr ? SDL_GetWindowPixelDensity(window) : 1.0f;
        return units * m_App.GetUiScale() / (density > 0.0f ? density : 1.0f);
    }

    void Frame(int count = 1, float seconds = 1.0f / 60.0f)
    {
        for (int index = 0; index < count; ++index)
        {
            m_App.PollEvents();
            m_App.UpdateInterface(seconds);
            m_App.BeginFrame();
            m_App.EndFrame();
        }
    }

    void Move(opane::Vec2 at)
    {
        SDL_Event event{};
        event.type = SDL_EVENT_MOUSE_MOTION;
        event.motion.x = ToWindow(at.X);
        event.motion.y = ToWindow(at.Y);
        event.motion.xrel = ToWindow(at.X - m_Pointer.X);
        event.motion.yrel = ToWindow(at.Y - m_Pointer.Y);
        SDL_PushEvent(&event);
        m_Pointer = at;
        Frame();
    }

    void Button(opane::Vec2 at, bool down, Uint8 button = SDL_BUTTON_LEFT)
    {
        SDL_Event event{};
        event.type = down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP;
        event.button.button = button;
        event.button.down = down;
        event.button.x = ToWindow(at.X);
        event.button.y = ToWindow(at.Y);
        SDL_PushEvent(&event);
        Frame();
    }

    void Click(opane::Vec2 at, Uint8 button = SDL_BUTTON_LEFT)
    {
        Move(at);
        Button(at, true, button);
        Button(at, false, button);
    }

    void DoubleClick(opane::Vec2 at)
    {
        Click(at);
        Click(at);
    }

    // A press, a walk to somewhere else in steps, and a release there.
    void Drag(opane::Vec2 from, opane::Vec2 to, int steps = 8)
    {
        Move(from);
        Button(from, true);
        for (int step = 1; step <= steps; ++step)
        {
            const float t = static_cast<float>(step) / static_cast<float>(steps);
            Move(opane::Vec2{ from.X + (to.X - from.X) * t, from.Y + (to.Y - from.Y) * t });
        }
        Button(to, false);
    }

    void Key(SDL_Scancode scancode, SDL_Keymod modifiers = SDL_KMOD_NONE, bool repeat = false)
    {
        SDL_Event down{};
        down.type = SDL_EVENT_KEY_DOWN;
        down.key.scancode = scancode;
        down.key.mod = modifiers;
        down.key.down = true;
        down.key.repeat = repeat;
        SDL_PushEvent(&down);

        SDL_Event up = down;
        up.type = SDL_EVENT_KEY_UP;
        up.key.down = false;
        up.key.repeat = false;
        SDL_PushEvent(&up);
        Frame();
    }

    void KeyDown(SDL_Scancode scancode, SDL_Keymod modifiers = SDL_KMOD_NONE, bool repeat = false)
    {
        SDL_Event down{};
        down.type = SDL_EVENT_KEY_DOWN;
        down.key.scancode = scancode;
        down.key.mod = modifiers;
        down.key.down = true;
        down.key.repeat = repeat;
        SDL_PushEvent(&down);
        Frame();
    }

    void KeyUp(SDL_Scancode scancode)
    {
        SDL_Event up{};
        up.type = SDL_EVENT_KEY_UP;
        up.key.scancode = scancode;
        SDL_PushEvent(&up);
        Frame();
    }

    // Typed text arrives composed, as SDL delivers it; the string has to
    // outlive the frame that reads it.
    void Type(const std::string& text)
    {
        m_Typed.push_back(text);
        SDL_Event event{};
        event.type = SDL_EVENT_TEXT_INPUT;
        event.text.text = m_Typed.back().c_str();
        SDL_PushEvent(&event);
        Frame();
    }

    opane::App& App() { return m_App; }

private:
    opane::App m_App;
    opane::Vec2 m_Pointer;
    std::vector<std::string> m_Typed;
};

void Reset(Driver& driver)
{
    opane::Element* root = driver.App().GetRoot();
    root->RemoveAllChildren();
    root->ChildLayout = opane::LayoutMode::Absolute;
    root->Padding = 0.0f;
    root->Spacing = 0.0f;
    driver.App().GetOverlay()->RemoveAllChildren();
    driver.Frame(2);
}

} // namespace

int main(int argumentCount, char** arguments)
{
    examples::InstallCrashReport();

    // "--scale 1.5" runs every check with the interface scaled, which is the
    // proof that sizes, positions, and input all agree at that scale.
    float scale = 1.0f;
    for (int index = 1; index + 1 < argumentCount; ++index)
    {
        if (std::string(arguments[index]) == "--scale")
        {
            scale = static_cast<float>(std::atof(arguments[index + 1]));
        }
    }

    std::printf("\nThe interface, driven, at a scale of %.2f (opane %s)\n", static_cast<double>(scale),
                opane::VersionString);
    std::printf("=========================================\n");

    opane::App app = opane::StartApp(
        { .Title = "opane checks", .Width = 1000, .Height = 700, .UiScale = scale, .Hidden = true });
    if (!app.IsValid())
    {
        std::printf("Could not start.\n");
        return 1;
    }

    Driver driver(app);
    driver.Frame(2);
    opane::Element* root = app.GetRoot();

    // -----------------------------------------------------------------------
    std::printf("\n1. Layout\n");
    {
        root->ChildLayout = opane::LayoutMode::Horizontal;
        auto* side = root->Add<opane::Element>();
        side->Size = opane::Size2::FromOffset(200.0f, 0.0f);
        auto* rest = root->Add<opane::Element>();
        rest->Flex = 1.0f;
        driver.Frame();

        const float width = app.GetWindowSize().X;
        Check(Near(side->GetBounds().Width, 200.0f), "a fixed child keeps its own width");
        Check(Near(rest->GetBounds().Width, width - 200.0f), "a Flex child fills the rest of the row");

        auto* third = root->Add<opane::Element>();
        third->Flex = 2.0f;
        driver.Frame();
        const float remaining = width - 200.0f;
        Check(Near(rest->GetBounds().Width, remaining / 3.0f, 1.0f) &&
                  Near(third->GetBounds().Width, remaining * 2.0f / 3.0f, 1.0f),
              "two Flex children split the remainder by their weights");

        root->Spacing = 10.0f;
        driver.Frame();
        Check(Near(rest->GetBounds().Width + third->GetBounds().Width, remaining - 20.0f, 1.0f),
              "and spacing comes out of the remainder, not on top of it");

        side->MinSize = opane::Vec2{ 260.0f, 0.0f };
        driver.Frame();
        Check(Near(side->GetBounds().Width, 260.0f), "MinSize raises a child above its own size");
    }

    // -----------------------------------------------------------------------
    std::printf("\n2. Removing things the tree is holding\n");
    Reset(driver);
    {
        root->ChildLayout = opane::LayoutMode::Vertical;
        auto* panel = root->Add<opane::Panel>();
        panel->Size = opane::Size2{ opane::Dim::FromScale(1.0f), opane::Dim::FromOffset(200.0f) };
        panel->ChildLayout = opane::LayoutMode::Vertical;
        auto* field = panel->Add<opane::TextInput>();
        driver.Frame();

        driver.Click(Middle(field));
        Check(app.GetFocusedElement() == field, "clicking a field gives it the keyboard");

        // The field's parent goes, not the field: the tree has to notice that
        // what it points at was inside.
        root->Remove(panel);
        driver.Frame(2);
        Check(app.GetFocusedElement() == nullptr, "removing its panel takes the keyboard back rather than leaving it dangling");

        driver.Type("still here");
        driver.Key(SDL_SCANCODE_A);
        Check(true, "and typing afterwards reaches nothing, safely");

        // A child that removes itself while its parent is removing all of them.
        auto* box = root->Add<opane::Element>();
        auto* inner = box->Add<opane::Button>();
        driver.Frame();
        root->RemoveAllChildren();
        box->Remove(inner);
        driver.Frame(2);
        Check(root->GetChildren().empty(), "a removal inside a removed subtree is skipped, not followed");

        // Pressed and then removed mid-drag.
        auto* slider = root->Add<opane::Slider>();
        driver.Frame();
        driver.Move(Middle(slider));
        driver.Button(Middle(slider), true);
        root->Remove(slider);
        driver.Frame();
        driver.Move(opane::Vec2{ 10.0f, 10.0f });
        driver.Button(opane::Vec2{ 10.0f, 10.0f }, false);
        Check(true, "an element removed while it holds the pointer lets go of it");

        // A callback that replaces itself while it runs.
        int firstClicks = 0;
        int laterClicks = 0;
        auto* swapper = root->Add<opane::Button>();
        swapper->Size = opane::Size2::FromOffset(120.0f, 34.0f);
        swapper->OnClick = [&, swapper]() {
            ++firstClicks;
            swapper->OnClick = [&]() { ++laterClicks; };
        };
        driver.Frame();
        driver.Click(Middle(swapper));
        driver.Click(Middle(swapper));
        Check(firstClicks == 1 && laterClicks == 1, "a click handler can replace itself while it runs");

        // A reference that outlives what it refers to.
        opane::ElementRef<opane::Button> watched = swapper;
        Check(watched.Get() == swapper, "an ElementRef finds its element while it exists");
        root->Remove(swapper);
        driver.Frame(2);
        Check(!watched && watched.Get() == nullptr, "and finds nothing once it has been removed");
    }

    // -----------------------------------------------------------------------
    std::printf("\n3. Keyboard focus and the basic controls\n");
    Reset(driver);
    {
        root->ChildLayout = opane::LayoutMode::Vertical;
        root->Padding = 20.0f;
        root->Spacing = 8.0f;

        int clicks = 0;
        auto* first = root->Add<opane::Button>();
        first->Text = "First";
        first->OnClick = [&]() { ++clicks; };
        auto* second = root->Add<opane::Button>();
        second->Text = "Second";
        auto* label = root->Add<opane::Label>();
        label->Text = "Not focusable";
        auto* check = root->Add<opane::Checkbox>();
        check->Text = "Check";
        auto* slider = root->Add<opane::Slider>();
        slider->Minimum = 0.0f;
        slider->Maximum = 10.0f;
        slider->Step = 1.0f;
        slider->Value = 5.0f;
        driver.Frame();

        driver.Key(SDL_SCANCODE_TAB);
        Check(app.GetFocusedElement() == first, "Tab reaches the first focusable element");
        Check(first->IsFocusVisible(), "and focus that arrived by keyboard shows its ring");
        driver.Key(SDL_SCANCODE_TAB);
        driver.Key(SDL_SCANCODE_TAB);
        Check(app.GetFocusedElement() == check, "Tab skips what cannot take focus");
        driver.Key(SDL_SCANCODE_TAB, SDL_KMOD_LSHIFT);
        Check(app.GetFocusedElement() == second, "Shift+Tab goes back");

        driver.Click(Middle(first));
        Check(clicks == 1, "a left click presses a button");
        Check(!first->IsFocusVisible(), "and focus that arrived by a click shows no ring");

        driver.Click(Middle(first), SDL_BUTTON_RIGHT);
        Check(clicks == 1, "a right click does not");

        driver.KeyDown(SDL_SCANCODE_SPACE);
        driver.KeyDown(SDL_SCANCODE_SPACE, SDL_KMOD_NONE, true);
        driver.KeyDown(SDL_SCANCODE_SPACE, SDL_KMOD_NONE, true);
        driver.KeyUp(SDL_SCANCODE_SPACE);
        Check(clicks == 2, "a held Space presses once, not once per repeat");

        driver.Click(Middle(check));
        Check(check->Checked, "a checkbox checks on a click");
        driver.Key(SDL_SCANCODE_SPACE);
        Check(!check->Checked, "and unchecks on Space");

        driver.Click(Middle(slider));
        const float clicked = slider->Value;
        driver.Key(SDL_SCANCODE_RIGHT);
        Check(Near(slider->Value, std::min(10.0f, clicked + 1.0f)), "a slider steps with the arrow keys");
        driver.Key(SDL_SCANCODE_HOME);
        Check(Near(slider->Value, 0.0f), "and goes to its minimum on Home");
        driver.Key(SDL_SCANCODE_END);
        Check(Near(slider->Value, 10.0f), "and its maximum on End");

        driver.Click(opane::Vec2{ 990.0f, 690.0f });
        Check(app.GetFocusedElement() == nullptr || app.GetFocusedElement() == root,
              "clicking empty space takes the keyboard away");
    }

    // -----------------------------------------------------------------------
    std::printf("\n4. A text field\n");
    Reset(driver);
    {
        root->ChildLayout = opane::LayoutMode::Vertical;
        root->Padding = 20.0f;
        auto* field = root->Add<opane::TextInput>();
        driver.Frame();

        driver.Click(Middle(field));
        driver.Type("hello");
        driver.Type(" world");
        Check(field->Text == "hello world", "typed text arrives");

        driver.Key(SDL_SCANCODE_BACKSPACE, SDL_KMOD_LCTRL);
        Check(field->Text == "hello ", "Ctrl+Backspace deletes a word");

        driver.Key(SDL_SCANCODE_Z, SDL_KMOD_LCTRL);
        Check(field->Text == "hello world", "Ctrl+Z undoes it");
        driver.Key(SDL_SCANCODE_Z, SDL_KMOD_LCTRL);
        Check(field->Text.empty(), "and one burst of typing undoes as one step");
        driver.Key(SDL_SCANCODE_Y, SDL_KMOD_LCTRL);
        Check(field->Text == "hello world", "Ctrl+Y redoes");

        // Assigned from outside while the caret sits past where the new text
        // ends: the next edit must not run off the end of the string.
        field->Text = "hi";
        driver.Frame();
        driver.Type("!");
        Check(field->Text == "hi!", "text assigned from outside keeps the caret inside it");

        field->Text = "\xC3\xA9t\xC3\xA9";   // "été", two-byte letters
        field->SetCaret(1);                   // inside the first letter
        driver.Frame();
        driver.Key(SDL_SCANCODE_DELETE);
        Check(field->Text == "t\xC3\xA9", "a caret put inside a character is moved to its start");

        driver.Key(SDL_SCANCODE_LEFT, SDL_KMOD_LCTRL);
        driver.Key(SDL_SCANCODE_RIGHT, static_cast<SDL_Keymod>(SDL_KMOD_LCTRL | SDL_KMOD_LSHIFT));
        driver.Key(SDL_SCANCODE_C, SDL_KMOD_LCTRL);
        Check(app.GetClipboardText() == "t\xC3\xA9", "Ctrl+Shift+Right selects a word to copy");
    }

    // -----------------------------------------------------------------------
    std::printf("\n5. Popups, menus, and shortcuts\n");
    Reset(driver);
    {
        root->ChildLayout = opane::LayoutMode::Vertical;
        int saves = 0;
        int opens = 0;
        int copies = 0;
        bool wrap = false;
        std::string chosen;

        auto* bar = root->Add<opane::MenuBar>();
        bar->Menus.push_back({ "File",
                               {
                                   opane::MenuItem::Action("Open", [&]() { ++opens; }, "Ctrl+O"),
                                   opane::MenuItem::Action("Save", [&]() { ++saves; }, "Ctrl+S"),
                                   opane::MenuItem::Divider(),
                                   opane::MenuItem::Submenu("Recent",
                                                            {
                                                                opane::MenuItem::Action("a.txt", [&]() { chosen = "a"; }),
                                                                opane::MenuItem::Action("b.txt", [&]() { chosen = "b"; }),
                                                            }),
                               } });
        bar->Menus.push_back({ "Edit",
                               {
                                   opane::MenuItem::Action("Copy", [&]() { ++copies; }, "Ctrl+C"),
                                   opane::MenuItem::Check("Word wrap", false, [&]() { wrap = !wrap; }),
                               } });

        int pressed = 0;
        auto* below = root->Add<opane::Button>();
        below->Text = "Beneath";
        below->OnClick = [&]() { ++pressed; };
        auto* field = root->Add<opane::TextInput>();
        driver.Frame();

        Check(opane::Shortcut::Parse("Ctrl+Shift+Z").ToString() == "Ctrl+Shift+Z", "shortcuts parse and print");
        Check(!opane::Shortcut::Parse("Hyper+Q").IsValid(), "and nonsense is refused rather than guessed");

        const opane::Vec2 fileTitle{ bar->GetBounds().X + 25.0f, Middle(bar).Y };
        driver.Click(fileTitle);
        opane::Menu* menu = nullptr;
        for (opane::Element* child : app.GetOverlay()->GetChildren())
        {
            if (auto* candidate = dynamic_cast<opane::Menu*>(child); candidate != nullptr && candidate->IsOpen())
            {
                menu = candidate;
            }
        }
        Check(menu != nullptr, "clicking a menu title opens its menu");

        if (menu != nullptr)
        {
            // The second row is Save.
            opane::Vec2 save{};
            for (float y = menu->GetBounds().Y; y < menu->GetBounds().Y + menu->GetBounds().Height; y += 2.0f)
            {
                const opane::Vec2 probe{ menu->GetBounds().X + 40.0f, y };
                if (menu->RowAt(probe) == 1)
                {
                    save = probe;
                    break;
                }
            }
            driver.Click(save);
            Check(saves == 1, "choosing an item runs it");
            Check(!menu->IsOpen(), "and closes the menu");

            // The submenu, by pointer: rest on its row, then choose inside it.
            driver.Click(fileTitle);
            opane::Vec2 recent{};
            for (float y = menu->GetBounds().Y; y < menu->GetBounds().Y + menu->GetBounds().Height; y += 2.0f)
            {
                const opane::Vec2 probe{ menu->GetBounds().X + 40.0f, y };
                if (menu->RowAt(probe) == 3)
                {
                    recent = probe;
                    break;
                }
            }
            driver.Move(recent);
            driver.Frame(20);
            opane::Menu* submenu = nullptr;
            for (opane::Element* child : app.GetOverlay()->GetChildren())
            {
                if (auto* candidate = dynamic_cast<opane::Menu*>(child);
                    candidate != nullptr && candidate != menu && candidate->IsOpen())
                {
                    submenu = candidate;
                }
            }
            Check(submenu != nullptr, "resting on a submenu's row opens it");
            if (submenu != nullptr)
            {
                const opane::Rect sub = submenu->GetBounds();
                driver.Move(opane::Vec2{ sub.X + 30.0f, sub.Y + 20.0f });
                driver.Click(opane::Vec2{ sub.X + 30.0f, sub.Y + 20.0f });
                Check(chosen == "a", "an item in a submenu runs");
                Check(!menu->IsOpen() && !submenu->IsOpen(), "and the whole chain closes");
            }
        }

        // Light dismissal: open, then press on the button beneath.
        driver.Click(fileTitle);
        driver.Click(Middle(below));
        Check(menu != nullptr && !menu->IsOpen(), "a press outside a menu closes it");
        Check(pressed == 0, "and is spent there, not passed to what is beneath");
        driver.Click(Middle(below));
        Check(pressed == 1, "the next press reaches it");

        // Shortcuts, from anywhere.
        driver.Key(SDL_SCANCODE_S, SDL_KMOD_LCTRL);
        Check(saves == 2, "an item's shortcut works while its menu is closed");

        driver.Click(Middle(field));
        driver.Type("abc");
        driver.Key(SDL_SCANCODE_A, SDL_KMOD_LCTRL);
        driver.Key(SDL_SCANCODE_C, SDL_KMOD_LCTRL);
        Check(copies == 0 && app.GetClipboardText() == "abc",
              "a text field keeps Ctrl+C for itself rather than losing it to the menu");
        driver.Key(SDL_SCANCODE_O, SDL_KMOD_LCTRL);
        Check(opens == 1, "while shortcuts it does not use still reach the menu");

        // A checkable item keeps its state between openings.
        driver.Click(opane::Vec2{ bar->GetBounds().X + 75.0f, Middle(bar).Y });
        opane::Menu* edit = menu;
        opane::Vec2 wrapRow{};
        if (edit != nullptr)
        {
            for (float y = edit->GetBounds().Y; y < edit->GetBounds().Y + edit->GetBounds().Height; y += 2.0f)
            {
                const opane::Vec2 probe{ edit->GetBounds().X + 40.0f, y };
                if (edit->RowAt(probe) == 1)
                {
                    wrapRow = probe;
                    break;
                }
            }
            driver.Click(wrapRow);
        }
        Check(wrap && bar->Menus[1].Items[1].Checked, "a checkable item checks and the bar remembers it");

        // Context menu and Escape.
        bool contextRan = false;
        opane::Menu* context =
            app.ShowMenu({ opane::MenuItem::Action("Rename", [&]() { contextRan = true; }) }, opane::Vec2{ 400.0f, 300.0f });
        driver.Frame();
        Check(context != nullptr && context->IsOpen(), "a context menu opens where it is asked to");
        driver.Key(SDL_SCANCODE_ESCAPE);
        Check(context != nullptr && !context->IsOpen() && !contextRan, "Escape closes it without choosing");

        context = app.ShowMenu({ opane::MenuItem::Action("Rename", [&]() { contextRan = true; }) },
                               opane::Vec2{ 990.0f, 690.0f });
        driver.Frame();
        Check(context->GetBounds().X + context->GetBounds().Width <= app.GetWindowSize().X &&
                  context->GetBounds().Y + context->GetBounds().Height <= app.GetWindowSize().Y,
              "opened at the corner, it stays on screen");
        driver.Key(SDL_SCANCODE_DOWN);
        driver.Key(SDL_SCANCODE_RETURN);
        Check(contextRan, "and the keyboard can choose from it");
    }

    // -----------------------------------------------------------------------
    std::printf("\n6. Drop-downs and dialogs\n");
    Reset(driver);
    {
        root->ChildLayout = opane::LayoutMode::Vertical;
        root->Padding = 20.0f;
        auto* dropdown = root->Add<opane::Dropdown>();
        dropdown->Options = { "Red", "Green", "Blue" };
        int changes = 0;
        dropdown->OnChanged = [&](int) { ++changes; };
        int pressed = 0;
        auto* button = root->Add<opane::Button>();
        button->OnClick = [&]() { ++pressed; };
        driver.Frame();

        driver.Click(Middle(dropdown));
        Check(dropdown->IsListOpen(), "clicking a drop-down opens its list");
        const opane::Rect box = dropdown->GetBounds();
        driver.Click(opane::Vec2{ box.X + 30.0f, box.Y + box.Height + 4.0f + 26.0f * 1.5f });
        Check(dropdown->Selected == 1 && changes == 1, "clicking an option selects it");
        Check(!dropdown->IsListOpen(), "and closes the list");

        driver.Key(SDL_SCANCODE_DOWN);
        Check(dropdown->Selected == 2, "the arrow keys change it while closed");

        int result = -2;
        app.ShowDialog("Quit?", "Unsaved changes will be lost.", { "Quit", "Cancel" }, [&](int button) { result = button; });
        driver.Frame();
        driver.Click(Middle(button));
        Check(pressed == 0, "a modal dialog blocks what is beneath it");
        driver.Key(SDL_SCANCODE_RETURN);
        Check(result == 0, "Enter presses the default button");
        driver.Frame(2);
        driver.Click(Middle(button));
        Check(pressed == 1, "and once answered, the interface is reachable again");

        result = -2;
        app.ShowDialog("Delete?", "This cannot be undone.", { "Delete", "Keep" }, [&](int button) { result = button; });
        driver.Frame();
        driver.Key(SDL_SCANCODE_ESCAPE);
        Check(result == -1, "Escape dismisses a dialog and says so");
    }

    // -----------------------------------------------------------------------
    std::printf("\n7. Containers\n");
    Reset(driver);
    {
        root->ChildLayout = opane::LayoutMode::Absolute;
        auto* split = root->Add<opane::Splitter>();
        split->Size = opane::Size2::FromOffset(600.0f, 300.0f);
        split->Add<opane::Panel>();
        split->Add<opane::Panel>();
        driver.Frame();

        const opane::Rect divider = split->GetDividerBounds();
        driver.Drag(Middle(divider), opane::Vec2{ Middle(divider).X - 150.0f, Middle(divider).Y });
        Check(split->Ratio < 0.3f && split->Ratio > 0.2f, "dragging a splitter's divider moves it");
        Check(Near(split->GetChildren()[0]->GetBounds().Width, split->GetDividerBounds().X - split->GetBounds().X, 1.0f),
              "and the panes follow");

        driver.Drag(Middle(split->GetDividerBounds()), opane::Vec2{ 0.0f, Middle(divider).Y });
        Check(split->GetChildren()[0]->GetBounds().Width >= split->MinimumPane - 0.5f,
              "neither pane is dragged below its minimum");

        root->RemoveAllChildren();
        driver.Frame();

        auto* tabs = root->Add<opane::TabView>();
        tabs->Size = opane::Size2::FromOffset(600.0f, 300.0f);
        tabs->Closable = true;
        tabs->AddPage("One");
        tabs->AddPage("Two");
        tabs->AddPage("Three");
        driver.Frame();
        Check(tabs->GetPage(0)->Visible && !tabs->GetPage(1)->Visible, "a tab view shows one page");
        driver.Click(Middle(tabs->GetTabBounds(1)));
        Check(tabs->Active == 1 && tabs->GetPage(1)->Visible, "clicking a tab shows its page");
        driver.Click(opane::Vec2{ Middle(tabs->GetTabBounds(1)).X, Middle(tabs->GetTabBounds(1)).Y }, SDL_BUTTON_MIDDLE);
        driver.Frame(2);
        Check(tabs->GetPageCount() == 2 && tabs->GetTitle(1) == "Three", "a middle click closes a tab");

        auto* window = app.GetOverlay()->Add<opane::Window>();
        window->Title = "Floating";
        window->SetFrame(opane::Rect{ 100.0f, 100.0f, 300.0f, 200.0f });
        driver.Frame();
        const opane::Vec2 title = Middle(window->GetTitleBounds());
        driver.Drag(title, opane::Vec2{ title.X + 120.0f, title.Y + 60.0f });
        Check(Near(window->GetFrame().X, 220.0f, 1.0f) && Near(window->GetFrame().Y, 160.0f, 1.0f),
              "a window moves with its title");
        const opane::Rect frame = window->GetBounds();
        driver.Drag(opane::Vec2{ frame.X + frame.Width - 2.0f, frame.Y + frame.Height - 2.0f },
                    opane::Vec2{ frame.X + frame.Width + 80.0f, frame.Y + frame.Height + 40.0f });
        Check(Near(window->GetFrame().Width, 380.0f, 2.0f) && Near(window->GetFrame().Height, 240.0f, 2.0f),
              "and resizes from its corner");
    }

    // -----------------------------------------------------------------------
    std::printf("\n8. Docking\n");
    Reset(driver);
    {
        root->ChildLayout = opane::LayoutMode::Vertical;
        auto* dock = root->Add<opane::DockSpace>();
        dock->Flex = 1.0f;

        opane::DockPanel* scene = dock->AddPanel("Scene");
        opane::DockPanel* inspector = dock->AddPanel("Inspector", opane::DockSide::Right, nullptr, 0.3f);
        opane::DockPanel* console = dock->AddPanel("Console", opane::DockSide::Bottom, scene, 0.3f);
        opane::DockPanel* log = dock->AddPanel("Log", opane::DockSide::Center, console);
        auto* note = inspector->Add<opane::TextInput>();
        driver.Frame(2);

        Check(dock->IsDocked(scene) && dock->IsDocked(inspector) && dock->IsDocked(console) && dock->IsDocked(log),
              "panels dock where they are asked to");
        Check(inspector->GetBounds().X > scene->GetBounds().X + scene->GetBounds().Width - 1.0f,
              "one to the right of another is to its right");
        Check(console->Visible != log->Visible, "two in one group are tabs, one showing");
        Check(Near(log->GetBounds().Width, 0.0f) || log->Visible, "and the hidden one takes no room");

        const std::string saved = dock->SaveLayout();
        std::printf("      (layout: %s)\n", saved.substr(0, saved.find('\n', saved.find('\n') + 1)).c_str());
        Check(dock->LoadLayout(saved) && dock->SaveLayout() == saved, "a saved layout loads back exactly");
        Check(!dock->LoadLayout("not a layout") && dock->SaveLayout() == saved,
              "a malformed one is refused and changes nothing");

        // Type into a panel, then tear it off: the field and its text go with it.
        driver.Frame();
        driver.Click(Middle(note));
        driver.Type("kept");
        const opane::Rect inspectorBounds = inspector->GetBounds();
        const opane::Vec2 inspectorTab{ inspectorBounds.X + 30.0f, inspectorBounds.Y - 14.0f };
        // Dropped well away from any guide: the middle of a group is a
        // target, so the corner of one is used instead.
        const opane::Rect sceneCorner = scene->GetBounds();
        driver.Drag(inspectorTab, opane::Vec2{ sceneCorner.X + 24.0f, sceneCorner.Y + 24.0f }, 12);
        driver.Frame(2);
        Check(dock->IsFloating(inspector), "a tab dragged into the open tears off into a window");
        Check(note->Text == "kept" && note->IsInside(inspector), "carrying what it held, untouched");

        // Dock it back: drag its window's title onto the Scene group's right guide.
        auto* floatWindow = dynamic_cast<opane::Window*>(inspector->GetParent());
        if (floatWindow != nullptr)
        {
            const opane::Rect sceneArea = scene->GetBounds();
            const opane::Vec2 guide{ sceneArea.X + sceneArea.Width * 0.5f + 35.0f, sceneArea.Y + sceneArea.Height * 0.5f };
            driver.Drag(Middle(floatWindow->GetTitleBounds()), guide, 12);
            driver.Frame(2);
        }
        Check(dock->IsDocked(inspector), "dropping a window on a group's guide docks it there");
        Check(inspector->GetBounds().X > scene->GetBounds().X, "on the side the guide showed");

        // Close from the tab, then bring back.
        driver.Frame();
        const opane::Rect consoleBounds = console->Visible ? console->GetBounds() : log->GetBounds();
        (void)consoleBounds;
        dock->ClosePanel(log);
        driver.Frame();
        Check(!dock->IsOpen(log) && !log->Visible, "a closed panel is hidden");
        dock->Show(log);
        driver.Frame();
        Check(dock->IsDocked(log) && log->Visible, "Show brings it back");

        // A panel removed by the program is forgotten, not left as a hole.
        dock->Remove(log);
        driver.Frame(2);
        Check(dock->FindPanel("Log") == nullptr && dock->GetPanels().size() == 3, "a panel removed outright is forgotten");
        Check(dock->LoadLayout(dock->SaveLayout()), "and the layout still round-trips");

        // Floating and closing through the window.
        dock->Float(console, opane::Rect{ 200.0f, 150.0f, 320.0f, 220.0f });
        driver.Frame(2);
        Check(dock->IsFloating(console), "Float tears a panel off on request");
        const std::string withWindow = dock->SaveLayout();
        Check(withWindow.find("float Console") != std::string::npos, "and the layout records the window");
        dock->ClosePanel(console);
        driver.Frame(2);
        Check(!dock->IsOpen(console), "closing a floating panel closes its window");
        Check(dock->LoadLayout(withWindow) && dock->IsFloating(console), "and loading a layout reopens it as it was");

        // The whole space removed while a panel floats: no dangling windows.
        root->Remove(dock);
        driver.Frame(3);
        int windows = 0;
        for (opane::Element* child : app.GetOverlay()->GetChildren())
        {
            windows += dynamic_cast<opane::Window*>(child) != nullptr ? 1 : 0;
        }
        Check(windows == 0, "removing a dock space takes its floating windows with it");
    }

    // -----------------------------------------------------------------------
    std::printf("\n9. Lists and trees\n");
    Reset(driver);
    {
        root->ChildLayout = opane::LayoutMode::Horizontal;
        auto* list = root->Add<opane::ListView>();
        list->Size = opane::Size2::FromOffset(300.0f, 0.0f);
        list->MultiSelect = true;
        for (int index = 0; index < 1000; ++index)
        {
            list->Items.push_back("Row " + std::to_string(index));
        }
        int activated = -1;
        list->OnActivated = [&](int index) { activated = index; };

        auto* tree = root->Add<opane::TreeView>();
        tree->Flex = 1.0f;
        tree->Items = {
            { "Scene", 1, { { "Camera", 2, {} }, { "Lights", 3, { { "Sun", 4, {} } } } }, false },
            { "Assets", 5, {}, false },
        };
        uint64_t treeActivated = 0;
        tree->OnActivated = [&](const opane::TreeView::Item& item) { treeActivated = item.Id; };
        driver.Frame();

        const opane::Rect listBounds = list->GetBounds();
        driver.Click(opane::Vec2{ listBounds.X + 50.0f, listBounds.Y + 26.0f * 2.5f });
        Check(list->Selected == 2, "clicking a row selects it");
        driver.Key(SDL_SCANCODE_DOWN, SDL_KMOD_LSHIFT);
        driver.Key(SDL_SCANCODE_DOWN, SDL_KMOD_LSHIFT);
        Check(list->GetSelection().size() == 3, "Shift and the arrows extend the selection");
        driver.Key(SDL_SCANCODE_END);
        Check(list->Selected == 999, "End goes to the last of a thousand rows");
        driver.Key(SDL_SCANCODE_RETURN);
        Check(activated == 999, "and Enter activates it");

        driver.Click(opane::Vec2{ tree->GetBounds().X + 60.0f, tree->GetBounds().Y + 12.0f });
        Check(tree->Selected == 1, "clicking a tree row selects it");
        driver.Key(SDL_SCANCODE_RIGHT);
        Check(tree->Find(1)->Expanded, "Right opens a branch");
        driver.Key(SDL_SCANCODE_DOWN);
        driver.Key(SDL_SCANCODE_DOWN);
        driver.Key(SDL_SCANCODE_RIGHT);
        driver.Key(SDL_SCANCODE_DOWN);
        Check(tree->Selected == 4, "the arrows walk into an opened branch");
        driver.Key(SDL_SCANCODE_RETURN);
        Check(treeActivated == 4, "and Enter activates a leaf");
        driver.Key(SDL_SCANCODE_LEFT);
        Check(tree->Selected == 3, "Left from a leaf goes to its parent");

        // Items rebuilt mid-session, as a program reloading its data would.
        tree->Items = { { "Scene", 1, { { "Only child", 9, {} } }, true } };
        driver.Frame();
        driver.Key(SDL_SCANCODE_UP);
        Check(true, "items rebuilt under a selection are walked safely");

        tree->Reveal(9);
        Check(tree->Find(1)->Expanded, "Reveal opens the way to an item");
    }

    // -----------------------------------------------------------------------
    std::printf("\n10. Many lines, and numbers\n");
    Reset(driver);
    {
        root->ChildLayout = opane::LayoutMode::Vertical;
        root->Padding = 20.0f;
        root->Spacing = 10.0f;
        auto* area = root->Add<opane::TextArea>();
        area->Size = opane::Size2::FromOffset(400.0f, 200.0f);
        auto* number = root->Add<opane::NumberField>();
        number->Size = opane::Size2::FromOffset(200.0f, 30.0f);
        number->Value = 10.0;
        number->Step = 0.5;
        number->Minimum = 0.0;
        number->Maximum = 100.0;
        driver.Frame();

        driver.Click(Middle(area));
        driver.Type("first line");
        driver.Key(SDL_SCANCODE_RETURN);
        driver.Type("second");
        Check(area->Text == "first line\nsecond", "Enter starts a new line");

        driver.Key(SDL_SCANCODE_UP);
        driver.Type("X");
        Check(area->Text.find("X") < area->Text.find('\n'), "Up moves to the line above, keeping its place");

        driver.Key(SDL_SCANCODE_Z, SDL_KMOD_LCTRL);
        Check(area->Text == "first line\nsecond", "Ctrl+Z takes back the last edit");

        driver.Key(SDL_SCANCODE_A, SDL_KMOD_LCTRL);
        driver.Type(std::string(300, 'w') + " " + std::string(10, 'z'));
        Check(area->Text.size() == 311, "typing over a selection replaces it");
        driver.Frame(2);

        driver.Drag(Middle(number), opane::Vec2{ Middle(number).X + 40.0f, Middle(number).Y });
        Check(number->Value > 10.0 && number->Value <= 30.0, "dragging a number field scrubs it");

        driver.DoubleClick(Middle(number));
        driver.Key(SDL_SCANCODE_A, SDL_KMOD_LCTRL);
        for (int index = 0; index < 8; ++index)
        {
            driver.Key(SDL_SCANCODE_BACKSPACE);
        }
        driver.Type("42.5");
        driver.Key(SDL_SCANCODE_RETURN);
        Check(std::abs(number->Value - 42.5) < 1e-9, "double-clicking opens it for typing an exact value");
        driver.Key(SDL_SCANCODE_UP);
        Check(std::abs(number->Value - 43.0) < 1e-9, "and the arrows step it");
        number->SetValue(1.0e9);
        Check(number->Value == 100.0, "values are kept inside the range");
    }

    // -----------------------------------------------------------------------
    std::printf("\n11. Many frames of it\n");
    Reset(driver);
    {
        // A busy screen stepped for a while, removing and re-adding things,
        // to shake out anything that only fails over time.
        root->ChildLayout = opane::LayoutMode::Vertical;
        for (int round = 0; round < 30; ++round)
        {
            auto* dock = root->Add<opane::DockSpace>();
            dock->Flex = 1.0f;
            opane::DockPanel* a = dock->AddPanel("A");
            dock->AddPanel("B", opane::DockSide::Left, a);
            dock->AddPanel("C", opane::DockSide::Bottom, a);
            dock->Float(a, opane::Rect{ 50.0f, 50.0f, 200.0f, 150.0f });
            auto* menu = root->Add<opane::MenuBar>();
            menu->Menus.push_back({ "M", { opane::MenuItem::Action("x", []() {}) } });
            driver.Frame(2);
            driver.Click(opane::Vec2{ 20.0f, 690.0f });
            root->RemoveAllChildren();
            driver.Frame(2);
        }
        Check(root->GetChildren().empty() && app.GetOverlay()->GetChildren().size() <= 2,
              "thirty rounds of building and tearing down leave nothing behind");
    }

    // -----------------------------------------------------------------------
    std::printf("\n12. Looks of your own\n");
    Reset(driver);
    {
        root->ChildLayout = opane::LayoutMode::Absolute;

        // A look for each state, eased between.
        auto* card = root->Add<opane::Panel>();
        card->Size = opane::Size2::FromOffset(200.0f, 120.0f);
        card->Position = opane::Position2::FromOffset(40.0f, 40.0f);
        card->Appearance.Normal.Background = opane::Fill::Solid(opane::Color{ 0.2f, 0.3f, 0.4f, 1.0f });
        opane::BoxStyle lit = card->Appearance.Normal;
        lit.Background = opane::Fill::Solid(opane::Color{ 0.9f, 0.6f, 0.2f, 1.0f });
        card->Appearance.Hovered = lit;
        card->Appearance.Transition = 0.2f;
        card->Appearance.Curve = opane::Easing::Linear;
        auto* inside = card->Add<opane::Element>();
        inside->Size = opane::Size2::FromOffset(60.0f, 30.0f);
        inside->Position = opane::Position2::FromOffset(10.0f, 10.0f);
        driver.Frame(2);
        Check(Near(card->GetCurrentLook().Background.Color.R, 0.2f, 0.001f), "an element starts in its normal look");

        driver.Move(Middle(inside));
        driver.Frame(1, 0.08f);
        const float halfway = card->GetCurrentLook().Background.Color.R;
        std::printf("      (part way, red is %.3f)\n", static_cast<double>(halfway));
        Check(halfway > 0.3f && halfway < 0.8f,
              "the pointer over something inside it eases it toward its hovered look");
        driver.Frame(10, 0.05f);
        Check(Near(card->GetCurrentLook().Background.Color.R, 0.9f, 0.001f), "and it arrives there");
        driver.Move(opane::Vec2{ 900.0f, 650.0f });
        driver.Frame(10, 0.05f);
        Check(Near(card->GetCurrentLook().Background.Color.R, 0.2f, 0.001f), "and goes back when the pointer leaves");

        // Turned elements are found where they are drawn.
        auto* turned = root->Add<opane::Button>();
        turned->Text = "turned";
        turned->Size = opane::Size2::FromOffset(200.0f, 40.0f);
        turned->Position = opane::Position2::FromOffset(400.0f, 300.0f);
        turned->Transform.Rotation = 90.0f;
        int clicks = 0;
        turned->OnClick = [&clicks]() { ++clicks; };
        driver.Frame(2);
        driver.Click(opane::Vec2{ 500.0f, 240.0f });
        Check(clicks == 1, "a turned button is pressed where it is drawn");
        driver.Click(opane::Vec2{ 590.0f, 320.0f });
        Check(clicks == 1, "and not where it would have been unturned");

        turned->Enabled = false;
        driver.Frame();
        driver.Click(opane::Vec2{ 500.0f, 240.0f });
        Check(clicks == 1, "a disabled button takes no clicks");
        turned->RequestFocus();
        driver.Frame();
        Check(!turned->IsFocused(), "and cannot have the keyboard");
        turned->Enabled = true;

        // A theme file: colours, looks for the widgets, and looks by name.
        const std::string themePath =
            (std::filesystem::temp_directory_path() / "opane-check-theme.json").string();
        {
            std::ofstream file(themePath);
            file << R"({
  // Comments and a trailing comma are forgiven.
  "colors": { "accent": "#ff8800" },
  "cornerRadius": 9,
  "widgets": {
    "button": {
      "normal": { "background": "#102030", "radius": 6, "border": { "color": "#ffffff40", "width": 2 } },
      "hovered": { "background": "#203040" },
      "transition": 0.1
    },
    "sliderThumb": { "background": "white", "radius": 10 },
  },
  "styles": {
    "card": {
      "background": { "linear": ["#000000", "#ffffff"], "angle": 0 },
      "shadows": [ { "color": "#00000080", "blur": 10, "offset": [0, 6] } ]
    }
  }
})";
        }
        Check(app.LoadTheme(themePath, false), "a theme file loads");
        const opane::Theme& theme = app.GetTheme();
        Check(Near(theme.Accent.R, 1.0f, 0.01f) && Near(theme.Accent.G, 136.0f / 255.0f, 0.01f) &&
                  theme.CornerRadius == 9.0f,
              "with its colours and sizes");
        Check(theme.Styles.Button.Hovered.has_value() && theme.Styles.Button.Hovered->Radius.TopLeft == 6.0f &&
                  theme.Styles.Button.Hovered->BorderWidth == 2.0f &&
                  Near(theme.Styles.Button.Hovered->Background.Color.R, 0x20 / 255.0f, 0.01f),
              "a state lists only what differs, and keeps the rest of the normal look");

        auto* skinned = root->Add<opane::Button>();
        skinned->Size = opane::Size2::FromOffset(120.0f, 36.0f);
        skinned->Position = opane::Position2::FromOffset(40.0f, 500.0f);
        auto* named = root->Add<opane::Panel>();
        named->StyleName = "card";
        named->Size = opane::Size2::FromOffset(120.0f, 80.0f);
        named->Position = opane::Position2::FromOffset(200.0f, 500.0f);
        driver.Frame(2);
        const opane::Style* buttonLook = skinned->GetEffectiveAppearance();
        Check(buttonLook != nullptr && Near(buttonLook->Normal.Background.Color.B, 0x30 / 255.0f, 0.01f),
              "a built-in button takes the theme's look for buttons");
        const opane::Style* cardLook = named->GetEffectiveAppearance();
        Check(cardLook != nullptr && cardLook->Normal.Background.Kind == opane::FillKind::LinearGradient &&
                  cardLook->Normal.Shadows.size() == 1,
              "and an element of your own takes one by name");

        {
            std::ofstream file(themePath);
            file << R"({ "colors": { "accent": } })";
        }
        Check(!app.LoadTheme(themePath, false), "a file with a mistake is refused");
        Check(Near(app.GetTheme().Accent.G, 136.0f / 255.0f, 0.01f), "and the theme that worked stays");
        app.SetTheme(opane::Theme::Dark(app.GetDefaultFont()));

        // The drawing looks are made of.
        std::vector<uint32_t> pixels(16 * 16, 0xFFFFFFFFu);
        const opane::TextureId frame = app.CreateTexture(16, 16, opane::PixelFormat::Rgba8, pixels.data());
        size_t nineSlice = 0;
        size_t shadow = 0;
        bool painted = false;
        auto* canvas = root->Add<opane::Element>();
        // Inside the window at every scale the checks run at, so no piece is
        // clipped away.
        canvas->Size = opane::Size2::FromOffset(200.0f, 100.0f);
        canvas->Position = opane::Position2::FromOffset(40.0f, 40.0f);
        canvas->Painter = [&](opane::DrawList& list, opane::Element& self) {
            painted = true;
            size_t before = list.GetCommandCount();
            list.DrawImage(self.GetBounds(), frame, opane::ImageFit::NineSlice, opane::Color{ 1, 1, 1, 1 }, {},
                           opane::Insets{ 4.0f });
            nineSlice = list.GetCommandCount() - before;
            before = list.GetCommandCount();
            list.DrawShadow(self.GetBounds(), opane::CornerRadii{ 8.0f }, opane::Shadow{});
            shadow = list.GetCommandCount() - before;
        };
        driver.Frame();
        Check(painted, "a painter draws in place of the element's own painting");
        Check(nineSlice == 9, "a nine-slice image is nine pieces");
        Check(shadow == 1, "and a soft shadow is one");

        painted = false;
        canvas->Opacity = 0.0f;
        driver.Frame();
        Check(!painted, "an element with no opacity is not drawn at all");
        app.DestroyTexture(frame);
    }

    app.Shutdown();

    std::printf("\n%d passed, %d failed.\n%s\n\n", g_Passes, g_Failures,
                g_Failures == 0 ? "All checks passed." : "SOME CHECKS FAILED.");
    return g_Failures == 0 ? 0 : 1;
}
