
#include "game_level.h"
#include "game_settings.h"
#include "game_systems.h"
#include "hash.h"
#include "player_system.h"
#include "profile.h"
#include "sokol_gfx.h"
#include "sprite_draw.h"
#include "stb_ds.h"
#include "tinyobj_loader_c.h"
#include "tx_input.h"
#include "tx_math.h"
#include "tx_rand.h"
#include "tx_types.h"
#include <GL/gl3w.h>
#include <SDL2/SDL.h>

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include <cimgui.h>
#include <cimgui_impl.h>

#include <stdio.h>
#include <stdlib.h>
#include <time.h>

struct debug_ui {
    bool show;
    bool open;
    int fps;
    int frame_limit;
    float load_proj_ms;
};

struct debug_ui dbgui = {
    .show = true,
    .open = true,
};

struct config_ui {
    bool show;
    bool open;
};

struct config_ui configui = {
    .show = false,
    .open = true,
};

int main(int argc, char* argv[])
{
    load_game_settings(NULL);
    game_settings* const settings = get_game_settings();

    SDL_Window* window = SDL_CreateWindow(
        "cauldron",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        settings->options.video.display_width,
        settings->options.video.display_height,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    SDL_GLContext gl_context = SDL_GL_CreateContext(window);

    SDL_GL_SetSwapInterval((settings->options.video.enable_vsync) ? 1 : 0);

    if (gl3wInit() != GL3W_OK) {
        return 1;
    }

    sg_setup(&(sg_desc){0});
    spr_init();
    txinp_init();
    txrng_seed((uint32_t)time(NULL));

    game_level_proj proj = {0};
    if (load_game_level_project("assets/test_level.json", &proj) != TX_SUCCESS) {
        return 1;
    }

    uint64_t load_ticks = profile_get_last_ticks("load_game_level_project");
    dbgui.load_proj_ms = (float)(load_ticks * 1000) / SDL_GetPerformanceFrequency();

    game_systems_init(&(game_settings){0});
    game_systems_load_level(&proj.levels[0]);

    igCreateContext(NULL);
    ImGuiIO* io = igGetIO();
    io->ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init(NULL);
    igStyleColorsDark(NULL);

    unsigned char* text_pixels = NULL;
    int text_w, text_h;
    ImFontAtlas_GetTexDataAsRGBA32(io->Fonts, &text_pixels, &text_w, &text_h, NULL);

    uint64_t last_ticks = SDL_GetPerformanceCounter();
    float time = 0.0f;
    float next_sec = time + 1.0f;
    bool is_running = true;
    int frames_this_sec = 0;
    while (is_running) {
        txinp_update();

        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL2_ProcessEvent(&event);
            switch (event.type) {
            case SDL_QUIT:
                is_running = false;
                break;

            case SDL_KEYDOWN:
                if (event.key.keysym.scancode == SDL_SCANCODE_ESCAPE) {
                    is_running = false;
                }

                txinp_on_key_event((txinp_event_key){
                    .key = (txinp_key)event.key.keysym.scancode,
                    .is_down = true,
                });
                break;

            case SDL_KEYUP:
                txinp_on_key_event((txinp_event_key){
                    .key = (txinp_key)event.key.keysym.scancode,
                    .is_down = false,
                });
                break;
            }
        }

        if (txinp_get_key_down(TXINP_KEY_F11) && txinp_get_key(TXINP_KEY_LSHIFT)) {
            configui.show = !configui.show;
        }

        if (txinp_get_key_down(TXINP_KEY_F12) && txinp_get_key(TXINP_KEY_LSHIFT)) {
            dbgui.show = !dbgui.show;
        }

        // time
        uint64_t frequency = SDL_GetPerformanceFrequency();
        uint64_t ticks = SDL_GetPerformanceCounter();
        uint64_t delta_ticks = ticks - last_ticks;
        int frame_limit = settings->options.video.frame_limit;
        uint64_t min_frame_delta_ticks = (frame_limit > 0) ? frequency / frame_limit : 0;
        uint64_t over_ticks = 0;

        while (settings->options.video.frame_limit > 0 && delta_ticks < min_frame_delta_ticks) {
            ticks = SDL_GetPerformanceCounter();
            delta_ticks = ticks - last_ticks;
            over_ticks = delta_ticks - min_frame_delta_ticks;
        }

        last_ticks = ticks - over_ticks;
        float dt = (float)(delta_ticks - over_ticks) / frequency;
        time += dt;
        ++frames_this_sec;

        if (time >= next_sec) {
            next_sec += 1.0f;
            dbgui.fps = frames_this_sec;
            frames_this_sec = 0;
        }

        game_systems_update(dt);
        game_systems_render();

        // update
        vec2 input = {0};

        if (txinp_get_key(TXINP_KEY_UP)) {
            input.y -= 1.0f;
        }

        if (txinp_get_key(TXINP_KEY_DOWN)) {
            input.y += 1.0f;
        }

        if (txinp_get_key(TXINP_KEY_LEFT)) {
            input.x -= 1.0f;
        }

        if (txinp_get_key(TXINP_KEY_RIGHT)) {
            input.x += 1.0f;
        }

        for (int lid = 0; lid < arrlen(proj.levels[0].layer_insts); ++lid) {
            game_layer_inst layer = proj.levels[0].layer_insts[lid];
            for (uint32_t x = 0; x < layer.cell_w; x++) {
                for (uint32_t y = 0; y < layer.cell_h; y++) {
                    if (layer.type != GAME_LAYER_TYPE_TILES) {
                        continue;
                    }
                    game_tile tile = layer.tiles[x + y * layer.cell_w];
                    int id = tile.value;
                    sprite_flip flip = SPRITE_FLIP_NONE;
                    if ((tile.flags & GAME_TILE_FLAGS_FLIP_X) != 0) {
                        flip |= SPRITE_FLIP_X;
                    }
                    if ((tile.flags & GAME_TILE_FLAGS_FILP_Y) != 0) {
                        flip |= SPRITE_FLIP_Y;
                    }

                    if (id > 0) {
                        spr_draw(&(sprite_draw_desc){
                            .sprite_id = id,
                            .pos = (vec2){.x = (float)x, .y = (float)y},
                            .flip = flip,
                        });
                    }
                }
            }
        }

        // render
        int cur_width, cur_height;
        SDL_GetWindowSize(window, &cur_width, &cur_height);
        spr_render(cur_width, cur_height);
        sg_commit();

        int win_w, win_h;
        SDL_GetWindowSize(window, &win_w, &win_h);
        io->DisplaySize = (ImVec2){.x = (float)win_w, .y = (float)win_h};
        io->DeltaTime = dt;
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame(window);
        igNewFrame();

        if (dbgui.show) {
            igBegin("Debug", &dbgui.open, ImGuiWindowFlags_None);
            igText("FPS: %d", dbgui.fps);
            igText("dt: %0.5f", dt);
            igSliderInt(
                "Frame Limit",
                &settings->options.video.frame_limit,
                0,
                144,
                "%d FPS",
                ImGuiSliderFlags_AlwaysClamp);
            igText("Project Load Time: %0.2fms", dbgui.load_proj_ms);

            actor_handle hactor = get_player_actor(0);
            actor* actor = actor_get(hactor);
            if (actor) {
                igText("Pos: %0.2f, %0.2f", actor->pos.x, actor->pos.y);
                igText("Vel: %0.2f, %0.2f", actor->vel.x, actor->vel.y);
                igText("Jump Forgive: %0.2f", actor->jump_forgive_timer);
            }

            igEnd();
        }

        // config ui
        if (configui.show) {
            if (igBegin("config", &configui.show, ImGuiWindowFlags_None)) {
                game_systems_config_ui();
            }
            igEnd();
        }

        igRender();
        ImGui_ImplOpenGL3_RenderDrawData(igGetDrawData());

        SDL_GL_SwapWindow(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();
    igDestroyContext(NULL);

    game_systems_unload_level();
    game_systems_shutdown();

    free_game_level_project(&proj);

    spr_shutdown();
    sg_shutdown();
    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);

    return 0;
}