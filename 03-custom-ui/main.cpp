// Example 03: an interface with its own look.
//
// A window without a system frame, with its own title bar: drag the bar to move
// it, drag the edges to resize it, and double-click the bar to maximize it. The
// background is an image. The sidebar and control panel are frosted glass that
// blurs it; the cards lift and glow under the pointer, and one tilts slowly on
// its own. There is also a nine-slice frame, a tiled pattern, a glowing orb,
// and a wave drawn with a painter. The built-in widgets use the theme's skins.
//
// The look comes from theme.json next to this file. The file is watched, so
// editing and saving it changes the look while the program runs. The images
// are generated at startup.
//
// Controls:
//   Drag the title bar   move the window
//   Title bar buttons    minimize, maximize or restore, close
//   Escape               quit

#include <opane/opane.h>

#include "../common/Png.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>

namespace
{

float Smooth(float edge0, float edge1, float x)
{
    const float t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

// A dusk sky with soft aurora bands and a scatter of stars.
std::vector<uint8_t> DrawBackground(int width, int height)
{
    std::vector<uint8_t> pixels(static_cast<size_t>(width) * height * 4);
    uint32_t seed = 12345u;
    auto Random = [&seed]() {
        seed = seed * 1664525u + 1013904223u;
        return static_cast<float>(seed >> 8) / 16777216.0f;
    };
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            const float u = static_cast<float>(x) / static_cast<float>(width);
            const float v = static_cast<float>(y) / static_cast<float>(height);
            float r = 0.04f + 0.10f * v, g = 0.05f + 0.06f * v, b = 0.12f + 0.12f * v;

            const float band1 = std::exp(-std::pow((v - 0.35f - 0.08f * std::sin(u * 6.0f)) * 9.0f, 2.0f));
            const float band2 = std::exp(-std::pow((v - 0.55f - 0.06f * std::sin(u * 4.0f + 1.3f)) * 11.0f, 2.0f));
            r += 0.10f * band1 + 0.35f * band2;
            g += 0.45f * band1 + 0.10f * band2;
            b += 0.40f * band1 + 0.45f * band2;

            // Hills along the bottom.
            const float hill = 0.78f + 0.05f * std::sin(u * 7.0f) + 0.03f * std::sin(u * 19.0f + 2.0f);
            const float ground = Smooth(hill, hill + 0.004f, v);
            r = r * (1.0f - ground) + 0.03f * ground;
            g = g * (1.0f - ground) + 0.035f * ground;
            b = b * (1.0f - ground) + 0.06f * ground;

            uint8_t* pixel = &pixels[(static_cast<size_t>(y) * width + x) * 4];
            pixel[0] = static_cast<uint8_t>(std::clamp(r, 0.0f, 1.0f) * 255.0f);
            pixel[1] = static_cast<uint8_t>(std::clamp(g, 0.0f, 1.0f) * 255.0f);
            pixel[2] = static_cast<uint8_t>(std::clamp(b, 0.0f, 1.0f) * 255.0f);
            pixel[3] = 255;
        }
    }
    for (int star = 0; star < 420; ++star)
    {
        const int x = static_cast<int>(Random() * static_cast<float>(width));
        const int y = static_cast<int>(Random() * static_cast<float>(height) * 0.7f);
        const uint8_t level = static_cast<uint8_t>(150 + Random() * 105);
        uint8_t* pixel = &pixels[(static_cast<size_t>(y) * width + x) * 4];
        pixel[0] = std::max(pixel[0], level);
        pixel[1] = std::max(pixel[1], level);
        pixel[2] = std::max(pixel[2], level);
    }
    return pixels;
}

// A frame for nine-slicing: a gradient ring with rounded corners, 16 pixels of
// border, clear inside.
std::vector<uint8_t> DrawFrame(int size)
{
    std::vector<uint8_t> pixels(static_cast<size_t>(size) * size * 4, 0);
    const float half = size * 0.5f;
    for (int y = 0; y < size; ++y)
    {
        for (int x = 0; x < size; ++x)
        {
            const float px = std::abs(x + 0.5f - half) - (half - 14.0f);
            const float py = std::abs(y + 0.5f - half) - (half - 14.0f);
            const float outside = std::sqrt(std::max(px, 0.0f) * std::max(px, 0.0f) +
                                            std::max(py, 0.0f) * std::max(py, 0.0f)) +
                                  std::min(std::max(px, py), 0.0f) - 12.0f;
            const float ring = std::clamp(0.5f - outside, 0.0f, 1.0f) * std::clamp(outside + 6.5f, 0.0f, 1.0f);
            const float t = static_cast<float>(x + y) / static_cast<float>(size * 2);
            uint8_t* pixel = &pixels[(static_cast<size_t>(y) * size + x) * 4];
            pixel[0] = static_cast<uint8_t>((0.95f - 0.35f * t) * 255.0f);
            pixel[1] = static_cast<uint8_t>((0.60f + 0.25f * t) * 255.0f);
            pixel[2] = static_cast<uint8_t>((0.40f + 0.55f * t) * 255.0f);
            pixel[3] = static_cast<uint8_t>(ring * 255.0f);
        }
    }
    return pixels;
}

// Diagonal stripes, to tile.
std::vector<uint8_t> DrawPattern(int size)
{
    std::vector<uint8_t> pixels(static_cast<size_t>(size) * size * 4);
    for (int y = 0; y < size; ++y)
    {
        for (int x = 0; x < size; ++x)
        {
            const bool stripe = ((x + y) / (size / 4)) % 2 == 0;
            uint8_t* pixel = &pixels[(static_cast<size_t>(y) * size + x) * 4];
            pixel[0] = stripe ? 60 : 36;
            pixel[1] = stripe ? 200 : 150;
            pixel[2] = stripe ? 170 : 120;
            pixel[3] = 255;
        }
    }
    return pixels;
}

opane::Label* AddLabel(opane::Element* parent, const std::string& text, bool muted = false, float height = 22.0f)
{
    auto* label = parent->Add<opane::Label>();
    label->Text = text;
    label->Muted = muted;
    label->Size = opane::Size2{ opane::Dim::FromScale(1.0f), opane::Dim::FromOffset(height) };
    return label;
}

// A button that draws a small symbol from lines instead of text, over the
// look its style gives it.
opane::Button* AddSymbolButton(opane::Element* parent, const char* style,
                               std::function<void(opane::DrawList&, opane::Vec2)> symbol)
{
    auto* button = parent->Add<opane::Button>();
    button->StyleName = style;
    button->Size = opane::Size2::FromOffset(40.0f, 28.0f);
    button->Painter = [symbol](opane::DrawList& list, opane::Element& self) {
        const opane::Rect bounds = self.GetBounds();
        symbol(list, opane::Vec2{ bounds.X + bounds.Width * 0.5f, bounds.Y + bounds.Height * 0.5f });
    };
    return button;
}

} // namespace

int main()
{
    opane::App app = opane::StartApp({
        .Title = "opane: your own look",
        .Width = 1180,
        .Height = 720,
        .Borderless = true,
        .ClearColor = opane::Color{ 0.02f, 0.03f, 0.05f, 1.0f },
    });
    if (!app.IsValid())
    {
        return 1;
    }

    // The pictures, drawn now and written beside the theme's other assets.
    const std::filesystem::path assets = std::filesystem::temp_directory_path() / "opane-custom-ui";
    std::filesystem::create_directories(assets);
    examples::WritePng((assets / "background.png").string(), 1024, 640, DrawBackground(1024, 640));
    examples::WritePng((assets / "frame.png").string(), 64, 64, DrawFrame(64));
    examples::WritePng((assets / "pattern.png").string(), 32, 32, DrawPattern(32));
    opane::AddAssetRoot(assets.string());

    const std::string themePath = std::string(EXAMPLE_DIR) + "/theme.json";
    app.LoadTheme(themePath, true);

    opane::Element* root = app.GetRoot();
    root->ChildLayout = opane::LayoutMode::Vertical;
    root->Appearance.Normal.Background = opane::Fill::FromImage(app.LoadTexture("background.png"),
                                                                 opane::ImageFit::Cover);

    // --- the title bar: drag to move, double-click to maximize --------------
    auto* titleBar = root->Add<opane::Panel>();
    titleBar->StyleName = "titleBar";
    titleBar->Region = opane::WindowRegion::Drag;
    titleBar->Size = opane::Size2{ opane::Dim::FromScale(1.0f), opane::Dim::FromOffset(44.0f) };
    titleBar->ChildLayout = opane::LayoutMode::Horizontal;
    titleBar->Padding = 8.0f;
    titleBar->Spacing = 4.0f;

    auto* title = titleBar->Add<opane::Label>();
    title->Text = "   opane: your own look";
    title->Flex = 1.0f;
    title->Interactive = false;

    const opane::Color symbolColor{ 0.9f, 0.93f, 1.0f, 1.0f };
    AddSymbolButton(titleBar, "titleButton", [symbolColor](opane::DrawList& list, opane::Vec2 c) {
        list.DrawLine({ c.X - 6.0f, c.Y }, { c.X + 6.0f, c.Y }, 1.5f, symbolColor);
    })->OnClick = [app]() mutable { app.MinimizeWindow(); };
    AddSymbolButton(titleBar, "titleButton", [symbolColor](opane::DrawList& list, opane::Vec2 c) {
        list.StrokeRect({ c.X - 5.5f, c.Y - 5.5f, 11.0f, 11.0f }, 1.5f, symbolColor, 1.5f);
    })->OnClick = [app]() mutable {
        if (app.IsWindowMaximized())
        {
            app.RestoreWindow();
        }
        else
        {
            app.MaximizeWindow();
        }
    };
    AddSymbolButton(titleBar, "closeButton", [symbolColor](opane::DrawList& list, opane::Vec2 c) {
        list.DrawLine({ c.X - 5.0f, c.Y - 5.0f }, { c.X + 5.0f, c.Y + 5.0f }, 1.5f, symbolColor);
        list.DrawLine({ c.X + 5.0f, c.Y - 5.0f }, { c.X - 5.0f, c.Y + 5.0f }, 1.5f, symbolColor);
    })->OnClick = [app]() mutable { app.Close(); };

    // --- the body ---------------------------------------------------------------
    auto* body = root->Add<opane::Element>();
    body->Flex = 1.0f;
    body->Size = opane::Size2::Fill();
    body->ChildLayout = opane::LayoutMode::Horizontal;
    body->Padding = 18.0f;
    body->Spacing = 18.0f;
    body->Interactive = false;

    // A frosted-glass sidebar with navigation that remembers its choice.
    auto* sidebar = body->Add<opane::Panel>();
    sidebar->StyleName = "glass";
    sidebar->Size = opane::Size2{ opane::Dim::FromOffset(210.0f), opane::Dim::FromScale(1.0f) };
    sidebar->ChildLayout = opane::LayoutMode::Vertical;
    sidebar->Padding = 14.0f;
    sidebar->Spacing = 6.0f;
    AddLabel(sidebar, "Workspace", true, 26.0f);

    std::vector<opane::Button*> navigation;
    for (const char* name : { "Overview", "Library", "Activity", "Settings" })
    {
        auto* item = sidebar->Add<opane::Button>();
        item->Text = name;
        item->StyleName = "navButton";
        item->Size = opane::Size2{ opane::Dim::FromScale(1.0f), opane::Dim::FromOffset(38.0f) };
        navigation.push_back(item);
    }
    navigation.front()->SetSelected(true);
    for (opane::Button* item : navigation)
    {
        item->OnClick = [item, &navigation]() {
            for (opane::Button* other : navigation)
            {
                other->SetSelected(other == item);
            }
        };
    }

    // The middle: cards, a frame, a pattern, an orb, and a painter.
    auto* middle = body->Add<opane::Element>();
    middle->Flex = 1.0f;
    middle->Size = opane::Size2{ opane::Dim::FromOffset(0.0f), opane::Dim::FromScale(1.0f) };
    middle->ChildLayout = opane::LayoutMode::Vertical;
    middle->Spacing = 18.0f;
    middle->Interactive = false;

    auto* cards = middle->Add<opane::Element>();
    cards->Size = opane::Size2{ opane::Dim::FromScale(1.0f), opane::Dim::FromOffset(150.0f) };
    cards->ChildLayout = opane::LayoutMode::Horizontal;
    cards->Spacing = 18.0f;
    cards->Interactive = false;

    std::vector<opane::Panel*> cardList;
    const char* cardText[3][2] = { { "Gradients", "Linear and radial, any stops" },
                                   { "Shadows & glows", "Soft, inset, or coloured light" },
                                   { "Transforms", "Turned and scaled, still clickable" } };
    for (const auto& text : cardText)
    {
        auto* card = cards->Add<opane::Panel>();
        card->StyleName = "card";
        card->Flex = 1.0f;
        card->Size = opane::Size2{ opane::Dim::FromOffset(0.0f), opane::Dim::FromScale(1.0f) };
        card->ChildLayout = opane::LayoutMode::Vertical;
        card->Padding = 18.0f;
        card->Spacing = 4.0f;
        card->Tooltip = "A named style from theme.json: \"card\"";
        AddLabel(card, text[0], false, 26.0f)->Interactive = false;
        AddLabel(card, text[1], true)->Interactive = false;
        cardList.push_back(card);
    }

    auto* showcase = middle->Add<opane::Element>();
    showcase->Flex = 1.0f;
    showcase->Size = opane::Size2{ opane::Dim::FromScale(1.0f), opane::Dim::FromOffset(0.0f) };
    showcase->ChildLayout = opane::LayoutMode::Horizontal;
    showcase->Spacing = 18.0f;
    showcase->Interactive = false;

    // A nine-slice frame: its corners keep their shape at any size.
    auto* framed = showcase->Add<opane::Panel>();
    framed->Flex = 1.0f;
    framed->Size = opane::Size2{ opane::Dim::FromOffset(0.0f), opane::Dim::FromScale(1.0f) };
    framed->Appearance.Normal.Background =
        opane::Fill::NineSliced(app.LoadTexture("frame.png"), opane::Insets{ 22.0f });
    framed->ChildLayout = opane::LayoutMode::Vertical;
    framed->Padding = 30.0f;
    AddLabel(framed, "Nine-slice frame")->Interactive = false;
    AddLabel(framed, "Crisp corners at any size", true)->Interactive = false;

    // A tiled pattern, rounded like any other box.
    auto* tiled = showcase->Add<opane::Panel>();
    tiled->Flex = 1.0f;
    tiled->Size = opane::Size2{ opane::Dim::FromOffset(0.0f), opane::Dim::FromScale(1.0f) };
    tiled->Appearance.Normal.Background = opane::Fill::FromImage(app.LoadTexture("pattern.png"), opane::ImageFit::Tile);
    tiled->Appearance.Normal.Radius = opane::CornerRadii{ 28.0f, 6.0f, 28.0f, 6.0f };
    tiled->Appearance.Normal.Shadows = { opane::Shadow{ opane::Color{ 0.0f, 0.0f, 0.0f, 0.35f },
                                                        opane::Vec2{ 0.0f, 0.0f }, 18.0f, 0.0f, true } };
    tiled->ChildLayout = opane::LayoutMode::Vertical;
    tiled->Padding = 22.0f;
    AddLabel(tiled, "Tiled, with an inset shadow")->Interactive = false;

    // A radial orb that glows, from the theme.
    auto* orbHolder = showcase->Add<opane::Element>();
    orbHolder->Size = opane::Size2{ opane::Dim::FromOffset(170.0f), opane::Dim::FromScale(1.0f) };
    orbHolder->ChildLayout = opane::LayoutMode::Absolute;
    orbHolder->Interactive = false;
    auto* orb = orbHolder->Add<opane::Element>();
    orb->StyleName = "orb";
    orb->Size = opane::Size2::FromOffset(140.0f, 140.0f);
    orb->Position = opane::Position2::Center();
    orb->AnchorPoint = opane::Vec2{ 0.5f, 0.5f };
    orb->Tooltip = "A radial gradient and a glow";

    // A painter: the element draws exactly this, every frame.
    auto* wave = middle->Add<opane::Element>();
    wave->Size = opane::Size2{ opane::Dim::FromScale(1.0f), opane::Dim::FromOffset(90.0f) };
    wave->Appearance.Normal.Background = opane::Fill::Linear(opane::Color{ 0.10f, 0.13f, 0.22f, 0.85f },
                                                             opane::Color{ 0.07f, 0.09f, 0.15f, 0.85f }, 90.0f);
    wave->Appearance.Normal.Radius = opane::CornerRadii{ 16.0f };
    float clock = 0.0f;
    float amplitude = 0.6f;
    wave->Painter = [&clock, &amplitude](opane::DrawList& list, opane::Element& self) {
        const opane::Rect bounds = self.GetBounds();
        const int segments = 96;
        opane::Vec2 previous{};
        for (int index = 0; index <= segments; ++index)
        {
            const float u = static_cast<float>(index) / segments;
            const float x = bounds.X + 16.0f + u * (bounds.Width - 32.0f);
            const float y = bounds.Y + bounds.Height * 0.5f +
                            std::sin(u * 12.0f + clock * 2.4f) * bounds.Height * 0.3f * amplitude *
                                std::sin(u * 3.14159f);
            const opane::Vec2 point{ x, y };
            if (index > 0)
            {
                const opane::Color color{ 0.49f + 0.3f * u, 0.61f - 0.2f * u, 1.0f, 1.0f };
                list.DrawLine(previous, point, 3.0f, color);
            }
            previous = point;
        }
    };

    // The controls, in a second pane of glass, skinned by the theme.
    auto* controls = body->Add<opane::Panel>();
    controls->StyleName = "glass";
    controls->Size = opane::Size2{ opane::Dim::FromOffset(290.0f), opane::Dim::FromScale(1.0f) };
    controls->ChildLayout = opane::LayoutMode::Vertical;
    controls->Padding = 18.0f;
    controls->Spacing = 12.0f;
    AddLabel(controls, "Controls", true, 26.0f);

    auto* name = controls->Add<opane::TextInput>();
    name->Placeholder = "Your name";
    name->Size = opane::Size2{ opane::Dim::FromScale(1.0f), opane::Dim::FromOffset(38.0f) };

    auto* choice = controls->Add<opane::Dropdown>();
    choice->Options = { "Aurora", "Dusk", "Midnight" };
    choice->Selected = 0;
    choice->Size = opane::Size2{ opane::Dim::FromScale(1.0f), opane::Dim::FromOffset(38.0f) };

    auto* notify = controls->Add<opane::Checkbox>();
    notify->Text = "Notifications";
    notify->Checked = true;
    auto* glow = controls->Add<opane::Toggle>();
    glow->Text = "Animate the cards";
    glow->On = true;

    AddLabel(controls, "Wave height", true);
    auto* slider = controls->Add<opane::Slider>();
    slider->Value = 0.6f;
    slider->Size = opane::Size2{ opane::Dim::FromScale(1.0f), opane::Dim::FromOffset(28.0f) };
    slider->OnChanged = [&amplitude](float value) { amplitude = value; };

    auto* progress = controls->Add<opane::ProgressBar>();
    progress->Size = opane::Size2{ opane::Dim::FromScale(1.0f), opane::Dim::FromOffset(12.0f) };

    auto* apply = controls->Add<opane::Button>();
    apply->Text = "Apply";
    apply->Accent = true;
    apply->Size = opane::Size2{ opane::Dim::FromScale(1.0f), opane::Dim::FromOffset(40.0f) };
    auto* later = controls->Add<opane::Button>();
    later->Text = "Unavailable";
    later->Enabled = false;
    later->Size = opane::Size2{ opane::Dim::FromScale(1.0f), opane::Dim::FromOffset(40.0f) };

    auto* hint = controls->Add<opane::Label>();
    hint->Text = "Edit theme.json and save:";
    hint->Muted = true;
    hint->Size = opane::Size2{ opane::Dim::FromScale(1.0f), opane::Dim::FromOffset(20.0f) };
    auto* hint2 = controls->Add<opane::Label>();
    hint2->Text = "the look changes live.";
    hint2->Muted = true;
    hint2->Size = opane::Size2{ opane::Dim::FromScale(1.0f), opane::Dim::FromOffset(20.0f) };

    std::printf("The look comes from %s. Edit and save it while this runs.\n", themePath.c_str());

    app.Run([&](float deltaSeconds) {
        clock += deltaSeconds;
        progress->Value = 0.5f + 0.5f * std::sin(clock * 0.7f);

        // One card idles with a gentle tilt; the pointer still finds it.
        cardList[2]->Transform.Rotation = glow->On ? std::sin(clock * 1.3f) * 2.5f : 0.0f;

        if (app.GetInput().WasKeyPressed(opane::Key::Escape))
        {
            app.Close();
        }
    });

    app.Shutdown();
    return 0;
}
