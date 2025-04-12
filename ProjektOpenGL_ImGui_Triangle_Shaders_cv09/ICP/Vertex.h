#pragma once

#include <glm/glm.hpp> 

struct Vertex {
    glm::vec3 Position;
    glm::vec3 Normal;
    glm::vec2 TexCoords;

    bool operator==(const Vertex& other) const {
        return Position == other.Position &&
            Normal == other.Normal &&
            TexCoords == other.TexCoords;
    }
};

