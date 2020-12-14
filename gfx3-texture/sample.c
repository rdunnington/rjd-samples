#include <stdio.h>
#include <stdbool.h>
#include "app.h"

struct shader_constants
{
	rjd_math_mat4 proj_matrix;
	rjd_math_mat4 modelview_matrix;
};

uint32_t calc_shader_constants_stride()
{
	return rjd_gfx_calc_constant_buffer_stride(sizeof(struct shader_constants));
}

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

void env_close(const struct rjd_window_environment* env)
{
	struct app_data* app = env->userdata;
	rjd_mem_free(app->window);
}

void window_init(struct rjd_window* window, const struct rjd_window_environment* env)
{
	struct app_data* app = env->userdata;

	app->gfx.context = rjd_mem_alloc(struct rjd_gfx_context, app->allocator);
	app->gfx.shader_vertex = rjd_mem_alloc(struct rjd_gfx_shader, app->allocator);
	app->gfx.shader_pixel = rjd_mem_alloc(struct rjd_gfx_shader, app->allocator);
	app->gfx.pipeline_state = rjd_mem_alloc(struct rjd_gfx_pipeline_state, app->allocator);
	app->gfx.mesh = rjd_mem_alloc(struct rjd_gfx_mesh, app->allocator);
	app->gfx.texture = rjd_mem_alloc(struct rjd_gfx_texture, app->allocator);

	{
		struct rjd_gfx_context_desc desc = {
			.backbuffer_color_format = RJD_GFX_FORMAT_COLOR_U8_BGRA_NORM_SRGB,
			.backbuffer_depth_format = RJD_GFX_FORMAT_DEPTHSTENCIL_F32_D32,
			.allocator = app->allocator,
		};

		#if RJD_PLATFORM_WINDOWS
			desc.win32.hwnd = rjd_window_win32_get_hwnd(window);
		#elif RJD_PLATFORM_OSX
			desc.osx.view = rjd_window_osx_get_mtkview(window);
		#endif

		{
			struct rjd_result result = rjd_gfx_context_create(app->gfx.context, desc);
			if (!rjd_result_isok(result)) {
				RJD_LOG("Failed to create gfx context: %s", result.error);
				return;
			}
		}
	}

	// resources
	{
		// shaders
		{
			const char* filename = rjd_gfx_backend_ismetal() ? "Shaders.metal" : "shaders.hlsl";
			system("pwd");
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
					.shader_slot_d3d11 = 0,
					.shader_slot_metal = 0,
					.stride = sizeof(float) * 3,
					.step_rate = 1,
					.offset = 0,
				},
				// uv
				{
					.type = RJD_GFX_VERTEX_FORMAT_TYPE_FLOAT2,
					.step = RJD_GFX_VERTEX_FORMAT_STEP_VERTEX,
					.semantic = RJD_GFX_VERTEX_SEMANTIC_TEXCOORD,
					.attribute_index = 1,
					.shader_slot_d3d11 = 1,
					.shader_slot_metal = 1,
					.stride = sizeof(float) * 2,
					.step_rate = 1,
					.offset = 0,
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
				.depth_write_enabled = true,
	            .winding_order = RJD_GFX_WINDING_ORDER_CLOCKWISE,
	            .cull_mode = RJD_GFX_CULL_BACK,
			};
			struct rjd_result result = rjd_gfx_pipeline_state_create(app->gfx.context, app->gfx.pipeline_state, desc);
			if (!rjd_result_isok(result)) {
				RJD_LOG("Error creating pipeline state: %s", result.error);
			}
		}

		// cube mesh
		{
			enum
			{
				NUM_VERTS = 3 * 2 * 6, // 3 verts per tri, 2 tris per face, 6 faces
			};

			const float size = 0.5f;

			const float x = size;
			const float y = size;
			const float z = size;

			const float positions[NUM_VERTS * 3] = {
				// front
				-x,  y, z,
				-x, -y, z,
				 x, -y, z,
				-x,  y, z,
				 x, -y, z,
				 x,  y, z,
			
				// back
				-x,  y, -z,
				x,  -y, -z,
				-x, -y, -z,
				-x,  y, -z,
				x,   y, -z,
				x,  -y, -z,
			
				//  left
				-x, -y, -z,
				-x, -y,  z,
				-x,  y,  z,
				-x, -y, -z,
				-x,  y,  z,
				-x,  y, -z,
			
				//  right
				x, -y, -z,
				x,  y, -z,
				x,  y,  z,
				x, -y, -z,
				x,  y,  z,
				x, -y,  z,
			
				//  top
				 x,  y, -z,
				-x,  y, -z,
				-x,  y,  z,
				 x,  y, -z,
				-x,  y,  z,
				 x,  y,  z,
			
				//  bottom
				-x, -y,  z,
				-x, -y, -z,
				 x, -y, -z,
				-x, -y,  z,
				 x, -y, -z,
				 x, -y,  z,
			};

			float uvs[NUM_VERTS * 2] =
			{
				// front
				0.0f, 1.0f,
				0.0f, 0.0f,
				1.0f, 0.0f,
				0.0f, 1.0f,
				1.0f, 0.0f,
				1.0f, 1.0f,

				// back
				0.0f, 1.0f,
				1.0f, 0.0f,
				0.0f, 0.0f,
				0.0f, 1.0f,
				1.0f, 1.0f,
				1.0f, 0.0f,
			
				//  left
				0.0f, 0.0f,
				0.0f, 1.0f,
				1.0f, 1.0f,
				0.0f, 0.0f,
				1.0f, 1.0f,
				1.0f, 0.0f,
			
				//  right
				0.0f, 0.0f,
				1.0f, 0.0f,
				1.0f, 1.0f,
				0.0f, 0.0f,
				1.0f, 1.0f,
				0.0f, 1.0f,

				//  top
				1.0f, 0.0f,
				0.0f, 0.0f,
				0.0f, 1.0f,
				1.0f, 0.0f,
				0.0f, 1.0f,
				1.0f, 1.0f,

				//  bottom
				0.0f, 1.0f,
				0.0f, 0.0f,
				1.0f, 0.0f,
				0.0f, 1.0f,
				1.0f, 0.0f,
				1.0f, 1.0f,
			};

			struct rjd_gfx_mesh_buffer_desc buffers_desc[] =
			{
				// vertices
				{
					.common = {
						.vertex = {
							.data = positions,
							.length = sizeof(positions),
							.stride = sizeof(float) * 3,
						}
					},
					.usage_flags = RJD_GFX_MESH_BUFFER_USAGE_VERTEX,
					.shader_slot_metal = 0,
					.shader_slot_d3d11 = 0,
				},
				// uv
				{
					.common = {
						.vertex = {
							.data = uvs,
							.length = sizeof(uvs),
							.stride = sizeof(float) * 2,
						}
					},
					.usage_flags = RJD_GFX_MESH_BUFFER_USAGE_VERTEX,
					.shader_slot_metal = 1,
					.shader_slot_d3d11 = 1,
				},
				// constants
				{
					.common = {
						.constant = {
							.capacity = calc_shader_constants_stride() * 3,
						}
					},
					.usage_flags = RJD_GFX_MESH_BUFFER_USAGE_VERTEX_CONSTANT,
					.shader_slot_metal = 2,
					.shader_slot_d3d11 = 0,
				}
			};

			struct rjd_gfx_mesh_vertexed_desc desc =
			{
				.primitive = RJD_GFX_PRIMITIVE_TYPE_TRIANGLES,
				.buffers = buffers_desc,
				.count_buffers = rjd_countof(buffers_desc),
				.count_vertices = NUM_VERTS,
			};

			struct rjd_result result = rjd_gfx_mesh_create_vertexed(app->gfx.context, app->gfx.mesh, desc);
			if (!rjd_result_isok(result)) {
				RJD_LOG("Error creating mesh: %s", result.error);
			}
		}

		// checkered texture
		{
			const uint32_t white = 0xFFFFFFFF;
			const uint32_t black = 0xFF000000;

			enum
			{
				height = 64,
				width = 64,
			};

			uint32_t color[width * height] = {0};
			for (size_t y = 0; y < height; ++y) {
				bool offset = (y % 8 < 4) ? false : true;
				for (size_t x = 0; x < width; ++x) {
					const uint32_t color1 = offset ? white : black;
					const uint32_t color2 = offset ? black : white;
					const uint32_t c = (x % 8 < 4) ? color1 : color2;
					color[y * width + x] = c;
				}
			}

			struct rjd_gfx_texture_desc desc = {
				.data = color,
				.data_length = sizeof(color),
				.pixels_width = width,
				.pixels_height = height,
				.format = RJD_GFX_FORMAT_COLOR_U8_BGRA_NORM,
				.msaa_samples = 1,
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
			.clear_color = rjd_gfx_format_make_color_u8_rgba(64, 64, 64, 255),
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
		struct rjd_gfx_pass_draw_buffer_offset_desc buffer_offset_descs[1] = {0};
		
		// update constant buffer transforms
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
			
			const struct shader_constants constants = {
				.proj_matrix = proj_matrix,
				.modelview_matrix = rjd_math_mat4_mul(view_matrix, model_matrix)
			};
			
			// Upload matrices to constant buffer
			const uint32_t buffer_index = 2;
			const uint32_t stride = calc_shader_constants_stride();
			const uint32_t offset = rjd_gfx_backbuffer_current_index(app->gfx.context) * stride;

			rjd_gfx_mesh_modify(app->gfx.context, &command_buffer, app->gfx.mesh, buffer_index, offset, &constants, sizeof(constants));

			buffer_offset_descs[0].mesh_index = 0;
			buffer_offset_descs[0].buffer_index = buffer_index;
			buffer_offset_descs[0].offset_bytes = offset;
			buffer_offset_descs[0].range_bytes = stride;
		}

		const struct rjd_window_size window_size = rjd_window_size_get(app->window);
		const struct rjd_gfx_viewport viewport = {
			.width = window_size.width,
			.height = window_size.height
		};

		uint32_t texture_indices[1] = {0};

		struct rjd_gfx_pass_draw_desc desc = {
			.viewport = &viewport,
			.pipeline_state = app->gfx.pipeline_state,
			.meshes = app->gfx.mesh,
			.buffer_offset_descs = buffer_offset_descs,
			.textures = app->gfx.texture,
			.texture_indices = texture_indices,
			.count_meshes = 1,
			.count_constant_descs = 1,
			.count_textures = 1,
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

	rjd_gfx_mesh_destroy(app->gfx.context, app->gfx.mesh);
	rjd_gfx_pipeline_state_destroy(app->gfx.context, app->gfx.pipeline_state);
	rjd_gfx_shader_destroy(app->gfx.context, app->gfx.shader_vertex);
	rjd_gfx_shader_destroy(app->gfx.context, app->gfx.shader_pixel);
	rjd_gfx_context_destroy(app->gfx.context);

	rjd_mem_free(app->gfx.context);
	rjd_mem_free(app->gfx.shader_vertex);
	rjd_mem_free(app->gfx.shader_pixel);
	rjd_mem_free(app->gfx.pipeline_state);
	rjd_mem_free(app->gfx.mesh);
}

