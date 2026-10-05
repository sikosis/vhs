#ifndef VHS_TAPE_H
#define VHS_TAPE_H

#include <cstddef>
#include <string>
#include <vector>

namespace vhs {

enum class CommandKind {
    Output,
    Require,
    Set,
    Type,
    Key,
    ModifiedKey,
    Sleep,
    Wait,
    Hide,
    Show,
    Screenshot,
    Copy,
    Paste,
    Source,
    Env
};

struct Command {
    CommandKind kind;
    std::string name;
    std::string option;
    std::string argument;
    std::size_t line;
};

struct Diagnostic {
    std::size_t line;
    std::size_t column;
    std::string message;
};

struct ParseResult {
    std::vector<Command> commands;
    std::vector<Diagnostic> diagnostics;

    bool ok() const { return diagnostics.empty(); }
};

ParseResult parseTape(const std::string& source);
std::string commandKindName(CommandKind kind);

} // namespace vhs

#endif

