#pragma once

#include "rjd_wrapped.h"

struct app_data
{
	struct rjd_mem_allocator* allocator;
	struct rjd_window* window;
	struct rjd_input* input;
	struct {
		struct rjd_gfx_context* context;
		struct rjd_gfx_texture* texture;
		struct rjd_gfx_shader* shader;
		struct rjd_gfx_pipeline_state* pipeline_state;
		struct rjd_gfx_mesh* meshes;
	} gfx;
	uint32_t current_mesh_index;
};

void env_init(const struct rjd_window_environment* env);
void window_init(struct rjd_window* window, const struct rjd_window_environment* env);
void window_update(struct rjd_window* window, const struct rjd_window_environment* env);
void window_close(struct rjd_window* window, const struct rjd_window_environment* env);

