#ifndef STLVIEWER_CONSOLE_PROGRESS_H
#define STLVIEWER_CONSOLE_PROGRESS_H

#include "Progress_abstract.h"
#include <string>

// NOTE: This is a copy of ConsoleProgress from the main myGuiFramework project
// It is included here to allow mySTLviewer to compile and run standalone

// Console-based progress indicator
// Displays progress as a text-based progress bar in the console
class ConsoleProgress : public Progress_abstract {
public:
    ConsoleProgress();
    virtual ~ConsoleProgress() = default;
    
    // Set progress value (0.0 to 1.0)
    void setProgress(float progress) override;
    
    // Set message text
    void setMessage(const std::string& message) override;
    
    // Check if cancelled (always returns false for console progress)
    bool isCancelled() const override { return false; }
    
    // Set progress bar width (default 40)
    void setBarWidth(int width) { barWidth_ = width; }
    
    // Get current progress
    float getProgress() const { return progress_; }
    
    // Get current message
    const std::string& getMessage() const { return message_; }

private:
    void updateDisplay();
    
    float progress_;
    std::string message_;
    int barWidth_;
    int lastPercent_;
    std::string lastMessage_;
};

#endif // STLVIEWER_CONSOLE_PROGRESS_H
