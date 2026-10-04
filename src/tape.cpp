#include "vhs/tape.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <set>
#include <sstream>

namespace vhs {
namespace {

std::string trim(const std::string& value)
{
    const std::string whitespace = " \t\r\n";
    const std::size_t first = value.find_first_not_of(whitespace);
    if (first == std::string::npos)
        return "";
    const std::size_t last = value.find_last_not_of(whitespace);
    return value.substr(first, last - first + 1);
}

bool isIdentifier(const std::string& value)
{
    if (value.empty() || !(std::isalpha(static_cast<unsigned char>(value[0])) || value[0] == '_'))
        return false;
    return std::all_of(value.begin() + 1, value.end(), [](unsigned char character) {
        return std::isalnum(character) || character == '_';
    });
}

bool isPositiveInteger(const std::string& value)
{
    if (value.empty())
        return false;
    return std::all_of(value.begin(), value.end(), [](unsigned char character) {
        return std::isdigit(character);
    }) && std::strtol(value.c_str(), nullptr, 10) > 0;
}

bool isNumber(const std::string& value)
{
    if (value.empty())
        return false;
    char* end = nullptr;
    std::strtod(value.c_str(), &end);
    return end != value.c_str() && *end == '\0';
}

bool isDuration(const std::string& value)
{
    if (value.empty())
        return false;
    char* end = nullptr;
    const double amount = std::strtod(value.c_str(), &end);
    if (end == value.c_str() || amount < 0)
        return false;
    const std::string unit(end);
    return unit.empty() || unit == "ns" || unit == "us" || unit == "µs"
        || unit == "ms" || unit == "s" || unit == "m" || unit == "h";
}

std::size_t findComment(const std::string& line)
{
    char quote = '\0';
    bool escaped = false;
    for (std::size_t index = 0; index < line.size(); ++index) {
        const char character = line[index];
        if (escaped) {
            escaped = false;
            continue;
        }
        if (character == '\\' && quote != '`') {
            escaped = true;
            continue;
        }
        if ((character == '\'' || character == '"' || character == '`')) {
            if (quote == '\0')
                quote = character;
            else if (quote == character)
                quote = '\0';
            continue;
        }
        if (character == '#' && quote == '\0')
            return index;
    }
    return std::string::npos;
}

bool splitWord(const std::string& value, std::string& word, std::string& rest)
{
    const std::string clean = trim(value);
    if (clean.empty())
        return false;
    const std::size_t split = clean.find_first_of(" \t");
    if (split == std::string::npos) {
        word = clean;
        rest.clear();
    } else {
        word = clean.substr(0, split);
        rest = trim(clean.substr(split + 1));
    }
    return true;
}

bool isDurationUnit(const std::string& value)
{
    return value == "ns" || value == "us" || value == "µs" || value == "ms"
        || value == "s" || value == "m" || value == "h";
}

bool normaliseDuration(const std::string& value, std::string& result)
{
    std::string amount;
    std::string unit;
    if (!splitWord(value, amount, unit))
        return false;
    if (!unit.empty()) {
        std::string firstUnit;
        std::string remainder;
        if (!splitWord(unit, firstUnit, remainder) || !remainder.empty()
            || !isDurationUnit(firstUnit))
            return false;
        amount += firstUnit;
    }
    if (!isDuration(amount))
        return false;
    result = amount;
    return true;
}

void consumeSeparatedDurationUnit(std::string& timing, std::string& rest)
{
    if (timing.empty() || !isNumber(timing))
        return;
    std::string unit;
    std::string remaining;
    if (splitWord(rest, unit, remaining) && isDurationUnit(unit)) {
        timing += unit;
        rest = remaining;
    }
}

bool unquote(const std::string& value, std::string& result)
{
    const std::string clean = trim(value);
    if (clean.size() < 2)
        return false;
    const char quote = clean.front();
    if ((quote != '\'' && quote != '"' && quote != '`') || clean.back() != quote)
        return false;

    result.clear();
    bool escaped = false;
    for (std::size_t index = 1; index + 1 < clean.size(); ++index) {
        const char character = clean[index];
        if (escaped) {
            if (character == 'n')
                result.push_back('\n');
            else if (character == 't')
                result.push_back('\t');
            else
                result.push_back(character);
            escaped = false;
        } else if (character == '\\' && quote != '`') {
            escaped = true;
        } else {
            result.push_back(character);
        }
    }
    return !escaped;
}

void addError(ParseResult& result, std::size_t line, const std::string& message)
{
    result.diagnostics.push_back({line, 1, message});
}

bool splitTimedName(const std::string& token, std::string& name, std::string& timing)
{
    const std::size_t at = token.find('@');
    name = token.substr(0, at);
    timing = at == std::string::npos ? "" : token.substr(at + 1);
    return timing.empty() || isDuration(timing);
}

const std::set<std::string> keyCommands = {
    "Backspace", "Delete", "Insert", "Down", "Enter", "Left", "Right",
    "Space", "Up", "Tab", "Escape", "PageUp", "PageDown", "ScrollUp",
    "ScrollDown"
};

const std::set<std::string> settings = {
    "Shell", "FontFamily", "FontSize", "Framerate", "Height", "Width",
    "Columns", "Rows", "LetterSpacing", "LineHeight", "PlaybackSpeed",
    "Padding", "Theme", "TypingSpeed", "LoopOffset", "MarginFill", "Margin",
    "WindowBar", "WindowBarSize", "BorderRadius", "WaitPattern", "WaitTimeout",
    "CursorBlink"
};

bool settingValueIsValid(const std::string& name, const std::string& value)
{
    if (name == "FontSize" || name == "Framerate" || name == "Height"
        || name == "Width" || name == "Columns" || name == "Rows"
        || name == "WindowBarSize")
        return isPositiveInteger(value);
    if (name == "Padding" || name == "Margin" || name == "BorderRadius")
        return isNumber(value) && std::strtod(value.c_str(), nullptr) >= 0;
    if (name == "LetterSpacing" || name == "LineHeight" || name == "PlaybackSpeed")
        return isNumber(value);
    if (name == "TypingSpeed" || name == "WaitTimeout")
        return isDuration(value);
    if (name == "CursorBlink")
        return value == "true" || value == "false";
    return !value.empty();
}

} // namespace

ParseResult parseTape(const std::string& source)
{
    ParseResult result;
    std::istringstream input(source);
    std::string rawLine;
    std::size_t lineNumber = 0;

    while (std::getline(input, rawLine)) {
        ++lineNumber;
        const std::size_t comment = findComment(rawLine);
        const std::string line = trim(rawLine.substr(0, comment));
        if (line.empty())
            continue;

        std::string token;
        std::string rest;
        splitWord(line, token, rest);

        std::string name;
        std::string timing;
        if (!splitTimedName(token, name, timing)) {
            addError(result, lineNumber, "invalid duration after '@' in " + token);
            continue;
        }
        consumeSeparatedDurationUnit(timing, rest);

        if (name == "Output" || name == "Require" || name == "Source") {
            if (rest.empty()) {
                addError(result, lineNumber, name + " requires a value");
                continue;
            }
            CommandKind kind = name == "Output" ? CommandKind::Output
                : name == "Require" ? CommandKind::Require : CommandKind::Source;
            result.commands.push_back({kind, name, "", rest, lineNumber});
        } else if (name == "Set") {
            std::string setting;
            std::string value;
            if (!splitWord(rest, setting, value) || value.empty()) {
                addError(result, lineNumber, "Set requires a setting and value");
            } else if (settings.count(setting) == 0) {
                addError(result, lineNumber, "unknown setting '" + setting + "'");
            } else {
                std::string decoded;
                if ((value.front() == '\'' || value.front() == '"' || value.front() == '`')
                    && !unquote(value, decoded)) {
                    addError(result, lineNumber, "unterminated quoted setting value");
                } else {
                    std::string checked = decoded.empty() ? value : decoded;
                    if (setting == "TypingSpeed" || setting == "WaitTimeout") {
                        std::string duration;
                        if (normaliseDuration(checked, duration))
                            checked = duration;
                    }
                    if (!settingValueIsValid(setting, checked))
                        addError(result, lineNumber, "invalid value for " + setting + ": " + value);
                    else
                        result.commands.push_back({CommandKind::Set, name, setting, checked, lineNumber});
                }
            }
        } else if (name == "Type" || name == "Copy") {
            std::string decoded;
            if (!unquote(rest, decoded)) {
                addError(result, lineNumber, name + " requires a quoted string");
            } else {
                const CommandKind kind = name == "Type" ? CommandKind::Type : CommandKind::Copy;
                result.commands.push_back({kind, name, timing, decoded, lineNumber});
            }
        } else if (keyCommands.count(name) != 0) {
            if (!rest.empty() && !isPositiveInteger(rest))
                addError(result, lineNumber, name + " repeat count must be a positive integer");
            else
                result.commands.push_back({CommandKind::Key, name, timing, rest.empty() ? "1" : rest, lineNumber});
        } else if (name.rfind("Ctrl+", 0) == 0 || name.rfind("Alt+", 0) == 0
            || name.rfind("Shift+", 0) == 0) {
            if (!rest.empty())
                addError(result, lineNumber, "modified key command has unexpected arguments");
            else
                result.commands.push_back({CommandKind::ModifiedKey, name, timing, name, lineNumber});
        } else if (name == "Sleep") {
            std::string duration;
            if (!normaliseDuration(rest, duration))
                addError(result, lineNumber, "Sleep requires a valid duration such as 500ms or 2s");
            else
                result.commands.push_back({CommandKind::Sleep, name, "", duration, lineNumber});
        } else if (name == "Wait" || name == "Wait+Screen" || name == "Wait+Line") {
            if (rest.size() < 2 || rest.front() != '/' || rest.back() != '/')
                addError(result, lineNumber, name + " requires a /regular expression/");
            else
                result.commands.push_back({CommandKind::Wait, name, timing, rest.substr(1, rest.size() - 2), lineNumber});
        } else if (name == "Hide" || name == "Show" || name == "Paste") {
            if (!rest.empty())
                addError(result, lineNumber, name + " does not accept arguments");
            else {
                const CommandKind kind = name == "Hide" ? CommandKind::Hide
                    : name == "Show" ? CommandKind::Show : CommandKind::Paste;
                result.commands.push_back({kind, name, "", "", lineNumber});
            }
        } else if (name == "Screenshot") {
            result.commands.push_back({CommandKind::Screenshot, name, "", rest, lineNumber});
        } else if (name == "Env") {
            std::string variable;
            std::string value;
            if (!splitWord(rest, variable, value) || !isIdentifier(variable) || value.empty()) {
                addError(result, lineNumber, "Env requires a variable name and value");
            } else {
                std::string decoded;
                if ((value.front() == '\'' || value.front() == '"' || value.front() == '`')
                    && !unquote(value, decoded))
                    addError(result, lineNumber, "unterminated quoted environment value");
                else
                    result.commands.push_back({CommandKind::Env, name, variable,
                        decoded.empty() ? value : decoded, lineNumber});
            }
        } else {
            addError(result, lineNumber, "unknown command '" + name + "'");
        }
    }

    return result;
}

std::string commandKindName(CommandKind kind)
{
    switch (kind) {
    case CommandKind::Output: return "Output";
    case CommandKind::Require: return "Require";
    case CommandKind::Set: return "Set";
    case CommandKind::Type: return "Type";
    case CommandKind::Key: return "Key";
    case CommandKind::ModifiedKey: return "ModifiedKey";
    case CommandKind::Sleep: return "Sleep";
    case CommandKind::Wait: return "Wait";
    case CommandKind::Hide: return "Hide";
    case CommandKind::Show: return "Show";
    case CommandKind::Screenshot: return "Screenshot";
    case CommandKind::Copy: return "Copy";
    case CommandKind::Paste: return "Paste";
    case CommandKind::Source: return "Source";
    case CommandKind::Env: return "Env";
    }
    return "Unknown";
}

} // namespace vhs
