#include <stdio.h>
#include <stdbool.h>

#include "app.h"

#include <errno.h>

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

	struct rjd_result result = rjd_window_create(app->window, window_desc);
	if (!rjd_result_isok(result)) {
		RJD_LOG("Failed to create window: %s", result.error);
		return;
	}

	rjd_window_runloop(app->window);
}

void window_init(struct rjd_window* window, const struct rjd_window_environment* env)
{
	struct app_data* app = env->userdata;

	app->input = rjd_mem_alloc(struct rjd_input, app->allocator);
	rjd_input_create(app->input, app->allocator);
	rjd_input_hook(app->input, window, env);

	app->gfx.context = rjd_mem_alloc(struct rjd_gfx_context, app->allocator);
	app->gfx.texture = rjd_mem_alloc(struct rjd_gfx_texture, app->allocator);
	app->gfx.shader_vertex = rjd_mem_alloc(struct rjd_gfx_shader, app->allocator);
	app->gfx.shader_pixel = rjd_mem_alloc(struct rjd_gfx_shader, app->allocator);
	app->gfx.pipeline_state = rjd_mem_alloc(struct rjd_gfx_pipeline_state, app->allocator);
	app->gfx.meshes = rjd_mem_alloc_array(struct rjd_gfx_mesh, RJD_PROCGEO_TYPE_COUNT, app->allocator);

	{
		uint32_t msaa_sample_counts[] = { 16, 8, 4, 2, 1 };
		
		struct rjd_gfx_context_desc desc = {
			.backbuffer_color_format = RJD_GFX_FORMAT_COLOR_U8_BGRA_NORM_SRGB,
			.backbuffer_depth_format = RJD_GFX_FORMAT_DEPTHSTENCIL_F32_D32,
			.optional_desired_msaa_samples = msaa_sample_counts, // TODO make a separate MSAA sample based on the triangle sample
			.count_desired_msaa_samples = rjd_countof(msaa_sample_counts),
			.allocator = app->allocator,
		};

		#if RJD_PLATFORM_WINDOWS
			desc.win32.hwnd = rjd_window_win32_get_hwnd(window);
		#elif RJD_PLATFORM_OSX
			desc.osx.view = rjd_window_osx_get_mtkview(window);
		#endif

		struct rjd_result result = rjd_gfx_context_create(app->gfx.context, desc);
		if (!rjd_result_isok(result)) {
			RJD_LOG("Failed to create gfx context: %s", result.error);
			return;
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

		// shaders
		{
			const char* filename = rjd_gfx_backend_ismetal() ? "Shaders.metal" : "shaders.hlsl";
			char* data = 0;
			{
				struct rjd_result result = rjd_fio_read(filename, &data, app->allocator);
				RJD_ASSERTMSG(rjd_result_isok(result), "Error loading shader file '%s': %s", filename, result.error);
				rjd_array_push(data, '\0');
			}

			{
				struct rjd_gfx_shader_desc desc = {
					.source_name = filename,
					.function_name = "vertexShader",
					.data = data,
					.count_data = rjd_array_count(data) - 1,
					.type = RJD_GFX_SHADER_TYPE_VERTEX,
				};
				
				struct rjd_result result = rjd_gfx_shader_create(app->gfx.context, app->gfx.shader_vertex, desc);
				if (!rjd_result_isok(result)) {
					RJD_LOG("Error creating shader: %s", result.error);
				}
			}
			
			{
				struct rjd_gfx_shader_desc desc = {
					.source_name = filename,
					.function_name = "pixelShader",
					.data = data,
					.count_data = rjd_array_count(data) - 1,
					.type = RJD_GFX_SHADER_TYPE_PIXEL,
				};
				
				struct rjd_result result = rjd_gfx_shader_create(app->gfx.context, app->gfx.shader_pixel, desc);
				if (!rjd_result_isok(result)) {
					RJD_LOG("Error creating shader: %s", result.error);
				}
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
					.semantic = RJD_GFX_VERTEX_SEMANTIC_POSITION,
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
					.semantic = RJD_GFX_VERTEX_SEMANTIC_COLOR,
					.attribute_index = 1,
					.buffer_index = 1,
					.stride = sizeof(float) * 4,
					.step_rate = 1,
				},
			};
			
			struct rjd_gfx_pipeline_state_desc desc = {
				.debug_name = "2D Pipeline",
				.shader_vertex = *app->gfx.shader_vertex,
				.shader_pixel = *app->gfx.shader_pixel,
				.render_target = RJD_GFX_TEXTURE_BACKBUFFER,
				.depthstencil_target = RJD_GFX_TEXTURE_BACKBUFFER,
				.vertex_attributes = vertex_attributes,
				.count_vertex_attributes = rjd_countof(vertex_attributes),
				.depth_compare = RJD_GFX_DEPTH_COMPARE_GREATEREQUAL,
	            .winding_order = RJD_GFX_WINDING_ORDER_CLOCKWISE,
	            .cull_mode = RJD_GFX_CULL_BACK,
			};
			struct rjd_result result = rjd_gfx_pipeline_state_create(app->gfx.context, app->gfx.pipeline_state, desc);
			if (!rjd_result_isok(result)) {
				RJD_LOG("Error creating pipeline state: %s", result.error);
			}
		}

		// meshes
		for (enum rjd_procgeo_type geo = 0; geo < RJD_PROCGEO_TYPE_COUNT; ++geo)
		{
			const float shape_size = .5;
			const uint32_t tesselation = 16;
 
			const uint32_t num_verts = rjd_procgeo_calc_num_verts(geo, tesselation);
			float* positions = rjd_mem_alloc_array(float, num_verts * 3, app->allocator);
			rjd_procgeo(geo, tesselation, shape_size, shape_size, shape_size, positions, num_verts * 3, 0);

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
							.stride = sizeof(float) * 3,
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
							.stride = sizeof(float) * 4,
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
					.usage_flags = RJD_GFX_MESH_BUFFER_USAGE_VERTEX | RJD_GFX_MESH_BUFFER_USAGE_PIXEL,
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

			struct rjd_result result = rjd_gfx_mesh_create_vertexed(app->gfx.context, app->gfx.meshes + geo, desc);
			if (!rjd_result_isok(result)) {
				RJD_LOG("Error creating mesh: %s", result.error);
			}

			rjd_mem_free(positions);
			rjd_mem_free(tints);
		}
	}

	{
		struct rjd_result result = rjd_gfx_present(app->gfx.context);
		if (!rjd_result_isok(result)) {
			RJD_LOG("Failed to present: %s", result.error);
		}
	}
}

bool window_update(struct rjd_window* window, const struct rjd_window_environment* env)
{
	struct app_data* app = env->userdata;

	{
		struct rjd_result result = rjd_gfx_wait_for_frame_begin(app->gfx.context);
		if (!rjd_result_isok(result)) {
			RJD_LOG("Failed to wait for frame begin: %s", result.error);
		}
	}

	if (rjd_input_keyboard_triggered(app->input, RJD_INPUT_KEYBOARD_ARROW_LEFT) ||
		rjd_input_mouse_triggered(app->input, RJD_INPUT_MOUSE_BUTTON_RIGHT)) {
		app->current_mesh_index = (app->current_mesh_index + RJD_PROCGEO_TYPE_COUNT - 1) % RJD_PROCGEO_TYPE_COUNT;
	}

	if (rjd_input_keyboard_triggered(app->input, RJD_INPUT_KEYBOARD_ARROW_RIGHT) ||
		rjd_input_mouse_triggered(app->input, RJD_INPUT_MOUSE_BUTTON_LEFT)) {
		app->current_mesh_index = (app->current_mesh_index + 1) % RJD_PROCGEO_TYPE_COUNT;
	}

	rjd_input_markframe(app->input);

	struct rjd_gfx_command_buffer command_buffer = {0};
	{
		struct rjd_result result = rjd_gfx_command_buffer_create(app->gfx.context, &command_buffer);
		if (!rjd_result_isok(result)) {
			RJD_LOG("failed to create command buffer: %s", result.error);
			return false;
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
			return false;
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
			}

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
			
			rjd_gfx_mesh_modify(app->gfx.context, &command_buffer, app->gfx.meshes + app->current_mesh_index, buffer_index, offset, matrices, sizeof(matrices));
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
			.meshes = app->gfx.meshes + app->current_mesh_index,
			.textures = app->gfx.texture,
			.texture_indices = texture_indices,
			.count_meshes = 1,
			.count_textures = 0,
			.debug_label = "a shape",
		};

		rjd_gfx_command_pass_draw(app->gfx.context, &command_buffer, &desc);
	}

	rjd_gfx_command_buffer_commit(app->gfx.context, &command_buffer);

	rjd_gfx_present(app->gfx.context);

	return true;
}

void window_close(struct rjd_window* window, const struct rjd_window_environment* env)
{
	RJD_UNUSED_PARAM(window);

	struct app_data* app = env->userdata;

	rjd_input_destroy(app->input);
	rjd_mem_free(app->input);

	for (uint32_t i = 0; i < RJD_PROCGEO_TYPE_COUNT; ++i) {
		if (rjd_slot_isvalid(app->gfx.meshes[i].handle)) {
			rjd_gfx_mesh_destroy(app->gfx.context, app->gfx.meshes + i);
		}
	}

	if (rjd_slot_isvalid(app->gfx.texture->handle)) {
		rjd_gfx_texture_destroy(app->gfx.context, app->gfx.texture);
	}
	rjd_gfx_pipeline_state_destroy(app->gfx.context, app->gfx.pipeline_state);
	rjd_gfx_shader_destroy(app->gfx.context, app->gfx.shader_vertex);
	rjd_gfx_shader_destroy(app->gfx.context, app->gfx.shader_pixel);
	rjd_gfx_context_destroy(app->gfx.context);

	rjd_mem_free(app->gfx.context);
	rjd_mem_free(app->gfx.texture);
	rjd_mem_free(app->gfx.shader_vertex);
	rjd_mem_free(app->gfx.shader_pixel);
	rjd_mem_free(app->gfx.pipeline_state);
	rjd_mem_free(app->gfx.meshes);

	rjd_mem_free(app->window);
	rjd_mem_free(app->allocator);
}

