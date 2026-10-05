#ifndef VHS_VIDEO_H
#define VHS_VIDEO_H

#include "vhs/renderer.h"
#include "vhs/terminal.h"

#include <iosfwd>
#include <string>
#include <vector>

namespace vhs {

struct VideoOptions {
    int framerate = 30;
    double playbackSpeed = 1.0;
    std::string loopOffset = "0";
};

bool renderOutputs(std::vector<TerminalScreen> frames, const RenderOptions& render,
    const VideoOptions& video, const std::vector<std::string>& frameDirectories,
    const std::vector<std::string>& videoOutputs, std::vector<std::string>& created,
    std::ostream& error);

} // namespace vhs

#endif

