#pragma once

#include <filesystem>
#include <string>
#include <vector> 
#include <glm/glm.hpp> 

#include "Vertex.h"
#include "Mesh.h"
#include "ShaderProgram.hpp"
#include "OBJloader.hpp"

class Model
{
public:

    std::vector<Mesh> meshes;
    std::string name;
    glm::vec3 origin{};
    glm::vec3 orientation{};
    glm::vec3 size{};

    Model() = default;

    //Model(const std::filesystem::path filename, ShaderProgram& shader) {
    Model(const char* path, ShaderProgram& shader, const char* texturePath) {
        std::vector<GLuint> indices;
        std::vector<Vertex> vertices;

        loadOBJ(path, vertices, indices);
        orientation = glm::vec3(0.0f);
        origin = glm::vec3(0.0f);
        size = glm::vec3(1.0f);

        Mesh mesh = Mesh(GL_TRIANGLES, shader, vertices, indices, origin, orientation, texturePath);
        meshes.push_back(mesh);
    }


    // update position etc. based on running time
    void update(const float delta_t) {
        // origin += glm::vec3(3,0,0) * delta_t; s=s0+v*dt
    }

    void draw(glm::vec3 const& offset = glm::vec3(0.0f), glm::vec3 const& rotation = glm::vec3(0.0f)) {
        // call draw() on mesh (all meshes)
        for (auto &mesh : meshes) {
            mesh.draw(origin + offset, orientation + rotation);
        }
    }
}
;
