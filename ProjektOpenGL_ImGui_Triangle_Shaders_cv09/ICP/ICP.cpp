// icp.cpp 
// author: JJ

#include <iostream>
#include <chrono>
#include <stack>
#include <random>
#include "App.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "../include/stb_image.h" 

// define our application
App app;



int main()
{
    if (app.init())
        return app.run();
}
