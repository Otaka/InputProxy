#pragma once
#include <string>
#include <vector>
#include <optional>
#include <functional>

struct VidAxisRef {
    std::string vidId;
    std::string axisName;  // retained for deferred resolution
    int         axisIndex = -1;
};

struct VidAxisKey {
    std::string vidId;
    int         axisIndex = -1;
    bool operator==(const VidAxisKey& o) const {
        return vidId == o.vidId && axisIndex == o.axisIndex;
    }
};

struct VidAxisKeyHash {
    std::size_t operator()(const VidAxisKey& k) const {
        return std::hash<std::string>{}(k.vidId) ^ (std::hash<int>{}(k.axisIndex) << 16);
    }
};

struct HotkeyPart {
    std::vector<VidAxisRef>  modifiers;
    std::optional<VidAxisRef> activationAxis;  // absent = modifier-only part
    std::vector<std::string>  involvedVids;    // derived: unique VIDs in this part
};
