#include "physics.h"
#include "entities.h"
#include "game_state.h"
#include "audio.h"
#include <iostream>
#include <cmath>

// section 5.1: physics implementation
// applying friction based on the terrain type.
// handling ball bouncing off the walls and goals.

void apply_physics(float dt) {
    // 1. apply terrain friction to the ball
    float friction = 1.0f;
    if (g_game_state.current_terrain == TerrainType::IcePlanet) {
        friction = 0.2f; // very slippery
    } else if (g_game_state.current_terrain == TerrainType::MudWasteland) {
        friction = 3.0f; // very sticky
    } else {
        friction = 1.0f; // neon grid standard
    }
    
    g_ball.apply_friction(friction, dt);
    
    // 2. goal detection and pitch boundaries
    // checking if the ball crosses the goal line
    if (g_ball.pos.x <= 0.0f) {
        // left side (Human goal)
        if (g_ball.pos.z > (PITCH_WIDTH/2.0f - GOAL_WIDTH/2.0f) && 
            g_ball.pos.z < (PITCH_WIDTH/2.0f + GOAL_WIDTH/2.0f) &&
            g_ball.pos.y < GOAL_HEIGHT) {
            
            // goal for aliens!
            if (g_game_state.phase == MatchPhase::Playing) {
                g_game_state.score_goal(TEAM_ALIEN);
            }
        } else {
            // bounce off wall
            g_ball.pos.x = 0.0f;
            g_ball.vel.x = -g_ball.vel.x * 0.5f;
        }
    } else if (g_ball.pos.x >= PITCH_LENGTH) {
        // right side (Alien goal)
        if (g_ball.pos.z > (PITCH_WIDTH/2.0f - GOAL_WIDTH/2.0f) && 
            g_ball.pos.z < (PITCH_WIDTH/2.0f + GOAL_WIDTH/2.0f) &&
            g_ball.pos.y < GOAL_HEIGHT) {
            
            // goal for humans!
            if (g_game_state.phase == MatchPhase::Playing) {
                g_game_state.score_goal(TEAM_HUMAN);
            }
        } else {
            // bounce off wall
            g_ball.pos.x = PITCH_LENGTH;
            g_ball.vel.x = -g_ball.vel.x * 0.5f;
        }
    }
    
    // side walls bounce
    if (g_ball.pos.z <= 0.0f) {
        g_ball.pos.z = 0.0f;
        g_ball.vel.z = -g_ball.vel.z * 0.5f;
    } else if (g_ball.pos.z >= PITCH_WIDTH) {
        g_ball.pos.z = PITCH_WIDTH;
        g_ball.vel.z = -g_ball.vel.z * 0.5f;
    }
    
    // 3. auto-dribbling logic
    // if the ball is loose, check if any player is close enough to claim it
    if (g_ball.owner_id == -1) {
        for (int i = 0; i < 22; i++) {
            float dist = sqrt(pow(g_players[i].pos.x - g_ball.pos.x, 2) + 
                              pow(g_players[i].pos.z - g_ball.pos.z, 2));
                              
            // if very close and ball is low enough, claim it
            if (dist < 1.0f && g_ball.pos.y < 1.0f) {
                if (g_ball.kick_cooldown > 0.0f && g_players[i].id == g_ball.last_kicker_id) {
                    continue; // Kicker cannot reclaim the ball instantly
                }
                
                g_ball.owner_id = g_players[i].id;
                
                // if it's a human player, automatically switch control to them!
                if (g_players[i].team == TEAM_HUMAN) {
                    g_active_human_player = g_players[i].id;
                }
                break;
            }
        }
    }
    
    // 4. stick ball to owner and handle tackles / fouls
    if (g_ball.owner_id != -1) {
        Player& owner = g_players[g_ball.owner_id];
        
        // Check for opponent tackles
        for (int i = 0; i < 22; i++) {
            if (i == g_ball.owner_id) continue;
            Player& attacker = g_players[i];
            
            if (attacker.team != owner.team) {
                float dist = sqrt(pow(attacker.pos.x - owner.pos.x, 2) + pow(attacker.pos.z - owner.pos.z, 2));
                if (dist < 1.5f) {
                    float attacker_speed = sqrt(pow(attacker.vel.x, 2) + pow(attacker.vel.z, 2));
                    
                    // If moving very fast (slide tackle)
                    if (attacker_speed > attacker.max_speed * 1.2f) {
                        float rand_val = RandomFloat();
                        
                        // Foul angle calculation
                        float owner_speed = sqrt(pow(owner.vel.x, 2) + pow(owner.vel.z, 2));
                        float foul_chance = 0.3f; // Default 30%
                        
                        if (attacker_speed > 0.1f && owner_speed > 0.1f) {
                            float ax = attacker.vel.x / attacker_speed;
                            float az = attacker.vel.z / attacker_speed;
                            float ox = owner.vel.x / owner_speed;
                            float oz = owner.vel.z / owner_speed;
                            
                            float dot = (ax * ox) + (az * oz);
                            if (dot > 0.5f) {
                                foul_chance = 0.8f; // Tackle from behind
                            } else if (dot < -0.5f) {
                                foul_chance = 0.1f; // Clean tackle from front
                            }
                        }
                        
                        if (rand_val < foul_chance) {
                            // FOUL!
                            std::cout << "REFEREE: FOUL by Player " << attacker.id << "!\n";
                            play_sound_whistle();
                            g_game_state.referee_message = "FOUL! Free Kick Awarded.";
                            g_game_state.referee_timer = 3.0f;
                            
                            // Check if within 25m of the goal
                            float target_goal_x = (owner.team == TEAM_HUMAN) ? PITCH_LENGTH : 0.0f;
                            float target_goal_z = PITCH_WIDTH / 2.0f;
                            float dist_to_goal = sqrt(pow(owner.pos.x - target_goal_x, 2) + pow(owner.pos.z - target_goal_z, 2));
                            
                            if (dist_to_goal < 25.0f) {
                                g_game_state.trigger_free_kick();
                                g_ball.pos.x = attacker.pos.x;
                                g_ball.pos.z = attacker.pos.z;
                                g_ball.vel = Vector3f(0.0f);
                                g_ball.owner_id = owner.id;
                            }
                            
                            // Push the attacker away (punishment)
                            attacker.pos.x += (attacker.team == TEAM_HUMAN ? -5.0f : 5.0f);
                            attacker.vel.x = 0;
                            attacker.vel.z = 0;
                        } else {
                            // Clean Tackle!
                            std::cout << "Clean Tackle by Player " << attacker.id << "!\n";
                            g_game_state.referee_message = "Clean Tackle!";
                            g_game_state.referee_timer = 1.5f;
                            
                            g_ball.owner_id = attacker.id;
                            if (attacker.team == TEAM_HUMAN) g_active_human_player = attacker.id;
                            break;
                        }
                    } else if (dist < 1.0f && attacker_speed > 1.0f) {
                        // Normal standing tackle / steal
                        g_ball.owner_id = attacker.id;
                        if (attacker.team == TEAM_HUMAN) g_active_human_player = attacker.id;
                        break;
                    }
                }
            }
        }
        
        // Re-get owner in case it changed during tackles
        Player& current_owner = g_players[g_ball.owner_id];
        float rad = current_owner.heading_deg * M_PI / 180.0f;
        g_ball.pos.x = current_owner.pos.x + cos(rad) * 0.8f;
        g_ball.pos.z = current_owner.pos.z + sin(rad) * 0.8f;
        g_ball.pos.y = g_ball.radius;
        g_ball.vel = current_owner.vel; 
    }
}
