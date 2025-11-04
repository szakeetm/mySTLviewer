#ifndef STLVIEWER_PROGRESS_ABSTRACT_H
#define STLVIEWER_PROGRESS_ABSTRACT_H

#include <string>

// Abstract base class for progress indicators
// Provides interface for setting progress and messages
class Progress_abstract {
public:
    virtual ~Progress_abstract() = default;
    
    // Set progress value (0.0 to 1.0)
    virtual void setProgress(float progress) = 0;
    
    // Set message text
    virtual void setMessage(const std::string& message) = 0;
    
    // Check if cancelled
    virtual bool isCancelled() const = 0;
};

#endif // STLVIEWER_PROGRESS_ABSTRACT_H

