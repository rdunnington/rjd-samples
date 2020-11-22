// #include <stdio.h>
// #include <stdbool.h>
#include <errno.h>

#include "app.h"


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

	app->gfx.context = rjd_mem_alloc(struct rjd_gfx_context, app->allocator);
	app->gfx.shader_vertex = rjd_mem_alloc(struct rjd_gfx_shader, app->allocator);
	app->gfx.shader_pixel = rjd_mem_alloc(struct rjd_gfx_shader, app->allocator);
	app->gfx.pipeline_state = rjd_mem_alloc(struct rjd_gfx_pipeline_state, app->allocator);
	app->gfx.mesh = rjd_mem_alloc(struct rjd_gfx_mesh, app->allocator);

	{
		struct rjd_gfx_context_desc desc = {
			.backbuffer_color_format = RJD_GFX_FORMAT_COLOR_U8_BGRA_NORM_SRGB,
			.backbuffer_depth_format = RJD_GFX_FORMAT_DEPTHSTENCIL_F32_D32,
			.optional_desired_msaa_samples = NULL,
			.count_desired_msaa_samples = 0,
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
				// .cull_mode = RJD_GFX_CULL_BACK,
				.cull_mode = RJD_GFX_CULL_NONE,
			};
			struct rjd_result result = rjd_gfx_pipeline_state_create(app->gfx.context, app->gfx.pipeline_state, desc);
			if (!rjd_result_isok(result)) {
				RJD_LOG("Error creating pipeline state: %s", result.error);
			}
		}

		// vertexed mesh
		{
			const float p = 0.8f;
			const float positions[] =
			{
				0, p, .5,
				p, -p, .5,
				-p, -p, .5,
			};
			const float tints[] = 
			{
				1,0,0,1,
				0,1,0,1,
				0,0,1,1,
			};

			struct rjd_gfx_mesh_vertex_buffer_desc buffers_desc[] =
			{
				{
					.type = RJD_GFX_MESH_BUFFER_TYPE_VERTEX,
					.common = {
						.vertex = {
							.data = positions,
							.length = sizeof(positions),
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
							.length = sizeof(tints),
							.stride = sizeof(float) * 4,
						}
					},
					.usage_flags = RJD_GFX_MESH_BUFFER_USAGE_VERTEX,
					.buffer_index = 1,
				},
			};

			struct rjd_gfx_mesh_vertexed_desc desc =
			{
				.primitive = RJD_GFX_PRIMITIVE_TYPE_TRIANGLES,
				.buffers = buffers_desc,
				.count_buffers = rjd_countof(buffers_desc),
				.count_vertices = rjd_countof(positions) / 3,
			};

			struct rjd_result result = rjd_gfx_mesh_create_vertexed(app->gfx.context, app->gfx.mesh, desc);
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
}

bool window_update(struct rjd_window* window, const struct rjd_window_environment* env)
{
	RJD_UNUSED_PARAM(window);

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
			.clear_color = rjd_gfx_format_make_color_u8_rgba(0, 0, 0, 0),
			.clear_depthstencil = rjd_gfx_format_make_depthstencil_f32_d32(0.0f),
			.debug_label = "single quad render pass",
		};
		struct rjd_result result = rjd_gfx_command_pass_begin(app->gfx.context, &command_buffer, &begin);
		if (!rjd_result_isok(result)) {
			RJD_LOG("failed to begin pass: %s", result.error);
			return false;
		}
	}

	// draw a quad
	{
		const struct rjd_window_size window_size = rjd_window_size_get(app->window);
		const struct rjd_gfx_viewport viewport = {
			.width = window_size.width,
			.height = window_size.height
		};

		struct rjd_gfx_pass_draw_desc desc = {
			.viewport = &viewport,
			.pipeline_state = app->gfx.pipeline_state,
			.meshes = app->gfx.mesh,
			.textures = NULL,
			.texture_indices = NULL,
			.count_meshes = 1,
			.count_textures = 0,
			.debug_label = "a triangle",
		};

		struct rjd_result result = rjd_gfx_command_pass_draw(app->gfx.context, &command_buffer, &desc);
		if (!rjd_result_isok(result)) {
			RJD_LOG("Failed to draw: %s", result.error);
		}
	}

	struct rjd_result result = rjd_gfx_command_buffer_commit(app->gfx.context, &command_buffer);
	if (!rjd_result_isok(result)) {
		RJD_LOG("Failed to commit command buffer: %s", result.error);
	}

	result = rjd_gfx_present(app->gfx.context);
	if (!rjd_result_isok(result)) {
		RJD_LOG("Failed to present: %s", result.error);
	}

	return true;
}

void window_close(struct rjd_window* window, const struct rjd_window_environment* env)
{
	RJD_UNUSED_PARAM(window);

	struct app_data* app = env->userdata;

	if (rjd_slot_isvalid(app->gfx.mesh->handle)) {
		rjd_gfx_mesh_destroy(app->gfx.context, app->gfx.mesh);
	}
	rjd_gfx_pipeline_state_destroy(app->gfx.context, app->gfx.pipeline_state);
	rjd_gfx_shader_destroy(app->gfx.context, app->gfx.shader_vertex);
	rjd_gfx_shader_destroy(app->gfx.context, app->gfx.shader_pixel);
	rjd_gfx_context_destroy(app->gfx.context);

	rjd_mem_free(app->gfx.context);
	rjd_mem_free(app->gfx.shader_vertex);
	rjd_mem_free(app->gfx.shader_pixel);
	rjd_mem_free(app->gfx.pipeline_state);
	rjd_mem_free(app->gfx.mesh);

	rjd_mem_free(app->window);
	rjd_mem_free(app->allocator);
}

