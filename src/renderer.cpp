#include "vhs/renderer.h"
#include "vhs/terminal.h"

#ifdef __HAIKU__
#include <Application.h>
#include <Bitmap.h>
#include <BitmapStream.h>
#include <File.h>
#include <Font.h>
#include <TranslatorFormats.h>
#include <TranslatorRoster.h>
#include <View.h>
#endif

#include <algorithm>
#include <cstdlib>

namespace vhs {

#ifdef __HAIKU__
namespace {

rgb_color colour(const Colour& value)
{
    return {value.red, value.green, value.blue, 255};
}

rgb_color parseColour(const std::string& value)
{
    if (value.size() == 7 && value.front() == '#') {
        return {
            static_cast<std::uint8_t>(std::strtol(value.substr(1, 2).c_str(), nullptr, 16)),
            static_cast<std::uint8_t>(std::strtol(value.substr(3, 2).c_str(), nullptr, 16)),
            static_cast<std::uint8_t>(std::strtol(value.substr(5, 2).c_str(), nullptr, 16)), 255
        };
    }
    return {0, 0, 0, 255};
}

} // namespace
#endif

bool ensureNativeEnvironment(std::string& message)
{
#ifdef __HAIKU__
    static BApplication* application = nullptr;
    if (be_app == nullptr) {
        application = new BApplication("application/x-vnd.sikosis-vhs");
        if (application->InitCheck() != B_OK) {
            message = "cannot connect to the Haiku application server";
            return false;
        }
    }
    return true;
#else
    message = "native rendering is available on Haiku builds";
    return false;
#endif
}

bool renderPng(const TerminalScreen& screen, const RenderOptions& options,
    const std::string& path, std::string& message)
{
#ifdef __HAIKU__
    if (!ensureNativeEnvironment(message))
        return false;

    const int width = std::max(1, options.width + options.margin * 2);
    const int height = std::max(1, options.height + options.margin * 2);
    BBitmap* bitmap = new BBitmap(BRect(0, 0, width - 1, height - 1), B_RGBA32, true);
    if (bitmap->InitCheck() != B_OK) {
        message = "cannot allocate bitmap";
        delete bitmap;
        return false;
    }
    BView* view = new BView(bitmap->Bounds(), "vhs-render", B_FOLLOW_NONE, B_WILL_DRAW);
    bitmap->AddChild(view);
    bitmap->Lock();
    view->SetHighColor(parseColour(options.marginFill));
    view->FillRect(bitmap->Bounds());

    const BRect terminal(options.margin, options.margin,
        options.margin + options.width - 1, options.margin + options.height - 1);
    view->SetHighColor(parseColour(options.background));
    if (options.borderRadius > 0)
        view->FillRoundRect(terminal, options.borderRadius, options.borderRadius);
    else
        view->FillRect(terminal);

    if (!options.windowBar.empty() && options.windowBarSize > 0) {
        const float centreY = static_cast<float>(options.margin + options.windowBarSize / 2);
        const float radius = std::max(4.0f, options.windowBarSize / 7.0f);
        const rgb_color buttons[3] = {
            {255, 95, 86, 255}, {255, 189, 46, 255}, {39, 201, 63, 255}
        };
        for (int index = 0; index < 3; ++index) {
            const float centreX = static_cast<float>(options.margin + options.padding)
                + radius + index * radius * 2.8f;
            view->SetHighColor(buttons[index]);
            view->FillEllipse(BPoint(centreX, centreY), radius, radius);
        }
    }

    BFont font(be_fixed_font);
    font.SetSize(static_cast<float>(options.fontSize));
    if (!options.fontFamily.empty())
        font.SetFamilyAndFace(options.fontFamily.c_str(), B_REGULAR_FACE);
    view->SetFont(&font);
    font_height metrics;
    font.GetHeight(&metrics);
    const float lineHeight = metrics.ascent + metrics.descent + metrics.leading;
    const float cellWidth = std::max(1.0f, font.StringWidth("M"));
    const float originX = static_cast<float>(options.margin + options.padding);
    const float originY = static_cast<float>(options.margin + options.padding + options.windowBarSize);

    for (int row = 0; row < screen.rows(); ++row) {
        for (int column = 0; column < screen.columns(); ++column) {
            const Cell& cell = screen.cell(column, row);
            const float left = originX + column * cellWidth;
            const float top = originY + row * lineHeight;
            if (left >= terminal.right || top >= terminal.bottom)
                continue;
            view->SetHighColor(colour(cell.background));
            view->FillRect(BRect(left, top, left + cellWidth, top + lineHeight));
            if (cell.text != " ") {
                BFont styled(font);
                if (cell.bold)
                    styled.SetFace(B_BOLD_FACE);
                view->SetFont(&styled);
                view->SetHighColor(colour(cell.foreground));
                view->DrawString(cell.text.c_str(), BPoint(left, top + metrics.ascent));
                if (cell.underline)
                    view->StrokeLine(BPoint(left, top + metrics.ascent + 2),
                        BPoint(left + cellWidth, top + metrics.ascent + 2));
            }
        }
    }
    view->Sync();
    bitmap->Unlock();

    BFile output(path.c_str(), B_WRITE_ONLY | B_CREATE_FILE | B_ERASE_FILE);
    if (output.InitCheck() != B_OK) {
        message = "cannot create PNG output: " + path;
        delete bitmap;
        return false;
    }
    BBitmapStream stream(bitmap);
    const status_t status = BTranslatorRoster::Default()->Translate(
        &stream, nullptr, nullptr, &output, B_PNG_FORMAT);
    stream.DetachBitmap(&bitmap);
    delete bitmap;
    if (status != B_OK) {
        message = "Haiku PNG translator failed";
        return false;
    }
    return true;
#else
    (void)screen;
    (void)options;
    (void)path;
    message = "PNG rendering is available on Haiku builds";
    return false;
#endif
}

} // namespace vhs
