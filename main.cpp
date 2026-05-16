#include <iostream>
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include "game_state.h"
#include "entities.h"
#include "physics.h"
#include "input.h"
#include "renderer.h"

GLFWwindow* g_window = nullptr;

static float get_delta_time() {
    static double last = glfwGetTime();
    double now = glfwGetTime();
    double dt = now - last;
    last = now;
    return static_cast<float>(dt);
}

static bool init_glfw_and_window(int width, int height) {
    if (!glfwInit()) {
        std::cerr << "failed to initialize glfw\n";
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    g_window = glfwCreateWindow(width, height, "11v11 Stickman vs Aliens", nullptr, nullptr);
    if (!g_window) {
        std::cerr << "failed to create window\n";
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(g_window);
    return true;
}

static bool init_glew() {
    glewExperimental = GL_TRUE;
    GLenum err = glewInit();
    if (err != GLEW_OK) {
        std::cerr << "failed to initialize glew\n";
        return false;
    }
    glEnable(GL_DEPTH_TEST);
    return true;
}

int main(int argc, char* argv[]) {
    (void)argc;
    (void)argv;

    if (!init_glfw_and_window(1280, 720)) return EXIT_FAILURE;
    if (!init_glew()) return EXIT_FAILURE;

    // setup all our modular systems
    setup_input(g_window);
    init_renderer();
    init_entities(); // sets up the 22 players and the ball

    // imgui setup
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(g_window, true);
    ImGui_ImplOpenGL3_Init("#version 410");

    // main game loop
    while (!glfwWindowShouldClose(g_window)) {
        float dt = get_delta_time();
        
        // 1. Process keyboard actions
        process_input_frame(dt);
        
        // 2. High-level match state (timers, score)
        g_game_state.update(dt);
        
        if (g_game_state.phase == MatchPhase::Playing) {
            // 3. Move players and ball
            update_all_entities(dt);
            
            // 4. Resolve collisions and boundaries
            apply_physics(dt);
        }

        // 5. Draw everything
        render_frame();

        glfwSwapBuffers(g_window);
        glfwPollEvents();
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    
    // Kill any lingering audio processes on macOS
    std::system("killall afplay > /dev/null 2>&1");
    
    glfwTerminate();

    return EXIT_SUCCESS;
}
