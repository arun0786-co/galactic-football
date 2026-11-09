#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>
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
#include "audio.h"
#include "png.h"

GLFWwindow* g_window = nullptr;

static float get_delta_time() {
    static double last = glfwGetTime();
    double now = glfwGetTime();
    double dt = now - last;
    last = now;
    return static_cast<float>(dt);
}

static bool init_glfw_and_window(int width, int height, bool visible) {
    if (!glfwInit()) {
        std::cerr << "failed to initialize glfw\n";
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_VISIBLE, visible ? GLFW_TRUE : GLFW_FALSE);

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

// one fixed step of the match, in the same order as the game loop. in
// watch mode the set pieces also need the entity update, because that is
// where the ai takes its kickoffs.
static void step_match(float dt) {
    g_game_state.update(dt);
    MatchPhase ph = g_game_state.phase;
    if (ph == MatchPhase::Playing || (g_autoplay && (ph == MatchPhase::Kickoff || ph == MatchPhase::FreeKick))) {
        update_all_entities(dt);
        if (g_game_state.phase == MatchPhase::Playing) apply_physics(dt);
    }
}

// renders one frame into an offscreen framebuffer and saves it as a png
static bool capture_png(const char* path, int w, int h) {
    GLuint fbo, color, depth;
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glGenRenderbuffers(1, &color);
    glBindRenderbuffer(GL_RENDERBUFFER, color);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, w, h);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, color);
    glGenRenderbuffers(1, &depth);
    glBindRenderbuffer(GL_RENDERBUFFER, depth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w, h);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depth);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) return false;

    g_render_w = w;
    g_render_h = h;
    g_draw_hud = false;
    glViewport(0, 0, w, h);
    render_frame();
    glFinish();

    std::vector<uint8_t> px(size_t(w) * h * 3), flipped(px.size());
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, px.data());
    for (int y = 0; y < h; y++)
        std::memcpy(&flipped[size_t(y) * w * 3], &px[size_t(h - 1 - y) * w * 3], size_t(w) * 3);
    return png::write_rgb(path, flipped.data(), w, h);
}

static void usage() {
    std::fprintf(stderr,
        "usage: galactic_football [--watch] [--terrain neon|ice|mud]\n"
        "                         [--aliens brute|speedster|teleporter]\n"
        "       galactic_football --shot out.png [--seconds t] [--camera 0-3]\n"
        "                         [--terrain ...] [--aliens ...] [--size WxH] [--seed s]\n");
}

int main(int argc, char* argv[]) {
    const char* shot = nullptr;
    float seconds = 20.0f;
    int width = 1600, height = 900, camera = 0;
    unsigned seed = 1;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        bool has = i + 1 < argc;
        if (a == "--watch") g_autoplay = true;
        else if (a == "--shot" && has) shot = argv[++i];
        else if (a == "--seconds" && has) seconds = std::atof(argv[++i]);
        else if (a == "--camera" && has) camera = std::atoi(argv[++i]);
        else if (a == "--seed" && has) seed = unsigned(std::atoi(argv[++i]));
        else if (a == "--size" && has) {
            if (std::sscanf(argv[++i], "%dx%d", &width, &height) != 2) { usage(); return EXIT_FAILURE; }
        } else if (a == "--terrain" && has) {
            std::string t = argv[++i];
            if (t == "ice") g_game_state.current_terrain = TerrainType::IcePlanet;
            else if (t == "mud") g_game_state.current_terrain = TerrainType::MudWasteland;
            else g_game_state.current_terrain = TerrainType::NeonGrid;
        } else if (a == "--aliens" && has) {
            std::string t = argv[++i];
            if (t == "speedster") g_game_state.current_alien_type = AlienType::Speedster;
            else if (t == "teleporter") g_game_state.current_alien_type = AlienType::Teleporter;
            else g_game_state.current_alien_type = AlienType::Brute;
        } else { usage(); return EXIT_FAILURE; }
    }
    if (shot) {
        g_autoplay = true;
        g_audio_enabled = false;
        std::cout.rdbuf(nullptr); // the game logs every kickoff and tackle
    }

    if (!init_glfw_and_window(1280, 720, shot == nullptr)) return EXIT_FAILURE;
    if (!init_glew()) return EXIT_FAILURE;

    // setup all our modular systems
    setup_input(g_window);
    init_renderer();
    init_entities(); // sets up the 22 players and the ball
    if (g_autoplay) {
        g_game_state.team_to_kickoff = TEAM_HUMAN;
        g_game_state.reset_match(); // kickoff without a keyboard player
    }

    if (shot) {
        // play the match without drawing, then take one frame
        srandom(seed);
        for (float t = 0.0f; t < seconds; t += 1.0f / 60.0f) step_match(1.0f / 60.0f);
        g_camera_mode = camera;
        bool ok = capture_png(shot, width, height);
        std::fprintf(stderr, ok ? "wrote %s\n" : "could not write %s\n", shot);
        glfwTerminate();
        return ok ? EXIT_SUCCESS : EXIT_FAILURE;
    }

    // imgui setup
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(g_window, true);
    ImGui_ImplOpenGL3_Init("#version 410");

    // main game loop
    while (!glfwWindowShouldClose(g_window)) {
        float dt = get_delta_time();
        
        if (g_autoplay) {
            // watch mode: the ai plays itself
            step_match(dt);
        } else {
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
        }

        // 5. Draw everything
        int fbw, fbh;
        glfwGetFramebufferSize(g_window, &fbw, &fbh);
        glViewport(0, 0, fbw, fbh);
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
