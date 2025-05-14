#pragma once

#include <vector>
#include <opencv2/opencv.hpp>

#include <GL/glew.h>
#include <GL/wglew.h>
#include <GLFW/glfw3.h>

#include "assets.hpp"
#include "ShaderProgram.hpp"
#include "Model.h"
#include "miniaudio.h"




class App {
public:
    App();

    ma_engine audio_engine; 

    GLuint camera_texture = 0;
 
    float lastTime = 0.0f;  


    void init_audio();
    void play_audio();
    void stop_audio();
    void destroy_audio();

    bool init(void);
    void init_imgui();
    int run(void);
    void destroy(void);
    void camera_loop();

    //CAMERA
    std::thread camera_thread;
    bool camera_running = true;

    ~App();
private:
    cv::VideoCapture capture;

    cv::Mat scene_hsv, scene_threshold;

    // GL
    GLFWwindow* window = nullptr;
    bool is_vsync_on = true;
    bool show_imgui = true;

    bool music_is_run = false;
    bool music_is_init = false;
    ma_sound mySound;
    
    
    //SPOTLIGHT
    bool spotlight_on = true;
    float spotlight_intensity = 1.0f;

    //FULLSCREEN
    bool is_fullscreen = false;
    int windowed_pos_x = 100, windowed_pos_y = 100;
    int windowed_width = 800, windowed_height = 600;

    //FREECAM
    glm::vec3 cameraPos = glm::vec3(0.0f, 0.0f, 3.0f);
    glm::vec3 cameraFront = glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 cameraUp = glm::vec3(0.0f, 1.0f, 0.0f);

    float yaw = -90.0f;   // směr "dozadu" (v ose Z)
    float pitch = 0.0f;   // nahoru/dolu
    float lastX = 400, lastY = 300; // střed okna (přednastaveno na velikost 800x600)
    float fov = 45.0f;

    bool firstMouse = true;

    float cameraSpeed = 2.5f; // pohybová rychlost


    //void thread_code(void);

    void init_opencv();
    void init_glew(void);
    void init_glfw(void);
    void init_gl_debug();
    void init_assets(void);
    void start_capture_thread();

    void print_opencv_info();
    void print_glfw_info(void);
    void print_glm_info();
    void print_gl_info();
    void toggleFullscreen();

    //callbacks
    static void glfw_error_callback(int error, const char* description);
    static void glfw_framebuffer_size_callback(GLFWwindow* window, int width, int height);
    static void glfw_mouse_button_callback(GLFWwindow* window, int button, int action, int mods);
    static void glfw_cursor_position_callback(GLFWwindow* window, double xpos, double ypos);
    static void glfw_key_callback(GLFWwindow* window, int key, int scancode, int action, int mods);
    static void glfw_scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
    static void GLAPIENTRY MessageCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam);
    
    void play_if_color();

    //new GL stuff
    GLuint shader_prog_ID{ 0 };
    GLuint VBO_ID{ 0 };
    GLuint VAO_ID{ 0 };

    GLfloat r{ 1.0f }, g{ 0.0f }, b{ 0.0f }, a{ 1.0f };

    std::vector<vertex> triangle_vertices =
    {
        {{0.0f,  0.5f,  0.0f}},
        {{0.5f, -0.5f,  0.0f}},
        {{-0.5f, -0.5f,  0.0f}}
    };

    ShaderProgram shader;
    std::unordered_map<std::string, Model> scene;
};

