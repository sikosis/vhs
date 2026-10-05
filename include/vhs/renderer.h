#ifndef VHS_RENDERER_H
#define VHS_RENDERER_H

#include <string>

namespace vhs {

class TerminalScreen;

struct RenderOptions {
    int width = 1000;
    int height = 600;
    int fontSize = 24;
    std::string fontFamily = "Noto Sans Mono";
    int padding = 20;
    int margin = 0;
    int borderRadius = 0;
    std::string marginFill = "#000000";
    std::string windowBar = "Haiku";
    int windowBarSize = 30;
    std::string background = "#1e1e2e";
    std::string foreground = "#cdd6f4";
};

bool renderPng(const TerminalScreen& screen, const RenderOptions& options,
    const std::string& path, std::string& message);
bool ensureNativeEnvironment(std::string& message);

} // namespace vhs

#endif
