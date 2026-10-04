#include "vhs/video.h"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <iomanip>
#include <sstream>
#include <stdlib.h>
#include <sys/stat.h>
#ifdef __HAIKU__
#include <posix/sys/wait.h>
#else
#include <sys/wait.h>
#endif
#include <unistd.h>

namespace vhs {
namespace {

class TemporaryFrames {
public:
    ~TemporaryFrames()
    {
        for (const std::string& path : files)
            unlink(path.c_str());
        if (!directory.empty())
            rmdir(directory.c_str());
    }

    std::string directory;
    std::vector<std::string> files;
};

std::string frameName(const std::string& directory, std::size_t index)
{
    std::ostringstream path;
    path << directory;
    if (!directory.empty() && directory.back() != '/')
        path << '/';
    path << "frame-" << std::setw(6) << std::setfill('0') << index << ".png";
    return path.str();
}

bool ensureDirectory(const std::string& path, std::ostream& error)
{
    std::string clean = path;
    while (clean.size() > 1 && clean.back() == '/')
        clean.pop_back();
    if (mkdir(clean.c_str(), 0755) == 0 || errno == EEXIST)
        return true;
    error << "vhs: cannot create frame directory " << clean << ": "
          << std::strerror(errno) << "\n";
    return false;
}

bool run(const std::vector<std::string>& arguments, std::ostream& error)
{
    std::vector<char*> values;
    values.reserve(arguments.size() + 1);
    for (const std::string& argument : arguments)
        values.push_back(const_cast<char*>(argument.c_str()));
    values.push_back(nullptr);

    const pid_t child = fork();
    if (child < 0) {
        error << "vhs: cannot start FFmpeg: " << std::strerror(errno) << "\n";
        return false;
    }
    if (child == 0) {
        execvp(values[0], values.data());
        _exit(127);
    }
    int status = 0;
    while (waitpid(child, &status, 0) < 0 && errno == EINTR) {
    }
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
        error << "vhs: FFmpeg failed for " << arguments.back() << "\n";
        return false;
    }
    return true;
}

void applyLoopOffset(std::vector<TerminalScreen>& frames, const std::string& value)
{
    if (frames.empty() || value.empty() || value == "0")
        return;
    std::size_t offset = 0;
    try {
        if (value.back() == '%') {
            const double percent = std::stod(value.substr(0, value.size() - 1));
            offset = static_cast<std::size_t>(frames.size() * percent / 100.0);
        } else {
            offset = static_cast<std::size_t>(std::stoul(value));
        }
    } catch (...) {
        return;
    }
    if (offset >= frames.size())
        offset %= frames.size();
    std::rotate(frames.begin(), frames.begin() + static_cast<std::ptrdiff_t>(offset), frames.end());
}

} // namespace

bool renderOutputs(std::vector<TerminalScreen> frames, const RenderOptions& render,
    const VideoOptions& video, const std::vector<std::string>& frameDirectories,
    const std::vector<std::string>& videoOutputs, std::vector<std::string>& created,
    std::ostream& error)
{
    if (frames.empty())
        frames.emplace_back();
    applyLoopOffset(frames, video.loopOffset);

    TemporaryFrames temporary;
    if (!videoOutputs.empty()) {
        char pattern[] = "/tmp/vhs-frames-XXXXXX";
        const int temporaryFile = mkstemp(pattern);
        if (temporaryFile < 0) {
            error << "vhs: cannot create temporary frame directory\n";
            return false;
        }
        close(temporaryFile);
        unlink(pattern);
        if (mkdir(pattern, 0700) != 0) {
            error << "vhs: cannot create temporary frame directory\n";
            return false;
        }
        temporary.directory = pattern;
    }

    for (std::size_t index = 0; index < frames.size(); ++index) {
        if (!temporary.directory.empty()) {
            const std::string path = frameName(temporary.directory, index);
            std::string message;
            if (!renderPng(frames[index], render, path, message)) {
                error << "vhs: " << message << "\n";
                return false;
            }
            temporary.files.push_back(path);
        }
        for (const std::string& directory : frameDirectories) {
            if (!ensureDirectory(directory, error))
                return false;
            const std::string path = frameName(directory, index);
            std::string message;
            if (!renderPng(frames[index], render, path, message)) {
                error << "vhs: " << message << "\n";
                return false;
            }
            created.push_back(path);
        }
    }

    const std::string input = frameName(temporary.directory, 0);
    const std::size_t marker = input.find("000000");
    const std::string inputPattern = marker == std::string::npos ? input
        : input.substr(0, marker) + "%06d" + input.substr(marker + 6);
    for (const std::string& output : videoOutputs) {
        std::vector<std::string> arguments = {
            "ffmpeg", "-v", "error", "-y", "-framerate",
            std::to_string(video.framerate), "-i", inputPattern
        };
        std::ostringstream timing;
        timing << "setpts=PTS/" << video.playbackSpeed;
        if (output.size() >= 4 && output.substr(output.size() - 4) == ".gif") {
            arguments.insert(arguments.end(), {"-filter_complex",
                timing.str() + ",split[a][b];[a]palettegen[p];[b][p]paletteuse",
                "-loop", "0", output});
        } else if (output.size() >= 4 && output.substr(output.size() - 4) == ".mp4") {
            arguments.insert(arguments.end(), {"-vf", timing.str(), "-r",
                std::to_string(video.framerate), "-pix_fmt", "yuv420p", output});
        } else {
            arguments.insert(arguments.end(), {"-vf", timing.str(), "-r",
                std::to_string(video.framerate), "-c:v", "libvpx-vp9",
                "-pix_fmt", "yuv420p", output});
        }
        if (!run(arguments, error))
            return false;
        created.push_back(output);
    }

    return true;
}

} // namespace vhs
