#include "level_system.h"
#include "entity_system.h"
#include "event_system.h"
#include "game_level.h"
#include "game_systems.h"
#include "sprite_draw.h"

game_level_proj proj = {0};
game_level* active = NULL;

void on_change_level(event_message* message)
{
    change_level_event* change_level = (change_level_event*)message;

    if (active) {
        game_systems_unload_level();
    }
    active = level_find_id(change_level->level_id);
    if (active) {
        game_systems_load_level(active);
    }
}

void on_reload_level_proj(event_message* message)
{
    strhash level_id = (strhash){0};
    if (active) {
        level_id = active->name_id;
        game_systems_unload_level();
        active = NULL;
    }
    free_game_level_project(&proj);
    load_game_level_project("assets/test_level.json", &proj);
    level_load_id(level_id);
}

tx_result level_system_init(game_settings* settings)
{
    tx_result ret = load_game_level_project("assets/test_level.json", &proj);

    if (ret != TX_SUCCESS) {
        return ret;
    }

    event_system_subscribe(EventMessage_ChangeLevel, on_change_level);
    event_system_subscribe(EventMessage_ReloadLevelProject, on_reload_level_proj);

    return TX_SUCCESS;
}

void level_system_term(void)
{
    free_game_level_project(&proj);
}

void level_system_load_level(game_level* level)
{
    active = level;
}

void level_system_unload_level(void)
{
    active = NULL;
}

void level_system_render(float ft)
{
    if (active) {
        for (int lid = 0; lid < arrlen(active->layer_insts); ++lid) {
            game_layer_inst layer = active->layer_insts[lid];
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
    }
}

void level_reload(void)
{
    if (active) {
        level_load_id(active->name_id);
    }
}

void level_load_id(strhash id)
{
    event_send((event_message*)&(change_level_event){
        .event.msg_type = EventMessage_ChangeLevel,
        .level_id = id,
    });
}

void level_load_name(char* name)
{
    level_load_id(strhash_get(name));
}

void level_system_reload_project(void)
{
    event_send((event_message*)&(reload_level_proj_event){
        .event.msg_type = EventMessage_ReloadLevelProject,
    });
}

game_level* level_find_id(strhash id)
{
    for (int i = 0; i < arrlen(proj.levels); ++i) {
        if (proj.levels[i].name_id.value == id.value) {
            return &proj.levels[i];
        }
    }
    return NULL;
}

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include <cimgui.h>
void level_select_window(bool* show)
{
    if (igBegin("Levels", show, ImGuiWindowFlags_None)) {
        game_level* next_level = NULL;
        if (igButton("Reload Level Project", (ImVec2){0})) {
            level_system_reload_project();
        }
        igSeparator();
        igBeginGroupPanel("Level Select", (ImVec2){0});
        for (int i = 0; i < arrlen(proj.levels); ++i) {
            game_level* level = &proj.levels[i];
            if (igButton(strhash_cstr(level->name_id), (ImVec2){0})) {
                next_level = level;
            }
        }
        igEndGroupPanel();

        if (next_level) {
            level_load_id(next_level->name_id);
        }
        igEnd();
    }
}