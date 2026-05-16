// section 3: game entities
// here we define the data structures for the things moving around on the pitch:
// the ball and the players (both human stickmen and alien opponents).
// i keep them plain structs mostly so it's easy to read and manipulate their data.

#ifndef ENTITIES_H
#define ENTITIES_H

#include "math_utils.h"

// the ball
// the ball has position, velocity, and state (is it rolling, in the air, or held by a player?)
struct Ball {
    Vector3f pos;
    Vector3f vel;
    Vector3f angular_vel; // for spin
    
    float radius = 0.4f;
    int owner_id = -1; // -1 means loose ball, otherwise it's the ID of the player dribbling it
    int last_kicker_id = -1;
    float kick_cooldown = 0.0f;
    
    void update(float dt);
    void kick(const Vector3f& direction, float power);
    void apply_friction(float friction_factor, float dt);
};

// player roles
enum class PlayerRole {
    Goalkeeper,
    Defender,
    Midfielder,
    Attacker
};

// stickman animation state
// i use this to pass procedural limb angles to the renderer
struct StickmanAnim {
    float left_arm_angle = 0.0f;
    float right_arm_angle = 0.0f;
    float left_leg_angle = 0.0f;
    float right_leg_angle = 0.0f;
    float run_cycle_time = 0.0f;
};

// a single player on the pitch
struct Player {
    int id;
    int team; // TEAM_HUMAN or TEAM_ALIEN
    PlayerRole role;
    
    Vector3f pos;
    Vector3f vel;
    float heading_deg = 0.0f;
    
    // attributes
    float max_speed = 6.0f;
    float accel = 15.0f;
    float kick_power = 25.0f;
    
    // animation and visual state
    StickmanAnim anim;
    
    // AI state tracking
    Vector3f base_position; // where they should stand ideally
    
    // core logic
    void update(float dt);
    void move_towards(const Vector3f& target, float dt);
    void update_animation(float dt);
};

// global entity lists
extern Ball g_ball;
extern Player g_players[22]; // 0-10 are humans, 11-21 are aliens
extern int g_active_human_player; // the player currently controlled by the keyboard

// entity initialization
void init_entities();
void reset_positions_for_kickoff();
void update_all_entities(float dt);

#endif // ENTITIES_H
