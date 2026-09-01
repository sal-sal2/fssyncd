#pragma once

#include <string>

namespace fssyncd {

// Three types of directory changes FileWatcher handles
enum class FileEventType {
    Created,
    Modified,
    Deleted,
};

// FileEvent data
struct FileEvent {
    std::string path;
    FileEventType type;
};

}
