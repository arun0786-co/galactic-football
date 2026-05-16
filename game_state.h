// section 1: global constants and game state definitions
// here we define the overall rules of the game, dimensions, and terrains.
// the goal is to make it easy to tweak the balance and feel.
// following the conversational style, i put all the high-level match
// configuration right here at the start.

#ifndef GAME_STATE_H
#define GAME_STATE_H

#include <string>

// pitch dimensions (standard 105x68 meters in our units)
const float PITCH_LENGTH = 105.0f; // x axis
const float PITCH_WIDTH = 68.0f;   // z axis
const float GOAL_WIDTH = 7.32f;
const float GOAL_DEPTH = 2.0f;
const float GOAL_HEIGHT = 2.44f;

// team constants
const int TEAM_HUMAN = 0;
const int TEAM_ALIEN = 1;
const int NUM_PLAYERS_PER_TEAM = 11;

// terrain types
// different terrains change physics properties like friction and bounce
enum class TerrainType {
    NeonGrid,    // standard fast futuristic pitch
    IcePlanet,   // very low friction, players slide
    MudWasteland // very high friction, players move slowly
};

// alien types
enum class AlienType {
    Brute,       // slow but pushes players easily
    Speedster,   // fast but weak
    Teleporter   // can dash
};

// match phases
enum class MatchPhase {
    Kickoff,
    Playing,
    GoalScored,
    GameOver,
    FreeKick
};

// the central game state object that holds the high-level match rules
struct GameState {
    int score_human = 0;
    int score_alien = 0;
    
    MatchPhase phase = MatchPhase::Kickoff;
    TerrainType current_terrain = TerrainType::NeonGrid;
    AlienType current_alien_type = AlienType::Brute;

    float aim_angle_deg = 0.0f; // For decoupled aiming
    int team_to_kickoff = 0;
    int team_conceded = -1;

    float match_timer = 0.0f; // in seconds
    const float MATCH_DURATION = 180.0f; // 3 minute match
    
    // time to wait after a goal before resetting
    float goal_wait_timer = 0.0f;

    // referee messages
    std::string referee_message = "";
    float referee_timer = 0.0f;
    
    // input charging state
    bool is_charging = false;
    float action_charge_time = 0.0f;
    int current_charge_key = -1;

    // updates the match timer and phase transitions
    void update(float dt);
    
    // resets positions for a kickoff
    void trigger_kickoff();
    
    void trigger_free_kick();

    // handles scoring a goal
    void score_goal(int team);

    // resets the match
    void reset_match();
};

// a global instance to hold the state
extern GameState g_game_state;

#endif // GAME_STATE_H
