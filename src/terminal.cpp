#include "vhs/terminal.h"

#include <algorithm>
#include <sstream>

namespace vhs {
namespace {

const Colour ansiColours[16] = {
    {0, 0, 0}, {205, 49, 49}, {13, 188, 121}, {229, 229, 16},
    {36, 114, 200}, {188, 63, 188}, {17, 168, 205}, {229, 229, 229},
    {102, 102, 102}, {241, 76, 76}, {35, 209, 139}, {245, 245, 67},
    {59, 142, 234}, {214, 112, 214}, {41, 184, 219}, {255, 255, 255}
};

Colour indexedColour(int index)
{
    if (index < 16)
        return ansiColours[std::max(0, index)];
    if (index >= 232) {
        const std::uint8_t value = static_cast<std::uint8_t>(8 + (index - 232) * 10);
        return {value, value, value};
    }
    index -= 16;
    const int red = index / 36;
    const int green = (index / 6) % 6;
    const int blue = index % 6;
    const auto component = [](int value) -> std::uint8_t {
        return static_cast<std::uint8_t>(value == 0 ? 0 : 55 + value * 40);
    };
    return {component(red), component(green), component(blue)};
}

std::string encodeUtf8(std::uint32_t codepoint)
{
    std::string result;
    if (codepoint <= 0x7f) {
        result.push_back(static_cast<char>(codepoint));
    } else if (codepoint <= 0x7ff) {
        result.push_back(static_cast<char>(0xc0 | (codepoint >> 6)));
        result.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
    } else if (codepoint <= 0xffff) {
        result.push_back(static_cast<char>(0xe0 | (codepoint >> 12)));
        result.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
        result.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
    } else {
        result.push_back(static_cast<char>(0xf0 | (codepoint >> 18)));
        result.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3f)));
        result.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3f)));
        result.push_back(static_cast<char>(0x80 | (codepoint & 0x3f)));
    }
    return result;
}

bool isCombining(std::uint32_t codepoint)
{
    return (codepoint >= 0x0300 && codepoint <= 0x036f)
        || (codepoint >= 0x1ab0 && codepoint <= 0x1aff)
        || (codepoint >= 0x1dc0 && codepoint <= 0x1dff)
        || (codepoint >= 0xfe20 && codepoint <= 0xfe2f);
}

bool isWide(std::uint32_t codepoint)
{
    return (codepoint >= 0x1100 && codepoint <= 0x115f)
        || (codepoint >= 0x2e80 && codepoint <= 0xa4cf)
        || (codepoint >= 0xac00 && codepoint <= 0xd7a3)
        || (codepoint >= 0xf900 && codepoint <= 0xfaff)
        || (codepoint >= 0x1f300 && codepoint <= 0x1faff)
        || (codepoint >= 0x20000 && codepoint <= 0x3fffd);
}

std::vector<int> parameters(const std::string& sequence)
{
    std::vector<int> result;
    std::string value;
    for (char character : sequence) {
        if (character == ';' || character == ':') {
            result.push_back(value.empty() ? 0 : std::stoi(value));
            value.clear();
        } else if (character >= '0' && character <= '9') {
            value.push_back(character);
        }
    }
    result.push_back(value.empty() ? 0 : std::stoi(value));
    return result;
}

} // namespace

TerminalScreen::TerminalScreen(int columns, int rows)
    : columns_(columns)
    , rows_(rows)
    , scrollBottom_(rows - 1)
    , cells_(static_cast<std::size_t>(columns * rows))
{
    resetAttributes();
}

void TerminalScreen::resize(int columns, int rows)
{
    columns_ = std::max(1, columns);
    rows_ = std::max(1, rows);
    cells_.assign(static_cast<std::size_t>(columns_ * rows_), Cell {});
    column_ = 0;
    row_ = 0;
    scrollTop_ = 0;
    scrollBottom_ = rows_ - 1;
}

void TerminalScreen::setDefaultColours(Colour foreground, Colour background)
{
    defaultForeground_ = foreground;
    defaultBackground_ = background;
    resetAttributes();
    for (Cell& cell : cells_) {
        if (cell.text == " ") {
            cell.foreground = foreground;
            cell.background = background;
        }
    }
}

const Cell& TerminalScreen::cell(int column, int row) const
{
    return cells_[static_cast<std::size_t>(row * columns_ + column)];
}

void TerminalScreen::clearCell(int column, int row)
{
    Cell blank;
    blank.foreground = defaultForeground_;
    blank.background = defaultBackground_;
    cells_[static_cast<std::size_t>(row * columns_ + column)] = blank;
}

void TerminalScreen::lineFeed()
{
    ++row_;
    if (row_ <= scrollBottom_)
        return;
    row_ = scrollBottom_;
    for (int row = scrollTop_; row < scrollBottom_; ++row) {
        for (int column = 0; column < columns_; ++column)
            cells_[static_cast<std::size_t>(row * columns_ + column)]
                = cells_[static_cast<std::size_t>((row + 1) * columns_ + column)];
    }
    for (int column = 0; column < columns_; ++column)
        clearCell(column, scrollBottom_);
}

void TerminalScreen::putCodepoint(std::uint32_t codepoint)
{
    if (isCombining(codepoint) && (column_ > 0 || row_ > 0)) {
        const int previousColumn = column_ > 0 ? column_ - 1 : columns_ - 1;
        const int previousRow = column_ > 0 ? row_ : std::max(0, row_ - 1);
        cells_[static_cast<std::size_t>(previousRow * columns_ + previousColumn)].text
            += encodeUtf8(codepoint);
        return;
    }
    const int width = isWide(codepoint) ? 2 : 1;
    if (width == 2 && column_ == columns_ - 1) {
        column_ = 0;
        lineFeed();
    }
    if (column_ >= columns_) {
        column_ = 0;
        lineFeed();
    }
    Cell cell = attributes_;
    if (cell.inverse)
        std::swap(cell.foreground, cell.background);
    cell.text = encodeUtf8(codepoint);
    cells_[static_cast<std::size_t>(row_ * columns_ + column_)] = cell;
    if (width == 2 && column_ + 1 < columns_)
        clearCell(column_ + 1, row_);
    column_ += width;
}

void TerminalScreen::resetAttributes()
{
    attributes_ = Cell {};
    attributes_.foreground = defaultForeground_;
    attributes_.background = defaultBackground_;
}

void TerminalScreen::applyGraphicRendition(const std::vector<int>& values)
{
    for (std::size_t index = 0; index < values.size(); ++index) {
        const int value = values[index];
        if (value == 0) resetAttributes();
        else if (value == 1) attributes_.bold = true;
        else if (value == 3) attributes_.italic = true;
        else if (value == 4) attributes_.underline = true;
        else if (value == 7) attributes_.inverse = true;
        else if (value == 22) attributes_.bold = false;
        else if (value == 23) attributes_.italic = false;
        else if (value == 24) attributes_.underline = false;
        else if (value == 27) attributes_.inverse = false;
        else if (value >= 30 && value <= 37) attributes_.foreground = ansiColours[value - 30];
        else if (value >= 90 && value <= 97) attributes_.foreground = ansiColours[value - 90 + 8];
        else if (value >= 40 && value <= 47) attributes_.background = ansiColours[value - 40];
        else if (value >= 100 && value <= 107) attributes_.background = ansiColours[value - 100 + 8];
        else if (value == 39) attributes_.foreground = defaultForeground_;
        else if (value == 49) attributes_.background = defaultBackground_;
        else if ((value == 38 || value == 48) && index + 2 < values.size() && values[index + 1] == 5) {
            const Colour colour = indexedColour(values[index + 2]);
            (value == 38 ? attributes_.foreground : attributes_.background) = colour;
            index += 2;
        } else if ((value == 38 || value == 48) && index + 4 < values.size() && values[index + 1] == 2) {
            const Colour colour {
                static_cast<std::uint8_t>(values[index + 2]),
                static_cast<std::uint8_t>(values[index + 3]),
                static_cast<std::uint8_t>(values[index + 4])
            };
            (value == 38 ? attributes_.foreground : attributes_.background) = colour;
            index += 4;
        }
    }
}

void TerminalScreen::executeCsi(char finalCharacter)
{
    const bool privateMode = !sequence_.empty() && sequence_.front() == '?';
    const std::vector<int> values = parameters(sequence_);
    const int first = values.empty() || values.front() == 0 ? 1 : values.front();
    if (finalCharacter == 'A') row_ = std::max(0, row_ - first);
    else if (finalCharacter == 'B') row_ = std::min(rows_ - 1, row_ + first);
    else if (finalCharacter == 'C') column_ = std::min(columns_ - 1, column_ + first);
    else if (finalCharacter == 'D') column_ = std::max(0, column_ - first);
    else if (finalCharacter == 'G') column_ = std::min(columns_ - 1, first - 1);
    else if (finalCharacter == 'H' || finalCharacter == 'f') {
        row_ = std::min(rows_ - 1, std::max(0, (values.empty() ? 1 : values[0]) - 1));
        column_ = std::min(columns_ - 1, std::max(0, (values.size() < 2 ? 1 : values[1]) - 1));
    } else if (finalCharacter == 'J') {
        if (values.empty() || values[0] == 0) {
            for (int row = row_; row < rows_; ++row)
                for (int column = row == row_ ? column_ : 0; column < columns_; ++column)
                    clearCell(column, row);
        } else if (values[0] == 2 || values[0] == 3) {
            for (int row = 0; row < rows_; ++row)
                for (int column = 0; column < columns_; ++column)
                    clearCell(column, row);
        }
    } else if (finalCharacter == 'K') {
        const int mode = values.empty() ? 0 : values[0];
        const int start = mode == 1 || mode == 2 ? 0 : column_;
        const int end = mode == 0 || mode == 2 ? columns_ : column_ + 1;
        for (int column = start; column < end; ++column)
            clearCell(column, row_);
    } else if (finalCharacter == 'm') {
        applyGraphicRendition(values);
    } else if (finalCharacter == 's') {
        savedColumn_ = column_;
        savedRow_ = row_;
    } else if (finalCharacter == 'u') {
        column_ = savedColumn_;
        row_ = savedRow_;
    } else if (finalCharacter == 'r') {
        scrollTop_ = std::max(0, (values.empty() ? 1 : values[0]) - 1);
        scrollBottom_ = std::min(rows_ - 1, (values.size() < 2 || values[1] == 0 ? rows_ : values[1]) - 1);
    } else if (privateMode && (finalCharacter == 'h' || finalCharacter == 'l')) {
        if (sequence_.find("25") != std::string::npos)
            cursorVisible_ = finalCharacter == 'h';
        if (sequence_.find("1049") != std::string::npos) {
            if (finalCharacter == 'h' && !alternateScreen_) {
                primaryCells_ = cells_;
                savedColumn_ = column_;
                savedRow_ = row_;
                alternateScreen_ = true;
                for (int row = 0; row < rows_; ++row)
                    for (int column = 0; column < columns_; ++column)
                        clearCell(column, row);
                column_ = 0;
                row_ = 0;
            } else if (finalCharacter == 'l' && alternateScreen_) {
                cells_ = primaryCells_;
                primaryCells_.clear();
                column_ = savedColumn_;
                row_ = savedRow_;
                alternateScreen_ = false;
            }
        }
    }
}

void TerminalScreen::feed(const char* data, std::size_t size)
{
    for (std::size_t index = 0; index < size; ++index) {
        const unsigned char character = static_cast<unsigned char>(data[index]);
        if (state_ == State::Osc) {
            if (character == '\a') state_ = State::Text;
            else if (character == 0x1b) state_ = State::OscEscape;
            continue;
        }
        if (state_ == State::OscEscape) {
            state_ = character == '\\' ? State::Text : State::Osc;
            continue;
        }
        if (state_ == State::Escape) {
            if (character == '[') { state_ = State::Csi; sequence_.clear(); }
            else if (character == ']') state_ = State::Osc;
            else if (character == '7') { savedColumn_ = column_; savedRow_ = row_; state_ = State::Text; }
            else if (character == '8') { column_ = savedColumn_; row_ = savedRow_; state_ = State::Text; }
            else state_ = State::Text;
            continue;
        }
        if (state_ == State::Csi) {
            if (character >= 0x40 && character <= 0x7e) {
                executeCsi(static_cast<char>(character));
                state_ = State::Text;
            } else {
                sequence_.push_back(static_cast<char>(character));
            }
            continue;
        }
        if (utf8Remaining_ > 0) {
            if ((character & 0xc0) == 0x80) {
                utf8Codepoint_ = (utf8Codepoint_ << 6) | (character & 0x3f);
                if (--utf8Remaining_ == 0)
                    putCodepoint(utf8Codepoint_);
            } else {
                utf8Remaining_ = 0;
                putCodepoint(0xfffd);
                --index;
            }
            continue;
        }
        if (character == 0x1b) state_ = State::Escape;
        else if (character == '\r') column_ = 0;
        else if (character == '\n') lineFeed();
        else if (character == '\b') column_ = std::max(0, column_ - 1);
        else if (character == '\t') column_ = std::min(columns_ - 1, ((column_ / 8) + 1) * 8);
        else if (character >= 0x20 && character < 0x7f) putCodepoint(character);
        else if ((character & 0xe0) == 0xc0) { utf8Codepoint_ = character & 0x1f; utf8Remaining_ = 1; }
        else if ((character & 0xf0) == 0xe0) { utf8Codepoint_ = character & 0x0f; utf8Remaining_ = 2; }
        else if ((character & 0xf8) == 0xf0) { utf8Codepoint_ = character & 0x07; utf8Remaining_ = 3; }
    }
}

std::string TerminalScreen::text() const
{
    std::ostringstream output;
    for (int row = 0; row < rows_; ++row) {
        int last = columns_ - 1;
        while (last >= 0 && cell(last, row).text == " ")
            --last;
        for (int column = 0; column <= last; ++column)
            output << cell(column, row).text;
        if (row + 1 < rows_)
            output << '\n';
    }
    return output.str();
}

} // namespace vhs
