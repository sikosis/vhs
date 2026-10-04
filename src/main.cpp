#include "vhs/executor.h"
#include "vhs/record.h"
#include "vhs/tape.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#ifndef VHS_VERSION
#define VHS_VERSION "development"
#endif

namespace {

const char* starterTape = R"TAPE(# VHS for Haiku
Output demo.txt
Output demo.png

Set Shell bash
Set FontSize 32
Set Width 1000
Set Height 600
Set TypingSpeed 50ms

Type "echo 'Hello from Haiku!'"
Enter
Sleep 2s
)TAPE";

void printUsage(std::ostream& output)
{
    output << "VHS for Haiku v" << VHS_VERSION << "\n"
           << "Write terminal recordings as code.\n\n"
           << "Usage:\n"
           << "  vhs new [file]       Create a starter tape\n"
           << "  vhs check <file|->   Validate tape syntax\n"
           << "  vhs record           Record an interactive shell as tape commands\n"
           << "  vhs manual           Show the command summary\n"
           << "  vhs version          Show the version\n";
}

void printManual()
{
    std::cout
        << "Tape commands\n\n"
        << "  Output <path>                 Add GIF, MP4, WebM, PNG-frame, or text output\n"
        << "  Require <program>             Require a program on PATH\n"
        << "  Set <setting> <value>         Configure the terminal and recording\n"
        << "  Type[@time] \"text\"          Type text into the terminal\n"
        << "  Enter[@time] [count]          Press a key, optionally repeatedly\n"
        << "  Ctrl[+Alt][+Shift]+<key>      Send a modified key\n"
        << "  Sleep <time>                  Pause playback\n"
        << "  Wait[+Screen|+Line] /regex/   Wait for terminal output\n"
        << "  Hide / Show                   Pause or resume captured frames\n"
        << "  Screenshot [path]             Save the current frame\n"
        << "  Copy \"text\" / Paste         Use the clipboard\n"
        << "  Env <name> <value>            Set the child environment\n"
        << "  Source <path>                 Include another tape\n\n"
        << "Times accept ns, us, µs, ms, s, m, or h. Run `vhs check file.tape`\n"
        << "to validate a tape without executing it.\n";
}

bool readAll(const std::string& path, std::string& contents)
{
    std::ostringstream buffer;
    if (path == "-") {
        buffer << std::cin.rdbuf();
    } else {
        std::ifstream input(path);
        if (!input)
            return false;
        buffer << input.rdbuf();
    }
    contents = buffer.str();
    return true;
}

int checkTape(const std::string& path)
{
    std::string source;
    if (!readAll(path, source)) {
        std::cerr << "vhs: cannot read " << path << "\n";
        return 1;
    }

    const vhs::ParseResult parsed = vhs::parseTape(source);
    for (const vhs::Diagnostic& diagnostic : parsed.diagnostics)
        std::cerr << path << ':' << diagnostic.line << ':' << diagnostic.column
                  << ": error: " << diagnostic.message << "\n";
    if (!parsed.ok())
        return 1;

    std::cout << path << ": valid tape (" << parsed.commands.size() << " commands)\n";
    return 0;
}

int newTape(const std::string& path)
{
    if (path == "-") {
        std::cout << starterTape;
        return 0;
    }

    std::ifstream existing(path);
    if (existing.good()) {
        std::cerr << "vhs: refusing to overwrite existing file: " << path << "\n";
        return 1;
    }
    std::ofstream output(path);
    if (!output) {
        std::cerr << "vhs: cannot create " << path << "\n";
        return 1;
    }
    output << starterTape;
    std::cout << "Created " << path << "\n";
    return output ? 0 : 1;
}

} // namespace

int main(int argc, char** argv)
{
    if (argc < 2) {
        printUsage(std::cout);
        return 0;
    }

    const std::string command = argv[1];
    if (command == "version" || command == "--version" || command == "-v") {
        std::cout << "vhs v" << VHS_VERSION << "\n";
        return 0;
    }
    if (command == "help" || command == "--help" || command == "-h") {
        printUsage(std::cout);
        return 0;
    }
    if (command == "manual") {
        printManual();
        return 0;
    }
    if (command == "record")
        return vhs::recordTape(std::cout, std::cerr);
    if (command == "new")
        return newTape(argc >= 3 ? argv[2] : "demo.tape");
    if (command == "check" || command == "--check") {
        if (argc < 3) {
            std::cerr << "vhs: check requires a tape path or - for standard input\n";
            return 2;
        }
        return checkTape(argv[2]);
    }
    if (command.size() >= 5 && command.substr(command.size() - 5) == ".tape") {
        return vhs::executeTapeFile(command, std::cout, std::cerr);
    }

    std::cerr << "vhs: unknown command: " << command << "\n";
    printUsage(std::cerr);
    return 2;
}
