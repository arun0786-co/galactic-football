## makefile for assignment 3 (3d car world)
##
## standard opengl project makefile for mac.
## the tech stack is identical: opengl + glew + glfw + imgui on mac.
##
## i should be able to type `make` and get a running binary,
## and `make clean` to start fresh.

CC = g++
RM = /bin/rm -rf

## compiler flags:
##   -O3   : optimizations
##   -Wall : enable most warnings
##   -g    : debug info for gdb/lldb
CFLAGS = -O3 -Wall -g

## include paths for glew, glfw, and imgui headers
INCDIRS = -I/opt/homebrew/opt/glew/include -I/opt/homebrew/opt/glfw/include -I./imgui

## library paths
LIBDIRS = -L. -L/usr/local/lib -L/opt/homebrew/opt/glew/lib -L/opt/homebrew/opt/glfw/lib

## libraries to link against (mac framework for opengl)
LIBS = -framework OpenGL -lGLEW -lglfw

## name of the final executable
BIN = sample

## imgui source files
IMGUI_SRCS = imgui/imgui.cpp imgui/imgui_draw.cpp imgui/imgui_tables.cpp imgui/imgui_widgets.cpp imgui/imgui_demo.cpp imgui/imgui_impl_glfw.cpp imgui/imgui_impl_opengl3.cpp
## game source files
GAME_SRCS = main.cpp game_state.cpp entities.cpp physics.cpp input.cpp renderer.cpp audio.cpp
SRCS = $(GAME_SRCS) $(IMGUI_SRCS)

## object files
IMGUI_OBJS = $(IMGUI_SRCS:.cpp=.o)
GAME_OBJS = $(GAME_SRCS:.cpp=.o)
OBJS = $(GAME_OBJS) $(IMGUI_OBJS)

## build the final binary from object files
${BIN} : ${OBJS}
	${CC} ${OBJS} ${LIBDIRS} ${LIBS} -o $@

## compile any .cpp to .o
%.o : %.cpp
	${CC} ${CFLAGS} ${INCDIRS} -c $< -o $@

.PHONY : clean remake

clean :
	${RM} ${BIN}
	${RM} ${OBJS}

remake : clean ${BIN}

depend:
	makedepend -- $(CFLAGS) -- -Y $(SRCS)
