#pragma once
#ifndef OBJloader_H
#define OBJloader_H

#include <vector>
#include <glm/fwd.hpp>

#include "Vertex.h"

bool loadOBJ(
	const char * path,
	std::vector <Vertex> & vertices,
	std::vector <GLuint>& indices
);

#endif
