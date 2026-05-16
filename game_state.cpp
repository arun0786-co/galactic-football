#include "game_state.h"
#include "audio.h"
#include <iostream>
#include <vector>
#include <algorithm>

// instantiate the global state
GameState g_game_state;

// section 2: game state logic
// here we handle the flow of time and transitions between match phases.
// if a goal is scored, we wait a few seconds so the player can celebrate,
// and then we trigger a kickoff.

void GameState::update(float dt) {
    if (phase == MatchPhase::Playing) {
        match_timer += dt;
        if (match_timer >= MATCH_DURATION) {
            phase = MatchPhase::GameOver;
            play_sound_gameover();
            std::cout << "Match Over! Final Score: Human " << score_human 
                      << " - Alien " << score_alien << "\n";
        }
    } else if (phase == MatchPhase::GoalScored) {
        goal_wait_timer -= dt;
        if (goal_wait_timer <= 0.0f) {
            trigger_kickoff();
        }
    } else if (phase == MatchPhase::Kickoff || phase == MatchPhase::FreeKick) {
        // Just wait for input to switch to playing (handled in input.cpp)
    }
    
    if (referee_timer > 0.0f) {
        referee_timer -= dt;
        if (referee_timer <= 0.0f) {
            referee_message = "";
        }
    }
}

#include "entities.h"

void GameState::trigger_kickoff() {
    phase = MatchPhase::Kickoff;
    std::cout << "Kickoff Phase!\n";
    
    g_ball.pos = Vector3f(PITCH_LENGTH / 2.0f, 0.0f, PITCH_WIDTH / 2.0f);
    g_ball.vel = Vector3f(0.0f);
    g_ball.owner_id = -1;
    
    // player repositioning
    reset_positions_for_kickoff();
}

void GameState::trigger_free_kick() {
    phase = MatchPhase::FreeKick;
    std::cout << "Free Kick Phase!\n";
    
    int attacking_team = (g_ball.owner_id != -1) ? g_players[g_ball.owner_id].team : TEAM_HUMAN;
    int defending_team = (attacking_team == TEAM_HUMAN) ? TEAM_ALIEN : TEAM_HUMAN;
    
    float target_goal_x = (attacking_team == TEAM_HUMAN) ? PITCH_LENGTH : 0.0f;
    float target_goal_z = PITCH_WIDTH / 2.0f;
    
    Vector3f to_goal(target_goal_x - g_ball.pos.x, 0.0f, target_goal_z - g_ball.pos.z);
    to_goal.Normalize();
    
    // Wall center is 9.15m away
    Vector3f wall_center = g_ball.pos;
    wall_center.x += to_goal.x * 9.15f;
    wall_center.z += to_goal.z * 9.15f;
    
    Vector3f wall_perp(-to_goal.z, 0.0f, to_goal.x);
    
    std::vector<int> defender_ids;
    for (int i=0; i<22; i++) {
        if (g_players[i].team == defending_team && g_players[i].role != PlayerRole::Goalkeeper) {
            defender_ids.push_back(i);
        }
    }
    
    std::sort(defender_ids.begin(), defender_ids.end(), [](int a, int b) {
        float distA = sqrt(pow(g_players[a].pos.x - g_ball.pos.x, 2) + pow(g_players[a].pos.z - g_ball.pos.z, 2));
        float distB = sqrt(pow(g_players[b].pos.x - g_ball.pos.x, 2) + pow(g_players[b].pos.z - g_ball.pos.z, 2));
        return distA < distB;
    });
    
    for (int i=0; i<4 && i<defender_ids.size(); i++) {
        float offset = (i - 1.5f) * 1.5f; 
        g_players[defender_ids[i]].pos.x = wall_center.x + wall_perp.x * offset;
        g_players[defender_ids[i]].pos.z = wall_center.z + wall_perp.z * offset;
        g_players[defender_ids[i]].vel = Vector3f(0.0f);
    }
}

void GameState::score_goal(int team) {
    if (phase != MatchPhase::Playing) return;

    if (team == TEAM_HUMAN) {
        score_human++;
        team_to_kickoff = TEAM_ALIEN;
        std::cout << "GOOOOAL! Humans score!\n";
    } else {
        score_alien++;
        team_to_kickoff = TEAM_HUMAN;
        std::cout << "GOOOOAL! Aliens score!\n";
    }
    
    play_sound_goal();

    phase = MatchPhase::GoalScored;
    goal_wait_timer = 3.0f; // wait 3 seconds before kickoff
}

void GameState::reset_match() {
    score_human = 0;
    score_alien = 0;
    match_timer = 0.0f;
    trigger_kickoff();
}
