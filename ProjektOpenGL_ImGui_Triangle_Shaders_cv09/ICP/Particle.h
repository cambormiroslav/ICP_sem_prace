#pragma once

#include <glm/glm.hpp>

class Particle {
public:
    glm::vec3 position;  // Pozice ��stice
    glm::vec3 velocity;  // Rychlost ��stice
    float lifeTime;      // Jak dlouho ��stice bude aktivn�

    Particle(glm::vec3 startPos, glm::vec3 startVelocity, float startLifeTime)
        : position(startPos), velocity(startVelocity), lifeTime(startLifeTime) {}

    void update(float deltaTime) {
        position += velocity * deltaTime;  // Pohyb podle rychlosti
        lifeTime -= deltaTime;  // Sni�ov�n� �ivotnosti
    }

    bool isAlive() const {
        return lifeTime > 0.0f;  // Pokud �ivotnost je v�t�� ne� 0, ��stice �ije
    }
};
