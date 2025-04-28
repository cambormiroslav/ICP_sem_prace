#pragma once

#include <glm/glm.hpp>

class Particle {
public:
    glm::vec3 position;  // Pozice èástice
    glm::vec3 velocity;  // Rychlost èástice
    float lifeTime;      // Jak dlouho èástice bude aktivní

    Particle(glm::vec3 startPos, glm::vec3 startVelocity, float startLifeTime)
        : position(startPos), velocity(startVelocity), lifeTime(startLifeTime) {}

    void update(float deltaTime) {
        position += velocity * deltaTime;  // Pohyb podle rychlosti
        lifeTime -= deltaTime;  // Snižování životnosti
    }

    bool isAlive() const {
        return lifeTime > 0.0f;  // Pokud životnost je vìtší než 0, èástice žije
    }
};
