#include "entities.h"
#include "game_state.h"
#include "physics.h"
#include "audio.h"
#include <cmath>
#include <algorithm>
#include <iostream>

// instantiate global entities
Ball g_ball;
Player g_players[22];
int g_active_human_player = 0; // default to first human player

// section 4: entity update logic
// this is where the AI and physics for the entities are processed.
// we separate this from rendering so we can reason about pure logic.

void Ball::update(float dt) {
    if (kick_cooldown > 0.0f) {
        kick_cooldown -= dt;
    }
    
    // gravity
    if (pos.y > radius) {
        vel.y -= 9.81f * dt;
    }
    
    // integrate position
    pos.x += vel.x * dt;
    pos.y += vel.y * dt;
    pos.z += vel.z * dt;
    
    // floor collision (bouncing)
    if (pos.y < radius) {
        pos.y = radius;
        if (vel.y < -1.0f) {
            vel.y = -vel.y * 0.6f; // bounce restitution
        } else {
            vel.y = 0.0f;
        }
    }
}

void Ball::kick(const Vector3f& direction, float power) {
    // un-attach from owner if possessed
    last_kicker_id = owner_id;
    kick_cooldown = 0.5f;
    owner_id = -1;
    
    // apply kick velocity
    vel.x = direction.x * power;
    vel.y = direction.y * power;
    vel.z = direction.z * power;
}

void Ball::apply_friction(float friction_factor, float dt) {
    // only apply friction when touching the ground
    if (pos.y <= radius + 0.01f) {
        float speed = sqrt(vel.x * vel.x + vel.z * vel.z);
        if (speed > 0.1f) {
            float drop = speed * friction_factor * dt;
            float new_speed = std::max(speed - drop, 0.0f);
            float ratio = new_speed / speed;
            vel.x *= ratio;
            vel.z *= ratio;
        } else {
            vel.x = 0.0f;
            vel.z = 0.0f;
        }
    }
}

void Player::update(float dt) {
    // integrate position
    pos.x += vel.x * dt;
    pos.z += vel.z * dt;
    
    // Boundary enforcement (Pillar 2: Physics)
    // Zero out velocity when touching boundary for smooth stopping
    if (pos.x < 0.0f) {
        pos.x = 0.0f;
        vel.x = 0.0f;
    } else if (pos.x > PITCH_LENGTH) {
        pos.x = PITCH_LENGTH;
        vel.x = 0.0f;
    }
    
    if (pos.z < 0.0f) {
        pos.z = 0.0f;
        vel.z = 0.0f;
    } else if (pos.z > PITCH_WIDTH) {
        pos.z = PITCH_WIDTH;
        vel.z = 0.0f;
    }
    
    update_animation(dt);
}

void Player::move_towards(const Vector3f& target, float dt) {
    float dx = target.x - pos.x;
    float dz = target.z - pos.z;
    float dist = sqrt(dx*dx + dz*dz);
    
    if (dist > 0.1f) {
        float dir_x = dx / dist;
        float dir_z = dz / dist;
        
        // accelerate towards target
        vel.x += dir_x * accel * dt;
        vel.z += dir_z * accel * dt;
        
        // clamp to max speed
        float speed = sqrt(vel.x*vel.x + vel.z*vel.z);
        if (speed > max_speed) {
            vel.x = (vel.x / speed) * max_speed;
            vel.z = (vel.z / speed) * max_speed;
        }
        
        // update heading smoothly
        float target_heading = atan2(vel.z, vel.x) * 180.0f / M_PI;
        heading_deg = target_heading; // simplified: instant turn
    } else {
        // slow down
        vel.x *= 0.8f;
        vel.z *= 0.8f;
    }
}

void Player::update_animation(float dt) {
    float speed = sqrt(vel.x*vel.x + vel.z*vel.z);
    
    // if moving, cycle the animation using a sine wave.
    // this creates a procedural running motion without keyframes!
    if (speed > 0.5f) {
        anim.run_cycle_time += speed * dt * 2.0f;
        float cycle = sin(anim.run_cycle_time);
        
        // arms and legs move opposite each other
        anim.left_arm_angle = cycle * 45.0f;
        anim.right_arm_angle = -cycle * 45.0f;
        anim.left_leg_angle = -cycle * 45.0f;
        anim.right_leg_angle = cycle * 45.0f;
    } else {
        // return to neutral standing pose
        anim.run_cycle_time = 0.0f;
        anim.left_arm_angle *= 0.9f;
        anim.right_arm_angle *= 0.9f;
        anim.left_leg_angle *= 0.9f;
        anim.right_leg_angle *= 0.9f;
    }
}

void init_entities() {
    // initialize all 22 players
    for (int i = 0; i < 22; i++) {
        g_players[i].id = i;
        g_players[i].team = (i < 11) ? TEAM_HUMAN : TEAM_ALIEN;
        
        // assign roles (simplified)
        if (i % 11 == 0) g_players[i].role = PlayerRole::Goalkeeper;
        else if (i % 11 < 5) g_players[i].role = PlayerRole::Defender;
        else if (i % 11 < 9) g_players[i].role = PlayerRole::Midfielder;
        else g_players[i].role = PlayerRole::Attacker;
        
        // set alien attributes based on alien type
        if (g_players[i].team == TEAM_ALIEN) {
            if (g_game_state.current_alien_type == AlienType::Brute) {
                g_players[i].max_speed = 4.0f;
                g_players[i].accel = 10.0f;
            } else if (g_game_state.current_alien_type == AlienType::Speedster) {
                g_players[i].max_speed = 9.0f;
                g_players[i].accel = 20.0f;
            }
        }
    }
    
    reset_positions_for_kickoff();
}

void reset_positions_for_kickoff() {
    g_ball.pos = Vector3f(PITCH_LENGTH / 2.0f, g_ball.radius, PITCH_WIDTH / 2.0f);
    g_ball.vel = Vector3f(0.0f, 0.0f, 0.0f);
    g_ball.owner_id = -1;
    
    // assign base positions for a classic 4-4-2 formation (scaled to pitch)
    float cx = PITCH_LENGTH / 2.0f;
    float cz = PITCH_WIDTH / 2.0f;
    
    for (int i = 0; i < 22; i++) {
        Player& p = g_players[i];
        p.vel = Vector3f(0,0,0);
        
        int idx = i % 11;
        float dir = (p.team == TEAM_HUMAN) ? -1.0f : 1.0f; // human defends left side
        
        if (idx == 0) { // Goalie
            p.pos = Vector3f(cx + dir * 50.0f, 0.0f, cz);
        } else if (idx < 5) { // Defense
            float z_offset = (idx - 2.5f) * 12.0f;
            p.pos = Vector3f(cx + dir * 35.0f, 0.0f, cz + z_offset);
        } else if (idx < 9) { // Midfield
            float z_offset = (idx - 6.5f) * 12.0f;
            p.pos = Vector3f(cx + dir * 15.0f, 0.0f, cz + z_offset);
        } else { // Attack
            float z_offset = (idx - 9.5f) * 10.0f;
            p.pos = Vector3f(cx + dir * 2.0f, 0.0f, cz + z_offset);
        }
        p.base_position = p.pos;
        p.heading_deg = (p.team == TEAM_HUMAN) ? 0.0f : 180.0f;
    }
    
    int kickoff_team = g_game_state.team_to_kickoff;
    
    if (kickoff_team == TEAM_HUMAN) {
        g_active_human_player = 9; // forward
        g_players[g_active_human_player].pos = Vector3f(cx - 0.5f, 0.0f, cz);
        g_ball.owner_id = g_active_human_player;
    } else {
        int alien_forward = 20;
        g_players[alien_forward].pos = Vector3f(cx + 0.5f, 0.0f, cz);
        g_ball.owner_id = alien_forward;
    }
}

void update_all_entities(float dt) {
    g_ball.update(dt);
    
    // Kickoff & FreeKick Phase Lockout & AI Auto-Start
    if (g_game_state.phase == MatchPhase::Kickoff || g_game_state.phase == MatchPhase::FreeKick) {
        if (g_ball.owner_id != -1 && g_players[g_ball.owner_id].team == TEAM_ALIEN) {
            static float ai_wait_timer = 0.0f;
            ai_wait_timer += dt;
            if (ai_wait_timer > 1.5f) { // wait 1.5s before taking kick
                g_game_state.phase = MatchPhase::Playing;
                Vector3f aim(0.0f - g_players[g_ball.owner_id].pos.x, 0.0f, PITCH_WIDTH/2.0f - g_players[g_ball.owner_id].pos.z);
                aim.Normalize();
                g_ball.kick(aim, g_players[g_ball.owner_id].kick_power * 1.0f);
                play_sound_kick();
                ai_wait_timer = 0.0f;
            }
        } else if (g_ball.owner_id == -1 && g_game_state.phase == MatchPhase::FreeKick) {
            // Ball was kicked, break out of FreeKick!
            g_game_state.phase = MatchPhase::Playing;
        }
        
        // Update all players so they stand still
        for (int i=0; i<22; i++) {
            g_players[i].vel.x = 0;
            g_players[i].vel.z = 0;
            g_players[i].update(dt);
        }
        return; // Freeze AI logic
    }
    
    // Find closest players to the ball
    int closest_alien_id = -1;
    float min_alien_dist = 99999.0f;
    int closest_human_id = -1;
    float min_human_dist = 99999.0f;
    
    for (int i = 0; i < 22; i++) {
        if (g_players[i].role == PlayerRole::Goalkeeper) continue;
        float dist = sqrt(pow(g_players[i].pos.x - g_ball.pos.x, 2) + pow(g_players[i].pos.z - g_ball.pos.z, 2));
        if (g_players[i].team == TEAM_ALIEN) {
            if (dist < min_alien_dist) {
                min_alien_dist = dist;
                closest_alien_id = i;
            }
        } else {
            if (i != g_active_human_player && dist < min_human_dist) {
                min_human_dist = dist;
                closest_human_id = i;
            }
        }
    }
    
    static float possession_timer = 0.0f;
    static int current_possession_team = -1;
    
    int actual_possession = -1;
    if (g_ball.owner_id != -1) {
        actual_possession = g_players[g_ball.owner_id].team;
    }
    
    if (actual_possession != -1 && actual_possession != current_possession_team) {
        if (possession_timer <= 0.0f) {
            possession_timer = 0.5f; // Start Hysteresis
        }
        possession_timer -= dt;
        if (possession_timer <= 0.0f) {
            current_possession_team = actual_possession;
        }
    } else {
        possession_timer = 0.0f;
        if (actual_possession != -1) {
            current_possession_team = actual_possession;
        }
    }
    
    bool human_has_ball = (current_possession_team == TEAM_HUMAN);
    bool alien_has_ball = (current_possession_team == TEAM_ALIEN);
    
    for (int i = 0; i < 22; i++) {
        // active human player is controlled by input, not AI.
        if (g_players[i].id == g_active_human_player) {
            g_players[i].update(dt);
            continue; 
        }
        
        Player& p = g_players[i];
        Vector3f target = p.base_position;
        
        if (p.role == PlayerRole::Goalkeeper) {
            // true goalie AI: maximum speed limit and defensive arc
            float cz = PITCH_WIDTH / 2.0f;
            float gx = (p.team == TEAM_HUMAN) ? 0.0f : PITCH_LENGTH; // goal line
            
            float dist_x = fabs(g_ball.pos.x - gx);
            if (dist_x < 6.0f && fabs(g_ball.pos.z - cz) < 10.0f) {
                // Ball is in the box! Abandon arc and rush
                target = g_ball.pos;
            } else {
                // Move along a shallow arc to cut off angles
                float angle_to_ball = atan2(g_ball.pos.z - cz, g_ball.pos.x - gx);
                float arc_radius = 5.0f; // Step out of the goal 5 meters
                target.x = gx + arc_radius * cos(angle_to_ball);
                target.z = cz + arc_radius * sin(angle_to_ball);
            }
            
            // Clamping to goal width
            target.z = std::max(cz - GOAL_WIDTH/2.0f, std::min(cz + GOAL_WIDTH/2.0f, target.z));
            
            // Limit goalie speed so they can be beaten by fast passes/shots
            p.max_speed = 3.5f; 
        } else if (g_ball.owner_id == p.id && p.team == TEAM_ALIEN) {
            // AI Possession Logic (Dribbling towards goal)
            target = Vector3f(0.0f, 0.0f, PITCH_WIDTH / 2.0f); 
            
            // AI Cross Logic
            bool in_final_third = (p.pos.x < PITCH_LENGTH / 3.0f);
            bool near_sidelines = (p.pos.z < 15.0f || p.pos.z > PITCH_WIDTH - 15.0f);
            if (in_final_third && near_sidelines) {
                Vector3f aim(0.0f - p.pos.x, 0.0f, PITCH_WIDTH/2.0f - p.pos.z);
                aim.Normalize();
                aim.y = p.kick_power * 1.5f * 0.6f; // Lifted pass formula
                g_ball.kick(aim, p.kick_power * 1.5f);
                play_sound_kick();
            } 
            // AI Passing Logic
            else {
                static float ai_pass_timer = 0.0f;
                ai_pass_timer += dt;
                
                if (ai_pass_timer > 0.5f) {
                    float min_def_dist = 9999.0f;
                    for (int d = 0; d < 22; d++) {
                        if (g_players[d].team != p.team) {
                            float dist = sqrt(pow(g_players[d].pos.x - p.pos.x, 2) + pow(g_players[d].pos.z - p.pos.z, 2));
                            if (dist < min_def_dist) min_def_dist = dist;
                        }
                    }
                    
                    if (min_def_dist < 3.0f) { // Under pressure!
                        int best_mate = -1;
                        float best_mate_def_dist = 0.0f;
                        
                        for (int t = 0; t < 22; t++) {
                            if (g_players[t].team == p.team && t != p.id && g_players[t].role != PlayerRole::Goalkeeper) {
                                float mate_min_def = 9999.0f;
                                for (int d = 0; d < 22; d++) {
                                    if (g_players[d].team != p.team) {
                                        float dist = sqrt(pow(g_players[d].pos.x - g_players[t].pos.x, 2) + pow(g_players[d].pos.z - g_players[t].pos.z, 2));
                                        if (dist < mate_min_def) mate_min_def = dist;
                                    }
                                }
                                if (mate_min_def > best_mate_def_dist) {
                                    best_mate_def_dist = mate_min_def;
                                    best_mate = t;
                                }
                            }
                        }
                        
                        if (best_mate != -1) {
                            Vector3f aim(g_players[best_mate].pos.x - p.pos.x, 0.0f, g_players[best_mate].pos.z - p.pos.z);
                            aim.Normalize();
                            g_ball.kick(aim, p.kick_power * 1.2f);
                            play_sound_kick();
                            ai_pass_timer = 0.0f;
                        }
                    }
                }
            }
        } else {
            // Field Player AI
            bool is_closest = (p.team == TEAM_ALIEN && p.id == closest_alien_id) || 
                              (p.team == TEAM_HUMAN && p.id == closest_human_id);
            
            if (is_closest && g_ball.owner_id == -1) {
                // Ball is loose, closest player chases it
                target = g_ball.pos;
            } else if (is_closest && ((p.team == TEAM_HUMAN && alien_has_ball) || (p.team == TEAM_ALIEN && human_has_ball))) {
                // Opponent has ball, closest player chases/presses
                target = g_ball.pos;
            } else if (p.team == TEAM_HUMAN && human_has_ball) {
                // WE HAVE THE BALL - Smart Teammate AI (Support Attack)
                if (p.role == PlayerRole::Attacker) {
                    target.x = g_ball.pos.x + 25.0f; // Run deep into box
                } else if (p.role == PlayerRole::Midfielder) {
                    target.x = g_ball.pos.x + 10.0f; // Offer short pass options
                } else if (p.role == PlayerRole::Defender) {
                    target.x = p.base_position.x; // Stay back to prevent counter
                }
                target.z = p.base_position.z; 
            } else if (p.team == TEAM_ALIEN && alien_has_ball) {
                // ALIEN HAS BALL - Smart Teammate AI
                if (p.role == PlayerRole::Attacker) {
                    target.x = g_ball.pos.x - 25.0f; // Run deep
                } else if (p.role == PlayerRole::Midfielder) {
                    target.x = g_ball.pos.x - 10.0f; // Offer short pass options
                } else if (p.role == PlayerRole::Defender) {
                    target.x = p.base_position.x; // Stay back
                }
                target.z = p.base_position.z;
            } else {
                // Retreat to defensive formation
                target = p.base_position;
                target.x += (g_ball.pos.x - PITCH_LENGTH/2.0f) * 0.3f;
                target.z += (g_ball.pos.z - PITCH_WIDTH/2.0f) * 0.2f;
            }
        }
        
        p.move_towards(target, dt);
        p.update(dt);
    }
}
