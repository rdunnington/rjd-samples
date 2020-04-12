#include <stdio.h>
#include <stdbool.h>

#include "rjd_wrapped.h"
#include "entrypoint.h"
//struct rjd_gfx_windowsize g_window_size = { .width = 480, .height = 640 };

#include <errno.h>

////////////////////////////////////////////////////////////////////////////////////////////
// implementation

struct uniforms
{
    rjd_math_mat4 viewproj_matrix;
    rjd_math_mat4 model_matrix;
};

void env_init(const struct rjd_window_environment* env)
{
	struct app_data* app = env->userdata;

	// bootstrap the allocator
	{
		struct rjd_mem_allocator allocator = rjd_mem_allocator_init_default();
		app->allocator = rjd_mem_alloc(struct rjd_mem_allocator, &allocator);
		*app->allocator = allocator;
	}

	// create the window and start the event loop
	app->window = rjd_mem_alloc(struct rjd_window, app->allocator);
    memset(app->window, 0, sizeof(struct rjd_window));

    struct rjd_window_desc window_desc = {
        .title = "test",
        .requested_size = {
            .width = 640,
            .height = 480,
        },
        .env = *env,
		.init_func = window_init,
		.update_func = window_update,
        .close_func = window_close,
    };
	#if RJD_PLATFORM_WINDOWS
        window_desc.win32.handle_instance = env->handle_instance;
	#endif

	struct rjd_result result = rjd_window_create(app->window, window_desc);
	if (!rjd_result_isok(result)) {
        RJD_LOG_ERROR("Failed to create window: %s", result.error);
        return;
	}

    rjd_window_runloop(app->window);
}

void window_init(struct rjd_window* window, const struct rjd_window_environment* env)
{
    struct app_data* app = env->userdata;

	app->gfx.context = rjd_mem_alloc(struct rjd_gfx_context, app->allocator);
	app->gfx.texture = rjd_mem_alloc(struct rjd_gfx_texture, app->allocator);
	app->gfx.shader = rjd_mem_alloc(struct rjd_gfx_shader, app->allocator);
	app->gfx.pipeline_state = rjd_mem_alloc(struct rjd_gfx_pipeline_state, app->allocator);
	app->gfx.mesh = rjd_mem_alloc(struct rjd_gfx_mesh, app->allocator);

    {
        struct rjd_gfx_context_desc desc = {
            .backbuffer_color_format = RJD_GFX_FORMAT_COLOR_U8_BGRA_NORM_SRGB,
            .backbuffer_depth_format = RJD_GFX_FORMAT_DEPTHSTENCIL_F32_D32,
            .allocator = app->allocator,
        };

        #if RJD_PLATFORM_WINDOWS
            desc.win32.window_handle = rjd_window_win32_get_hwnd(window);
        #elif RJD_PLATFORM_OSX
            desc.osx.view = rjd_window_osx_get_mtkview(window);
        #endif

        struct rjd_result result = rjd_gfx_context_create(app->gfx.context, desc);
		if (!rjd_result_isok(result)) {
			RJD_LOG("Failed to create gfx context: %s", result.error);
			return;
		}
        
        uint32_t msaa_sample_counts[] = { 16, 8, 4, 2, 1 };
        for (size_t i = 0; i < rjd_countof(msaa_sample_counts); ++i) {
            if (rjd_gfx_msaa_is_count_supported(app->gfx.context, msaa_sample_counts[i])) {
                rjd_gfx_msaa_set_count(app->gfx.context, msaa_sample_counts[i]);
                break;
            }
        }
    }

    // resources
    {
        // square white texture
        {
            const enum rjd_gfx_format format = RJD_GFX_FORMAT_COLOR_U8_BGRA_NORM;
            uint32_t color[64 * 64 * 4] = {0};
			for (size_t i = 0; i < rjd_countof(color); ++i) {
				color[i] = 0xFF0080FF;
			}
            
            struct rjd_gfx_texture_desc desc = {
                .data = color,
                .data_length = sizeof(color),
                .pixels_width = 64,
                .pixels_height = 64,
                .format = format,
                .access = RJD_GFX_TEXTURE_ACCESS_CPU_WRITE_GPU_READWRITE,
                .usage = RJD_GFX_TEXTURE_USAGE_DEFAULT,
                .debug_label = "my_texture"
            };

            struct rjd_result result = rjd_gfx_texture_create(app->gfx.context, app->gfx.texture, desc);
            if (!rjd_result_isok(result))
            {
                RJD_LOG("Error loading texture: %s", result.error);
            }
        }

        // shader
        {
//            system("pwd");

			const char* filename = rjd_gfx_backend_ismetal() ? "Shaders.metal" : "../bin/shader/quad.shader";
            char* data = 0;
            struct rjd_result result = rjd_fio_read(filename, &data, app->allocator);
            RJD_ASSERTMSG(rjd_result_isok(result), "Error loading shader file '%s': %s", filename, result.error);
            rjd_array_push(data, '\0');

            struct rjd_gfx_shader_desc desc = {
                .data = data,
                .count_data = rjd_array_count(data) - 1,
            };
            
            result = rjd_gfx_shader_create(app->gfx.context, app->gfx.shader, desc);
            if (!rjd_result_isok(result)) {
                RJD_LOG("Error creating shader: %s", result.error);
            }
            rjd_array_free(data);
        }

        // pipeline state
        {
            struct rjd_gfx_vertex_format_attribute vertex_attributes[] =
            {
                // position
                {
                    .type = RJD_GFX_VERTEX_FORMAT_TYPE_FLOAT3,
                    .step = RJD_GFX_VERTEX_FORMAT_STEP_VERTEX,
                    .attribute_index = 0,
                    .buffer_index = 0,
                    .stride = sizeof(rjd_math_float3),
                    .step_rate = 1,
                    .offset = 0,
                },
                // texcoord
                {
                    .type = RJD_GFX_VERTEX_FORMAT_TYPE_FLOAT2,
                    .step = RJD_GFX_VERTEX_FORMAT_STEP_VERTEX,
                    .attribute_index = 1,
                    .buffer_index = 1,
                    .stride = sizeof(rjd_math_float2),
                    .step_rate = 1,
                },
                // tint
//                {
//                    .type = RJD_GFX_VERTEX_FORMAT_TYPE_FLOAT4,
//                    .step = RJD_GFX_VERTEX_FORMAT_STEP_VERTEX,
//                    .attribute_index = 2,
//                    .buffer_index = 2,
//                    .stride = sizeof(rjd_math_float4),
//                    .step_rate = 1,
//                },
            };
            
            struct rjd_gfx_pipeline_state_desc desc = {
                .debug_name = "2D Pipeline",
                .shader = *app->gfx.shader,
                .render_target = RJD_GFX_TEXTURE_BACKBUFFER,
                .depthstencil_target = RJD_GFX_TEXTURE_BACKBUFFER,
                .vertex_attributes = vertex_attributes,
                .count_vertex_attributes = rjd_countof(vertex_attributes),
                .depth_compare = RJD_GFX_DEPTH_COMPARE_GREATEREQUAL,
            };
            struct rjd_result result = rjd_gfx_pipeline_state_create(app->gfx.context, app->gfx.pipeline_state, desc);
            if (!rjd_result_isok(result)) {
                RJD_LOG("Error creating pipeline state: %s", result.error);
            }
        }

        // vertexed mesh
        {
            const struct rjd_window_size size = rjd_window_size_get(window);
            const rjd_math_float3 positions[] =
            {
                // triangle across the whole window
                rjd_math_float3_xyz(size.width, size.height, -10.0),
                rjd_math_float3_xyz(0, size.height, -10.0),
                rjd_math_float3_xyz(size.width / 2, 0, -10.0),
                
                // screenspace triangle
//                rjd_math_float3_xyz(1, -1, 1),
//                rjd_math_float3_xyz(0, 1, 1),
//                rjd_math_float3_xyz(-1, -1, 1),
            };
            
            const rjd_math_float2 uvs[] =
            {
                rjd_math_float2_xy(0.0f, 0.0f),
                rjd_math_float2_xy(1.0f, 1.0f),
                rjd_math_float2_xy(0.0f, 1.0f),
//                rjd_math_float2_xy(0.0f, 0.0f),
//                rjd_math_float2_xy(1.0f, 0.0f),
//                rjd_math_float2_xy(1.0f, 1.0f),
            };
            
            struct rjd_gfx_mesh_vertex_buffer_desc buffers_desc[] =
            {
                {
                    .type = RJD_GFX_MESH_BUFFER_TYPE_VERTEX,
                    .common = {
                        .vertex = {
                            .data = positions,
                            .length = sizeof(positions),
                        }
                    },
                    .usage_flags = RJD_GFX_MESH_BUFFER_USAGE_VERTEX,
                    .buffer_index = 0,
                },
                {
                    .type = RJD_GFX_MESH_BUFFER_TYPE_VERTEX,
                    .common = {
                        .vertex = {
                            .data = uvs,
                            .length = sizeof(uvs),
                        }
                    },
                    .usage_flags = RJD_GFX_MESH_BUFFER_USAGE_VERTEX,
                    .buffer_index = 1,
                },
                // uniforms
                {
                    .type = RJD_GFX_MESH_BUFFER_TYPE_UNIFORMS,
                    .common = {
                        .uniforms = {
                            .capacity = rjd_math_maxu32(sizeof(struct uniforms), 256) * 3,
                        }
                    },
                    .usage_flags = RJD_GFX_MESH_BUFFER_USAGE_VERTEX | RJD_GFX_MESH_BUFFER_USAGE_FRAGMENT,
                    .buffer_index = 2,
                }
            };
            
            struct rjd_gfx_mesh_vertexed_desc desc =
            {
                .primitive = RJD_GFX_PRIMITIVE_TYPE_TRIANGLES,
                .buffers = buffers_desc,
                .count_buffers = rjd_countof(buffers_desc),
                .count_vertices = rjd_countof(positions),
            };

            struct rjd_result result = rjd_gfx_mesh_create_vertexed(app->gfx.context, app->gfx.mesh, desc, app->allocator);
            if (!rjd_result_isok(result)) {
                RJD_LOG("Error creating mesh: %s", result.error);
            }
        }
    }
 
    {
        struct rjd_result result = rjd_gfx_present(app->gfx.context);
        if (!rjd_result_isok(result)) {
            RJD_LOG("Failed to present: %s", result.error);
        }
    }

    if (!rjd_gfx_vsync_try_enable(app->gfx.context)) {
        RJD_LOG("No VSync support detected.");
    }
}

void window_update(struct rjd_window* window, const struct rjd_window_environment* env)
{
    struct app_data* app = env->userdata;

    {
        struct rjd_result result = rjd_gfx_wait_for_frame_begin(app->gfx.context);
        if (!rjd_result_isok(result)) {
            RJD_LOG("Failed to wait for frame begin: %s", result.error);
        }
    }

    struct rjd_gfx_command_buffer command_buffer = {0};
    {
        struct rjd_result result = rjd_gfx_command_buffer_create(app->gfx.context, &command_buffer);
        if (!rjd_result_isok(result)) {
            RJD_LOG("failed to create command buffer: %s", result.error);
            return;
        }
    }

    {
        struct rjd_gfx_pass_begin_desc begin = {
            .render_target = RJD_GFX_TEXTURE_BACKBUFFER,
            .clear_color = rjd_gfx_format_make_color_u8_rgba(0, 0, 0, 0),
            .clear_depthstencil = rjd_gfx_format_make_depthstencil_f32_d32(0.0f),
            .debug_label = "single quad render pass",
        };
        struct rjd_result result = rjd_gfx_command_pass_begin(app->gfx.context, &command_buffer, &begin);
        if (!rjd_result_isok(result)) {
            RJD_LOG("failed to create command buffer: %s", result.error);
            return;
        }
    }

	// draw a quad
    {
        // update uniform transforms
        {
            static float x = 0;
            static float y = 0;
//            x += .5f;
//            y += 1.0f;
            x = fmodf(x, 100.0f);
            y = fmodf(y, 100.0f);
            
            struct rjd_gfx_camera camera = rjd_gfx_camera_init(RJD_GFX_CAMERA_MODE_ORTHOGRAPHIC);
            camera.pos = rjd_math_float3_xyz(x, y, 0.0f);
            
            const struct rjd_window_size bounds = rjd_window_size_get(window);
            
            rjd_math_mat4 proj_matrix = rjd_math_mat4_ortho_righthanded(0.0f, bounds.width, 0, bounds.height, 0.1, 100.0f);
            rjd_math_mat4 view_matrix = rjd_gfx_camera_lookat_ortho_righthanded(&camera);
            rjd_math_mat4 model_matrix = rjd_math_mat4_identity();
            
            rjd_math_mat4 modelview_matrix = rjd_math_mat4_mul(view_matrix, model_matrix);
            
            rjd_math_mat4 matrices[] = { proj_matrix, modelview_matrix };
            
            // TODO fix hacky frame_index
            static uint32_t frame_index = 0;
            uint32_t buffer_index = 2;
            uint32_t offset = frame_index * rjd_math_maxu32(sizeof(struct uniforms), 256);
            frame_index = (frame_index + 1) % 3;
            
            rjd_gfx_mesh_modify(app->gfx.context, app->gfx.mesh, buffer_index, offset, matrices, sizeof(matrices));
        }

        struct rjd_window_size window_size = rjd_window_size_get(app->window);
        struct rjd_gfx_viewport viewport = {
            .width = window_size.width,
            .height = window_size.height
        };

        const uint32_t texture_indices[] = {0};

        struct rjd_gfx_pass_draw_desc desc = {
            .viewport = &viewport,
            .pipeline_state = app->gfx.pipeline_state,
            .meshes = app->gfx.mesh,
            .textures = app->gfx.texture,
            .texture_indices = texture_indices,
            .count_meshes = 1,
            .count_textures = 1,
            .winding_order = RJD_GFX_WINDING_ORDER_CLOCKWISE,
            .cull_mode = RJD_GFX_CULL_BACK,
            .debug_label = "a quad",
        };

        rjd_gfx_command_pass_draw(app->gfx.context, &command_buffer, &desc);
    }

    rjd_gfx_command_buffer_commit(app->gfx.context, &command_buffer);

	rjd_gfx_present(app->gfx.context);
}

void window_close(struct rjd_window* window, const struct rjd_window_environment* env)
{
	struct app_data* app = env->userdata;

    if (rjd_slot_isvalid(app->gfx.mesh->handle)) {
        rjd_gfx_mesh_destroy(app->gfx.context, app->gfx.mesh);
    }
    if (rjd_slot_isvalid(app->gfx.texture->handle)) {
        rjd_gfx_texture_destroy(app->gfx.context, app->gfx.texture);
    }
	rjd_gfx_pipeline_state_destroy(app->gfx.context, app->gfx.pipeline_state);
	rjd_gfx_shader_destroy(app->gfx.context, app->gfx.shader);
	rjd_gfx_context_destroy(app->gfx.context);

	rjd_mem_free(app->gfx.context);
	rjd_mem_free(app->gfx.texture);
	rjd_mem_free(app->gfx.shader);
	rjd_mem_free(app->gfx.pipeline_state);
	rjd_mem_free(app->gfx.mesh);

	rjd_mem_free(app->window);
	rjd_mem_free(app->allocator);
}

