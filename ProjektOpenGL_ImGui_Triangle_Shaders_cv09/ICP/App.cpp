#include <iostream>
#include <chrono>
#include <numeric>
#include <string>
#include <filesystem>
#include <algorithm>
#include <random>
#include <thread>

#include <opencv2/opencv.hpp>

#include <glm/glm.hpp>
#include <glm/ext.hpp>

#include <opencv2/opencv.hpp>
#include <GL/glew.h> 
#include <GL/wglew.h> 
#include <GLFW/glfw3.h>
#include <glm/gtc/type_ptr.hpp>

#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>

#include "App.hpp"
#include "headers.hpp"
#include "assets.hpp"
#include "Model.h"

std::mutex mux;
std::condition_variable cvvv;
cv::Mat frame;
float target_quality = 35;
std::vector<uchar> bytes;
bool ready = false;
bool processed = false;
bool show_imgui = false;


App::App()
{
    // default constructor
    // nothing to do here (so far...)
    std::cout << "Constructed...\n";
}

void App::init_opencv()
{
    // ...
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

        init_gl_debug();

        print_opencv_info();
        print_glfw_info();
        print_gl_info();
        print_glm_info();

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

        capture = cv::VideoCapture(0, cv::CAP_ANY);
        if (!capture.isOpened())
            throw std::runtime_error("Can not open camera :(");

        std::cout << "Cam opened successfully.\n";

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
        glEnable(GL_DEBUG_OUTPUT);
        std::cout << "GL_DEBUG enabled." << std::endl;

        // filter notification noise...
        //glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr, GL_FALSE);
    }
}

void App::init_assets(void) {
    //
    // Initialize pipeline: compile, link and use shaders
    //

    //SHADERS - define & compile & link
    const char* vertex_shader =
        "#version 460 core\n"
        "in vec3 attribute_Position;"
        "void main() {"
        "  gl_Position = vec4(attribute_Position, 1.0);"
        "}";

    const char* fragment_shader =
        "#version 460 core\n"
        "uniform vec4 uniform_Color;"
        "out vec4 FragColor;"
        "void main() {"
        "  FragColor = uniform_Color;"
        "}";

    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vertex_shader, NULL);
    glCompileShader(vs);

    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fragment_shader, NULL);
    glCompileShader(fs);

    shader_prog_ID = glCreateProgram();
    glAttachShader(shader_prog_ID, fs);
    glAttachShader(shader_prog_ID, vs);
    glLinkProgram(shader_prog_ID);

    //now we can delete shader parts (they can be reused, if you have more shaders)
    //the final shader program already linked and stored separately
    glDeleteShader(vs);
    glDeleteShader(fs);

    // 
    // Create and load data into GPU using OpenGL DSA (Direct State Access)
    //

    // Create VAO + data description (just envelope, or container...)
    glCreateVertexArrays(1, &VAO_ID);

    GLint position_attrib_location = glGetAttribLocation(shader_prog_ID, "attribute_Position");

    glEnableVertexArrayAttrib(VAO_ID, position_attrib_location);
    glVertexArrayAttribFormat(VAO_ID, position_attrib_location, 3, GL_FLOAT, GL_FALSE, offsetof(vertex, position));
    glVertexArrayAttribBinding(VAO_ID, position_attrib_location, 0); // (GLuint vaobj, GLuint attribindex, GLuint bindingindex)

    // Create and fill data
    glCreateBuffers(1, &VBO_ID);
    glNamedBufferData(VBO_ID, triangle_vertices.size() * sizeof(vertex), triangle_vertices.data(), GL_STATIC_DRAW);

    // Connect together
    glVertexArrayVertexBuffer(VAO_ID, 0, VBO_ID, 0, sizeof(vertex)); // (GLuint vaobj, GLuint bindingindex, GLuint buffer, GLintptr offset, GLsizei stride)
}

void App::glfw_error_callback(int error, const char* description)
{
    std::cerr << "GLFW error: " << description << std::endl;
}

void App::glfw_key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
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
    auto this_inst = static_cast<App*>(glfwGetWindowUserPointer(window));
    if ((action == GLFW_PRESS) || (action == GLFW_REPEAT)) {
        switch (key) {
        case GLFW_KEY_ESCAPE:
            // Exit The App
            glfwSetWindowShouldClose(window, GLFW_TRUE);
            break;
        case GLFW_KEY_Q:
            target_quality += 1;
            break;
        case GLFW_KEY_A:
            target_quality -= 1;
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
        case GLFW_KEY_D:
            this_inst->show_imgui = !this_inst->show_imgui;
            break;
        default:
            break;
        }
    }
}

void App::glfw_scroll_callback(GLFWwindow* window, double xoffset, double yoffset) {
}

void App::glfw_framebuffer_size_callback(GLFWwindow* window, int width, int height) {
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

/*
* main function from Jenicek
*  -> transformed to lossy_quality_limit
*/
std::vector<uchar> App::lossy_bw_limit(cv::Mat& input_img, size_t size_limit)
{
    std::string suff(".jpg"); // target format
    if (!cv::haveImageWriter(suff))
        throw std::runtime_error("Can not compress to format:" + suff);

    std::vector<uchar> bytes;
    std::vector<int> compression_params;

    // prepare parameters for JPEG compressor
    // we use only quality, but other parameters are available (progressive, optimization...)
    std::vector<int> compression_params_template;
    compression_params_template.push_back(cv::IMWRITE_JPEG_QUALITY); 

    std::cout << '[';

    //try step-by-step to decrease quality by 5%, until it fits into limit
    for (auto i = 100; i > 0; i -= 5) {
        compression_params = compression_params_template; // reset parameters
        compression_params.push_back(i);                  // set desired quality
        std::cout << i << ',';

        // try to encode
        cv::imencode(suff, input_img, bytes, compression_params);

        // check the size limit
        if (bytes.size() <= size_limit)
            break; // ok, done
    }
    std::cout << "]\n";
    //std::cout << "DB: " << cv::PSNR(input_img, cv::imdecode(bytes, cv::IMREAD_ANYCOLOR)) << " %\n";

    return bytes;
}
/*
* the middle before main function and thread function
* -> transformed to lossy_quality_limit_thread
*/
std::vector<uchar> lossy_quality_limit(cv::Mat& input_img, float target_quality) 
{
    std::string suff(".jpg"); // target format
    if (!cv::haveImageWriter(suff))
        throw std::runtime_error("Can not compress to format:" + suff);

    std::vector<uchar> bytes;
    std::vector<int> compression_params;

    // prepare parameters for JPEG compressor
    // we use only quality, but other parameters are available (progressive, optimization...)
    std::vector<int> compression_params_template;
    compression_params_template.push_back(cv::IMWRITE_JPEG_QUALITY);

    std::cout << '[';

    //try step-by-step to decrease quality by 5%, until it fits into limit
    for (auto i = 0; i < 100; i += 5) {
        compression_params = compression_params_template; // reset parameters
        compression_params.push_back(i);                  // set desired quality

        // try to encode
        cv::imencode(suff, input_img, bytes, compression_params);
        std::cout << cv::PSNR(input_img, cv::imdecode(bytes, cv::IMREAD_ANYCOLOR)) << ',';

        //if quality is larger: it is our output
        if (cv::PSNR(input_img, cv::imdecode(bytes, cv::IMREAD_ANYCOLOR)) >= (target_quality))
            break; // ok, done
    }
    std::cout << "]\n";
    //std::cout << "DB: " << cv::PSNR(input_img, cv::imdecode(bytes, cv::IMREAD_ANYCOLOR)) << " %\n";

    return bytes;
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
        glUseProgram(shader_prog_ID);

        // Get uniform location in GPU program. This will not change, so it can be moved out of the game loop.
        GLint uniform_color_location = glGetUniformLocation(shader_prog_ID, "uniform_Color");
        if (uniform_color_location == -1) {
            std::cerr << "Uniform location is not found in active shader program. Did you forget to activate it?\n";
        }

        //while (capture.isOpened())
        while (!glfwWindowShouldClose(window))
        {
            // ImGui prepare render (only if required)
            if (show_imgui) {
                ImGui_ImplOpenGL3_NewFrame();
                ImGui_ImplGlfw_NewFrame();
                ImGui::NewFrame();
                //ImGui::ShowDemoWindow(); // Enable mouse when using Demo!
                ImGui::SetNextWindowPos(ImVec2(10, 10));
                ImGui::SetNextWindowSize(ImVec2(250, 100));

                ImGui::Begin("Info", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
                ImGui::Text("V-Sync: %s", is_vsync_on ? "ON" : "OFF");
                ImGui::Text("FPS: %.1f", FPS);
                ImGui::Text("(press RMB to release mouse)");
                ImGui::Text("(hit D to show/hide info)");
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

            //r and b part change by r: 7 and 4, b: 8 and 5 on numpad in func: glfw_key_callback

            //set uniform parameter for shader
            // (try to change the color in key callback)          
            glUniform4f(uniform_color_location, r, g, b, a);

            //bind 3d object data
            glBindVertexArray(VAO_ID);

            // draw all VAO data
            //glDrawArrays(GL_TRIANGLES, 0, triangle_vertices.size());
            std::filesystem::path p("D:\\OneDrive\\Skola\\ICP_2023_24\\projekty\\ProjektOpenGL_ImGui_Triangle_Shaders_cv09\\ICP\\obj\\bunny_tri_vn.obj");
            Model m(p, shader_prog_ID);
            m.draw();

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


            capture >> frame;

            if (frame.empty()) {
                std::cerr << "device closed (or video at the end)" << '\n';
                capture.release();
                break;
            }

            // encode image with bandwidth limit
            auto size_uncompressed = frame.elemSize() * frame.total();

            //gggg
            //auto size_compressed_limit = size_uncompressed * target_coefficient;

            auto start_compresssion = std::chrono::high_resolution_clock::now();
            //
            // Encode single image with limitation by bandwidth
            //
            //bytes = lossy_bw_limit(frame, size_compressed_limit); // returns JPG compressed stream for single image

            //bytes = lossy_quality_limit(frame, target_coefficient);
            std::thread worker(lossy_quality_limit_thread);

            {
                std::lock_guard<std::mutex> lk(mux);
                ready = true;
            }
            cvvv.notify_one();

            {
                std::unique_lock<std::mutex> lock(mux);
                cvvv.wait(lock, [] {return processed; });
                //processed = false;
            }

            worker.join();

            auto stop_compression = std::chrono::high_resolution_clock::now();
            
            auto duration_compression = std::chrono::duration_cast<std::chrono::microseconds>(stop_compression - start_compresssion);

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

            auto start_decompresssion = std::chrono::high_resolution_clock::now();
            //
            // decode compressed data
            //  
            cv::Mat decoded_frame = cv::imdecode(bytes, cv::IMREAD_ANYCOLOR);
            auto stop_decompresssion = std::chrono::high_resolution_clock::now();

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
}

App::~App()
{
    destroy();

    std::cout << "Bye...\n";
}
