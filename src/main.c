
#include "sokol_gfx.h"
#include "stb_ds.h"
#include "tinyobj_loader_c.h"
#include "tx_math.h"
#include "tx_types.h"
#include <GL/gl3w.h>
#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>

#define TX_INPUT_IMPLEMENTATION
#include "tx_input.h"

typedef struct vertex_pos_norm_tex {
    vec3 pos;
    vec3 norm;
    vec2 uv;
} vertex_pos_norm_tex;

typedef struct vertex_pos_norm {
    vec3 pos;
    vec3 norm;
} vertex_pos_norm;

typedef struct transform {
    vec3 pos;
    quat rot;
} transform;

typedef struct camera {
    vec3 pos;
    quat rot;
} camera;

mat4 camera_look_at(camera cam)
{
    vec3 forward = quat_mul_vec3(cam.rot, (vec3){0.0f, 0.0f, -1.0f});
    vec3 up = quat_mul_vec3(cam.rot, (vec3){0.0f, 1.0f, 0.0f});
    return mat4_look_at(cam.pos, vec3_add(cam.pos, forward), up);
}

void read_file_to_buffer(const char* filename, char** buffer, size_t* len)
{
    TX_ASSERT(buffer);

    size_t string_size = 0, read_size = 0;
    FILE* file = fopen(filename, "r");

    if (file) {
        fseek(file, 0, SEEK_END);
        long tell = ftell(file);

        if (tell < 0) {
            fclose(file);
            return;
        }

        string_size = (size_t)tell;

        rewind(file);
        *buffer = (char*)calloc(string_size + 1, sizeof(char));
        read_size = fread(*buffer, sizeof(char), string_size, file);
        (*buffer)[read_size + 1] = '\0';
        fclose(file);
    }

    if (len) {
        *len = read_size;
    }
}

typedef struct uniform_block {
    mat4 view_proj;
    mat4 model;
} uniform_block;

int main(int argc, char* argv[])
{
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    SDL_Window* window = SDL_CreateWindow(
        "cauldron",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        1280,
        720,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);

    SDL_GLContext gl_context = SDL_GL_CreateContext(window);

    TX_ASSERT(gl3wInit() == GL3W_OK);

    sg_setup(&(sg_desc){0});
    txinp_init();

    // sg_buffer vbuf, ibuf;

    vertex_pos_norm_tex* vertices = NULL;
    uint16_t* indices = NULL;
    uint8_t* material_pixels = NULL;
    int material_width = 0;
    {
        const char* obj_filename = "assets/taxi.obj";

        tinyobj_shape_t* shapes = NULL;
        tinyobj_material_t* materials = NULL;
        tinyobj_attrib_t attrib;

        size_t num_shapes;
        size_t num_materials;

        tinyobj_attrib_init(&attrib);

        int obj_result = tinyobj_parse_obj(
            &attrib,
            &shapes,
            &num_shapes,
            &materials,
            &num_materials,
            obj_filename,
            read_file_to_buffer,
            TINYOBJ_FLAG_TRIANGULATE);

        if (obj_result == TINYOBJ_SUCCESS) {
            uint16_t curr_idx = 0;
            for (size_t shape_idx = 0; shape_idx < num_shapes; ++shape_idx) {
                const tinyobj_shape_t shape = shapes[shape_idx];
                for (uint32_t i = 0; i < shape.length; ++i) {
                    uint32_t face_idx = shape.face_offset + i;
                    TX_ASSERT(attrib.face_num_verts[face_idx] == 3);
                    uint32_t material_id = attrib.material_ids[face_idx];

                    int32_t v_idx0 = attrib.faces[face_idx * 3 + 0].v_idx;
                    int32_t v_idx1 = attrib.faces[face_idx * 3 + 1].v_idx;
                    int32_t v_idx2 = attrib.faces[face_idx * 3 + 2].v_idx;

                    float* v_pos0 = &attrib.vertices[v_idx0 * 3];
                    float* v_pos1 = &attrib.vertices[v_idx1 * 3];
                    float* v_pos2 = &attrib.vertices[v_idx2 * 3];

                    int32_t vn_idx0 = attrib.faces[face_idx * 3 + 0].vn_idx;
                    int32_t vn_idx1 = attrib.faces[face_idx * 3 + 1].vn_idx;
                    int32_t vn_idx2 = attrib.faces[face_idx * 3 + 2].vn_idx;

                    float* v_norm0 = &attrib.normals[vn_idx0 * 3];
                    float* v_norm1 = &attrib.normals[vn_idx1 * 3];
                    float* v_norm2 = &attrib.normals[vn_idx2 * 3];

                    float u = (float)material_id / num_materials;

                    vertex_pos_norm_tex v0 = (vertex_pos_norm_tex){
                        .pos = {v_pos0[0], v_pos0[1], v_pos0[2]},
                        .norm = {v_norm0[0], v_norm0[1], v_norm0[2]},
                        .uv = {u, 0.0f},
                    };
                    vertex_pos_norm_tex v1 = (vertex_pos_norm_tex){
                        .pos = {v_pos1[0], v_pos1[1], v_pos1[2]},
                        .norm = {v_norm1[0], v_norm1[1], v_norm1[2]},
                        .uv = {u, 0.0f},
                    };
                    vertex_pos_norm_tex v2 = (vertex_pos_norm_tex){
                        .pos = {v_pos2[0], v_pos2[1], v_pos2[2]},
                        .norm = {v_norm2[0], v_norm2[1], v_norm2[2]},
                        .uv = {u, 0.0f},
                    };

                    arrput(vertices, v2);
                    arrput(vertices, v1);
                    arrput(vertices, v0);

                    arrput(indices, curr_idx++);
                    arrput(indices, curr_idx++);
                    arrput(indices, curr_idx++);
                    // arrput(vertices, (vertex){.pos = {v_pos1[0], v_pos1[1], v_pos1[2]}});
                    // arrput(vertices, (vertex){.pos = {v_pos2[0], v_pos2[1], v_pos2[2]}});

                    // attrib.faces[shape.face_offset + i].
                }
            }

            material_pixels = (uint8_t*)malloc(num_materials * 4);
            material_width = (int)num_materials;
            for (size_t i = 0; i < num_materials; ++i) {
                material_pixels[i * 4 + 0] = (uint8_t)(materials[i].diffuse[0] * 255.f);
                material_pixels[i * 4 + 1] = (uint8_t)(materials[i].diffuse[1] * 255.f);
                material_pixels[i * 4 + 2] = (uint8_t)(materials[i].diffuse[2] * 255.f);
                material_pixels[i * 4 + 3] = 255;
            }

            printf("#vertices: %d\n#faces: %d\n", (int)arrlen(vertices), (int)arrlen(indices) / 3);

            tinyobj_attrib_free(&attrib);
            if (shapes) {
                tinyobj_shapes_free(shapes, num_shapes);
            }
            if (materials) {
                tinyobj_materials_free(materials, num_materials);
            }
        }
    }

    // const vertex vertices[] = {
    //     {.pos = {.x = -0.5f, .y = 0.5f, .z = -0.5f}},
    //     {.pos = {.x = 0.5f, .y = 0.5f, .z = -0.5f}},
    //     {.pos = {.x = 0.5f, .y = -0.5f, .z = -0.5f}},
    //     {.pos = {.x = -0.5f, .y = -0.5f, .z = -0.5f}},
    //     {.pos = {.x = -0.5f, .y = 0.5f, .z = 0.5f}},
    //     {.pos = {.x = 0.5f, .y = 0.5f, .z = 0.5f}},
    //     {.pos = {.x = 0.5f, .y = -0.5f, .z = 0.5f}},
    //     {.pos = {.x = -0.5f, .y = -0.5f, .z = 0.5f}},
    // };

    // clang-format off
    // const uint16_t indices[] = {
    //     0, 2, 1, 0, 3, 2,   // front
    //     4, 5, 6, 4, 6, 7,   // back
    //     7, 3, 0, 0, 4, 7,
    //     5, 2, 6, 5, 1, 2,
    //     0, 1, 4, 5, 4, 1,
    //     2, 7, 6, 7, 2, 3,
    // };
    // clang-format on

    sg_buffer vbuf = sg_make_buffer(&(sg_buffer_desc){
        .size = (int)arrlen(vertices) * (int)sizeof(vertex_pos_norm_tex),
        .content = vertices,
    });

    sg_buffer ibuf = sg_make_buffer(&(sg_buffer_desc){
        .size = (int)arrlen(indices) * (int)sizeof(uint16_t),
        .type = SG_BUFFERTYPE_INDEXBUFFER,
        .content = indices,
    });

    sg_image texture = sg_make_image(&(sg_image_desc){
        .width = material_width,
        .height = 1,
        .pixel_format = SG_PIXELFORMAT_RGBA8,
        .min_filter = SG_FILTER_LINEAR,
        .mag_filter = SG_FILTER_LINEAR,
        .content.subimage[0][0] =
            {
                .ptr = material_pixels,
                .size = material_width * 4,
            },
    });

    if (material_pixels) {
        free(material_pixels);
    }

    sg_shader lit_colored_shader = sg_make_shader(&(sg_shader_desc){
        .vs.uniform_blocks =
            {
                [0] =
                    {
                        .size = sizeof(uniform_block),
                        .uniforms =
                            {
                                [0] = {.name = "view_proj", .type = SG_UNIFORMTYPE_MAT4},
                                [1] = {.name = "model", .type = SG_UNIFORMTYPE_MAT4},
                            },
                    },
                [1] =
                    {
                        .size = sizeof(vec3),
                        .uniforms =
                            {
                                [0] = {.name = "color_tint", .type = SG_UNIFORMTYPE_FLOAT3},
                            },
                    },
            },
        .vs.source = "#version 330\n"
                     "layout(location=0) in vec3 position;\n"
                     "layout(location=1) in vec3 norm;\n"
                     "uniform mat4 view_proj;\n"
                     "uniform mat4 model;\n"
                     "uniform vec3 color_tint;\n"
                     "out vec3 light;\n"
                     "out vec3 diffuse;\n"
                     "void main() {\n"
                     "  vec3 light_dir = normalize(vec3(0.7, -0.8, 0.1));\n"
                     "  mat3 normalMatrix = transpose(inverse(mat3(model)));\n"
                     "  vec3 world_norm = normalize(normalMatrix * norm);\n"
                     "  vec3 ambient = vec3(0.1, 0.1, 0.1);\n"
                     "  light = vec3(1, 1, 1) * (1 - dot(light_dir, world_norm));\n"
                     "  diffuse = clamp(color_tint * light + ambient + position, 0, 1);\n"
                     "  gl_Position = view_proj * model * vec4(position, 1.0f);\n"
                     "}\n",
        .fs.source = "#version 330\n"
                     "uniform sampler2D tex;\n"
                     "in vec3 diffuse;\n"
                     "out vec4 frag_color;\n"
                     "void main() {\n"
                     "  frag_color = vec4(diffuse, 1);\n"
                     "}\n",
    });

    sg_pipeline prim_pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = lit_colored_shader,
        .index_type = SG_INDEXTYPE_UINT16,
        .layout =
            {
                .attrs =
                    {
                        [0].format = SG_VERTEXFORMAT_FLOAT3,
                        [1].format = SG_VERTEXFORMAT_FLOAT3,
                    },
            },
        .depth_stencil =
            {
                .depth_compare_func = SG_COMPAREFUNC_LESS_EQUAL,
                .depth_write_enabled = true,
            },
        .rasterizer.cull_mode = SG_CULLMODE_BACK,
    });

    sg_shader shd = sg_make_shader(&(sg_shader_desc){
        .vs.uniform_blocks[0] =
            {
                .size = sizeof(uniform_block),
                .uniforms =
                    {
                        [0] = {.name = "view_proj", .type = SG_UNIFORMTYPE_MAT4},
                        [1] = {.name = "model", .type = SG_UNIFORMTYPE_MAT4},
                    },
            },
        .fs.images[0] = {.name = "tex", .type = SG_IMAGETYPE_2D},
        .vs.source = "#version 330\n"
                     "layout(location=0) in vec3 position;\n"
                     "layout(location=1) in vec3 norm;\n"
                     "layout(location=2) in vec2 texcoord0;\n"
                     "uniform mat4 view_proj;\n"
                     "uniform mat4 model;\n"
                     "out vec3 diffuse;\n"
                     "out vec2 uv;\n"
                     "void main() {\n"
                     "  vec3 light_dir = normalize(vec3(0.7, -0.8, 0.1));\n"
                     "  mat3 normalMatrix = transpose(inverse(mat3(model)));\n"
                     "  vec3 world_norm = normalize(normalMatrix * norm);\n"
                     "  diffuse = vec3(1, 1, 1) * (1 - dot(light_dir, world_norm));\n"
                     "  uv = texcoord0;\n"
                     "  gl_Position = view_proj * model * vec4(position, 1.0f);\n"
                     "}\n",
        .fs.source = "#version 330\n"
                     "uniform sampler2D tex;\n"
                     "in vec3 diffuse;\n"
                     "in vec2 uv;\n"
                     "out vec4 frag_color;\n"
                     "void main() {\n"
                     "  vec3 color = texture(tex, uv).rgb;\n"
                     "  frag_color = vec4(diffuse * color, 1);\n"
                     "}\n"});

    // default render states are fine for triangle
    sg_pipeline pip = sg_make_pipeline(&(sg_pipeline_desc){
        .shader = shd,
        .index_type = SG_INDEXTYPE_UINT16,
        .layout =
            {
                .attrs =
                    {
                        [0].format = SG_VERTEXFORMAT_FLOAT3,
                        [1].format = SG_VERTEXFORMAT_FLOAT3,
                        [2].format = SG_VERTEXFORMAT_FLOAT2,
                    },
            },
        .depth_stencil =
            {
                .depth_compare_func = SG_COMPAREFUNC_LESS_EQUAL,
                .depth_write_enabled = true,
            },
        .rasterizer.cull_mode = SG_CULLMODE_BACK,
    });

    sg_bindings binds = {
        .vertex_buffers[0] = vbuf,
        .index_buffer = ibuf,
        .fs_images[0] = texture,
    };

    vertex_pos_norm ground_verts[4] = {
        {.pos = {-1.0f, 0.0f, 1.0f}, .norm = {0.0f, 1.0f, 0.0f}},
        {.pos = {1.0f, 0.0f, 1.0f}, .norm = {0.0f, 1.0f, 0.0f}},
        {.pos = {1.0f, 0.0f, -1.0f}, .norm = {0.0f, 1.0f, 0.0f}},
        {.pos = {-1.0f, 0.0f, -1.0f}, .norm = {0.0f, 1.0f, 0.0f}},
    };

    uint16_t ground_indices[6] = {0, 3, 2, 1, 0, 2};

    sg_buffer ground_vbuf = sg_make_buffer(&(sg_buffer_desc){
        .size = sizeof(ground_verts),
        .content = ground_verts,
    });

    sg_buffer ground_ibuf = sg_make_buffer(&(sg_buffer_desc){
        .type = SG_BUFFERTYPE_INDEXBUFFER,
        .size = sizeof(ground_indices),
        .content = ground_indices,
    });

    // default: clear to grey
    sg_pass_action pass_action = {.colors[0] = {
                                      .action = SG_ACTION_CLEAR,
                                      .val = {0.482f, 0.451f, 0.957f},
                                  }};

    camera cam = {.pos = {.x = 0.0f, .y = 2.0f, .z = 10.0f}, .rot = quat_identity()};
    transform car_tx = {.pos = {0}, .rot = quat_identity()};
    uint64_t last_ticks = SDL_GetPerformanceCounter();
    float time = 0.0f;
    bool is_running = true;
    while (is_running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
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

        // time
        uint64_t ticks = SDL_GetPerformanceCounter();
        uint64_t delta_ticks = ticks - last_ticks;
        last_ticks = ticks;
        uint64_t frequency = SDL_GetPerformanceFrequency();
        float dt = (float)delta_ticks / frequency;
        time += dt;

        // update
        if (txinp_get_key(TXINP_KEY_UP)) {
            vec3 forward = quat_mul_vec3(car_tx.rot, (vec3){0.0f, 0.0f, 1.0f});
            car_tx.pos = vec3_add(car_tx.pos, vec3_scale(forward, dt));
        }

        if (txinp_get_key(TXINP_KEY_RIGHT)) {
            car_tx.rot = quat_mul(car_tx.rot, quat_rotate(-dt, (vec3){0.0f, 1.0f, 0.0f}));
        }

        if (txinp_get_key(TXINP_KEY_LEFT)) {
            car_tx.rot = quat_mul(car_tx.rot, quat_rotate(dt, (vec3){0.0f, 1.0f, 0.0f}));
        }

        // render
        int cur_width, cur_height;
        SDL_GetWindowSize(window, &cur_width, &cur_height);
        sg_begin_default_pass(&pass_action, cur_width, cur_height);

        mat4 view = camera_look_at(cam);
        mat4 projection = mat4_perspective(K_TX_PI / 4.0f, 16.0f / 9, 0.1f, 100.0f);
        mat4 view_proj = mat4_mul(projection, view);

        {
            mat4 model = mat4_scale(mat4_identity(), 50.0f);

            sg_apply_pipeline(prim_pip);
            sg_apply_bindings(&(sg_bindings){
                .vertex_buffers[0] = ground_vbuf,
                .index_buffer = ground_ibuf,
            });
            uniform_block uniforms = {
                .view_proj = view_proj,
                .model = model,
            };
            sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, &uniforms, sizeof(uniform_block));
            sg_apply_uniforms(SG_SHADERSTAGE_VS, 1, &(vec3){0.0f, 1.0f, 0.0f}, sizeof(vec3));
            sg_draw(0, 6, 1);
        }

        {
            static float r = 0.0f;
            r += dt * K_TX_PI;
            mat4 model = mat4_translate(
                mat4_from_quat(car_tx.rot), car_tx.pos.x, car_tx.pos.y, car_tx.pos.z);

            sg_apply_pipeline(pip);
            sg_apply_bindings(&binds);
            uniform_block uniforms = {
                .view_proj = view_proj,
                .model = model,
            };
            sg_apply_uniforms(SG_SHADERSTAGE_VS, 0, &uniforms, sizeof(uniform_block));
            sg_draw(0, (int)arrlen(indices), 1);
        }

        sg_end_pass();
        sg_commit();

        SDL_GL_SwapWindow(window);
    }

    sg_shutdown();
    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);

    return 0;
}