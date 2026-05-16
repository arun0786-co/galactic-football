// section 6: input handling
// here we process keyboard inputs. instead of just moving a car,
// we handle complex actions: tracking keys to detect chords like "L + P".

#ifndef INPUT_H
#define INPUT_H

struct GLFWwindow;

// initialize callbacks
void setup_input(GLFWwindow* window);

// process held keys for movement each frame
void process_input_frame(float dt);

#endif // INPUT_H
