#include "vhs/terminal.h"

#include <cstdlib>
#include <iostream>
#include <string>

namespace {

void expect(bool condition, const std::string& message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << "\n";
        std::exit(1);
    }
}

} // namespace

int main()
{
    vhs::TerminalScreen screen(12, 4);
    screen.feed("Hello\r\nWorld");
    expect(screen.cell(0, 0).text == "H", "plain text should occupy cells");
    expect(screen.cell(0, 1).text == "W", "CRLF should start the next row");

    screen.feed("\x1b[1;1H\x1b[31;1mR\x1b[0m");
    expect(screen.cell(0, 0).text == "R", "cursor positioning should work");
    expect(screen.cell(0, 0).foreground.red == 205, "ANSI red should be retained");
    expect(screen.cell(0, 0).bold, "bold SGR should be retained");
    expect(screen.text().find("[31") == std::string::npos,
        "ANSI control syntax must never become visible text");

    screen.feed("\x1b[2;1H\x1b[38;2;10;20;30mT");
    expect(screen.cell(0, 1).foreground.red == 10, "true colour red should be retained");
    expect(screen.cell(0, 1).foreground.green == 20, "true colour green should be retained");
    expect(screen.cell(0, 1).foreground.blue == 30, "true colour blue should be retained");

    screen.feed("\x1b[3;1HHaiku: 世界");
    expect(screen.cell(7, 2).text == "世", "UTF-8 codepoints should remain intact");
    screen.feed("\x1b[2K");
    expect(screen.cell(0, 2).text == " ", "erase-line should clear the row");

    screen.feed("\x1b[?25l");
    expect(!screen.cursorVisible(), "private cursor mode should hide the cursor");
    screen.feed("\x1b[?25h");
    expect(screen.cursorVisible(), "private cursor mode should show the cursor");

    screen.feed("\x1b[1;1Hprimary\x1b[?1049halternate");
    expect(screen.cell(0, 0).text == "a", "alternate screen should receive output");
    screen.feed("\x1b[?1049l");
    expect(screen.cell(0, 0).text == "p", "leaving alternate screen should restore content");

    screen.resize(4, 2);
    screen.setDefaultColours({1, 2, 3}, {4, 5, 6});
    screen.feed("X\x1b[0mY");
    expect(screen.cell(0, 0).foreground.green == 2, "theme foreground should become the SGR default");
    expect(screen.cell(1, 0).background.blue == 6, "theme background should become the SGR default");

    std::cout << "terminal screen tests passed\n";
    return 0;
}
