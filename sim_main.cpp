// headless ai vs ai simulator.
//
// plays full matches with no window, no audio and no keyboard: both teams
// run the same ai (g_autoplay) on a fixed 60 Hz step, exactly the update
// order of the game loop. after every step it checks invariants that the
// game should never break, and at the end it prints results per terrain
// and alien type. with --check it exits non-zero if any match broke one,
// which is what ci runs.
//
//   ./soccer_sim --matches 300 --seed 1 --check

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>

#include "audio.h"
#include "entities.h"
#include "game_state.h"
#include "physics.h"

namespace {

const float DT = 1.0f / 60.0f;
const float STALL_SECONDS = 30.0f;    // ball confined to a small area this long
const float STALL_RADIUS = 3.0f;
const float SET_PIECE_LIMIT = 10.0f;  // a kickoff or free kick never taken
const float MAX_BALL_HEIGHT = 50.0f;  // a ball higher than this was kicked wrong
const float JITTER = 0.5f;            // the ai is deterministic, so every
                                      // kickoff moves players up to this far
                                      // from their formation spot

void jitter_formation() {
    for (int i = 0; i < 22; i++) {
        if (i == g_ball.owner_id) continue;
        g_players[i].pos.x += (2.0f * float(random()) / float(RAND_MAX) - 1.0f) * JITTER;
        g_players[i].pos.z += (2.0f * float(random()) / float(RAND_MAX) - 1.0f) * JITTER;
    }
}

struct MatchResult {
    int human = 0, alien = 0;
    float possession[2] = {0, 0};     // seconds each team owned the ball
    int kickoffs = 0, free_kicks = 0;
    int stalls = 0;                   // ball stuck in one spot for STALL_SECONDS
    int stuck_set_pieces = 0;
    int out_of_bounds = 0;            // steps with ball or player off the pitch
    int non_finite = 0;               // steps with nan or inf positions
    float seconds = 0;
};

bool finite3(const Vector3f& v) {
    return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}

bool on_pitch(const Vector3f& v, float margin) {
    return v.x >= -margin && v.x <= PITCH_LENGTH + margin && v.z >= -margin &&
           v.z <= PITCH_WIDTH + margin;
}

MatchResult play_match(TerrainType terrain, AlienType aliens) {
    MatchResult r;
    // GameState has a const member, so it is reset field by field
    g_game_state.score_human = g_game_state.score_alien = 0;
    g_game_state.goal_wait_timer = g_game_state.referee_timer = 0.0f;
    g_game_state.is_charging = false;
    g_game_state.current_terrain = terrain;
    g_game_state.current_alien_type = aliens;
    g_game_state.team_to_kickoff = TEAM_HUMAN;
    init_entities();
    g_game_state.reset_match();
    jitter_formation();

    Vector3f anchor = g_ball.pos;
    float anchor_time = 0.0f, set_piece_time = 0.0f;
    MatchPhase last_phase = g_game_state.phase;

    // a match is 180 s of playing time plus pauses after goals; stop at an
    // hour of simulated time so a broken match cannot run forever
    while (g_game_state.phase != MatchPhase::GameOver && r.seconds < 3600.0f) {
        g_game_state.update(DT);
        MatchPhase ph = g_game_state.phase;
        if (ph == MatchPhase::Playing || ph == MatchPhase::Kickoff || ph == MatchPhase::FreeKick) {
            update_all_entities(DT);
            if (g_game_state.phase == MatchPhase::Playing) apply_physics(DT);
        }
        r.seconds += DT;

        ph = g_game_state.phase;
        if (ph != last_phase) {
            if (ph == MatchPhase::Kickoff) {
                r.kickoffs++;
                jitter_formation();
            }
            if (ph == MatchPhase::FreeKick) r.free_kicks++;
            last_phase = ph;
            set_piece_time = 0.0f;
        }
        if (ph == MatchPhase::Kickoff || ph == MatchPhase::FreeKick) {
            set_piece_time += DT;
            if (set_piece_time > SET_PIECE_LIMIT) {
                r.stuck_set_pieces++;
                set_piece_time = -1e9f; // count once
            }
        }

        if (ph == MatchPhase::Playing) {
            if (g_ball.owner_id >= 0) r.possession[g_players[g_ball.owner_id].team] += DT;
            float dx = g_ball.pos.x - anchor.x, dz = g_ball.pos.z - anchor.z;
            if (dx * dx + dz * dz > STALL_RADIUS * STALL_RADIUS) {
                anchor = g_ball.pos;
                anchor_time = 0.0f;
            } else if ((anchor_time += DT) > STALL_SECONDS) {
                r.stalls++;
                anchor = g_ball.pos;
                anchor_time = 0.0f;
            }
        } else {
            anchor = g_ball.pos;
            anchor_time = 0.0f;
        }

        bool bad = !finite3(g_ball.pos);
        bool out = !on_pitch(g_ball.pos, 1.0f) || g_ball.pos.y > MAX_BALL_HEIGHT;
        for (int i = 0; i < 22; i++) {
            bad |= !finite3(g_players[i].pos);
            out |= !on_pitch(g_players[i].pos, 1.0f);
        }
        r.non_finite += bad;
        r.out_of_bounds += out;
    }
    r.human = g_game_state.score_human;
    r.alien = g_game_state.score_alien;
    return r;
}

const char* terrain_name(TerrainType t) {
    switch (t) {
    case TerrainType::NeonGrid: return "neon grid";
    case TerrainType::IcePlanet: return "ice";
    default: return "mud";
    }
}

const char* alien_name(AlienType a) {
    switch (a) {
    case AlienType::Brute: return "brute";
    case AlienType::Speedster: return "speedster";
    default: return "teleporter";
    }
}

}  // namespace

int main(int argc, char** argv) {
    int matches = 90;
    unsigned seed = 1;
    bool check = false;
    for (int i = 1; i < argc; i++) {
        if (!std::strcmp(argv[i], "--matches") && i + 1 < argc) matches = std::atoi(argv[++i]);
        else if (!std::strcmp(argv[i], "--seed") && i + 1 < argc) seed = unsigned(std::atoi(argv[++i]));
        else if (!std::strcmp(argv[i], "--check")) check = true;
        else {
            std::fprintf(stderr, "usage: soccer_sim [--matches n] [--seed s] [--check]\n");
            return 2;
        }
    }

    g_autoplay = true;
    g_audio_enabled = false;
    std::cout.rdbuf(nullptr); // the game logs every tackle and kickoff to stdout

    const TerrainType terrains[] = {TerrainType::NeonGrid, TerrainType::IcePlanet, TerrainType::MudWasteland};
    const AlienType aliens[] = {AlienType::Brute, AlienType::Speedster, AlienType::Teleporter};

    std::printf("%-10s %-10s %7s %7s %7s %7s %7s %8s %6s %7s %6s %6s\n", "terrain", "aliens", "matches",
                "human W", "draws", "alien W", "goals", "human %", "fk", "stalls", "stuck", "broken");
    int total_broken = 0, total = 0;
    double sim_seconds = 0;
    std::clock_t t0 = std::clock();
    for (TerrainType t : terrains)
        for (AlienType a : aliens) {
            int n = matches / 9 + (total < matches % 9 ? 1 : 0);
            int hw = 0, dr = 0, aw = 0, goals = 0, fks = 0, stalls = 0, stuck = 0, broken = 0;
            double poss_h = 0, poss_all = 0;
            for (int m = 0; m < n; m++, total++) {
                srandom(seed * 7919u + unsigned(total));
                MatchResult r = play_match(t, a);
                hw += r.human > r.alien;
                dr += r.human == r.alien;
                aw += r.human < r.alien;
                goals += r.human + r.alien;
                fks += r.free_kicks;
                stalls += r.stalls;
                stuck += r.stuck_set_pieces;
                broken += (r.non_finite > 0 || r.out_of_bounds > 0 || r.seconds >= 3600.0f);
                poss_h += r.possession[0];
                poss_all += r.possession[0] + r.possession[1];
                sim_seconds += r.seconds;
            }
            total_broken += broken + stuck;
            std::printf("%-10s %-10s %7d %7d %7d %7d %7.2f %7.1f%% %6.2f %7d %6d %6d\n", terrain_name(t),
                        alien_name(a), n, hw, dr, aw, n ? double(goals) / n : 0.0,
                        poss_all > 0 ? 100.0 * poss_h / poss_all : 0.0, n ? double(fks) / n : 0.0, stalls,
                        stuck, broken);
        }
    double wall = double(std::clock() - t0) / CLOCKS_PER_SEC;
    std::printf("\n%d matches, %.0f simulated minutes in %.2f s of cpu (%.0fx real time)\n", total,
                sim_seconds / 60.0, wall, wall > 0 ? sim_seconds / wall : 0.0);
    std::printf("goals: mean per match; human %%: share of possession; fk: free kicks per match;\n"
                "stalls: ball within %.0f m for %.0f s; stuck: set piece not taken in %.0f s;\n"
                "broken: nan positions, ball or player off the pitch, ball above %.0f m, or no end\n",
                STALL_RADIUS, STALL_SECONDS, SET_PIECE_LIMIT, MAX_BALL_HEIGHT);
    if (check && total_broken > 0) {
        std::printf("FAILED: %d broken or stuck matches\n", total_broken);
        return 1;
    }
    return 0;
}
