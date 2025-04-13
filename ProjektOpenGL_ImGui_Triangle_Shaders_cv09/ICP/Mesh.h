#pragma once

#include <iostream>
#include <fstream>
#include <sstream>

#include <string>
#include <vector>

#include <glm/glm.hpp> 
#include <glm/ext.hpp>

#include "Vertex.h"
#include "ShaderProgram.hpp"

class Mesh {
public:
    // mesh data
    glm::vec3 origin{};
    glm::vec3 orientation{};
    
    GLuint texture_id{0}; // texture id=0  means no texture
    GLenum primitive_type = GL_POINT;
    ShaderProgram &shader;
    
    // mesh material
    glm::vec4 ambient_material{1.0f}; //white, non-transparent 
    glm::vec4 diffuse_material{1.0f}; //white, non-transparent 
    glm::vec4 specular_material{1.0f}; //white, non-transparent
    float reflectivity{1.0f}; 
    
    // indirect (indexed) draw 
    Mesh(GLenum primitive_type, ShaderProgram& shader, std::vector<Vertex/*pøepsáno*/> const& vertices, std::vector<GLuint> const& indices, glm::vec3 const& origin, glm::vec3 const& orientation, GLuint const texture_id = 0) :
        primitive_type(primitive_type),
        shader(shader),
        vertices(vertices),
        indices(indices),
        origin(origin),
        orientation(orientation),
        texture_id(texture_id)
    {
        // TODO: create and initialize VAO, VBO, EBO and parameters

        GLuint prog_h = shader.getID();

        // Create the VAO and VBO
        glCreateVertexArrays(1, &VAO);
        // Set Vertex Attribute to explain OpenGL how to interpret the data
        GLint position_attrib_location = glGetAttribLocation(prog_h, "aPos");
        if (position_attrib_location == -1)
            std::cerr << "Position of 'aPos' not found" << std::endl;
        else {
            glVertexArrayAttribFormat(VAO, position_attrib_location, 3, GL_FLOAT, GL_FALSE, offsetof(Vertex, Position));
            glVertexArrayAttribBinding(VAO, position_attrib_location, 0);
            glEnableVertexArrayAttrib(VAO, position_attrib_location);
        }

        GLint normal_attrib_location = glGetAttribLocation(prog_h, "aNormal");
        if (normal_attrib_location == -1)
            std::cerr << "Position of 'aNorm' not found" << std::endl;
        else {
            glVertexArrayAttribFormat(VAO, normal_attrib_location, 3, GL_FLOAT, GL_FALSE, offsetof(Vertex, Normal));
            glVertexArrayAttribBinding(VAO, normal_attrib_location, 0);
            glEnableVertexArrayAttrib(VAO, normal_attrib_location);
        }
            
        // Set and enable Vertex Attribute for Texture Coordinates
        GLint texture_attrib_location = glGetAttribLocation(prog_h, "aTex");
        if (texture_attrib_location == -1)
            std::cerr << "Position of 'aTex' not found" << std::endl;
        else {
            glVertexArrayAttribFormat(VAO, texture_attrib_location, 2, GL_FLOAT, GL_FALSE, offsetof(Vertex, TexCoords));
            glVertexArrayAttribBinding(VAO, texture_attrib_location, 0);
            glEnableVertexArrayAttrib(VAO, texture_attrib_location);
        }
                
        // Create and fill data
        glCreateBuffers(1, &VBO); // Vertex Buffer Object
        glCreateBuffers(1, &EBO); // Element Buffer Object
        glNamedBufferData(VBO, vertices.size() * sizeof(Vertex), vertices.data(), GL_STATIC_DRAW);
        glNamedBufferData(EBO, indices.size() * sizeof(GLuint), indices.data(), GL_STATIC_DRAW);
                
        //Connect together
        glVertexArrayVertexBuffer(VAO, 0, VBO, 0, sizeof(Vertex));
        glVertexArrayElementBuffer(VAO, EBO);


    };

    
    void draw(glm::vec3 const & offset, glm::vec3 const & rotation ) {
 		if (VAO == 0) {
			std::cerr << "VAO not initialized!\n";
			return;
		}
 
        shader.activate();
        
        // for future use: set uniform variables: position, textures, etc...  
        //set texture id etc...
        glBindVertexArray(VAO);
        glDrawElements(primitive_type, indices.size(), GL_UNSIGNED_INT, 0);
    }


	void clear(void) {
        texture_id = 0;
        primitive_type = GL_POINT;
        // TODO: clear rest of the member variables to safe default
        
        // TODO: delete all allocations 
        glDeleteBuffers(1, &VBO);
        glDeleteBuffers(1, &EBO);
        glDeleteVertexArrays(1, &VAO);
        
    };

private:
    // OpenGL buffer IDs
    // ID = 0 is reserved (i.e. uninitalized)
     unsigned int VAO{0}, VBO{0}, EBO{0};

     std::vector<Vertex> vertices; //doplnìno
     std::vector<GLuint> indices; //doplnìno
};
  


