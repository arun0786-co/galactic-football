# builds the game on macos (homebrew glew + glfw) or linux (libglew-dev
# libglfw3-dev), and the headless simulator, which needs no opengl

CXX      ?= c++
CXXFLAGS ?= -O2 -g
CXXFLAGS += -std=c++17 -Wall -Iimgui -MMD -MP

UNAME := $(shell uname -s)
ifeq ($(UNAME),Darwin)
  BREW     := $(shell brew --prefix 2>/dev/null || echo /opt/homebrew)
  CXXFLAGS += -I$(BREW)/include -DGL_SILENCE_DEPRECATION
  GLLIBS   := -L$(BREW)/lib -lGLEW -lglfw -framework OpenGL
else
  GLLIBS   := -lGLEW -lglfw -lGL -lpthread
endif

IMGUI := imgui imgui_draw imgui_tables imgui_widgets imgui_demo imgui_impl_glfw imgui_impl_opengl3
CORE  := game_state.o entities.o physics.o audio.o
GAME  := main.o input.o renderer.o $(IMGUI:%=imgui/%.o)

galactic_football: $(CORE) $(GAME)
	$(CXX) $^ $(GLLIBS) -o $@

clean:
	rm -f galactic_football $(CORE) $(GAME) *.d imgui/*.d

.PHONY: clean
-include $(wildcard *.d imgui/*.d)
