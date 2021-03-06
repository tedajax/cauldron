#pragma once

#include "game_systems.h"
#include "tx_math.h"
#include "tx_types.h"

enum {
    PHYS_LAYER_NONE = 0,
    PHYS_LAYER_COLLISION = 1,
    PHYS_LAYER_PLATFORM = 2,
    PHYS_LAYER_NARROW_WALL = 8,
};

typedef enum phys_tile_shape_type {
    PHYS_TILE_SHAPE_FULL_TILE = 0,
    PHYS_TILE_SHAPE_PLATFORM = 1,
    PHYS_TILE_SHAPE_SLOPE = 2,
    PHYS_TILE_SHAPE_WALL = 3,
} phys_tile_shape_type;

typedef struct phys_tile_shape {
    phys_tile_shape_type type;
    uint8_t flips;
} phys_tile_shape;

tx_result phys_system_init(game_settings* settings);
void phys_system_term(void);
void phys_system_load_level(game_level* level);
void phys_system_unload_level(void);
void phys_system_update(float dt);
void phys_system_render(float rt);

bool phys_solid(float x, float y, uint16_t layer_mask);
float phys_get_gravity();