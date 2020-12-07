#pragma once

#include "../rjd/rjd_all.h"

struct app_data
{
	struct rjd_mem_allocator* allocator;
	struct rjd_window* window;
	struct {
		struct rjd_gfx_context* context;
		struct rjd_gfx_shader* shader_vertex;
        struct rjd_gfx_shader* shader_pixel;
		struct rjd_gfx_pipeline_state* pipeline_state;
		struct rjd_gfx_mesh* mesh;
	} gfx;
};

void env_init(const struct rjd_window_environment* env);
void env_close(const struct rjd_window_environment* env);
void window_init(struct rjd_window* window, const struct rjd_window_environment* env);
bool window_update(struct rjd_window* window, const struct rjd_window_environment* env);
void window_close(struct rjd_window* window, const struct rjd_window_environment* env);
