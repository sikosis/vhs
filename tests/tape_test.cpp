#include "vhs/tape.h"

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
    const std::string valid = R"TAPE(
# comment
Output demo.gif
Require bash
Set FontSize 32
Set TypingSpeed 50 ms
Set CursorBlink false
Type@10 ms "echo \"Hello\" # still text"
Enter@20 ms 2
Ctrl+L
Wait+Screen@2s /Hello/
Env DEMO "Haiku value"
Hide
Show
Screenshot frame.png
Source common.tape
)TAPE";
    const vhs::ParseResult parsed = vhs::parseTape(valid);
    expect(parsed.ok(), "representative tape should parse");
    expect(parsed.commands.size() == 14, "all representative commands should be retained");
    expect(parsed.commands[5].argument == "echo \"Hello\" # still text",
        "quoted comments and escapes should be preserved");
    expect(parsed.commands[5].option == "10ms", "spaced Type duration should be normalised");
    expect(parsed.commands[6].option == "20ms", "spaced key duration should be normalised");

    const vhs::ParseResult spacedSleep = vhs::parseTape("Sleep 500 ms\n");
    expect(spacedSleep.ok(), "spaced Sleep duration should parse");
    expect(spacedSleep.commands.front().argument == "500ms", "Sleep duration should be normalised");

    const vhs::ParseResult windowBars = vhs::parseTape(
        "Set WindowBar Haiku\n"
        "Set WindowBar Colorful\n"
        "Set WindowBar None\n");
    expect(windowBars.ok(), "Haiku, Colorful, and None window bars should parse");
    expect(windowBars.commands.size() == 3, "window bar settings should be retained");

    const vhs::ParseResult invalid = vhs::parseTape(
        "Set FontSize nope\n"
        "Type missing-quotes\n"
        "Enter zero\n"
        "Sleep tomorrow\n"
        "Unknown thing\n");
    expect(!invalid.ok(), "invalid tape should fail");
    expect(invalid.diagnostics.size() == 5, "each invalid line should have a diagnostic");
    expect(invalid.diagnostics.front().line == 1, "diagnostic should retain line number");

    const vhs::ParseResult comments = vhs::parseTape("Type `# literal` # comment\n");
    expect(comments.ok(), "backtick string with hash should parse");
    expect(comments.commands.front().argument == "# literal", "backtick string should decode");

    std::cout << "tape tests passed\n";
    return 0;
}
