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
			const char* filename = "Shaders.metal";
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
                    .stride = sizeof(float) * 3,
                    .step_rate = 1,
                    .offset = 0,
                },
                // tint
                {
                    .type = RJD_GFX_VERTEX_FORMAT_TYPE_FLOAT4,
                    .step = RJD_GFX_VERTEX_FORMAT_STEP_VERTEX,
                    .attribute_index = 1,
                    .buffer_index = 1,
                    .stride = sizeof(float) * 4,
                    .step_rate = 1,
                },
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
			const float shape_size = 1;
			const uint32_t tesselation = 16;

			//uint32_t num_verts = rjd_procgeo_rect_calc_num_verts();
			//float* positions = rjd_mem_alloc_array(float, num_verts * 3, app->allocator);
			//rjd_procgeo_rect(shape_size, shape_size, positions, num_verts * 3);

			//uint32_t num_verts = rjd_procgeo_circle_calc_num_verts(tesselation);
			//float* positions = rjd_mem_alloc_array(float, num_verts * 3, app->allocator);
			//rjd_procgeo_circle(shape_size / 2, tesselation, positions, num_verts * 3);

			//const uint32_t num_verts = rjd_procgeo_box_calc_num_verts();
			//float* positions = rjd_mem_alloc_array(float, num_verts * 3, app->allocator);
			//rjd_procgeo_box(shape_size, shape_size, shape_size, positions, num_verts * 3);

			//const uint32_t num_verts = rjd_procgeo_cone_calc_num_verts(tesselation);
			//float* positions = rjd_mem_alloc_array(float, num_verts * 3, app->allocator);
			//rjd_procgeo_cone(shape_size, shape_size / 2, tesselation, positions, num_verts * 3);

			//const uint32_t num_verts = rjd_procgeo_cylinder_calc_num_verts(tesselation);
			//float* positions = rjd_mem_alloc_array(float, num_verts * 3, app->allocator);
			//rjd_procgeo_cylinder(shape_size, shape_size / 2, tesselation, positions, num_verts * 3);

			const uint32_t num_verts = rjd_procgeo_sphere_calc_num_verts(tesselation);
			float* positions = rjd_mem_alloc_array(float, num_verts * 3, app->allocator);
			rjd_procgeo_sphere(shape_size / 2, tesselation, positions, num_verts * 3);

			const rjd_math_vec4 k_red = rjd_math_vec4_xyzw(1,0,0,1);
			const rjd_math_vec4 k_green = rjd_math_vec4_xyzw(0,1,0,1);
			const rjd_math_vec4 k_blue = rjd_math_vec4_xyzw(0,0,1,1);

			float* tints = rjd_mem_alloc_array(float, num_verts * 4, app->allocator);
			for (uint32_t i = 0; i < num_verts * 4; i += 12)
			{
				rjd_math_vec4_write(k_red, tints + i);
				rjd_math_vec4_write(k_green, tints + i + 4);
				rjd_math_vec4_write(k_blue, tints + i + 8);
			}

            struct rjd_gfx_mesh_vertex_buffer_desc buffers_desc[] =
            {
                {
                    .type = RJD_GFX_MESH_BUFFER_TYPE_VERTEX,
                    .common = {
                        .vertex = {
                            .data = positions,
                            .length = num_verts * sizeof(float) * 3,
                        }
                    },
                    .usage_flags = RJD_GFX_MESH_BUFFER_USAGE_VERTEX,
                    .buffer_index = 0,
                },
				// tints
                {
                    .type = RJD_GFX_MESH_BUFFER_TYPE_VERTEX,
                    .common = {
                        .vertex = {
                            .data = tints,
                            .length = num_verts * sizeof(float) * 4,
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
                .count_vertices = num_verts,
            };

            struct rjd_result result = rjd_gfx_mesh_create_vertexed(app->gfx.context, app->gfx.mesh, desc, app->allocator);
            if (!rjd_result_isok(result)) {
                RJD_LOG("Error creating mesh: %s", result.error);
            }

			rjd_mem_free(positions);
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
            .debug_label = "shape render pass",
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
            const struct rjd_window_size bounds = rjd_window_size_get(window);

			rjd_math_vec3 pos, look, up;
			{
				const rjd_math_vec3 origin = rjd_math_vec3_xyz(0,0,0);
				const rjd_math_vec3 right = rjd_math_vec3_xyz(1,0,0);

				pos = rjd_math_vec3_xyz(0, 0, 100);
				look = rjd_math_vec3_normalize(rjd_math_vec3_sub(origin, pos));
				up = rjd_math_vec3_normalize(rjd_math_vec3_cross(right, look));

				//float* l = (float*)&look;
				//RJD_LOG("camera look: %.2f %.2f %.2f", l[0], l[1], l[2]);
			}

            //rjd_math_mat4 proj_matrix = rjd_math_mat4_ortho_righthanded(0.0f, bounds.width, 0, bounds.height, 0.1, 100.0f);
			const float y_field_of_view = RJD_MATH_PI*2*60/360;
			const float aspect = (float)bounds.width / bounds.height;
			const float near = 0.1f;
			const float far = 1000.0f;
            rjd_math_mat4 proj_matrix = rjd_math_mat4_perspective_righthanded(y_field_of_view, aspect, near, far);
            rjd_math_mat4 view_matrix = rjd_math_mat4_lookat_righthanded(pos, look, up);
            rjd_math_mat4 model_matrix;
			{
				static float s_rotation_x = 0;
				static float s_rotation_y = 5 * RJD_MATH_PI / 6.0f;
				//rjd_math_mat4 trans = rjd_math_mat4_translation(rjd_math_vec3_xyz(bounds.width / 2, bounds.height / 2, -10));
				rjd_math_mat4 trans = rjd_math_mat4_translation(rjd_math_vec3_xyz(0, 0, 0));
				rjd_math_mat4 rot1 = rjd_math_mat4_rotationx(s_rotation_x);
				rjd_math_mat4 rot2 = rjd_math_mat4_rotationy(s_rotation_y);
				model_matrix = rjd_math_mat4_mul(rot1, rot2);
				model_matrix = rjd_math_mat4_mul(trans, model_matrix);

				float speed = 8;
				s_rotation_x += (RJD_MATH_PI * 2.0f / (60.0f * 1 * speed));
				s_rotation_y += (RJD_MATH_PI * 2.0f / (60.0f * 2 * speed));
			}
            
            rjd_math_mat4 modelview_matrix = rjd_math_mat4_mul(view_matrix, model_matrix);
            
            rjd_math_mat4 matrices[] = { proj_matrix, modelview_matrix };
            
            // TODO fix hacky frame_index: we can do this by specifying the number of buffers we use for backbuffers.
			// Default in metal is triple-buffering which is why this works
            static uint32_t frame_index = 0;
            uint32_t buffer_index = 2;
            uint32_t offset = frame_index * rjd_math_maxu32(sizeof(struct uniforms), 256);
            frame_index = (frame_index + 1) % 3;
            
            rjd_gfx_mesh_modify(app->gfx.context, app->gfx.mesh, buffer_index, offset, matrices, sizeof(matrices));
        }

        const struct rjd_window_size window_size = rjd_window_size_get(app->window);
        const struct rjd_gfx_viewport viewport = {
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
            .count_textures = 0,
            .winding_order = RJD_GFX_WINDING_ORDER_CLOCKWISE,
            .cull_mode = RJD_GFX_CULL_BACK,
            //.cull_mode = RJD_GFX_CULL_FRONT,
            //.cull_mode = RJD_GFX_CULL_NONE,
            .debug_label = "a shape",
        };

        rjd_gfx_command_pass_draw(app->gfx.context, &command_buffer, &desc);
    }

    rjd_gfx_command_buffer_commit(app->gfx.context, &command_buffer);

	rjd_gfx_present(app->gfx.context);
}

void window_close(struct rjd_window* window, const struct rjd_window_environment* env)
{
	RJD_UNUSED_PARAM(window);

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

