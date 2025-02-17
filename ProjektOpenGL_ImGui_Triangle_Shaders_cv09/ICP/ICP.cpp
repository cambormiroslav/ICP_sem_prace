// icp.cpp 
// author: JJ

#include <iostream>
#include <chrono>
#include <stack>
#include <random>
#include "App.hpp"

// define our application
App app;



int main()
{
    if (app.init())
        return app.run();
}
