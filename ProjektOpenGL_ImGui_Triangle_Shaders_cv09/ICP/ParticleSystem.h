#pragma once

#include <vector>
#include "Particle.h"
#include <glm/glm.hpp>

class ParticleSystem {
public:
    std::vector<Particle> particles;

    void addParticle(glm::vec3 position, glm::vec3 velocity, float lifeTime) {
        particles.push_back(Particle(position, velocity, lifeTime));
    }

    void update(float deltaTime) {
        for (auto it = particles.begin(); it != particles.end(); ) {
            it->update(deltaTime);  // Aktualizuje èástici
            if (!it->isAlive()) {  // Pokud èástice "umøela"
                it = particles.erase(it);  // Odstraní ji ze seznamu
            }
            else {
                ++it;
            }
        }
    }

    void draw() {
        glPointSize(5.0f);  // Nastavíme velikost bodù (èástic)
        glBegin(GL_POINTS);

        for (auto& particle : particles) {
            glColor3f(1.0f, 1.0f, 1.0f);  // Nastavení barvy pro všechny èástice (bílá)
            glVertex3f(particle.position.x, particle.position.y, particle.position.z);  // Nastavení pozice
        }

        glEnd();
    }
};
