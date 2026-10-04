#include "vhs/record.h"

#include <cerrno>
#include <chrono>
#include <cstring>
#include <cstdlib>
#include <fcntl.h>
#include <iostream>
#include <poll.h>
#include <sstream>
#include <sys/ioctl.h>
#ifdef __HAIKU__
#include <posix/sys/wait.h>
#else
#include <sys/wait.h>
#endif
#include <termios.h>
#include <unistd.h>

namespace vhs {
namespace {

class RawTerminal {
public:
    explicit RawTerminal(int descriptor)
        : descriptor_(descriptor)
    {
        if (tcgetattr(descriptor_, &saved_) != 0)
            return;
        termios raw = saved_;
        raw.c_iflag &= static_cast<tcflag_t>(~(BRKINT | ICRNL | INPCK | ISTRIP | IXON));
        raw.c_oflag &= static_cast<tcflag_t>(~OPOST);
        raw.c_cflag |= CS8;
        raw.c_lflag &= static_cast<tcflag_t>(~(ECHO | ICANON | IEXTEN | ISIG));
        raw.c_cc[VMIN] = 1;
        raw.c_cc[VTIME] = 0;
        active_ = tcsetattr(descriptor_, TCSAFLUSH, &raw) == 0;
    }

    ~RawTerminal()
    {
        if (active_)
            tcsetattr(descriptor_, TCSAFLUSH, &saved_);
    }

    bool active() const { return active_; }

private:
    int descriptor_;
    termios saved_ {};
    bool active_ = false;
};

std::string quoted(const char* data, std::size_t size)
{
    std::string value = "Type@0ms \"";
    for (std::size_t index = 0; index < size; ++index) {
        const unsigned char character = static_cast<unsigned char>(data[index]);
        if (character == '\\' || character == '"') {
            value.push_back('\\');
            value.push_back(static_cast<char>(character));
        } else if (character == '\n') value += "\\n";
        else if (character == '\t') value += "\\t";
        else value.push_back(static_cast<char>(character));
    }
    value += "\"\n";
    return value;
}

std::string tapeCommand(const char* data, std::size_t size)
{
    const std::string value(data, size);
    if (value == "\r" || value == "\n") return "Enter\n";
    if (value == "\t") return "Tab\n";
    if (value == "\x7f" || value == "\b") return "Backspace\n";
    if (value == "\x1b[A") return "Up\n";
    if (value == "\x1b[B") return "Down\n";
    if (value == "\x1b[C") return "Right\n";
    if (value == "\x1b[D") return "Left\n";
    if (value == "\x1b") return "Escape\n";
    if (size == 1 && static_cast<unsigned char>(data[0]) >= 1
        && static_cast<unsigned char>(data[0]) <= 26) {
        std::string command = "Ctrl+";
        command.push_back(static_cast<char>('A' + data[0] - 1));
        return command + "\n";
    }
    return quoted(data, size);
}

} // namespace

int recordTape(std::ostream& tape, std::ostream& error)
{
    const int terminal = open("/dev/tty", O_RDWR);
    if (terminal < 0) {
        error << "vhs: record requires an interactive terminal\n";
        return 1;
    }
    int master = posix_openpt(O_RDWR | O_NOCTTY);
    if (master < 0 || grantpt(master) != 0 || unlockpt(master) != 0) {
        error << "vhs: cannot create recording PTY: " << std::strerror(errno) << "\n";
        close(terminal);
        return 1;
    }
    const char* slaveName = ptsname(master);
    const int slave = slaveName == nullptr ? -1 : open(slaveName, O_RDWR | O_NOCTTY);
    if (slave < 0) {
        error << "vhs: cannot open recording PTY\n";
        close(master);
        close(terminal);
        return 1;
    }

    const pid_t child = fork();
    if (child == 0) {
        setsid();
#ifdef TIOCSCTTY
        ioctl(slave, TIOCSCTTY, 0);
#endif
        dup2(slave, STDIN_FILENO);
        dup2(slave, STDOUT_FILENO);
        dup2(slave, STDERR_FILENO);
        close(master);
        close(terminal);
        if (slave > STDERR_FILENO) close(slave);
        const char* shell = std::getenv("SHELL");
        if (shell == nullptr) shell = "bash";
        execlp(shell, shell, nullptr);
        _exit(127);
    }
    close(slave);
    if (child < 0) {
        error << "vhs: cannot start recording shell\n";
        close(master);
        close(terminal);
        return 1;
    }

    RawTerminal raw(terminal);
    if (!raw.active()) {
        error << "vhs: cannot enter raw terminal mode\n";
        close(master);
        close(terminal);
        return 1;
    }
    const char* introduction = "\r\nVHS recording started; exit the shell to finish.\r\n\r\n";
    write(terminal, introduction, std::strlen(introduction));
    tape << "# Recorded by VHS for Haiku\nSet TypingSpeed 0ms\n\n";
    tape.flush();

    auto previousInput = std::chrono::steady_clock::now();
    bool running = true;
    while (running) {
        pollfd descriptors[2] = {{terminal, POLLIN, 0}, {master, POLLIN, 0}};
        if (poll(descriptors, 2, 100) < 0 && errno != EINTR)
            break;
        if (descriptors[0].revents & POLLIN) {
            char buffer[256];
            const ssize_t count = read(terminal, buffer, sizeof(buffer));
            if (count > 0) {
                const auto now = std::chrono::steady_clock::now();
                const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - previousInput).count();
                if (elapsed >= 50)
                    tape << "Sleep " << elapsed << "ms\n";
                tape << tapeCommand(buffer, static_cast<std::size_t>(count));
                tape.flush();
                write(master, buffer, static_cast<std::size_t>(count));
                previousInput = now;
            }
        }
        if (descriptors[1].revents & (POLLIN | POLLHUP)) {
            char buffer[4096];
            const ssize_t count = read(master, buffer, sizeof(buffer));
            if (count > 0)
                write(terminal, buffer, static_cast<std::size_t>(count));
        }
        int status = 0;
        if (waitpid(child, &status, WNOHANG) == child)
            running = false;
    }
    close(master);
    close(terminal);
    return 0;
}

} // namespace vhs
