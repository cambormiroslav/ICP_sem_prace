extern "C" {
    _declspec(dllexport) unsigned int NvOptimusEnablement = 0x00000001;
}

#include <iostream>
#include <chrono>
#include <numeric>
#include <string>
#include <filesystem>
#include <algorithm>
#include <random>
#include <thread>

#include <opencv2/opencv.hpp>

#include <GL/glew.h> 
#include <GL/wglew.h> 
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/ext.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <glm/gtc/matrix_transform.hpp>


#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#include "App.hpp"
#include "ShaderProgram.hpp"
#include "headers.hpp"
#include "assets.hpp"
#include "Model.h"

#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"




std::mutex mux;
std::condition_variable cvvv;
cv::Mat frame;
float target_quality = 35;
std::vector<uchar> bytes;
bool ready = false;
bool processed = false;
bool show_imgui = false;

void lossy_quality_limit_thread();



App::App()
{
    // default constructor
    // nothing to do here (so far...)
    std::cout << "Constructed...\n";
}

void App::init_audio() {
    if (ma_engine_init(NULL, &audio_engine) != MA_SUCCESS) {
        std::cerr << "Failed to initialize audio engine!" << std::endl;
    }
}

void App::play_audio() {
    if (ma_engine_play_sound(&audio_engine, "resources/music.mp3", NULL) != MA_SUCCESS) {
        std::cerr << "Failed to play sound!" << std::endl;
    }
}

void App::destroy_audio() {
    ma_engine_uninit(&audio_engine);
}

void App::init_opencv()
{
    // ...
}

void App::start_capture_thread()
{
    std::thread([this]() {
        while (!glfwWindowShouldClose(window))
        {
            {
                std::lock_guard<std::mutex> lock(mux);
                capture >> frame;
            }

            if (frame.empty()) {
                std::cerr << "device closed (or video at the end)" << '\n';
                break;
            }

            
            lossy_quality_limit_thread();

            std::this_thread::sleep_for(std::chrono::milliseconds(30)); // kamera 30 FPS
        }
        }).detach(); // oddìlené vlákno
}

void App::camera_loop()
{
    while (camera_running) {
        cv::Mat frame_local;
        if (capture.read(frame_local)) {
            std::lock_guard<std::mutex> lock(mux);
            frame = frame_local.clone();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10)); // malá pauza
    }
}



bool App::init()
{
    try {
        std::cout << "Current working directory: " << std::filesystem::current_path().generic_string() << '\n';

        if (!std::filesystem::exists("bin"))
            throw std::runtime_error("Directory 'bin' not found. DLLs are expected to be there.");

        if (!std::filesystem::exists("resources"))
            throw std::runtime_error("Directory 'resources' not found. Various media files are expected to be there.");

        init_opencv();
        init_glfw();
        init_glew();
        init_audio();
        init_gl_debug();

        print_opencv_info();
        print_glfw_info();
        print_gl_info();
        print_glm_info();

        camera_thread = std::thread(&App::camera_loop, this);


        if (!GLEW_ARB_direct_state_access)
            throw std::runtime_error("No DSA :-(");

        glfwSwapInterval(is_vsync_on ? 1 : 0); // vsync

        // For future implementation: init assets (models, sounds, textures, level map, ...)
        // (this may take a while, app window is hidden in the meantime)...
        init_assets();

        // When all is loaded, show the window.
        glfwShowWindow(window);

        // Initialize ImGUI (see https://github.com/ocornut/imgui/wiki/Getting-Started)
        init_imgui();


        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);


        capture = cv::VideoCapture(0, cv::CAP_ANY);
        if (!capture.isOpened())
            throw std::runtime_error("Can not open camera :(");

        std::cout << "Cam opened successfully.\n";

        glGenTextures(1, &camera_texture);
        glBindTexture(GL_TEXTURE_2D, camera_texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glBindTexture(GL_TEXTURE_2D, 0);

    }
    catch (std::exception const& e) {
        std::cerr << "Init failed : " << e.what() << std::endl;
        throw;
    }
    std::cout << "Initialized...\n";
    return true;
}

void App::init_glew(void) {
    //
    // Initialize all valid generic GL extensions with GLEW.
    // Usable AFTER creating GL context! (create with glfwInit(), glfwCreateWindow(), glfwMakeContextCurrent()
    //
    {
        GLenum glew_ret;
        glew_ret = glewInit();
        if (glew_ret != GLEW_OK) {
            throw std::runtime_error(std::string("GLEW failed with error: ") + reinterpret_cast<const char*>(glewGetErrorString(glew_ret)));
        }
        else {
            std::cout << "GLEW successfully initialized to version: " << glewGetString(GLEW_VERSION) << "\n";
        }

        // Platform specific init. (Change to GLXEW or ELGEW if necessary.)
        GLenum wglew_ret = wglewInit();
        if (wglew_ret != GLEW_OK) {
            throw std::runtime_error(std::string("WGLEW failed with error: ") + reinterpret_cast<const char*>(glewGetErrorString(wglew_ret)));
        }
        else {
            std::cout << "WGLEW successfully initialized platform specific functions.\n";
        }
    }
}

void App::init_glfw(void)
{

    /* Initialize the library */
    glfwSetErrorCallback(glfw_error_callback);


    if (!glfwInit()) {
        throw std::runtime_error("GLFW can not be initialized.");
    }

    // try to open OpenGL
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // open window, but hidden - it will be enabled later, after asset initialization
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);

    /* Create a windowed mode window and its OpenGL context */
    window = glfwCreateWindow(800, 600, "ICP", nullptr, nullptr);
    if (!window) {
        throw std::runtime_error("GLFW window can not be created.");
    }

    glfwSetWindowUserPointer(window, this);

    /* Make the window's context current */
    glfwMakeContextCurrent(window);

    // disable mouse cursor
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // GLFW callbacks registration
    glfwSetCursorPosCallback(window, glfw_cursor_position_callback);
    glfwSetFramebufferSizeCallback(window, glfw_framebuffer_size_callback);
    glfwSetMouseButtonCallback(window, glfw_mouse_button_callback);
    glfwSetKeyCallback(window, glfw_key_callback);
    glfwSetScrollCallback(window, glfw_scroll_callback);
}

void App::init_imgui()
{
    // see https://github.com/ocornut/imgui/wiki/Getting-Started

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init();
    std::cout << "ImGUI version: " << ImGui::GetVersion() << "\n";
}

void App::init_gl_debug()
{
    if (GLEW_ARB_debug_output)
    {
        glDebugMessageCallback(MessageCallback, 0);
        glEnable(GL_DEPTH_TEST);
        std::cout << "GL_DEBUG enabled." << std::endl;

        // filter notification noise...
        //glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr, GL_FALSE);
    }
}

void App::init_assets(void) {
    //
    // Initialize pipeline: compile, link and use shaders
    //

    shader = ShaderProgram("resources/lighting.vert", "resources/lighting.frag");

    // Model 1 – kostka
    Model cube_model("./obj/cube_triangles_vnt.obj", shader, "resources/tex_beton.jpg");
    scene.insert({ "cube", cube_model });

    // Model 2 – koule
    Model sphere_model("./obj/sphere_tri_vnt.obj", shader, "resources/tex_drevo.jpg");
    scene.insert({ "sphere", sphere_model });

    // Model 3 – kostka
    Model cube_modelalfa("./obj/cube_triangles_vnt.obj", shader, "resources/sklo.png");
    scene.insert({ "cubealfa", cube_modelalfa });

    
}

void App::glfw_error_callback(int error, const char* description)
{
    std::cerr << "GLFW error: " << description << std::endl;
}

void App::glfw_key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    auto this_inst = static_cast<App*>(glfwGetWindowUserPointer(window));
    if ((action == GLFW_PRESS) || (action == GLFW_REPEAT)) {

        float velocity = this_inst->cameraSpeed * 0.01f;
        switch (key) {
        case GLFW_KEY_ESCAPE:
            // Exit The App
            glfwSetWindowShouldClose(window, GLFW_TRUE);
            break;
        case GLFW_KEY_Q:
            target_quality += 1;
            break;
        case GLFW_KEY_V:
            // Vsync on/off
            this_inst->is_vsync_on = !this_inst->is_vsync_on;
            glfwSwapInterval(this_inst->is_vsync_on);
            std::cout << "VSync: " << this_inst->is_vsync_on << "\n";
            break;
        case GLFW_KEY_KP_7:
            this_inst -> r += 0.1;
            std::clamp(this_inst -> r, 0.0f, 1.0f);
            break;
        case GLFW_KEY_KP_4:
            this_inst->r -= 0.1;
            std::clamp(this_inst->r, 0.0f, 1.0f);
            break;
        case GLFW_KEY_KP_8:
            this_inst->b += 0.1;
            std::clamp(this_inst->r, 0.0f, 1.0f);
            break;
        case GLFW_KEY_KP_5:
            this_inst->b -= 0.1;
            std::clamp(this_inst->r, 0.0f, 1.0f);
            break;
        case GLFW_KEY_I:
            this_inst->show_imgui = !this_inst->show_imgui;
            break;
            //SVETLA
        case GLFW_KEY_L:
            this_inst->spotlight_on = !this_inst->spotlight_on;
            std::cout << "Spotlight: " << (this_inst->spotlight_on ? "ON" : "OFF") << "\n";
            break;
            //FULLSCREEN
        case GLFW_KEY_F11:
        {
            auto this_inst = static_cast<App*>(glfwGetWindowUserPointer(window));
            this_inst->toggleFullscreen();
            break;
        }
        case GLFW_KEY_W:
            this_inst->cameraPos += this_inst->cameraFront * velocity;
            break;
        case GLFW_KEY_S:
            this_inst->cameraPos -= this_inst->cameraFront * velocity;
            break;
        case GLFW_KEY_A:
            this_inst->cameraPos -= glm::normalize(glm::cross(this_inst->cameraFront, this_inst->cameraUp)) * velocity;
            break;
        case GLFW_KEY_D:
            this_inst->cameraPos += glm::normalize(glm::cross(this_inst->cameraFront, this_inst->cameraUp)) * velocity;
            break;
        case GLFW_KEY_SPACE:
            this_inst->play_audio();
            break;
        default:
            break;
        }
    }
}

void App::toggleFullscreen()
{
    this->is_fullscreen = !this->is_fullscreen;

    if (this->is_fullscreen)
    {
        // Uložíme si velikost a pozici normálního okna
        glfwGetWindowPos(window, &this->windowed_pos_x, &this->windowed_pos_y);
        glfwGetWindowSize(window, &this->windowed_width, &this->windowed_height);

        GLFWmonitor* monitor = glfwGetPrimaryMonitor();
        const GLFWvidmode* mode = glfwGetVideoMode(monitor);

        glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);

        glViewport(0, 0, mode->width, mode->height);
    }
    else
    {
        // Vrátíme se zpátky do okna
        glfwSetWindowMonitor(window, nullptr, this->windowed_pos_x, this->windowed_pos_y, this->windowed_width, this->windowed_height, 0);

        glViewport(0, 0, this->windowed_width, this->windowed_height);
    }
}



void App::glfw_scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
}

void App::glfw_framebuffer_size_callback(GLFWwindow* window, int width, int height) {
}

void App::glfw_cursor_position_callback(GLFWwindow* window, double xpos, double ypos)
{
    auto* app = static_cast<App*>(glfwGetWindowUserPointer(window));
    if (!app) return;

    if (app->firstMouse)
    {
        app->lastX = xpos;
        app->lastY = ypos;
        app->firstMouse = false;
    }

    float xoffset = xpos - app->lastX;
    float yoffset = app->lastY - ypos;

    app->lastX = xpos;
    app->lastY = ypos;

    float sensitivity = 0.1f;
    xoffset *= sensitivity;
    yoffset *= sensitivity;

    app->yaw += xoffset;
    app->pitch += yoffset;

    if (app->pitch > 89.0f)
        app->pitch = 89.0f;
    if (app->pitch < -89.0f)
        app->pitch = -89.0f;

    glm::vec3 front;
    front.x = cos(glm::radians(app->yaw)) * cos(glm::radians(app->pitch));
    front.y = sin(glm::radians(app->pitch));
    front.z = sin(glm::radians(app->yaw)) * cos(glm::radians(app->pitch));
    app->cameraFront = glm::normalize(front);
    std::cout << "Mouse moved: yaw = " << app->yaw << ", pitch = " << app->pitch << '\n';

}



void App::glfw_mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    if (action == GLFW_PRESS) {
        switch (button) {
        case GLFW_MOUSE_BUTTON_LEFT: {
            int mode = glfwGetInputMode(window, GLFW_CURSOR);
            if (mode == GLFW_CURSOR_NORMAL) {
                // we are aoutside of applicating, catch the cursor
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            }
            else {
                // we are inside our game: shoot, click, etc.
                std::cout << "Bang!\n";
            }
            break;
        }
        case GLFW_MOUSE_BUTTON_RIGHT:
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            break;
        default:
            break;
        }
    }
}

void GLAPIENTRY App::MessageCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam)
{
    auto const src_str = [source]() {
        switch (source)
        {
        case GL_DEBUG_SOURCE_API: return "API";
        case GL_DEBUG_SOURCE_WINDOW_SYSTEM: return "WINDOW SYSTEM";
        case GL_DEBUG_SOURCE_SHADER_COMPILER: return "SHADER COMPILER";
        case GL_DEBUG_SOURCE_THIRD_PARTY: return "THIRD PARTY";
        case GL_DEBUG_SOURCE_APPLICATION: return "APPLICATION";
        case GL_DEBUG_SOURCE_OTHER: return "OTHER";
        default: return "Unknown";
        }
        }();

    auto const type_str = [type]() {
        switch (type)
        {
        case GL_DEBUG_TYPE_ERROR: return "ERROR";
        case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: return "DEPRECATED_BEHAVIOR";
        case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR: return "UNDEFINED_BEHAVIOR";
        case GL_DEBUG_TYPE_PORTABILITY: return "PORTABILITY";
        case GL_DEBUG_TYPE_PERFORMANCE: return "PERFORMANCE";
        case GL_DEBUG_TYPE_MARKER: return "MARKER";
        case GL_DEBUG_TYPE_OTHER: return "OTHER";
        default: return "Unknown";
        }
        }();

    auto const severity_str = [severity]() {
        switch (severity) {
        case GL_DEBUG_SEVERITY_NOTIFICATION: return "NOTIFICATION";
        case GL_DEBUG_SEVERITY_LOW: return "LOW";
        case GL_DEBUG_SEVERITY_MEDIUM: return "MEDIUM";
        case GL_DEBUG_SEVERITY_HIGH: return "HIGH";
        default: return "Unknown";
        }
        }();

    std::cout << "[GL CALLBACK]: " <<
        "source = " << src_str <<
        ", type = " << type_str <<
        ", severity = " << severity_str <<
        ", ID = '" << id << '\'' <<
        ", message = '" << message << '\'' << std::endl;
}


void App::print_opencv_info()
{
    std::cout << "Capture source: "
        //<<  ": width=" << capture.get(cv::CAP_PROP_FRAME_WIDTH)
        //<<  ", height=" << capture.get(cv::CAP_PROP_FRAME_HEIGHT)
        << '\n';
}

void App::print_glfw_info(void)
{
    int major, minor, revision;
    glfwGetVersion(&major, &minor, &revision);
    std::cout << "Running GLFW DLL " << major << '.' << minor << '.' << revision << '\n';
    std::cout << "Compiled against GLFW "
        << GLFW_VERSION_MAJOR << '.' << GLFW_VERSION_MINOR << '.' << GLFW_VERSION_REVISION
        << '\n';
}

void App::print_glm_info()
{
    // GLM library
    std::cout << "GLM version: " << GLM_VERSION_MAJOR << '.' << GLM_VERSION_MINOR << '.' << GLM_VERSION_PATCH << "rev" << GLM_VERSION_REVISION << std::endl;
}

void App::print_gl_info()
{
    // get OpenGL info
    auto vendor_s = reinterpret_cast<const char*>(glGetString(GL_VENDOR));
    std::cout << "OpenGL driver vendor: " << (vendor_s ? vendor_s : "UNKNOWN") << '\n';

    auto renderer_s = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    std::cout << "OpenGL renderer: " << (renderer_s ? renderer_s : "<UNKNOWN>") << '\n';

    auto version_s = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    std::cout << "OpenGL version: " << (version_s ? version_s : "<UNKNOWN>") << '\n';

    auto glsl_s = reinterpret_cast<const char*>(glGetString(GL_SHADING_LANGUAGE_VERSION));
    std::cout << "Primary GLSL shading language version: " << (glsl_s ? glsl_s : "<UNKNOWN>") << std::endl;

    // get GL profile info
    {
        GLint profile_flags;
        glGetIntegerv(GL_CONTEXT_PROFILE_MASK, &profile_flags);
        std::cout << "Current profile: ";
        if (profile_flags & GL_CONTEXT_CORE_PROFILE_BIT)
            std::cout << "CORE";
        else
            std::cout << "COMPATIBILITY";
        std::cout << std::endl;
    }

    // get context flags
    {
        GLint context_flags;
        glGetIntegerv(GL_CONTEXT_FLAGS, &context_flags);
        std::cout << "Active context flags: ";
        if (context_flags & GL_CONTEXT_FLAG_FORWARD_COMPATIBLE_BIT)
            std::cout << "GL_CONTEXT_FLAG_FORWARD_COMPATIBLE_BIT ";
        if (context_flags & GL_CONTEXT_FLAG_DEBUG_BIT)
            std::cout << "GL_CONTEXT_FLAG_DEBUG_BIT ";
        if (context_flags & GL_CONTEXT_FLAG_ROBUST_ACCESS_BIT)
            std::cout << "GL_CONTEXT_FLAG_ROBUST_ACCESS_BIT ";
        if (context_flags & GL_CONTEXT_FLAG_NO_ERROR_BIT)
            std::cout << "GL_CONTEXT_FLAG_NO_ERROR_BIT";
        std::cout << std::endl;
    }

    { // get extension list
        GLint n = 0;
        glGetIntegerv(GL_NUM_EXTENSIONS, &n);
        std::cout << "GL extensions: " << n << '\n';

        //for (GLint i = 0; i < n; i++) {
        //    const char* extension_name = (const char*)glGetStringi(GL_EXTENSIONS, i);
        //    std::cout << extension_name << '\n';
        //}
    }
}


void lossy_quality_limit_thread()
{
    std::scoped_lock lock(mux);
    cv::Mat frame2;
    frame.copyTo(frame2);

    std::string suff(".jpg"); // target format
    if (!cv::haveImageWriter(suff))
        throw std::runtime_error("Can not compress to format:" + suff);
    
    std::vector<int> compression_params;

    // prepare parameters for JPEG compressor
    // we use only quality, but other parameters are available (progressive, optimization...)
    std::vector<int> compression_params_template;
    compression_params_template.push_back(cv::IMWRITE_JPEG_QUALITY);

    //gggg
    //std::cout << '[';

    //try step-by-step to decrease quality by 5%, until it fits into limit
    for (auto i = 0; i < 100; i += 5) {
        compression_params = compression_params_template; // reset parameters
        compression_params.push_back(i);                  // set desired quality

        // try to encode
        cv::imencode(suff, frame, bytes, compression_params);
        
        //gggg
        //std::cout << cv::PSNR(frame, cv::imdecode(bytes, cv::IMREAD_ANYCOLOR)) << ',';

        //if quality is larger: it is our output
        if (cv::PSNR(frame, cv::imdecode(bytes, cv::IMREAD_ANYCOLOR)) >= (target_quality))
            break; // ok, done
    }

    //ggggg
    //std::cout << "]\n";

    processed = true;
    cvvv.notify_one();
}

int App::run(void)
{
    //cv::Mat frame;
    //std::vector<uchar> bytes;
    //float target_coefficient = 35.0f; // used as size-ratio, or quality-coefficient


    try {
        double now = glfwGetTime();
        // FPS related
        double fps_last_displayed = now;
        int fps_counter_frames = 0;
        double FPS = 0.0;

        // animation related
        double frame_begin_timepoint = now;
        double previous_frame_render_time{};
        double time_speed{};

        // Clear color saved to OpenGL state machine: no need to set repeatedly in game loop
        glClearColor(0, 0, 0, 0);

        // Activate shader program. There is only one program, so activation can be out of the loop. 
        // In more realistic scenarios, you will activate different shaders for different 3D objects.
        //glUseProgram(shader_prog_ID);

        // Get uniform location in GPU program. This will not change, so it can be moved out of the game loop.
        /*GLint uniform_color_location = glGetUniformLocation(shader_prog_ID, "ucolor");
        if (uniform_color_location == -1) {
            std::cerr << "Uniform location is not found in active shader program. Did you forget to activate it?\n";
        }*/

        shader.activate();

        //while (capture.isOpened())
        while (!glfwWindowShouldClose(window))
        {
            // ImGui prepare render (only if required)
            if (show_imgui) {
                ImGui_ImplOpenGL3_NewFrame();
                ImGui_ImplGlfw_NewFrame();
                ImGui::NewFrame();

                ImGui::SetNextWindowPos(ImVec2(10, 10));
                ImGui::SetNextWindowSize(ImVec2(400, 400)); // klidnì vìtší pro kameru

                ImGui::Begin("Info", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);

                ImGui::Text("V-Sync: %s", is_vsync_on ? "ON" : "OFF");
                ImGui::Text("FPS: %.1f", FPS);
                ImGui::Text("(press RMB to release mouse)");
                ImGui::Text("(hit I to show/hide info)");
                ImGui::Separator();
                ImGui::SliderFloat("Spotlight intensity", &spotlight_intensity, 0.0f, 1.0f);

                ImGui::Separator();
                ImGui::Text("Kamera:");
                ImGui::Image((ImTextureID)camera_texture, ImVec2(320, 240));


                ImGui::End();
            }


            //
            // UPDATE: recompute object.position = object.position + object.speed * (previous_frame_render_time * time_speed); // s = s0 + v*delta_t
            //
            if (show_imgui) {
                // pause application
                time_speed = 0.0;
            }
            else {
                // imgui not displayed, run app at normal speed
                time_speed = 1.0;
            }

            // Clear OpenGL canvas, both color buffer and Z-buffer
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            //change by time the green part of RGB
            g = std::fabs(std::sinf(now));
            std::clamp(g, 0.0f, 1.0f);


            // 1. Aktivuj shader
            shader.activate();

            // 2. Nastav barvu (už máš)

            // 3. Nastav transformaèní matice
            float angle = (float)glfwGetTime(); // rotace v èase

            glm::mat4 model_matrix = glm::rotate(glm::mat4(1.0f), glm::radians(-20.0f), glm::vec3(1.0f, 0.0f, 0.0f));
            model_matrix = glm::rotate(model_matrix, angle, glm::vec3(0.0f, 1.0f, 0.0f));

            //ZMENA NA FREE
            glm::mat4 view_matrix = glm::lookAt(
                cameraPos,
                cameraPos + cameraFront,
                cameraUp
            );

            // Pro dynamické rozmìry:
            int width, height;
            glfwGetFramebufferSize(window, &width, &height);
            float aspect = (float)width / height;

            glm::mat4 projection_matrix = glm::perspective(
                glm::radians(45.0f),
                aspect,
                0.1f, 100.0f
            );

            // 4. Pošli matice do shaderu
            shader.setUniform("uM_m", model_matrix);
            shader.setUniform("uV_m", view_matrix);
            shader.setUniform("uP_m", projection_matrix);


            // Ambient
            shader.setUniform("ambientColor", glm::vec3(0.1f, 0.1f, 0.1f));

            // Directional light
            shader.setUniform("dirLightDirection", glm::vec3(-0.2f, -1.0f, -0.3f));
            shader.setUniform("dirLightColor", glm::vec3(0.9f));

            // Kamera (pozice a smìr)
            glm::vec3 cameraPos = glm::vec3(0.0f, 0.0f, 3.0f);
            glm::vec3 cameraFront = glm::normalize(glm::vec3(0.0f, 0.0f, -1.0f)); // jednoduchá verze

            // Spotlight
            shader.setUniform("spotPos", cameraPos);
            shader.setUniform("spotDir", cameraFront);

            // Tady rozsvítíme nebo zhasneme reflektor
            if (spotlight_on) {
                shader.setUniform("spotColor", glm::vec3(1.0f) * spotlight_intensity);
            }
            else {
                shader.setUniform("spotColor", glm::vec3(0.0f));
            }


            // Spotlight cutoff úhly
            shader.setUniform("spotCutOff", glm::cos(glm::radians(12.5f)));
            shader.setUniform("spotOuterCutOff", glm::cos(glm::radians(17.5f)));

            // Pro výpoèet zrcadlení
            shader.setUniform("viewPos", cameraPos);

            float deltaTime = 0.0f;  // Èas mezi snímky
            float currentFrame = glfwGetTime();
            deltaTime = currentFrame - lastTime;
            lastTime = currentFrame;

            // Pøidáme nové èástice (napø. na každém snímku)
            particleSystem.addParticle(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3((rand() % 2 ? 1 : -1) * 0.1f, 0.1f, (rand() % 2 ? 1 : -1) * 0.1f), 5.0f);

            // Aktualizujeme všechny èástice
            particleSystem.update(deltaTime);

            // Vykreslíme všechny èástice
            particleSystem.draw();


            for (auto& model : scene) {
                if (model.first == "cube" || model.first == "sphere") {
                    glm::mat4 model_matrix = glm::mat4(1.0f);

                    if (model.first == "cube") {
                        model_matrix = glm::translate(model_matrix, glm::vec3(-2.0f, 0.0f, 0.0f));
                        model_matrix = glm::rotate(model_matrix, angle, glm::vec3(0.0f, 1.0f, 0.0f));
                    }
                    else if (model.first == "sphere") {
                        model_matrix = glm::translate(model_matrix, glm::vec3(2.0f, 0.0f, 0.0f));
                        model_matrix = glm::rotate(model_matrix, angle * 1.5f, glm::vec3(1.0f, 0.0f, 0.0f));
                    }

                    shader.setUniform("uM_m", model_matrix);
                    model.second.draw();
                }
            }

            // Prùhledné objekty nakonec
            {
                glm::mat4 model_matrix = glm::mat4(1.0f);
                model_matrix = glm::translate(model_matrix, glm::vec3(0.0f, 0.0f, 0.0f)); // napø. žádný posun
                model_matrix = glm::scale(model_matrix, glm::vec3(2.0f)); 

                shader.setUniform("uM_m", model_matrix);
                scene["cubealfa"].draw();
            }




            // ImGui display
            if (show_imgui) {
                ImGui::Render();
                ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
            }


            //
            // SWAP + VSYNC
            //
            glfwSwapBuffers(window);

            //
            // POLL
            //
            glfwPollEvents();

            std::lock_guard<std::mutex> lock(mux);
            if (!frame.empty()) {
                cv::Mat frameRGB;
                cv::cvtColor(frame, frameRGB, cv::COLOR_BGR2RGB);

                glBindTexture(GL_TEXTURE_2D, camera_texture);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, frameRGB.cols, frameRGB.rows, 0, GL_RGB, GL_UNSIGNED_BYTE, frameRGB.data);
                glBindTexture(GL_TEXTURE_2D, 0);
            }



            // encode image with bandwidth limit


            //gggg
            //auto size_compressed_limit = size_uncompressed * target_coefficient;
          //
            // Encode single image with limitation by bandwidth
            //
            //bytes = lossy_bw_limit(frame, size_compressed_limit); // returns JPG compressed stream for single image

            //bytes = lossy_quality_limit(frame, target_coefficient);


            //
            // TASK 1: Replace function lossy_bw_limit() - limitation by bandwith - with limitation by quality.
            //         Implement the function:
            // bytes = lossy_quality_limit(frame, target_coefficient);
            // 
            //         Use PSNR (Peak Signal to Noise Ratio)
            //         or  SSIM (Structural Similarity) 
            // (from https://docs.opencv.org/2.4/doc/tutorials/highgui/video-input-psnr-ssim/video-input-psnr-ssim.html#image-similarity-psnr-and-ssim ) 
            //         to estimate quality of the compressed image.
            //


            // display compression ratio
            auto size_compreessed = bytes.size();

            //gggggg
            //std::cout << "Size: uncompressed = " << size_uncompressed << ", compressed = " << size_compreessed << ", = " << size_compreessed / (size_uncompressed / 100.0) << " % \n";


            //
            // decode compressed data
            //  


            //cv::namedWindow("original");
            //cv::imshow("original", frame);

            //cv::namedWindow("decoded");
            //cv::imshow("decoded", decoded_frame);

            // key handling
            /*int c = cv::pollKey();
            switch (c) {
            case 27:
                return EXIT_SUCCESS;
                break;
            case 'q':
                target_coefficient += 1;
                break;
            case 'a':
                target_coefficient -= 1;
                break;
            default:
                break;
            }*/

            target_quality = std::clamp(target_quality, 0.0f, 100.0f);
            //ggggggg
            //std::cout << "Target coeff: " << target_quality << " dB\n";
            //target_quality = target_coefficient;

            now = glfwGetTime();
            previous_frame_render_time = now - frame_begin_timepoint; //compute delta_t
            frame_begin_timepoint = now; // set new start

            fps_counter_frames++;
            if (now - fps_last_displayed >= 1) {
                FPS = fps_counter_frames / (now - fps_last_displayed);
                fps_last_displayed = now;
                fps_counter_frames = 0;
                std::cout << "\r[FPS]" << FPS << "     "; // Compare: FPS with/without ImGUI
            }
        }
    }
    catch (std::exception const& e) {
        std::cerr << "App failed : " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    std::cout << "Finished OK...\n";
    return EXIT_SUCCESS;
}

void App::destroy(void)
{
    // clean up ImGUI
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    // clean up OpenCV
    cv::destroyAllWindows();

    // clean-up GLFW
    if (window) {
        glfwDestroyWindow(window);
        window = nullptr;
    }
    glfwTerminate();
    destroy_audio();

    camera_running = false;
    if (camera_thread.joinable()) {
        camera_thread.join();
    }

}

App::~App()
{
    destroy();

    std::cout << "Bye...\n";
}
