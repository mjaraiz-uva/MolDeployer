#pragma once
#include <string>
#include <atomic>

namespace Screenshot {
    // Request a screenshot to be taken on the next frame render.
    // The actual capture happens in the GL thread after EndFrame().
    void Request(const std::string& filename);

    // Check if a screenshot is pending, and if so capture it.
    // Must be called from the GL thread after rendering, before swap.
    void CaptureIfPending();
}
