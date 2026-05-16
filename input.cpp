#include "input.h"
#include "entities.h"
#include "game_state.h"
#include "renderer.h"
#include "audio.h"
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include <cmath>

// track keys locally to support chords
static bool keys[1024];

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, GL_TRUE);
    }
    
    if (key >= 0 && key < 1024) {
        if (action == GLFW_PRESS) {
            keys[key] = true;
            // Handle camera modes
            if (key == GLFW_KEY_1) g_camera_mode = 0;
            if (key == GLFW_KEY_2) g_camera_mode = 1;
            if (key == GLFW_KEY_3) g_camera_mode = 2;
            if (key == GLFW_KEY_4) g_camera_mode = 3;
        } else if (action == GLFW_RELEASE) {
            keys[key] = false;
        }
    }
    
    // complex action triggers happen on PRESS
    if (action == GLFW_PRESS) {
        if (g_game_state.phase == MatchPhase::Kickoff || g_game_state.phase == MatchPhase::FreeKick) {
            if (key == GLFW_KEY_X || key == GLFW_KEY_W || key == GLFW_KEY_A || key == GLFW_KEY_D) {
                g_game_state.phase = MatchPhase::Playing;
                std::cout << "Play Resumed!\n";
            }
        }
        
        if (g_game_state.phase == MatchPhase::Playing || g_game_state.phase == MatchPhase::Kickoff || g_game_state.phase == MatchPhase::FreeKick) {
            Player& p = g_players[g_active_human_player];
            
            // Q: Switch Player (find closest human to the ball)
            if (key == GLFW_KEY_Q) {
                float min_dist = 9999.0f;
                int best_id = g_active_human_player;
                for (int i = 0; i < 11; i++) {
                    if (i == 0) continue; // skip goalie
                    float dist = sqrt(pow(g_players[i].pos.x - g_ball.pos.x, 2) + pow(g_players[i].pos.z - g_ball.pos.z, 2));
                    if (dist < min_dist) {
                        min_dist = dist;
                        best_id = i;
                    }
                }
                g_active_human_player = best_id;
                std::cout << "Switched to player " << best_id << "\n";
            }
            
            // Actions with the ball
            if (g_ball.owner_id == g_active_human_player) {
                if (key == GLFW_KEY_Z) {
                    // Z: Skill Move (Dash / Speed Burst)
                    std::cout << "Skill Move Dash!\n";
                    float rad = p.heading_deg * M_PI / 180.0f;
                    // Massive speed burst
                    p.vel.x += 20.0f * cos(rad);
                    p.vel.z += 20.0f * sin(rad);
                } else if (key == GLFW_KEY_X || key == GLFW_KEY_W || key == GLFW_KEY_A || key == GLFW_KEY_D) {
                    // Start Charging!
                    if (!g_game_state.is_charging) {
                        g_game_state.is_charging = true;
                        g_game_state.action_charge_time = 0.0f;
                        g_game_state.current_charge_key = key;
                    }
                }
            } else {
                // Actions without the ball
                if (key == GLFW_KEY_D) {
                    // D: Slide tackle (instant)
                    std::cout << "Slide Tackle!\n";
                    float rad = p.heading_deg * M_PI / 180.0f;
                    p.vel.x += 10.0f * cos(rad);
                    p.vel.z += 10.0f * sin(rad);
                }
            }
        }
    } else if (action == GLFW_RELEASE && (g_game_state.phase == MatchPhase::Playing || g_game_state.phase == MatchPhase::Kickoff || g_game_state.phase == MatchPhase::FreeKick)) {
        // Execute Charged Action
        if (g_game_state.is_charging && key == g_game_state.current_charge_key) {
            Player& p = g_players[g_active_human_player];
            
            // Aim vector is now decoupled from player heading
            float rad = g_game_state.aim_angle_deg * M_PI / 180.0f;
            Vector3f aim(cos(rad), 0.0f, sin(rad));
            
            // Calculate Power curve (charge_time ^ 1.5)
            float charge_ratio = std::min(g_game_state.action_charge_time / 1.5f, 1.0f);
            float power_mult = 0.3f + 1.2f * pow(charge_ratio, 1.5f); // 30% to 150% power
            
            // Error Injection (Inaccuracy expands with charge time)
            float max_error = 30.0f * charge_ratio; // Up to +/- 30 degrees deviation
            float error_deg = (RandomFloat() - 0.5f) * 2.0f * max_error;
            
            if (key == GLFW_KEY_X) {
                std::cout << "Ground Pass!\n";
                // Pass-Lock Auto-Aim (within 60 degrees)
                float best_score = -9999.0f;
                int best_target = -1;
                for (int i = 0; i < 11; i++) {
                    if (i == g_active_human_player) continue;
                    // Lead the target! pos + vel * 0.1
                    Vector3f target_pos(g_players[i].pos.x + g_players[i].vel.x * 0.1f, 0.0f, g_players[i].pos.z + g_players[i].vel.z * 0.1f);
                    
                    Vector3f to_mate(target_pos.x - p.pos.x, 0.0f, target_pos.z - p.pos.z);
                    float dist = to_mate.length();
                    if (dist > 1.0f) {
                        to_mate.Normalize();
                        float dot = to_mate.x * aim.x + to_mate.z * aim.z;
                        if (dot > cos(60.0f * M_PI / 180.0f)) { // 60 degree cone
                            float score = dot - (dist * 0.01f);
                            if (score > best_score) {
                                best_score = score;
                                best_target = i;
                            }
                        }
                    }
                }
                
                if (best_target != -1) {
                    Vector3f target_pos(g_players[best_target].pos.x + g_players[best_target].vel.x * 0.1f, 0.0f, g_players[best_target].pos.z + g_players[best_target].vel.z * 0.1f);
                    Vector3f to_mate(target_pos.x - p.pos.x, 0.0f, target_pos.z - p.pos.z);
                    to_mate.Normalize();
                    aim.x = to_mate.x;
                    aim.z = to_mate.z;
                }
                
                // Apply Error
                float aim_rad = atan2(aim.z, aim.x);
                aim_rad += error_deg * M_PI / 180.0f;
                aim.x = cos(aim_rad);
                aim.z = sin(aim_rad);
                
                g_ball.kick(aim, p.kick_power * power_mult);
                play_sound_kick();
                
            } else if (key == GLFW_KEY_W) {
                std::cout << "Through Ball!\n";
                float aim_rad = atan2(aim.z, aim.x);
                aim_rad += error_deg * M_PI / 180.0f;
                aim.x = cos(aim_rad);
                aim.z = sin(aim_rad);
                aim.x += 0.2f * cos(aim_rad);
                aim.z += 0.2f * sin(aim_rad);
                g_ball.kick(aim, p.kick_power * power_mult * 1.3f);
                play_sound_kick();
            } else if (key == GLFW_KEY_A) {
                std::cout << "Shoot!\n";
                float aim_rad = atan2(aim.z, aim.x);
                aim_rad += error_deg * M_PI / 180.0f;
                aim.x = cos(aim_rad);
                aim.z = sin(aim_rad);
                aim.y = 0.2f * charge_ratio; // lift increases with charge
                g_ball.kick(aim, p.kick_power * power_mult * 1.5f);
                play_sound_kick();
            } else if (key == GLFW_KEY_D) {
                std::cout << "Lofted Pass!\n";
                float aim_rad = atan2(aim.z, aim.x);
                aim_rad += error_deg * M_PI / 180.0f;
                aim.x = cos(aim_rad);
                aim.z = sin(aim_rad);
                aim.y = 0.5f;
                g_ball.kick(aim, p.kick_power * power_mult);
                play_sound_kick();
            }
            
            g_game_state.is_charging = false;
        }
    }
}

void mouse_button_callback(GLFWwindow* window, int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            if (g_game_state.phase == MatchPhase::Kickoff || g_game_state.phase == MatchPhase::FreeKick) {
                g_game_state.phase = MatchPhase::Playing;
                std::cout << "Play Resumed!\n";
            }
            
            if (g_game_state.phase == MatchPhase::Playing) {
                // Double tap check for lofted pass
                static double last_click_time = 0;
                double current_time = glfwGetTime();
                if (current_time - last_click_time < 0.3) {
                    if (g_ball.owner_id == -1 && g_ball.last_kicker_id == g_active_human_player && g_ball.kick_cooldown > 0.2f) {
                        g_ball.vel.y = 8.0f; // Loft it!
                        std::cout << "Double Tap Lofted Pass!\n";
                    }
                }
                last_click_time = current_time;
                
                if (g_ball.owner_id == g_active_human_player && !g_game_state.is_charging) {
                    g_game_state.is_charging = true;
                    g_game_state.action_charge_time = 0.0f;
                    g_game_state.current_charge_key = 9999;
                }
            }
        } else if (action == GLFW_RELEASE && (g_game_state.phase == MatchPhase::Playing || g_game_state.phase == MatchPhase::Kickoff || g_game_state.phase == MatchPhase::FreeKick)) {
            if (g_game_state.is_charging && g_game_state.current_charge_key == 9999) {
                double xpos, ypos;
                glfwGetCursorPos(window, &xpos, &ypos);
                int width, height;
                glfwGetWindowSize(window, &width, &height);
                
                // 3D Raycast / Unprojection
                float x_ndc = (2.0f * (float)xpos) / width - 1.0f;
                float y_ndc = 1.0f - (2.0f * (float)ypos) / height;
                
                Matrix4f invVP = (get_proj_matrix() * get_view_matrix()).Inverse();
                
                // Ray start (near plane z=-1)
                float v_near[4] = { x_ndc, y_ndc, -1.0f, 1.0f };
                float w_near[4] = {0};
                for (int i=0; i<4; i++) {
                    w_near[i] = invVP.m[i][0]*v_near[0] + invVP.m[i][1]*v_near[1] + invVP.m[i][2]*v_near[2] + invVP.m[i][3]*v_near[3];
                }
                if (w_near[3] != 0.0f) { w_near[0]/=w_near[3]; w_near[1]/=w_near[3]; w_near[2]/=w_near[3]; }
                
                // Ray end (far plane z=1)
                float v_far[4] = { x_ndc, y_ndc, 1.0f, 1.0f };
                float w_far[4] = {0};
                for (int i=0; i<4; i++) {
                    w_far[i] = invVP.m[i][0]*v_far[0] + invVP.m[i][1]*v_far[1] + invVP.m[i][2]*v_far[2] + invVP.m[i][3]*v_far[3];
                }
                if (w_far[3] != 0.0f) { w_far[0]/=w_far[3]; w_far[1]/=w_far[3]; w_far[2]/=w_far[3]; }
                
                // Ray equation: P(t) = near + t * (far - near), solve for Y = 0
                float dy = w_far[1] - w_near[1];
                if (fabs(dy) > 0.0001f) {
                    float t = -w_near[1] / dy;
                    float pitch_x = w_near[0] + t * (w_far[0] - w_near[0]);
                    float pitch_z = w_near[2] + t * (w_far[2] - w_near[2]);
                    
                    Player& p = g_players[g_active_human_player];
                    Vector3f aim(pitch_x - p.pos.x, 0.0f, pitch_z - p.pos.z);
                    if (aim.length() > 0.1f) {
                        aim.Normalize();
                        
                        float charge_ratio = std::min(g_game_state.action_charge_time / 1.5f, 1.0f);
                        float power_mult = 0.3f + 1.2f * pow(charge_ratio, 1.5f); // Updated power curve
                        float max_error = 30.0f * charge_ratio;
                        float error_deg = (RandomFloat() - 0.5f) * 2.0f * max_error;
                        
                        float aim_rad = atan2(aim.z, aim.x) + error_deg * M_PI / 180.0f;
                        aim.x = cos(aim_rad);
                        aim.z = sin(aim_rad);
                        
                        g_ball.kick(aim, p.kick_power * power_mult);
                        play_sound_kick();
                        std::cout << "3D Raycast Mouse Click Pass!\n";
                    }
                }
                g_game_state.is_charging = false;
            }
        }
    }
}

void setup_input(GLFWwindow* window) {
    for(int i = 0; i < 1024; i++) keys[i] = false;
    glfwSetKeyCallback(window, key_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);
}

void process_input_frame(float dt) {
    if (g_game_state.phase != MatchPhase::Playing && g_game_state.phase != MatchPhase::Kickoff && g_game_state.phase != MatchPhase::FreeKick) return;
    
    Player& p = g_players[g_active_human_player];
    
    // directional movement (Arrows)
    float dx = 0.0f;
    float dz = 0.0f;
    
    if (keys[GLFW_KEY_UP]) dz -= 1.0f;
    if (keys[GLFW_KEY_DOWN]) dz += 1.0f;
    if (keys[GLFW_KEY_LEFT]) dx -= 1.0f;
    if (keys[GLFW_KEY_RIGHT]) dx += 1.0f;
    
    // update charge
    if (g_game_state.is_charging) {
        g_game_state.action_charge_time += dt;
        if (g_game_state.action_charge_time > 1.5f) g_game_state.action_charge_time = 1.5f;
        
        // Decoupled aiming while charging
        if (dx != 0.0f || dz != 0.0f) {
            float angle = atan2(dz, dx) * 180.0f / M_PI;
            g_game_state.aim_angle_deg = angle;
        }
        
        // freeze player movement
        p.vel.x = 0;
        p.vel.z = 0;
        return;
    } else {
        g_game_state.aim_angle_deg = p.heading_deg;
    }
    
    if (g_game_state.phase != MatchPhase::Playing) return; // Prevent movement in Kickoff/FreeKick
    
    // C: Sprint
    bool sprinting = keys[GLFW_KEY_C];
    float current_accel = sprinting ? p.accel * 1.5f : p.accel;
    float current_max_speed = sprinting ? p.max_speed * 1.5f : p.max_speed;
    
    float len = sqrt(dx*dx + dz*dz);
    if (len > 0.0f) {
        dx /= len;
        dz /= len;
        
        // accelerate
        p.vel.x += dx * current_accel * dt;
        p.vel.z += dz * current_accel * dt;
        
        // clamp speed
        float speed = sqrt(p.vel.x*p.vel.x + p.vel.z*p.vel.z);
        if (speed > current_max_speed) {
            p.vel.x = (p.vel.x / speed) * current_max_speed;
            p.vel.z = (p.vel.z / speed) * current_max_speed;
        }
        
        // update heading smoothly
        p.heading_deg = atan2(p.vel.z, p.vel.x) * 180.0f / M_PI;
    } else {
        // friction slows down the player when no keys are pressed
        p.vel.x *= 0.8f;
        p.vel.z *= 0.8f;
    }
}
