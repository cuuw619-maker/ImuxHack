#pragma once

#include <cstdint>
#include <vector>

namespace imux::generator {

enum class ObjectType : std::uint8_t {
    Block,
    Spike,
    Orb,
    Portal,
    Slope,
    Decoration
};

struct GeneratedObject {
    ObjectType type = ObjectType::Block;
    float x = 0.0f;
    float y = 0.0f;
    float rotation = 0.0f;
    double time = 0.0;
    float importance = 0.0f;
};

struct LevelGraph {
    std::vector<GeneratedObject> objects;

    void clear() {
        objects.clear();
    }
};

}
