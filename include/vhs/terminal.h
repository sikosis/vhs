#ifndef VHS_TERMINAL_H
#define VHS_TERMINAL_H

#include <cstdint>
#include <string>
#include <vector>

namespace vhs {

struct Colour {
    std::uint8_t red;
    std::uint8_t green;
    std::uint8_t blue;
};

struct Cell {
    std::string text = " ";
    Colour foreground {205, 214, 244};
    Colour background {30, 30, 46};
    bool bold = false;
    bool italic = false;
    bool underline = false;
    bool inverse = false;
};

class TerminalScreen {
public:
    TerminalScreen(int columns = 80, int rows = 24);

    void resize(int columns, int rows);
    void setDefaultColours(Colour foreground, Colour background);
    void feed(const char* data, std::size_t size);
    void feed(const std::string& data) { feed(data.data(), data.size()); }

    int columns() const { return columns_; }
    int rows() const { return rows_; }
    int cursorColumn() const { return column_; }
    int cursorRow() const { return row_; }
    bool cursorVisible() const { return cursorVisible_; }
    const Cell& cell(int column, int row) const;
    std::string text() const;

private:
    enum class State { Text, Escape, Csi, Osc, OscEscape };

    void putCodepoint(std::uint32_t codepoint);
    void executeCsi(char finalCharacter);
    void applyGraphicRendition(const std::vector<int>& parameters);
    void lineFeed();
    void clearCell(int column, int row);
    void resetAttributes();

    int columns_;
    int rows_;
    int column_ = 0;
    int row_ = 0;
    int savedColumn_ = 0;
    int savedRow_ = 0;
    int scrollTop_ = 0;
    int scrollBottom_ = 0;
    bool cursorVisible_ = true;
    State state_ = State::Text;
    std::string sequence_;
    std::uint32_t utf8Codepoint_ = 0;
    int utf8Remaining_ = 0;
    Cell attributes_;
    Colour defaultForeground_ {205, 214, 244};
    Colour defaultBackground_ {30, 30, 46};
    std::vector<Cell> cells_;
    std::vector<Cell> primaryCells_;
    bool alternateScreen_ = false;
};

} // namespace vhs

#endif
