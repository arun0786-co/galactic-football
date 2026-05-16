#include "renderer.h"
#include "entities.h"
#include "game_state.h"
#include "file_utils.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <cmath>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

// section 7.1: renderer state
static GLuint g_vao_cube = 0;
static GLuint g_vbo_cube = 0;
static GLuint g_shader_program = 0;

static GLint g_model_location = -1;
static GLint g_view_location = -1;
static GLint g_proj_location = -1;
static GLint g_color_location = -1;

static Matrix4f g_view_matrix;
static Matrix4f g_proj_matrix;

extern GLFWwindow* g_window; // from main.cpp

int g_camera_mode = 0; // 0=classic, 1=player, 2=top-down, 3=side

const Matrix4f& get_view_matrix() { return g_view_matrix; }
const Matrix4f& get_proj_matrix() { return g_proj_matrix; }

// helpers
static Matrix4f make_translate(float tx, float ty, float tz) {
    Matrix4f t;
    t.InitIdentity();
    t.m[0][3] = tx;
    t.m[1][3] = ty;
    t.m[2][3] = tz;
    return t;
}

static Matrix4f make_srt_3d(float sx, float sy, float sz, float rot_x_deg,
                            float rot_y_deg, float rot_z_deg, float tx,
                            float ty, float tz) {
    Matrix4f s; s.InitScaleTransform(sx, sy, sz);
    Matrix4f r; r.InitRotateTransform(rot_x_deg, rot_y_deg, rot_z_deg);
    Matrix4f t = make_translate(tx, ty, tz);
    return t * r * s;
}

static void set_color(float r, float g, float b, float a = 1.0f) {
    glUniform4f(g_color_location, r, g, b, a);
}

static void upload_model(const Matrix4f& model) {
    glUniformMatrix4fv(g_model_location, 1, GL_TRUE, &model.m[0][0]);
}

// geometry
static void create_cube() {
    // simplified cube with just positions (we don't need textures or normals for the futuristic neon style)
    GLfloat vertices[] = {
        -0.5f,-0.5f, 0.5f,  0.5f,-0.5f, 0.5f,  0.5f, 0.5f, 0.5f, 
        -0.5f,-0.5f, 0.5f,  0.5f, 0.5f, 0.5f, -0.5f, 0.5f, 0.5f,
         0.5f,-0.5f,-0.5f, -0.5f,-0.5f,-0.5f, -0.5f, 0.5f,-0.5f,
         0.5f,-0.5f,-0.5f, -0.5f, 0.5f,-0.5f,  0.5f, 0.5f,-0.5f,
        -0.5f,-0.5f,-0.5f, -0.5f,-0.5f, 0.5f, -0.5f, 0.5f, 0.5f,
        -0.5f,-0.5f,-0.5f, -0.5f, 0.5f, 0.5f, -0.5f, 0.5f,-0.5f,
         0.5f,-0.5f, 0.5f,  0.5f,-0.5f,-0.5f,  0.5f, 0.5f,-0.5f,
         0.5f,-0.5f, 0.5f,  0.5f, 0.5f,-0.5f,  0.5f, 0.5f, 0.5f,
        -0.5f, 0.5f, 0.5f,  0.5f, 0.5f, 0.5f,  0.5f, 0.5f,-0.5f,
        -0.5f, 0.5f, 0.5f,  0.5f, 0.5f,-0.5f, -0.5f, 0.5f,-0.5f,
        -0.5f,-0.5f,-0.5f,  0.5f,-0.5f,-0.5f,  0.5f,-0.5f, 0.5f,
        -0.5f,-0.5f,-0.5f,  0.5f,-0.5f, 0.5f, -0.5f,-0.5f, 0.5f,
    };

    glGenVertexArrays(1, &g_vao_cube);
    glBindVertexArray(g_vao_cube);
    glGenBuffers(1, &g_vbo_cube);
    glBindBuffer(GL_ARRAY_BUFFER, g_vbo_cube);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), nullptr);
    glBindVertexArray(0);
}

static void create_shader() {
    // ultra simple shader: transforms positions, applies flat color.
    // perfect for Tron-like neon aesthetics.
    const char* vs_src = R"(
        #version 410 core
        layout(location = 0) in vec3 pos;
        uniform mat4 gModel;
        uniform mat4 gView;
        uniform mat4 gProj;
        void main() {
            gl_Position = gProj * gView * gModel * vec4(pos, 1.0);
        }
    )";
    
    const char* fs_src = R"(
        #version 410 core
        uniform vec4 uColor;
        out vec4 FragColor;
        void main() {
            FragColor = uColor;
        }
    )";

    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vs_src, nullptr);
    glCompileShader(vs);

    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fs_src, nullptr);
    glCompileShader(fs);

    g_shader_program = glCreateProgram();
    glAttachShader(g_shader_program, vs);
    glAttachShader(g_shader_program, fs);
    glLinkProgram(g_shader_program);

    glUseProgram(g_shader_program);
    g_model_location = glGetUniformLocation(g_shader_program, "gModel");
    g_view_location = glGetUniformLocation(g_shader_program, "gView");
    g_proj_location = glGetUniformLocation(g_shader_program, "gProj");
    g_color_location = glGetUniformLocation(g_shader_program, "uColor");
}

void init_renderer() {
    create_cube();
    create_shader();
}

static void setup_camera() {
    int fbw = 0, fbh = 0;
    glfwGetFramebufferSize(g_window, &fbw, &fbh);
    if (fbw <= 0 || fbh <= 0) return;

    PersProjInfo proj;
    proj.FOV = 60.0f;
    proj.Width = (float)fbw;
    proj.Height = (float)fbh;
    proj.zNear = 0.1f;
    proj.zFar = 300.0f;
    g_proj_matrix.InitPersProjTransform(proj);

    Vector3f eye, target;
    Vector3f up(0.0f, 1.0f, 0.0f);
    
    if (g_camera_mode == 0) {
        // camera follows the ball from above and behind
        eye = Vector3f(g_ball.pos.x - 20.0f, 35.0f, g_ball.pos.z + 20.0f);
        target = Vector3f(g_ball.pos.x, 0.0f, g_ball.pos.z);
        // smooth camera bounding (don't go too far outside pitch)
        eye.x = std::max(-10.0f, std::min(PITCH_LENGTH + 10.0f, eye.x));
        eye.z = std::max(-10.0f, std::min(PITCH_WIDTH + 10.0f, eye.z));
    } else if (g_camera_mode == 1) {
        // Player follow (third person)
        Player& p = g_players[g_active_human_player];
        float rad = p.heading_deg * M_PI / 180.0f;
        eye = Vector3f(p.pos.x - 10.0f * cos(rad), 5.0f, p.pos.z - 10.0f * sin(rad));
        target = Vector3f(p.pos.x + 10.0f * cos(rad), 2.0f, p.pos.z + 10.0f * sin(rad));
    } else if (g_camera_mode == 2) {
        // Top down
        eye = Vector3f(PITCH_LENGTH / 2.0f, 100.0f, PITCH_WIDTH / 2.0f);
        target = Vector3f(PITCH_LENGTH / 2.0f, 0.0f, PITCH_WIDTH / 2.0f);
        up = Vector3f(1.0f, 0.0f, 0.0f); // Up vector must be different for top-down
    } else if (g_camera_mode == 3) {
        // Side TV cam
        eye = Vector3f(g_ball.pos.x, 30.0f, PITCH_WIDTH + 40.0f);
        target = Vector3f(g_ball.pos.x, 0.0f, g_ball.pos.z);
    }

    if (g_game_state.phase == MatchPhase::FreeKick) {
        int attacking_team = (g_ball.owner_id != -1) ? g_players[g_ball.owner_id].team : TEAM_HUMAN;
        float target_goal_x = (attacking_team == TEAM_HUMAN) ? PITCH_LENGTH : 0.0f;
        float target_goal_z = PITCH_WIDTH / 2.0f;
        
        Vector3f to_goal(target_goal_x - g_ball.pos.x, 0.0f, target_goal_z - g_ball.pos.z);
        to_goal.Normalize();
        
        eye = Vector3f(g_ball.pos.x - to_goal.x * 6.0f, 4.0f, g_ball.pos.z - to_goal.z * 6.0f);
        target = Vector3f(target_goal_x, 2.0f, target_goal_z);
    }

    Vector3f dir(target.x - eye.x, target.y - eye.y, target.z - eye.z);
    dir.Normalize();

    Matrix4f cam_rotate; cam_rotate.InitCameraTransform(dir, up);
    Matrix4f cam_translate = make_translate(-eye.x, -eye.y, -eye.z);
    g_view_matrix = cam_rotate * cam_translate;

    glUniformMatrix4fv(g_view_location, 1, GL_TRUE, &g_view_matrix.m[0][0]);
    glUniformMatrix4fv(g_proj_location, 1, GL_TRUE, &g_proj_matrix.m[0][0]);
}

static void draw_pitch() {
    // floor color based on terrain
    if (g_game_state.current_terrain == TerrainType::NeonGrid) set_color(0.05f, 0.05f, 0.1f);
    else if (g_game_state.current_terrain == TerrainType::IcePlanet) set_color(0.7f, 0.9f, 1.0f);
    else set_color(0.3f, 0.2f, 0.1f); // mud

    Matrix4f floor = make_srt_3d(PITCH_LENGTH, 0.1f, PITCH_WIDTH, 0, 0, 0, PITCH_LENGTH/2.0f, -0.05f, PITCH_WIDTH/2.0f);
    upload_model(floor);
    glDrawArrays(GL_TRIANGLES, 0, 36);

    // neon grid lines
    set_color(0.0f, 1.0f, 1.0f); // bright cyan
    // center line
    Matrix4f cl = make_srt_3d(0.2f, 0.15f, PITCH_WIDTH, 0, 0, 0, PITCH_LENGTH/2.0f, 0.0f, PITCH_WIDTH/2.0f);
    upload_model(cl);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    // borders
    Matrix4f b1 = make_srt_3d(PITCH_LENGTH, 0.15f, 0.2f, 0, 0, 0, PITCH_LENGTH/2.0f, 0.0f, 0.0f);
    upload_model(b1); glDrawArrays(GL_TRIANGLES, 0, 36);
    Matrix4f b2 = make_srt_3d(PITCH_LENGTH, 0.15f, 0.2f, 0, 0, 0, PITCH_LENGTH/2.0f, 0.0f, PITCH_WIDTH);
    upload_model(b2); glDrawArrays(GL_TRIANGLES, 0, 36);
    Matrix4f b3 = make_srt_3d(0.2f, 0.15f, PITCH_WIDTH, 0, 0, 0, 0.0f, 0.0f, PITCH_WIDTH/2.0f);
    upload_model(b3); glDrawArrays(GL_TRIANGLES, 0, 36);
    Matrix4f b4 = make_srt_3d(0.2f, 0.15f, PITCH_WIDTH, 0, 0, 0, PITCH_LENGTH, 0.0f, PITCH_WIDTH/2.0f);
    upload_model(b4); glDrawArrays(GL_TRIANGLES, 0, 36);
    
    // goals
    set_color(1.0f, 1.0f, 1.0f); // white posts
    float cz = PITCH_WIDTH/2.0f;
    float hw = GOAL_WIDTH/2.0f;
    // Left Goal
    // Posts
    upload_model(make_srt_3d(0.2f, GOAL_HEIGHT, 0.2f, 0,0,0, 0, GOAL_HEIGHT/2.0f, cz - hw)); glDrawArrays(GL_TRIANGLES, 0, 36);
    upload_model(make_srt_3d(0.2f, GOAL_HEIGHT, 0.2f, 0,0,0, 0, GOAL_HEIGHT/2.0f, cz + hw)); glDrawArrays(GL_TRIANGLES, 0, 36);
    // Crossbar
    upload_model(make_srt_3d(0.2f, 0.2f, GOAL_WIDTH, 0,0,0, 0, GOAL_HEIGHT, cz)); glDrawArrays(GL_TRIANGLES, 0, 36);
    // Net back
    set_color(0.3f, 0.3f, 0.3f);
    upload_model(make_srt_3d(0.1f, GOAL_HEIGHT, GOAL_WIDTH, 0,0,0, -GOAL_DEPTH, GOAL_HEIGHT/2.0f, cz)); glDrawArrays(GL_TRIANGLES, 0, 36);
    // Net sides
    upload_model(make_srt_3d(GOAL_DEPTH, GOAL_HEIGHT, 0.1f, 0,0,0, -GOAL_DEPTH/2.0f, GOAL_HEIGHT/2.0f, cz - hw)); glDrawArrays(GL_TRIANGLES, 0, 36);
    upload_model(make_srt_3d(GOAL_DEPTH, GOAL_HEIGHT, 0.1f, 0,0,0, -GOAL_DEPTH/2.0f, GOAL_HEIGHT/2.0f, cz + hw)); glDrawArrays(GL_TRIANGLES, 0, 36);

    // Right Goal
    set_color(1.0f, 1.0f, 1.0f);
    upload_model(make_srt_3d(0.2f, GOAL_HEIGHT, 0.2f, 0,0,0, PITCH_LENGTH, GOAL_HEIGHT/2.0f, cz - hw)); glDrawArrays(GL_TRIANGLES, 0, 36);
    upload_model(make_srt_3d(0.2f, GOAL_HEIGHT, 0.2f, 0,0,0, PITCH_LENGTH, GOAL_HEIGHT/2.0f, cz + hw)); glDrawArrays(GL_TRIANGLES, 0, 36);
    upload_model(make_srt_3d(0.2f, 0.2f, GOAL_WIDTH, 0,0,0, PITCH_LENGTH, GOAL_HEIGHT, cz)); glDrawArrays(GL_TRIANGLES, 0, 36);
    set_color(0.3f, 0.3f, 0.3f);
    upload_model(make_srt_3d(0.1f, GOAL_HEIGHT, GOAL_WIDTH, 0,0,0, PITCH_LENGTH + GOAL_DEPTH, GOAL_HEIGHT/2.0f, cz)); glDrawArrays(GL_TRIANGLES, 0, 36);
    upload_model(make_srt_3d(GOAL_DEPTH, GOAL_HEIGHT, 0.1f, 0,0,0, PITCH_LENGTH + GOAL_DEPTH/2.0f, GOAL_HEIGHT/2.0f, cz - hw)); glDrawArrays(GL_TRIANGLES, 0, 36);
    upload_model(make_srt_3d(GOAL_DEPTH, GOAL_HEIGHT, 0.1f, 0,0,0, PITCH_LENGTH + GOAL_DEPTH/2.0f, GOAL_HEIGHT/2.0f, cz + hw)); glDrawArrays(GL_TRIANGLES, 0, 36);
}

static void draw_stickman(const Player& p) {
    // color based on team
    if (p.team == TEAM_HUMAN) {
        if (p.id == g_active_human_player) set_color(1.0f, 1.0f, 0.0f); // yellow active
        else set_color(0.0f, 0.8f, 1.0f); // blue humans
    } else {
        if (g_game_state.current_alien_type == AlienType::Brute) set_color(1.0f, 0.0f, 0.0f); // red brutes
        else if (g_game_state.current_alien_type == AlienType::Speedster) set_color(1.0f, 0.0f, 1.0f); // purple speedsters
        else set_color(0.0f, 1.0f, 0.0f); // green aliens
    }

    Matrix4f base_rot; base_rot.InitRotateTransform(0.0f, p.heading_deg, 0.0f);
    Matrix4f base_pos = make_translate(p.pos.x, p.pos.y, p.pos.z);
    Matrix4f world = base_pos * base_rot;

    // body (torso)
    Matrix4f torso_s; torso_s.InitScaleTransform(0.3f, 1.0f, 0.2f);
    Matrix4f torso_t = make_translate(0.0f, 1.5f, 0.0f);
    upload_model(world * torso_t * torso_s);
    glDrawArrays(GL_TRIANGLES, 0, 36);

    // head
    Matrix4f head_s; head_s.InitScaleTransform(0.4f, 0.4f, 0.4f);
    Matrix4f head_t = make_translate(0.0f, 2.2f, 0.0f);
    upload_model(world * head_t * head_s);
    glDrawArrays(GL_TRIANGLES, 0, 36);

    // hierarchical limbs (sine wave based animation from entities.cpp)
    Matrix4f limb_s; limb_s.InitScaleTransform(0.15f, 0.8f, 0.15f);
    
    // left leg
    Matrix4f ll_rot; ll_rot.InitRotateTransform(0.0f, 0.0f, p.anim.left_leg_angle);
    Matrix4f ll_t = make_translate(-0.1f, 0.4f, 0.0f);
    upload_model(world * ll_rot * ll_t * limb_s);
    glDrawArrays(GL_TRIANGLES, 0, 36);

    // right leg
    Matrix4f rl_rot; rl_rot.InitRotateTransform(0.0f, 0.0f, p.anim.right_leg_angle);
    Matrix4f rl_t = make_translate(0.1f, 0.4f, 0.0f);
    upload_model(world * rl_rot * rl_t * limb_s);
    glDrawArrays(GL_TRIANGLES, 0, 36);

    // left arm
    Matrix4f la_rot; la_rot.InitRotateTransform(0.0f, 0.0f, p.anim.left_arm_angle);
    Matrix4f la_t = make_translate(-0.25f, 1.4f, 0.0f);
    upload_model(world * la_rot * la_t * limb_s);
    glDrawArrays(GL_TRIANGLES, 0, 36);

    // right arm
    Matrix4f ra_rot; ra_rot.InitRotateTransform(0.0f, 0.0f, p.anim.right_arm_angle);
    Matrix4f ra_t = make_translate(0.25f, 1.4f, 0.0f);
    upload_model(world * ra_rot * ra_t * limb_s);
    glDrawArrays(GL_TRIANGLES, 0, 36);
}

static void draw_ball() {
    set_color(1.0f, 1.0f, 1.0f); // bright white glowing ball
    float r = g_ball.radius;
    
    // Simulate ball rolling by rotating based on position
    float roll_x_deg = g_ball.pos.z / r * (180.0f / M_PI);
    float roll_z_deg = -g_ball.pos.x / r * (180.0f / M_PI);
    
    Matrix4f ball_model = make_srt_3d(r*2, r*2, r*2, roll_x_deg, 0.0f, roll_z_deg, g_ball.pos.x, g_ball.pos.y, g_ball.pos.z);
    upload_model(ball_model);
    glDrawArrays(GL_TRIANGLES, 0, 36);
}

static void draw_spaceships(float time) {
    // Mothership (complex)
    float x1 = PITCH_LENGTH/2.0f + 50.0f * cos(time * 0.5f);
    float z1 = PITCH_WIDTH/2.0f + 50.0f * sin(time * 0.5f);
    Matrix4f base_rot; base_rot.InitRotateTransform(0, time*30.0f, 0);
    Matrix4f base_pos = make_translate(x1, 40.0f, z1);
    Matrix4f m_world = base_pos * base_rot;
    
    // Hull
    set_color(0.5f, 0.5f, 0.6f);
    Matrix4f hull_s; hull_s.InitScaleTransform(20.0f, 4.0f, 10.0f);
    upload_model(m_world * hull_s);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    // Cockpit
    set_color(0.2f, 0.8f, 1.0f);
    Matrix4f cock_s; cock_s.InitScaleTransform(8.0f, 3.0f, 6.0f);
    Matrix4f cock_t = make_translate(0.0f, 3.5f, 0.0f);
    upload_model(m_world * cock_t * cock_s);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    // Wings
    set_color(0.4f, 0.4f, 0.5f);
    Matrix4f w1_s; w1_s.InitScaleTransform(6.0f, 1.0f, 12.0f);
    Matrix4f w1_t = make_translate(0.0f, 0.0f, 10.0f);
    upload_model(m_world * w1_t * w1_s);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    Matrix4f w2_t = make_translate(0.0f, 0.0f, -10.0f);
    upload_model(m_world * w2_t * w1_s);
    glDrawArrays(GL_TRIANGLES, 0, 36);
}

static void draw_viewers() {
    // simple blocks along the sidelines representing stands
    set_color(0.5f, 0.1f, 0.1f);
    Matrix4f stand1 = make_srt_3d(PITCH_LENGTH, 5.0f, 5.0f, 0, 0, 0, PITCH_LENGTH/2.0f, 2.5f, -10.0f);
    upload_model(stand1); glDrawArrays(GL_TRIANGLES, 0, 36);
    Matrix4f stand2 = make_srt_3d(PITCH_LENGTH, 5.0f, 5.0f, 0, 0, 0, PITCH_LENGTH/2.0f, 2.5f, PITCH_WIDTH + 10.0f);
    upload_model(stand2); glDrawArrays(GL_TRIANGLES, 0, 36);
    
    // Add little cubes as heads
    set_color(1.0f, 0.8f, 0.6f); // human skin tones
    for (int i=0; i<20; i++) {
        float x = (PITCH_LENGTH / 20.0f) * i + 2.0f;
        Matrix4f head = make_srt_3d(0.5f, 0.5f, 0.5f, 0, 0, 0, x, 5.5f, -9.0f);
        upload_model(head); glDrawArrays(GL_TRIANGLES, 0, 36);
    }
    
    set_color(0.0f, 1.0f, 0.0f); // alien viewers
    for (int i=0; i<20; i++) {
        float x = (PITCH_LENGTH / 20.0f) * i + 2.0f;
        Matrix4f head = make_srt_3d(0.5f, 0.5f, 0.5f, 0, 0, 0, x, 5.5f, PITCH_WIDTH + 9.0f);
        upload_model(head); glDrawArrays(GL_TRIANGLES, 0, 36);
    }
}

static void draw_floodlights() {
    float positions[4][2] = {
        {-5.0f, -5.0f},
        {PITCH_LENGTH + 5.0f, -5.0f},
        {-5.0f, PITCH_WIDTH + 5.0f},
        {PITCH_LENGTH + 5.0f, PITCH_WIDTH + 5.0f}
    };
    
    for (int i=0; i<4; i++) {
        // Pole
        set_color(0.2f, 0.2f, 0.2f);
        upload_model(make_srt_3d(1.0f, 30.0f, 1.0f, 0,0,0, positions[i][0], 15.0f, positions[i][1]));
        glDrawArrays(GL_TRIANGLES, 0, 36);
        // Light box
        set_color(1.0f, 1.0f, 0.8f); // Bright yellow-white light
        // Aiming inwards
        float rot_y = 0.0f;
        if (i == 0) rot_y = 45.0f;
        else if (i == 1) rot_y = -45.0f;
        else if (i == 2) rot_y = 135.0f;
        else if (i == 3) rot_y = -135.0f;
        
        Matrix4f l_rot; l_rot.InitRotateTransform(0.0f, rot_y, 0.0f);
        Matrix4f l_tilt; l_tilt.InitRotateTransform(30.0f, 0.0f, 0.0f); // Tilt down
        Matrix4f l_pos = make_translate(positions[i][0], 31.0f, positions[i][1]);
        Matrix4f l_scale; l_scale.InitScaleTransform(4.0f, 2.0f, 4.0f);
        
        upload_model(l_pos * l_rot * l_tilt * l_scale);
        glDrawArrays(GL_TRIANGLES, 0, 36);
    }
}

static void draw_hud() {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGui::Begin("Match Info");
    ImGui::Text("SCORE: Humans %d - %d Aliens", g_game_state.score_human, g_game_state.score_alien);
    
    int minutes = (int)(g_game_state.match_timer) / 60;
    int seconds = (int)(g_game_state.match_timer) % 60;
    ImGui::Text("Time: %02d:%02d", minutes, seconds);

    const char* phase_str = "Playing";
    if (g_game_state.phase == MatchPhase::Kickoff) phase_str = "Kickoff";
    else if (g_game_state.phase == MatchPhase::GoalScored) phase_str = "GOAL SCORED!";
    else if (g_game_state.phase == MatchPhase::GameOver) phase_str = "Full Time";
    ImGui::Text("Phase: %s", phase_str);

    ImGui::Separator();
    ImGui::Text("Controls:");
    ImGui::Text("Arrows: Move Active Player");
    ImGui::Text("X: Pass, W: Through Ball");
    ImGui::Text("A: Shoot, D: Loft/Tackle");
    ImGui::Text("C: Dash, Z: Skill Move");
    ImGui::Text("Q: Switch Player");
    ImGui::Text("1-4: Switch Camera");
    
    if (g_game_state.referee_timer > 0.0f) {
        ImGui::Separator();
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "%s", g_game_state.referee_message.c_str());
    }
    
    // Power Bar
    if (g_game_state.is_charging) {
        ImGui::Separator();
        ImGui::Text("Power:");
        float charge_ratio = std::min(g_game_state.action_charge_time / 1.5f, 1.0f);
        ImGui::ProgressBar(charge_ratio, ImVec2(0.0f, 0.0f));
    }

    ImGui::End();

    // Game Over Pop-up
    if (g_game_state.phase == MatchPhase::GameOver) {
        // Center the window
        int width, height;
        glfwGetWindowSize(g_window, &width, &height);
        ImGui::SetNextWindowPos(ImVec2(width * 0.5f - 150.0f, height * 0.5f - 100.0f));
        ImGui::SetNextWindowSize(ImVec2(300.0f, 200.0f));
        
        ImGui::Begin("Full Time!", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
        ImGui::Text("Final Score");
        ImGui::Text("Human: %d - Alien: %d", g_game_state.score_human, g_game_state.score_alien);
        
        if (g_game_state.score_human > g_game_state.score_alien) {
            ImGui::TextColored(ImVec4(0.0f, 1.0f, 0.0f, 1.0f), "HUMANITY WINS!");
        } else if (g_game_state.score_alien > g_game_state.score_human) {
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "ALIENS WIN!");
        } else {
            ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), "MATCH DRAWN!");
        }
        
        ImGui::Separator();
        if (ImGui::Button("Restart Match", ImVec2(280.0f, 50.0f))) {
            g_game_state.reset_match();
        }
        ImGui::End();
    }

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void render_frame() {
    glClearColor(0.02f, 0.02f, 0.05f, 1.0f); // dark sky
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(g_shader_program);
    setup_camera();

    glBindVertexArray(g_vao_cube);

    if (g_game_state.is_charging && g_game_state.phase == MatchPhase::Playing) {
        Player& p = g_players[g_active_human_player];
        float charge_ratio = std::min(g_game_state.action_charge_time / 1.5f, 1.0f);
        float line_len = 2.0f + 10.0f * charge_ratio;
        float aim_rad = g_game_state.aim_angle_deg * M_PI / 180.0f;
        
        set_color(1.0f, 1.0f, 0.0f); // Yellow line
        Matrix4f line = make_srt_3d(line_len, 0.1f, 0.1f, 0, -g_game_state.aim_angle_deg, 0, 
                                    p.pos.x + cos(aim_rad)*line_len/2.0f, 0.1f, p.pos.z + sin(aim_rad)*line_len/2.0f);
        upload_model(line);
        glDrawArrays(GL_TRIANGLES, 0, 36);
    }

    draw_pitch();
    draw_viewers();
    draw_floodlights();
    draw_spaceships(g_game_state.match_timer);
    draw_ball();
    for (int i = 0; i < 22; i++) {
        draw_stickman(g_players[i]);
    }

    glBindVertexArray(0);

    draw_hud();
}
