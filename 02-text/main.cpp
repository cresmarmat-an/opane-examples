// Example 02: text.
//
// All glyphs come from one atlas of signed distance fields. This is why the
// column on the left can go from 8 to 200 pixels with sharp edges from one
// atlas, and why the heading can be drawn with its own material: an outline
// and a gradient need a distance field, not a coverage mask.
//
// The fields on the right support selection by dragging or with Shift, the
// arrow keys, Home and End, cut, copy, and paste, a character limit counted in
// characters rather than bytes, and a password field. The list above them
// scrolls with the wheel or by dragging its scroll bar.

#include <opane/opane.h>

#include <string>
#include <vector>

namespace
{

// A line whose words carry their own sizes and colours, and a paragraph broken
// to the width it is given. Both are written against the same public drawing
// calls any element has.
class Passage : public opane::Element
{
public:
    std::vector<opane::TextRun> Runs;
    std::string Paragraph;

    void Paint(opane::DrawList& drawList) override
    {
        const opane::Theme& theme = GetTheme();
        const opane::Rect bounds = GetContentBounds();

        drawList.DrawRichText(Runs, opane::Vec2{ bounds.X, bounds.Y }, theme.Font, theme.Text);

        const float used = GetLineHeight() * 1.8f;
        drawList.DrawTextWrapped(Paragraph,
                                 opane::Rect{ bounds.X, bounds.Y + used, bounds.Width,
                                              bounds.Height - used },
                                 theme.Font, theme.TextMuted);
    }
};

} // namespace

int main()
{
    opane::App app = opane::StartApp({
        .Title = "opane: text",
        .Width = 1280,
        .Height = 800,
    });
    if (!app.IsValid())
    {
        return 1;
    }

    // One face, asked for at eleven sizes. They share an atlas and the glyphs
    // in it; only the scale differs.
    const float ladder[] = { 8.0f,  10.0f, 12.0f, 14.0f, 18.0f, 24.0f,
                             32.0f, 48.0f, 72.0f, 120.0f, 200.0f };

    opane::Element* root = app.GetRoot();
    root->ChildLayout = opane::LayoutMode::Horizontal;
    root->Padding = 16.0f;
    root->Spacing = 16.0f;

    // --- the size ladder, in a scroll view ----------------------------------

    opane::Panel* left = root->Add<opane::Panel>();
    left->Size = opane::Size2{ opane::Dim{ 0.58f, 0.0f }, opane::Dim::FromScale(1.0f) };
    left->ChildLayout = opane::LayoutMode::Vertical;
    left->Padding = 16.0f;
    left->Spacing = 10.0f;

    opane::Label* ladderTitle = left->Add<opane::Label>();
    ladderTitle->Text = "One atlas, every size";
    ladderTitle->Size = opane::Size2{ opane::Dim::FromScale(1.0f), opane::Dim::FromOffset(26.0f) };

    opane::ScrollView* sizes = left->Add<opane::ScrollView>();
    sizes->Size = opane::Size2{ opane::Dim::FromScale(1.0f), opane::Dim{ 1.0f, -36.0f } };
    sizes->Spacing = 6.0f;
    sizes->Padding = 4.0f;

    for (const float size : ladder)
    {
        opane::Label* line = sizes->Add<opane::Label>();
        line->Font = app.LoadFont("C:/Windows/Fonts/segoeui.ttf", size);
        line->Text = std::to_string(static_cast<int>(size)) + " px: Handgloves, 0123456789";
        line->Size = opane::Size2{ opane::Dim::FromScale(1.0f),
                                   opane::Dim::FromOffset(size * 1.5f + 8.0f) };
    }

    // Characters outside ASCII. The atlas adds glyphs as they are needed
    // instead of baking them in advance.
    opane::Label* accents = sizes->Add<opane::Label>();
    accents->Font = app.LoadFont("C:/Windows/Fonts/segoeui.ttf", 34.0f);
    accents->Text = "Grüße · Ça va · Größenordnung · Ελλάδα · Привет · ¿Qué? · ★";
    accents->Size = opane::Size2{ opane::Dim::FromScale(1.0f), opane::Dim::FromOffset(52.0f) };

    opane::Label* note = sizes->Add<opane::Label>();
    note->Text = "Kerned, measured in characters rather than bytes. The star is missing from this "
                 "font, so it is drawn as a box instead of being left out.";
    note->Muted = true;
    note->Size = opane::Size2{ opane::Dim::FromScale(1.0f), opane::Dim::FromOffset(26.0f) };

    // A line of mixed sizes and colours, and a paragraph broken to the width
    // it is given.
    Passage* passage = sizes->Add<Passage>();
    passage->Size = opane::Size2{ opane::Dim::FromScale(1.0f), opane::Dim::FromOffset(140.0f) };
    passage->Runs = {
        { "A line ", {}, {} },
        { "of its own", app.LoadFont("C:/Windows/Fonts/segoeui.ttf", 30.0f),
          opane::Color::FromBytes(255, 196, 92) },
        { " sizes and ", {}, {} },
        { "colours", {}, opane::Color::FromBytes(120, 200, 255) },
        { ", on one baseline.", {}, {} },
    };
    passage->Paragraph =
        "Wrapped to the width it is given, breaking at the spaces and measuring every line "
        "the same way it draws them. Resize the window and the breaks move with it, because "
        "nothing here was decided in advance.";

    // --- a heading shaded by a material of its own ---------------------------

    opane::Panel* right = root->Add<opane::Panel>();
    right->Size = opane::Size2{ opane::Dim{ 0.42f, -32.0f }, opane::Dim::FromScale(1.0f) };
    right->ChildLayout = opane::LayoutMode::Vertical;
    right->Padding = 16.0f;
    right->Spacing = 12.0f;

    const opane::MaterialId inked = app.CreateMaterial({
        .ShaderPath = std::string(EXAMPLE_DIR) + "/text.hlsl",
        .Uniforms = {
            { "Top", opane::Color::FromBytes(255, 236, 170) },
            { "Bottom", opane::Color::FromBytes(236, 130, 86) },
            { "Outline", opane::Color::FromBytes(46, 26, 20) },
            { "Width", 1.2f },
        },
    });

    opane::Label* heading = right->Add<opane::Label>();
    heading->Font = app.LoadFont("C:/Windows/Fonts/segoeui.ttf", 56.0f);
    heading->Text = "Inked";
    heading->Align = opane::TextAlign::Center;
    heading->Material = inked;
    heading->Size = opane::Size2{ opane::Dim::FromScale(1.0f), opane::Dim::FromOffset(84.0f) };

    opane::Label* headingNote = right->Add<opane::Label>();
    headingNote->Text = "A shader reading the glyph's distance field.";
    headingNote->Muted = true;
    headingNote->Align = opane::TextAlign::Center;
    headingNote->Size = opane::Size2{ opane::Dim::FromScale(1.0f), opane::Dim::FromOffset(24.0f) };

    // Hovering thickens the outline over a quarter of a second. The value is
    // animated, not set, so it changes smoothly.
    opane::Button* thicken = right->Add<opane::Button>();
    thicken->Text = "Hover to ink";
    thicken->Size = opane::Size2{ opane::Dim::FromScale(1.0f), opane::Dim::FromOffset(40.0f) };
    thicken->OnHoverStart = [&] { app.AnimateMaterialUniform(inked, "Width", 3.2f, 0.0f, 0.0f, 0.0f, 0.25f); };
    thicken->OnHoverEnd = [&] { app.AnimateMaterialUniform(inked, "Width", 1.2f, 0.0f, 0.0f, 0.0f, 0.25f); };

    // --- editing ---------------------------------------------------------------

    opane::Label* fieldsTitle = right->Add<opane::Label>();
    fieldsTitle->Text = "Editing";
    fieldsTitle->Size = opane::Size2{ opane::Dim::FromScale(1.0f), opane::Dim::FromOffset(26.0f) };

    opane::TextInput* name = right->Add<opane::TextInput>();
    name->Placeholder = "Type here, try é, ß, Я";

    opane::Label* echo = right->Add<opane::Label>();
    echo->Text = "Nothing typed yet";
    echo->Muted = true;
    echo->Size = opane::Size2{ opane::Dim::FromScale(1.0f), opane::Dim::FromOffset(24.0f) };

    opane::Label* submitted = right->Add<opane::Label>();
    submitted->Text = "Press Enter to submit";
    submitted->Muted = true;
    submitted->Size = opane::Size2{ opane::Dim::FromScale(1.0f), opane::Dim::FromOffset(24.0f) };

    name->OnChanged = [&](const std::string& text) {
        if (text.empty())
        {
            echo->Text = "Nothing typed yet";
            return;
        }

        // Characters, not bytes: an accented letter is two bytes and one
        // character, and everything in the field counts it as one.
        int characters = 0;
        for (const char byte : text)
        {
            characters += ((static_cast<unsigned char>(byte) & 0xC0) == 0x80) ? 0 : 1;
        }
        echo->Text = std::to_string(characters) + " characters, " + std::to_string(text.size()) +
                     " bytes";
    };
    name->OnSubmitted = [&](const std::string& text) { submitted->Text = "Submitted: " + text; };

    opane::TextInput* limited = right->Add<opane::TextInput>();
    limited->Placeholder = "At most eight characters";
    limited->MaxLength = 8;

    opane::TextInput* secret = right->Add<opane::TextInput>();
    secret->Placeholder = "Masked";
    secret->Masked = true;

    opane::Label* help = right->Add<opane::Label>();
    help->Text = "Drag or Shift to select · Ctrl+A, C, X, V · Home and End";
    help->Muted = true;
    help->Size = opane::Size2{ opane::Dim::FromScale(1.0f), opane::Dim::FromOffset(24.0f) };

    app.Run([&](float) {
        if (app.GetInput().WasKeyPressed(opane::Key::Escape))
        {
            app.Close();
        }
    });

    app.Shutdown();
    return 0;
}
