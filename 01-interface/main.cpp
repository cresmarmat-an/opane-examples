// Example 01: opane's interface on its own.
//
// This program links opane without ludifex.
//
// It shows the element tree, the built-in widgets, theming, and a custom
// element. RadialGauge below uses the same public API as the built-in widgets
// and takes part in layout, painting, input, and focus in the same way.

#include <opane/opane.h>

#include "../common/Png.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace
{

constexpr float Pi = 3.14159265358979323846f;

// Writes a short decaying tone as a 16-bit mono WAV. The example synthesizes
// its own audio so it needs no asset files to demonstrate playback.
bool WriteToneWav(const std::string& path, float frequency, int milliseconds, float amplitude)
{
    constexpr int SampleRate = 44100;
    const int sampleCount = SampleRate * milliseconds / 1000;

    std::vector<int16_t> samples(static_cast<size_t>(sampleCount));
    for (int index = 0; index < sampleCount; ++index)
    {
        const float time = static_cast<float>(index) / static_cast<float>(SampleRate);
        const float envelope = std::exp(-time * 16.0f);
        const float value = std::sin(time * frequency * 2.0f * Pi) * envelope * amplitude;
        samples[static_cast<size_t>(index)] = static_cast<int16_t>(value * 32000.0f);
    }

    std::ofstream file(path, std::ios::binary);
    if (!file)
    {
        return false;
    }

    const uint32_t dataBytes = static_cast<uint32_t>(samples.size() * sizeof(int16_t));
    const uint32_t byteRate = SampleRate * 2;

    auto WriteU32 = [&](uint32_t value) { file.write(reinterpret_cast<const char*>(&value), 4); };
    auto WriteU16 = [&](uint16_t value) { file.write(reinterpret_cast<const char*>(&value), 2); };

    file.write("RIFF", 4);
    WriteU32(36 + dataBytes);
    file.write("WAVE", 4);
    file.write("fmt ", 4);
    WriteU32(16);
    WriteU16(1);           // PCM
    WriteU16(1);           // mono
    WriteU32(SampleRate);
    WriteU32(byteRate);
    WriteU16(2);           // block align
    WriteU16(16);          // bits per sample
    file.write("data", 4);
    WriteU32(dataBytes);
    file.write(reinterpret_cast<const char*>(samples.data()), dataBytes);

    return file.good();
}

// A user-written element. It overrides measurement, painting, hit-testing, and
// event handling, and nothing in opane treats it differently from a Button.
class RadialGauge : public opane::Element
{
public:
    float Value = 0.35f;
    std::string Caption = "Load";

    RadialGauge()
    {
        Size = opane::Size2::FromOffset(150.0f, 150.0f);
    }

    opane::Vec2 Measure(opane::Vec2 available) override
    {
        return Size.Resolve(available);
    }

    // A circular hit region rather than the default rectangle, so the corners
    // of the element fall through to whatever is beneath it.
    bool HitTest(opane::Vec2 point) const override
    {
        const opane::Rect bounds = GetBounds();
        const float centerX = bounds.X + bounds.Width * 0.5f;
        const float centerY = bounds.Y + bounds.Height * 0.5f;
        const float dx = point.X - centerX;
        const float dy = point.Y - centerY;
        const float radius = std::min(bounds.Width, bounds.Height) * 0.5f;
        return (dx * dx + dy * dy) <= radius * radius;
    }

    void Paint(opane::DrawList& drawList) override
    {
        const opane::Theme& theme = GetTheme();
        const opane::Rect bounds = GetBounds();

        const opane::Vec2 center{ bounds.X + bounds.Width * 0.5f, bounds.Y + bounds.Height * 0.5f };
        const float radius = std::min(bounds.Width, bounds.Height) * 0.5f - 6.0f;

        drawList.FillCircle(center, radius, theme.SurfacePressed);
        drawList.StrokeCircle(center, radius, 1.0f, theme.Border);

        // The arc is drawn as a run of small circles. A real arc primitive
        // would be one quad with a different distance function; this keeps the
        // example to the public API.
        const int segments = 48;
        const int filled = static_cast<int>(Value * static_cast<float>(segments));

        for (int segment = 0; segment < segments; ++segment)
        {
            const float t = static_cast<float>(segment) / static_cast<float>(segments);
            const float angle = (-0.5f * Pi) + t * 1.75f * Pi;

            const opane::Vec2 dot{ center.X + std::cos(angle) * (radius - 12.0f),
                                   center.Y + std::sin(angle) * (radius - 12.0f) };

            const bool active = segment <= filled;
            const opane::Color color = active ? theme.Accent
                                              : opane::Color{ 1.0f, 1.0f, 1.0f, 0.10f };
            drawList.FillCircle(dot, active ? 3.5f : 2.5f, color);
        }

        char percent[32];
        std::snprintf(percent, sizeof(percent), "%d%%", static_cast<int>(Value * 100.0f + 0.5f));

        drawList.DrawTextInRect(percent,
                                { bounds.X, center.Y - 18.0f, bounds.Width, 20.0f },
                                theme.Font, theme.Text, opane::TextAlign::Center);

        drawList.DrawTextInRect(Caption, { bounds.X, center.Y + 2.0f, bounds.Width, 18.0f },
                                theme.Font, theme.TextMuted, opane::TextAlign::Center);

        if (IsHovered())
        {
            drawList.StrokeCircle(center, radius, 2.0f, theme.AccentHovered);
        }
    }

    void OnEvent(opane::Event& event) override
    {
        if (event.Type == opane::EventType::PointerDown)
        {
            // Clicking sets the value from the angle around the center.
            const opane::Rect bounds = GetBounds();
            const float dx = event.Position.X - (bounds.X + bounds.Width * 0.5f);
            const float dy = event.Position.Y - (bounds.Y + bounds.Height * 0.5f);

            float angle = std::atan2(dy, dx) + 0.5f * Pi;
            if (angle < 0.0f)
            {
                angle += 2.0f * Pi;
            }

            Value = std::clamp(angle / (1.75f * Pi), 0.0f, 1.0f);
            event.Handled = true;
        }
    }
};

} // namespace

int main()
{
    opane::App app = opane::StartApp({
        .Title = "opane: interface",
        .Width = 960,
        .Height = 640,
    });

    if (!app.IsValid())
    {
        return 1;
    }

    opane::Element* root = app.GetRoot();

    // Synthesize two tones and load them. Missing audio is never fatal: if no
    // device opened, every call below is a silent no-op.
    const std::filesystem::path audioDirectory = std::filesystem::temp_directory_path();
    const std::string clickPath = (audioDirectory / "opane-click.wav").string();
    const std::string confirmPath = (audioDirectory / "opane-confirm.wav").string();

    WriteToneWav(clickPath, 660.0f, 120, 0.35f);
    WriteToneWav(confirmPath, 880.0f, 220, 0.40f);

    const opane::SoundId clickSound = app.LoadSound(clickPath, opane::AudioGroup::Interface);
    const opane::SoundId confirmSound = app.LoadSound(confirmPath, opane::AudioGroup::Interface);

    // A left sidebar laid out as a vertical stack. Children state their height
    // and inherit the column's width, so nothing repeats a width.
    opane::Panel* sidebar = root->Add<opane::Panel>();
    sidebar->Size = opane::Size2{ opane::Dim::FromOffset(300.0f), opane::Dim::FromScale(1.0f) };
    sidebar->ChildLayout = opane::LayoutMode::Vertical;
    sidebar->Padding = 20.0f;
    sidebar->Spacing = 12.0f;
    sidebar->CornerRadius = 0.0f;
    sidebar->DrawBorder = false;
    sidebar->Name = "Sidebar";

    opane::Label* title = sidebar->Add<opane::Label>();
    title->Text = "Controls";
    title->Size = opane::Size2{ opane::Dim::FromScale(1.0f), opane::Dim::FromOffset(30.0f) };

    opane::Label* readout = sidebar->Add<opane::Label>();
    readout->Text = "Nothing clicked yet";
    readout->Muted = true;
    readout->Size = opane::Size2{ opane::Dim::FromScale(1.0f), opane::Dim::FromOffset(24.0f) };

    int clickCount = 0;

    opane::Button* primary = sidebar->Add<opane::Button>();
    primary->Text = "Primary action";
    primary->Accent = true;
    primary->Size = opane::Size2{ opane::Dim::FromScale(1.0f), opane::Dim::FromOffset(42.0f) };
    primary->OnClick = [&] {
        ++clickCount;
        readout->Text = "Clicked " + std::to_string(clickCount) + " time" +
                        (clickCount == 1 ? "" : "s");
        app.PlaySound(confirmSound);
    };

    opane::Button* secondary = sidebar->Add<opane::Button>();
    secondary->Text = "Reset";
    secondary->Size = opane::Size2{ opane::Dim::FromScale(1.0f), opane::Dim::FromOffset(42.0f) };
    secondary->OnClick = [&] {
        clickCount = 0;
        readout->Text = "Nothing clicked yet";
        app.PlaySound(clickSound);
    };

    opane::Label* volumeLabel = sidebar->Add<opane::Label>();
    volumeLabel->Text = app.IsAudioRunning() ? "Interface volume" : "Interface volume (no device)";
    volumeLabel->Muted = true;
    volumeLabel->Size = opane::Size2{ opane::Dim::FromScale(1.0f), opane::Dim::FromOffset(22.0f) };

    opane::Slider* volume = sidebar->Add<opane::Slider>();
    volume->Size = opane::Size2{ opane::Dim::FromScale(1.0f), opane::Dim::FromOffset(30.0f) };
    volume->Value = 1.0f;
    volume->OnChanged = [&](float value) {
        app.SetGroupVolume(opane::AudioGroup::Interface, value);
    };

    // A user-authored shader, compiled at startup and hot-reloaded while the
    // program runs. Params are declared here and land in Params[0..2] in the
    // shader, in this order.
    opane::MaterialId aurora = app.CreateMaterial({
        .ShaderPath = std::string(EXAMPLE_DIR) + "/aurora.hlsl",
        .Uniforms = {
            { "ColorA", opane::Color::FromBytes(38, 66, 150) },
            { "ColorB", opane::Color::FromBytes(156, 62, 176) },
            { "Speed", 1.0f },
        },
    });

    // Attaching it is one assignment. The panel keeps its rounded corners, its
    // layout, and its place in the tree; only its pixels come from the shader.
    opane::Panel* hero = root->Add<opane::Panel>();
    hero->Size = opane::Size2{ opane::Dim{ 1.0f, -340.0f }, opane::Dim::FromOffset(110.0f) };
    hero->Position = opane::Position2::FromOffset(320.0f, 20.0f);
    hero->CornerRadius = 16.0f;
    hero->DrawBorder = false;
    hero->Material = aurora;

    opane::Label* heroText = hero->Add<opane::Label>();
    heroText->Text = "Custom shader on a normal panel";
    heroText->Align = opane::TextAlign::Center;
    heroText->Size = opane::Size2::Fill();

    // The custom element, placed in the right-hand area.
    RadialGauge* gauge = root->Add<RadialGauge>();
    gauge->Position = opane::Position2::FromScale(0.62f, 0.34f);
    gauge->AnchorPoint = { 0.5f, 0.5f };

    opane::Label* sliderLabel = sidebar->Add<opane::Label>();
    sliderLabel->Text = "Gauge value";
    sliderLabel->Muted = true;
    sliderLabel->Size = opane::Size2{ opane::Dim::FromScale(1.0f), opane::Dim::FromOffset(22.0f) };

    opane::Slider* slider = sidebar->Add<opane::Slider>();
    slider->Size = opane::Size2{ opane::Dim::FromScale(1.0f), opane::Dim::FromOffset(30.0f) };
    slider->Value = gauge->Value;
    slider->OnChanged = [&](float value) { gauge->Value = value; };

    opane::Checkbox* lightMode = sidebar->Add<opane::Checkbox>();
    lightMode->Text = "Light theme";
    lightMode->Size = opane::Size2{ opane::Dim::FromScale(1.0f), opane::Dim::FromOffset(28.0f) };
    lightMode->OnChanged = [&](bool checked) {
        // Appearance is data, so switching themes is one assignment and every
        // widget re-reads it on the next frame.
        app.SetTheme(checked ? opane::Theme::Light(app.GetDefaultFont())
                             : opane::Theme::Dark(app.GetDefaultFont()));
        app.SetClearColor(app.GetTheme().Background);
    };

    opane::Checkbox* showHint = sidebar->Add<opane::Checkbox>();
    showHint->Text = "Show hint";
    showHint->Checked = true;
    showHint->Size = opane::Size2{ opane::Dim::FromScale(1.0f), opane::Dim::FromOffset(28.0f) };

    opane::Label* hint = root->Add<opane::Label>();
    hint->Text = "Click the gauge, or drag the slider";
    hint->Muted = true;
    hint->Align = opane::TextAlign::Center;
    hint->Position = opane::Position2::FromScale(0.62f, 0.62f);
    hint->AnchorPoint = { 0.5f, 0.5f };
    hint->Size = opane::Size2::FromOffset(340.0f, 24.0f);

    showHint->OnChanged = [&](bool checked) { hint->Visible = checked; };

    // Images. The PNG is written at startup into a folder that is then added
    // as an asset root, so it is loaded by bare name exactly as a shipped
    // file in ./assets/images/ would be. Fine concentric rings are the worst
    // case for minification: without a mip chain the small copies would
    // shimmer into moire.
    const std::filesystem::path imageRoot = audioDirectory / "opane-example-assets";
    std::filesystem::create_directories(imageRoot);
    {
        constexpr int Size = 256;
        std::vector<uint8_t> pixels(static_cast<size_t>(Size) * Size * 4);
        for (int y = 0; y < Size; ++y)
        {
            for (int x = 0; x < Size; ++x)
            {
                const float dx = static_cast<float>(x) + 0.5f - Size * 0.5f;
                const float dy = static_cast<float>(y) + 0.5f - Size * 0.5f;
                const float distance = std::sqrt(dx * dx + dy * dy);

                // Coverage of a disc of radius 120 with a one-pixel soft edge.
                const float coverage = std::clamp(120.5f - distance, 0.0f, 1.0f);
                const bool ring = static_cast<int>(distance / 3.0f) % 2 == 0;

                uint8_t* texel = &pixels[(static_cast<size_t>(y) * Size + static_cast<size_t>(x)) * 4];
                texel[0] = ring ? 250 : 40;
                texel[1] = ring ? 190 : 60;
                texel[2] = ring ? 90 : 140;
                texel[3] = static_cast<uint8_t>(coverage * 255.0f);
            }
        }
        examples::WritePng((imageRoot / "rings.png").string(), Size, Size, pixels);
    }
    opane::AddAssetRoot(imageRoot.string());

    const opane::TextureId rings = app.LoadTexture("rings.png");
    const opane::TextureId missing = app.LoadTexture("no-such-image.png");

    opane::Element* gallery = root->Add<opane::Element>();
    gallery->ChildLayout = opane::LayoutMode::Horizontal;
    gallery->Spacing = 18.0f;
    gallery->Size = opane::Size2::FromOffset(160.0f + 64.0f + 24.0f + 64.0f + 18.0f * 3.0f, 160.0f);
    gallery->Position = opane::Position2{ opane::Dim{ 1.0f, -20.0f }, opane::Dim{ 1.0f, -20.0f } };
    gallery->AnchorPoint = { 1.0f, 1.0f };
    gallery->Interactive = false;

    const float imageSizes[] = { 160.0f, 64.0f, 24.0f };
    for (float size : imageSizes)
    {
        opane::Image* image = gallery->Add<opane::Image>();
        image->Texture = rings;
        image->Size = opane::Size2::FromOffset(size, size);
    }

    opane::Image* placeholder = gallery->Add<opane::Image>();
    placeholder->Texture = missing;
    placeholder->Size = opane::Size2::FromOffset(64.0f, 64.0f);

    // A post-process chain over the whole sidebar. The sidebar and everything
    // in it are drawn into a texture, the ripple runs over that, and the CRT
    // effect runs over the ripple's output. The widgets still work normally;
    // only their pixels are changed, not their input.
    const opane::MaterialId ripple = app.CreateMaterial({
        .ShaderPath = std::string(EXAMPLE_DIR) + "/ripple.hlsl",
        .Uniforms = { { "Wave", 2.5f, 18.0f } },
    });

    const opane::MaterialId crt = app.CreateMaterial({
        .ShaderPath = std::string(EXAMPLE_DIR) + "/crt.hlsl",
        .Uniforms = { { "Strength", 1.0f } },
    });

    opane::Checkbox* postProcess = sidebar->Add<opane::Checkbox>();
    postProcess->Text = "Post-process sidebar";
    postProcess->Checked = true;
    postProcess->Size = opane::Size2{ opane::Dim::FromScale(1.0f), opane::Dim::FromOffset(28.0f) };
    postProcess->OnChanged = [&](bool checked) {
        sidebar->PostProcess = checked ? std::vector<opane::MaterialId>{ ripple, crt }
                                       : std::vector<opane::MaterialId>{};
    };

    sidebar->PostProcess = { ripple, crt };

    app.SetClearColor(app.GetTheme().Background);

    app.Run([&](float) {
        if (app.GetInput().WasKeyPressed(opane::Key::Escape))
        {
            app.Close();
        }

        // The slider follows the gauge when the gauge is clicked directly, so
        // the two controls never disagree.
        slider->Value = gauge->Value;
    });

    app.Shutdown();
    return 0;
}
