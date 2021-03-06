#include "window_system.h"

#include "game_settings.h"
#include <GL/gl3w.h>
#include <SDL2/SDL.h>

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include <cimgui.h>
#include <cimgui_impl.h>

struct {
    SDL_Window* window;
    SDL_GLContext gl_context;
    ImGuiIO* io;
} window_state = {0};

tx_result window_system_init(game_settings* settings)
{
    window_state.window = SDL_CreateWindow(
        "cauldron",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        settings->options.video.display_width,
        settings->options.video.display_height,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    window_state.gl_context = SDL_GL_CreateContext(window_state.window);

    SDL_GL_SetSwapInterval((settings->options.video.enable_vsync) ? 1 : 0);

    if (gl3wInit() != GL3W_OK) {
        return TX_FAILURE;
    }

    return TX_SUCCESS;
}

void window_system_term(void)
{
    SDL_GL_DeleteContext(window_state.gl_context);
    SDL_DestroyWindow(window_state.window);
}

void window_swap(void)
{
    SDL_GL_SwapWindow(window_state.window);
}

void window_get_size(int* x, int* y)
{
    SDL_GetWindowSize(window_state.window, x, y);
}

SDL_Window* window_get_ptr(void)
{
    return window_state.window;
}

SDL_GLContext window_get_gl(void)
{
    return window_state.gl_context;
}

tx_result imgui_system_init(game_settings* settings)
{
    igCreateContext(NULL);
    window_state.io = igGetIO();
    window_state.io->ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui_ImplSDL2_InitForOpenGL(window_state.window, window_state.gl_context);
    ImGui_ImplOpenGL3_Init(NULL);
    igStyleColorsDark(NULL);

    return TX_SUCCESS;
}

void imgui_system_term(void)
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    igDestroyContext(NULL);
}

void imgui_begin(float dt)
{
    int win_w, win_h;
    SDL_GetWindowSize(window_state.window, &win_w, &win_h);
    window_state.io->DisplaySize = (ImVec2){.x = (float)win_w, .y = (float)win_h};
    window_state.io->DeltaTime = dt;
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplSDL2_NewFrame(window_state.window);
    igNewFrame();
}

void imgui_end(void)
{
    igRender();
    ImGui_ImplOpenGL3_RenderDrawData(igGetDrawData());
}
