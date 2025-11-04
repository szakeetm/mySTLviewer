// NOTE: This is a copy of ConsoleProgress from the main myGuiFramework project
// It is included here to allow mySTLviewer to compile and run standalone

#include "ConsoleProgress.h"
#include <iostream>
#include <iomanip>

ConsoleProgress::ConsoleProgress()
    : progress_(0.0f)
    , barWidth_(40)
    , lastPercent_(-1)
{
}

void ConsoleProgress::setProgress(float progress) {
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    
    progress_ = progress;
    updateDisplay();
}

void ConsoleProgress::setMessage(const std::string& message) {
    if (message_ != message) {
        message_ = message;
        updateDisplay();
    }
}

void ConsoleProgress::updateDisplay() {
    // Calculate current percentage
    int currentPercent = static_cast<int>(progress_ * 100.0f + 0.5f);
    
    // Only update display if percentage changed or message changed
    if (currentPercent != lastPercent_ || message_ != lastMessage_) {
        lastPercent_ = currentPercent;
        lastMessage_ = message_;
        
        // Calculate bar position
        int pos = static_cast<int>(progress_ * barWidth_ + 0.5f);
        
        // Build prefix (use message if available, otherwise default)
        std::string prefix = message_.empty() ? "Progress" : message_;
        
        // Print progress bar
        std::cout << "\r" << prefix << " [";
        for (int i = 0; i < barWidth_; ++i) {
            std::cout << (i < pos ? '=' : ' ');
        }
        std::cout << "] " << std::fixed << std::setprecision(1) << (progress_ * 100.0f) << "%";
        std::cout.flush();
        
        // Print newline when complete
        if (progress_ >= 1.0f) {
            std::cout << std::endl;
        }
    }
}
