#include "vhs/executor.h"
#include "vhs/renderer.h"
#include "vhs/tape.h"
#include "vhs/terminal.h"
#include "vhs/video.h"

#include <algorithm>
#include <cerrno>
#include <cctype>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <iomanip>
#include <map>
#include <poll.h>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <sys/ioctl.h>
#include <sys/stat.h>
#ifdef __HAIKU__
#include <posix/sys/wait.h>
#else
#include <sys/wait.h>
#endif
#include <termios.h>
#include <unistd.h>
#include <vector>

#ifdef __HAIKU__
#include <Clipboard.h>
#include <Message.h>
#include <TypeConstants.h>
#endif

namespace vhs {
namespace {

using Clock = std::chrono::steady_clock;
volatile sig_atomic_t interrupted = 0;
const char* readyPrompt = "\x1b]133;VHS_READY\a";

void handleInterruption(int)
{
    interrupted = 1;
}

class SignalGuard {
public:
    SignalGuard()
    {
        interrupted = 0;
        struct sigaction action {};
        action.sa_handler = handleInterruption;
        sigemptyset(&action.sa_mask);
        sigaction(SIGINT, &action, &oldInterrupt_);
        sigaction(SIGTERM, &action, &oldTerminate_);
    }

    ~SignalGuard()
    {
        sigaction(SIGINT, &oldInterrupt_, nullptr);
        sigaction(SIGTERM, &oldTerminate_, nullptr);
    }

private:
    struct sigaction oldInterrupt_ {};
    struct sigaction oldTerminate_ {};
};

struct Programme {
    std::vector<Command> commands;
    std::string shell = "bash";
    int columns = 80;
    int rows = 24;
    double typingSeconds = 0.05;
    double waitSeconds = 15.0;
    std::map<std::string, std::string> environment;
    RenderOptions render;
    VideoOptions video;
    std::vector<std::string> textOutputs;
    std::vector<std::string> pngOutputs;
    std::vector<std::string> frameDirectories;
    std::vector<std::string> videoOutputs;
    Colour foreground {205, 214, 244};
    Colour background {30, 30, 46};
};

class FrameTimeline {
public:
    explicit FrameTimeline(int framerate)
        : framerate_(std::max(1, framerate))
    {
    }

    void advance(double seconds, const TerminalScreen& screen, bool recording)
    {
        if (!recording)
            return;
        remainder_ += std::max(0.0, seconds) * framerate_;
        const int count = static_cast<int>(remainder_);
        remainder_ -= count;
        for (int index = 0; index < count; ++index)
            frames_.push_back(screen);
    }

    void ensureFrame(const TerminalScreen& screen)
    {
        if (frames_.empty())
            frames_.push_back(screen);
        else
            frames_.back() = screen;
    }

    const std::vector<TerminalScreen>& frames() const { return frames_; }

private:
    int framerate_;
    double remainder_ = 0.0;
    std::vector<TerminalScreen> frames_;
};

std::string directoryOf(const std::string& path)
{
    const std::size_t slash = path.find_last_of('/');
    return slash == std::string::npos ? "." : path.substr(0, slash);
}

std::string joinPath(const std::string& directory, const std::string& path)
{
    if (path.empty() || path.front() == '/')
        return path;
    return directory == "." ? path : directory + "/" + path;
}

std::string unquotePath(const std::string& value)
{
    if (value.size() >= 2 && ((value.front() == '"' && value.back() == '"')
        || (value.front() == '\'' && value.back() == '\'')
        || (value.front() == '`' && value.back() == '`')))
        return value.substr(1, value.size() - 2);
    return value;
}

bool parseHexColour(const std::string& value, Colour& colour)
{
    if (value.size() != 7 || value.front() != '#')
        return false;
    char* end = nullptr;
    const long number = std::strtol(value.substr(1).c_str(), &end, 16);
    if (end == nullptr || *end != '\0')
        return false;
    colour = {
        static_cast<std::uint8_t>((number >> 16) & 0xff),
        static_cast<std::uint8_t>((number >> 8) & 0xff),
        static_cast<std::uint8_t>(number & 0xff)
    };
    return true;
}

std::string colourString(const Colour& colour)
{
    std::ostringstream value;
    value << '#' << std::hex << std::setfill('0') << std::setw(2)
          << static_cast<int>(colour.red) << std::setw(2) << static_cast<int>(colour.green)
          << std::setw(2) << static_cast<int>(colour.blue);
    return value.str();
}

void applyTheme(const std::string& value, Programme& programme)
{
    static const std::map<std::string, std::pair<std::string, std::string>> themes = {
        {"Catppuccin Mocha", {"#cdd6f4", "#1e1e2e"}},
        {"Catppuccin Frappe", {"#c6d0f5", "#303446"}},
        {"Dracula", {"#f8f8f2", "#282a36"}},
        {"Nord", {"#d8dee9", "#2e3440"}},
        {"Tokyo Night", {"#c0caf5", "#1a1b26"}},
        {"Whimsy", {"#b3b0d6", "#29283b"}}
    };
    std::string foreground;
    std::string background;
    const auto named = themes.find(value);
    if (named != themes.end()) {
        foreground = named->second.first;
        background = named->second.second;
    } else {
        std::smatch match;
        const std::regex foregroundExpression("\\\"foreground\\\"\\s*:\\s*\\\"(#[0-9A-Fa-f]{6})\\\"");
        const std::regex backgroundExpression("\\\"background\\\"\\s*:\\s*\\\"(#[0-9A-Fa-f]{6})\\\"");
        if (std::regex_search(value, match, foregroundExpression)) foreground = match[1];
        if (std::regex_search(value, match, backgroundExpression)) background = match[1];
    }
    if (!foreground.empty()) parseHexColour(foreground, programme.foreground);
    if (!background.empty()) parseHexColour(background, programme.background);
    programme.render.foreground = colourString(programme.foreground);
    programme.render.background = colourString(programme.background);
}

bool copyToClipboard(const std::string& value, std::string& message)
{
#ifdef __HAIKU__
    if (!ensureNativeEnvironment(message))
        return false;
    BClipboard clipboard("system");
    if (!clipboard.Lock()) {
        message = "cannot lock the Haiku clipboard";
        return false;
    }
    clipboard.Clear();
    BMessage* data = clipboard.Data();
    const status_t status = data->AddData("text/plain", B_MIME_TYPE,
        value.c_str(), value.size() + 1);
    if (status == B_OK)
        clipboard.Commit();
    clipboard.Unlock();
    if (status != B_OK) {
        message = "cannot write the Haiku clipboard";
        return false;
    }
    return true;
#else
    (void)value;
    message = "clipboard commands are available on Haiku";
    return false;
#endif
}

bool pasteFromClipboard(std::string& value, std::string& message)
{
#ifdef __HAIKU__
    if (!ensureNativeEnvironment(message))
        return false;
    BClipboard clipboard("system");
    if (!clipboard.Lock()) {
        message = "cannot lock the Haiku clipboard";
        return false;
    }
    const void* data = nullptr;
    ssize_t size = 0;
    const status_t status = clipboard.Data()->FindData("text/plain", B_MIME_TYPE,
        &data, &size);
    if (status == B_OK && data != nullptr)
        value.assign(static_cast<const char*>(data), size > 0 ? static_cast<std::size_t>(size - 1) : 0);
    clipboard.Unlock();
    if (status != B_OK) {
        message = "the Haiku clipboard does not contain text";
        return false;
    }
    return true;
#else
    (void)value;
    message = "clipboard commands are available on Haiku";
    return false;
#endif
}

bool readFile(const std::string& path, std::string& contents)
{
    std::ifstream input(path);
    if (!input)
        return false;
    std::ostringstream buffer;
    buffer << input.rdbuf();
    contents = buffer.str();
    return true;
}

bool loadCommands(const std::string& path, std::vector<Command>& commands,
    std::set<std::string>& active, std::ostream& error)
{
    if (active.count(path) != 0) {
        error << "vhs: source cycle detected at " << path << "\n";
        return false;
    }
    std::string source;
    if (!readFile(path, source)) {
        error << "vhs: cannot read " << path << "\n";
        return false;
    }
    const ParseResult parsed = parseTape(source);
    if (!parsed.ok()) {
        for (const Diagnostic& diagnostic : parsed.diagnostics)
            error << path << ':' << diagnostic.line << ':' << diagnostic.column
                  << ": error: " << diagnostic.message << "\n";
        return false;
    }

    active.insert(path);
    const std::string directory = directoryOf(path);
    for (const Command& command : parsed.commands) {
        if (command.kind == CommandKind::Source) {
            const std::string child = joinPath(directory, unquotePath(command.argument));
            if (!loadCommands(child, commands, active, error)) {
                active.erase(path);
                return false;
            }
        } else {
            commands.push_back(command);
        }
    }
    active.erase(path);
    return true;
}

double durationSeconds(const std::string& value)
{
    if (value.empty())
        return 0.0;
    char* end = nullptr;
    const double amount = std::strtod(value.c_str(), &end);
    const std::string unit(end);
    if (unit == "ns") return amount / 1000000000.0;
    if (unit == "us" || unit == "µs") return amount / 1000000.0;
    if (unit == "ms") return amount / 1000.0;
    if (unit == "m") return amount * 60.0;
    if (unit == "h") return amount * 3600.0;
    return amount;
}

std::string stripControlSequences(const std::string& input)
{
    std::string output;
    for (std::size_t index = 0; index < input.size();) {
        const unsigned char character = static_cast<unsigned char>(input[index]);
        if (character == 0x1b) {
            ++index;
            if (index >= input.size())
                break;
            if (input[index] == '[') {
                ++index;
                while (index < input.size()) {
                    const unsigned char code = static_cast<unsigned char>(input[index++]);
                    if (code >= 0x40 && code <= 0x7e)
                        break;
                }
            } else if (input[index] == ']') {
                ++index;
                while (index < input.size()) {
                    if (input[index] == '\a') {
                        ++index;
                        break;
                    }
                    if (input[index] == 0x1b && index + 1 < input.size() && input[index + 1] == '\\') {
                        index += 2;
                        break;
                    }
                    ++index;
                }
            } else {
                ++index;
            }
        } else if (character == '\r') {
            if (index + 1 >= input.size() || input[index + 1] != '\n')
                output.push_back('\n');
            ++index;
        } else if (character == '\b' || character == 0x7f) {
            if (!output.empty() && output.back() != '\n')
                output.pop_back();
            ++index;
        } else if (character == '\0') {
            ++index;
        } else {
            output.push_back(static_cast<char>(character));
            ++index;
        }
    }
    return output;
}

class PtySession {
public:
    PtySession() = default;
    ~PtySession() { stop(); }

    bool start(const Programme& programme, std::string& message)
    {
        waitForReadyPrompt_ = programme.shell == "bash" || (programme.shell.size() >= 5
            && programme.shell.substr(programme.shell.size() - 5) == "/bash");
        screen_.resize(programme.columns, programme.rows);
        screen_.setDefaultColours(programme.foreground, programme.background);
        master_ = posix_openpt(O_RDWR | O_NOCTTY);
        if (master_ < 0 || grantpt(master_) != 0 || unlockpt(master_) != 0) {
            message = std::string("cannot open PTY: ") + std::strerror(errno);
            return false;
        }
        char* slaveName = ptsname(master_);
        if (slaveName == nullptr) {
            message = std::string("cannot name PTY: ") + std::strerror(errno);
            return false;
        }
        const int slave = open(slaveName, O_RDWR | O_NOCTTY);
        if (slave < 0) {
            message = std::string("cannot open PTY slave: ") + std::strerror(errno);
            return false;
        }
        winsize size {};
        size.ws_col = static_cast<unsigned short>(programme.columns);
        size.ws_row = static_cast<unsigned short>(programme.rows);
        ioctl(slave, TIOCSWINSZ, &size);

        child_ = fork();
        if (child_ < 0) {
            message = std::string("cannot fork shell: ") + std::strerror(errno);
            close(slave);
            return false;
        }
        if (child_ == 0) {
            setsid();
#ifdef TIOCSCTTY
            ioctl(slave, TIOCSCTTY, 0);
#endif
            dup2(slave, STDIN_FILENO);
            dup2(slave, STDOUT_FILENO);
            dup2(slave, STDERR_FILENO);
            if (slave > STDERR_FILENO)
                close(slave);
            close(master_);
            setenv("TERM", "xterm-256color", 1);
            setenv("PS1", readyPrompt, 1);
            setenv("PS2", "", 1);
            for (const auto& entry : programme.environment)
                setenv(entry.first.c_str(), entry.second.c_str(), 1);
            if (programme.shell == "bash" || (programme.shell.size() >= 5
                && programme.shell.substr(programme.shell.size() - 5) == "/bash"))
                execlp(programme.shell.c_str(), programme.shell.c_str(), "--noprofile", "--norc", nullptr);
            else
                execlp(programme.shell.c_str(), programme.shell.c_str(), nullptr);
            _exit(127);
        }

        close(slave);
        const int flags = fcntl(master_, F_GETFL, 0);
        fcntl(master_, F_SETFL, flags | O_NONBLOCK);
        return true;
    }

    bool waitUntilReady(std::string& message)
    {
        if (!waitForReadyPrompt_) {
            pump(0.5);
            rawScreen_.clear();
            rawTranscript_.clear();
            return running_;
        }
        const Clock::time_point deadline = Clock::now() + std::chrono::seconds(5);
        while (Clock::now() < deadline && running_ && !interrupted) {
            pump(0.02);
            if (rawScreen_.find(readyPrompt) != std::string::npos) {
                rawScreen_.clear();
                rawTranscript_.clear();
                return true;
            }
        }
        message = "shell did not become ready within 5s";
        return false;
    }

    bool writeInput(const std::string& value, std::string& message)
    {
        std::size_t written = 0;
        while (written < value.size()) {
            const ssize_t count = write(master_, value.data() + written, value.size() - written);
            if (count > 0) {
                written += static_cast<std::size_t>(count);
            } else if (count < 0 && (errno == EINTR || errno == EAGAIN)) {
                pump(0.01);
            } else {
                message = std::string("cannot write to shell: ") + std::strerror(errno);
                return false;
            }
        }
        return true;
    }

    void pump(double seconds)
    {
        const Clock::time_point deadline = Clock::now()
            + std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(seconds));
        do {
            const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - Clock::now()).count();
            pollfd descriptor {master_, POLLIN, 0};
            const int timeout = seconds <= 0 ? 0 : static_cast<int>(std::max<long long>(0, remaining));
            const int ready = poll(&descriptor, 1, timeout);
            if (ready > 0 && (descriptor.revents & (POLLIN | POLLHUP))) {
                char buffer[4096];
                while (true) {
                    const ssize_t count = read(master_, buffer, sizeof(buffer));
                    if (count > 0) {
                        rawScreen_.append(buffer, static_cast<std::size_t>(count));
                        screen_.feed(buffer, static_cast<std::size_t>(count));
                        if (recording_)
                            rawTranscript_.append(buffer, static_cast<std::size_t>(count));
                    } else {
                        break;
                    }
                }
            }
            updateChild();
            if (seconds <= 0)
                break;
        } while (Clock::now() < deadline && running_ && !interrupted);
    }

    bool waitFor(const std::string& pattern, bool lineOnly, double seconds, std::string& message)
    {
        std::regex expression;
        try {
            expression = std::regex(pattern);
        } catch (const std::regex_error& exception) {
            message = std::string("invalid wait expression: ") + exception.what();
            return false;
        }
        const Clock::time_point deadline = Clock::now()
            + std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(seconds));
        while (Clock::now() < deadline) {
            if (interrupted) {
                message = "interrupted while waiting";
                return false;
            }
            pump(0.02);
            std::string screen = stripControlSequences(rawScreen_);
            if (lineOnly) {
                const std::size_t newline = screen.find_last_of('\n');
                if (newline != std::string::npos)
                    screen = screen.substr(newline + 1);
            }
            if (std::regex_search(screen, expression))
                return true;
            if (!running_)
                break;
        }
        message = "timeout waiting for /" + pattern + "/";
        return false;
    }

    void setRecording(bool recording) { recording_ = recording; }
    bool running() { updateChild(); return running_; }
    std::string transcript() const { return stripControlSequences(rawTranscript_); }
    const TerminalScreen& screen() const { return screen_; }

    void stop()
    {
        if (master_ < 0)
            return;
        if (running_) {
            recording_ = false;
            std::string ignored;
            writeInput("exit\r", ignored);
            pump(0.3);
        }
        if (running_) {
            kill(-child_, SIGTERM);
            kill(child_, SIGTERM);
            waitpid(child_, nullptr, 0);
        }
        close(master_);
        master_ = -1;
        running_ = false;
    }

private:
    void updateChild()
    {
        if (!running_ || child_ <= 0)
            return;
        int status = 0;
        const pid_t result = waitpid(child_, &status, WNOHANG);
        if (result == child_)
            running_ = false;
    }

    int master_ = -1;
    pid_t child_ = -1;
    bool running_ = true;
    bool recording_ = true;
    bool waitForReadyPrompt_ = false;
    std::string rawScreen_;
    std::string rawTranscript_;
    TerminalScreen screen_;
};

std::string keySequence(const std::string& name)
{
    if (name == "Enter") return "\r";
    if (name == "Tab") return "\t";
    if (name == "Space") return " ";
    if (name == "Backspace") return "\x7f";
    if (name == "Delete") return "\x1b[3~";
    if (name == "Insert") return "\x1b[2~";
    if (name == "Up") return "\x1b[A";
    if (name == "Down") return "\x1b[B";
    if (name == "Right") return "\x1b[C";
    if (name == "Left") return "\x1b[D";
    if (name == "PageUp" || name == "ScrollUp") return "\x1b[5~";
    if (name == "PageDown" || name == "ScrollDown") return "\x1b[6~";
    if (name == "Escape") return "\x1b";
    return "";
}

std::string modifiedKeySequence(const std::string& name)
{
    const std::size_t lastPlus = name.find_last_of('+');
    if (lastPlus == std::string::npos || lastPlus + 1 >= name.size())
        return "";
    const std::string modifiers = name.substr(0, lastPlus);
    const std::string key = name.substr(lastPlus + 1);
    std::string sequence = keySequence(key);
    if (sequence.empty())
        sequence = key;
    if (modifiers.find("Shift") != std::string::npos && sequence.size() == 1)
        sequence[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(sequence[0])));
    if (modifiers.find("Ctrl") != std::string::npos && sequence.size() == 1) {
        const unsigned char character = static_cast<unsigned char>(std::toupper(static_cast<unsigned char>(sequence[0])));
        sequence.assign(1, static_cast<char>(character & 0x1f));
    }
    if (modifiers.find("Alt") != std::string::npos)
        sequence.insert(sequence.begin(), '\x1b');
    return sequence;
}

bool programmeFromCommands(const std::vector<Command>& commands, Programme& programme,
    std::ostream& error)
{
    programme.commands = commands;
    for (const Command& command : commands) {
        if (command.kind == CommandKind::Output) {
            const std::string path = unquotePath(command.argument);
            const std::size_t dot = path.find_last_of('.');
            const std::string extension = dot == std::string::npos ? "" : path.substr(dot);
            if (extension == ".txt" || extension == ".ascii")
                programme.textOutputs.push_back(path);
            else if (extension == ".png")
                programme.pngOutputs.push_back(path);
            else if (!path.empty() && path.back() == '/')
                programme.frameDirectories.push_back(path);
            else if (extension == ".gif" || extension == ".mp4" || extension == ".webm")
                programme.videoOutputs.push_back(path);
        } else if (command.kind == CommandKind::Require) {
            bool found = false;
            if (command.argument.find('/') != std::string::npos) {
                found = access(command.argument.c_str(), X_OK) == 0;
            } else {
                const char* pathValue = std::getenv("PATH");
                std::istringstream paths(pathValue == nullptr ? "" : pathValue);
                std::string directory;
                while (std::getline(paths, directory, ':')) {
                    if (directory.empty())
                        directory = ".";
                    if (access((directory + "/" + command.argument).c_str(), X_OK) == 0) {
                        found = true;
                        break;
                    }
                }
            }
            if (!found) {
                error << "vhs: required program not found: " << command.argument << "\n";
                return false;
            }
        } else if (command.kind == CommandKind::Env) {
            programme.environment[command.option] = command.argument;
        } else if (command.kind == CommandKind::Set) {
            if (command.option == "Shell") programme.shell = command.argument;
            else if (command.option == "Columns") programme.columns = std::stoi(command.argument);
            else if (command.option == "Rows") programme.rows = std::stoi(command.argument);
            else if (command.option == "Width") programme.render.width = std::stoi(command.argument);
            else if (command.option == "Height") programme.render.height = std::stoi(command.argument);
            else if (command.option == "FontSize") programme.render.fontSize = std::stoi(command.argument);
            else if (command.option == "FontFamily") programme.render.fontFamily = command.argument;
            else if (command.option == "Padding") programme.render.padding = std::stoi(command.argument);
            else if (command.option == "Margin") programme.render.margin = std::stoi(command.argument);
            else if (command.option == "BorderRadius") programme.render.borderRadius = std::stoi(command.argument);
            else if (command.option == "MarginFill") programme.render.marginFill = command.argument;
            else if (command.option == "WindowBar") {
                programme.render.windowBar = command.argument;
                if (programme.render.windowBarSize == 0)
                    programme.render.windowBarSize = 36;
            }
            else if (command.option == "WindowBarSize") programme.render.windowBarSize = std::stoi(command.argument);
            else if (command.option == "Framerate") programme.video.framerate = std::stoi(command.argument);
            else if (command.option == "PlaybackSpeed") programme.video.playbackSpeed = std::stod(command.argument);
            else if (command.option == "LoopOffset") programme.video.loopOffset = command.argument;
            else if (command.option == "Theme") applyTheme(command.argument, programme);
            else if (command.option == "TypingSpeed") programme.typingSeconds = durationSeconds(command.argument);
            else if (command.option == "WaitTimeout") programme.waitSeconds = durationSeconds(command.argument);
        }
    }
    if (programme.textOutputs.empty() && programme.pngOutputs.empty()
        && programme.frameDirectories.empty() && programme.videoOutputs.empty()) {
        error << "vhs: unsupported or missing Output\n";
        return false;
    }
    if (programme.video.playbackSpeed <= 0.0) {
        error << "vhs: PlaybackSpeed must be greater than zero\n";
        return false;
    }
    if (!programme.videoOutputs.empty()) {
        bool found = false;
        const char* pathValue = std::getenv("PATH");
        std::istringstream paths(pathValue == nullptr ? "" : pathValue);
        std::string directory;
        while (std::getline(paths, directory, ':')) {
            if (directory.empty()) directory = ".";
            if (access((directory + "/ffmpeg").c_str(), X_OK) == 0) {
                found = true;
                break;
            }
        }
        if (!found) {
            error << "vhs: animated output requires ffmpeg on PATH\n";
            return false;
        }
    }
    return true;
}

bool renderFrame(const Programme& programme, const PtySession& session,
    const std::string& path, std::ostream& error)
{
    std::string message;
    if (!renderPng(session.screen(), programme.render, path, message)) {
        error << "vhs: " << message << "\n";
        return false;
    }
    return true;
}

bool executeProgramme(const Programme& programme, std::string& transcript,
    std::vector<std::string>& createdPngs, std::vector<TerminalScreen>& frames,
    std::ostream& error)
{
    PtySession session;
    std::string message;
    if (!session.start(programme, message)) {
        error << "vhs: " << message << "\n";
        return false;
    }
    if (!session.waitUntilReady(message)) {
        error << "vhs: " << message << "\n";
        return false;
    }
    FrameTimeline timeline(programme.video.framerate);
    bool capturing = true;
    for (const Command& command : programme.commands) {
        if (interrupted) {
            error << "vhs: interrupted\n";
            return false;
        }
        if (!session.running()) {
            error << "vhs: shell exited before tape line " << command.line << "\n";
            return false;
        }
        if (command.kind == CommandKind::Type) {
            const double delay = command.option.empty() ? programme.typingSeconds : durationSeconds(command.option);
            for (unsigned char character : command.argument) {
                if (!session.writeInput(std::string(1, static_cast<char>(character)), message))
                    break;
                session.pump(delay);
                timeline.advance(delay, session.screen(), capturing);
            }
        } else if (command.kind == CommandKind::Key) {
            const int repeat = std::stoi(command.argument);
            const double delay = command.option.empty() ? programme.typingSeconds : durationSeconds(command.option);
            for (int count = 0; count < repeat; ++count) {
                if (!session.writeInput(keySequence(command.name), message))
                    break;
                session.pump(delay);
                timeline.advance(delay, session.screen(), capturing);
            }
        } else if (command.kind == CommandKind::ModifiedKey) {
            session.writeInput(modifiedKeySequence(command.name), message);
            session.pump(programme.typingSeconds);
            timeline.advance(programme.typingSeconds, session.screen(), capturing);
        } else if (command.kind == CommandKind::Sleep) {
            const double duration = durationSeconds(command.argument);
            session.pump(duration);
            timeline.advance(duration, session.screen(), capturing);
        } else if (command.kind == CommandKind::Wait) {
            const double timeout = command.option.empty() ? programme.waitSeconds : durationSeconds(command.option);
            const Clock::time_point started = Clock::now();
            if (!session.waitFor(command.argument, command.name == "Wait+Line", timeout, message)) {
                error << "vhs:" << command.line << ": " << message << "\n";
                return false;
            }
            const double elapsed = std::chrono::duration<double>(Clock::now() - started).count();
            timeline.advance(elapsed, session.screen(), capturing);
        } else if (command.kind == CommandKind::Hide) {
            session.pump(0);
            session.setRecording(false);
            capturing = false;
        } else if (command.kind == CommandKind::Show) {
            session.pump(0);
            session.setRecording(true);
            capturing = true;
        } else if (command.kind == CommandKind::Screenshot) {
            const std::string path = command.argument.empty() ? "screenshot.png" : unquotePath(command.argument);
            if (!renderFrame(programme, session, path, error))
                return false;
            createdPngs.push_back(path);
        } else if (command.kind == CommandKind::Copy) {
            if (!copyToClipboard(command.argument, message)) {
                error << "vhs:" << command.line << ": " << message << "\n";
                return false;
            }
        } else if (command.kind == CommandKind::Paste) {
            std::string pasted;
            if (!pasteFromClipboard(pasted, message)
                || !session.writeInput(pasted, message)) {
                error << "vhs:" << command.line << ": " << message << "\n";
                return false;
            }
            session.pump(programme.typingSeconds);
            timeline.advance(programme.typingSeconds, session.screen(), capturing);
        }
        if (!message.empty()) {
            error << "vhs:" << command.line << ": " << message << "\n";
            return false;
        }
    }
    session.pump(0.15);
    timeline.advance(0.15, session.screen(), capturing);
    timeline.ensureFrame(session.screen());
    frames = timeline.frames();
    transcript = session.transcript();
    for (const std::string& path : programme.pngOutputs) {
        if (!renderFrame(programme, session, path, error))
            return false;
        createdPngs.push_back(path);
    }
    session.stop();
    return true;
}

} // namespace

int executeTapeFile(const std::string& path, std::ostream& output, std::ostream& error)
{
    SignalGuard signals;
    std::vector<Command> commands;
    std::set<std::string> active;
    if (!loadCommands(path, commands, active, error))
        return 1;
    Programme programme;
    if (!programmeFromCommands(commands, programme, error))
        return 1;
    std::string transcript;
    std::vector<std::string> createdPngs;
    std::vector<TerminalScreen> frames;
    if (!executeProgramme(programme, transcript, createdPngs, frames, error))
        return 1;
    for (const std::string& path : programme.textOutputs) {
        std::ofstream file(path);
        if (!file) {
            error << "vhs: cannot create output: " << path << "\n";
            return 1;
        }
        file << transcript;
        if (!file) {
            error << "vhs: cannot finish output: " << path << "\n";
            return 1;
        }
        output << "Created " << path << "\n";
    }
    for (const std::string& path : createdPngs)
        output << "Created " << path << "\n";
    std::vector<std::string> createdAnimated;
    if ((!programme.frameDirectories.empty() || !programme.videoOutputs.empty())
        && !renderOutputs(frames, programme.render, programme.video,
            programme.frameDirectories, programme.videoOutputs, createdAnimated, error))
        return 1;
    for (const std::string& path : createdAnimated)
        output << "Created " << path << "\n";
    return 0;
}

} // namespace vhs
