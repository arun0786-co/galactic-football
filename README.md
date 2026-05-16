# Neon Pitch: Galactic Football

Hey there! Welcome to **Neon Pitch**, my custom-built 11v11 3D arcade football game. I built this entirely from scratch in C++ using OpenGL. It's heavily inspired by the fast-paced, fluid mechanics of classic arcade soccer games but with a sci-fi twist—Human stickmen face off against an Alien squad across multiple intergalactic terrains. 

No pre-built game engines here—everything from the collision physics to the AI decision trees was written by hand!

## 🚀 Features
* **Custom 3D Physics Engine:** Fully functional ball physics with gravity, bouncing, terrain-based friction (ice is slippery, mud is sticky), and raycasted mouse unprojection for passing.
* **Smart AI Systems:** The AI doesn't just chase the ball. They scan for open teammates, calculate pass trajectories, automatically whip crosses into the box, and dynamically build defensive walls during free-kicks.
* **Match Flow & Set Pieces:** Full kickoff routines, automated goal celebrations, referee foul detection, and structured free-kick phases.
* **Decoupled Power Aiming:** A robust charging system where you can lock in your power and adjust your pass angle mid-stride before releasing.
* **Double-Tap Mouse Controls:** Left-click to fire a precision ground pass to your cursor, or double-tap to loft a massive cross across the pitch.
* **Dynamic Audio:** Custom audio debouncing system for smooth sound effects without overlapping spam.

## 🛠 Tech Stack
* **Language:** C++
* **Graphics:** OpenGL (with GLEW & GLFW)
* **UI:** ImGui
* **Build System:** Make

## 🎮 Controls
* **Movement:** Arrow Keys
* **Sprint:** Hold `C`
* **Switch Player:** `Q`
* **Dash / Skill Move:** `Z`
* **Ground Pass:** `X` (Hold to charge)
* **Lofted Pass:** `D` (Hold to charge)
* **Through Ball:** `W` (Hold to charge)
* **Shoot:** `A` (Hold to charge)
* **Mouse Controls:** Left Click to pass to the cursor (Double Click to loft!)
* **Cameras:** `1` (Follow), `2` (Side TV), `3` (Top-Down Tactical), `4` (Goalkeeper Cam)

## 🏃‍♂️ How to Run
If you're on macOS (with Homebrew dependencies installed):
```bash
make clean
make
./sample
```

Enjoy the match!
