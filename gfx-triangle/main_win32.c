#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#define WIN32_EXTRA_LEAN
#include <windows.h>
#undef near
#undef far

#define RJD_IMPL 1
#include "rjd_wrapped.h"
#include "entrypoint.h"
#include <stdio.h>

int WINAPI WinMain(HINSTANCE handle_instance, HINSTANCE handle_instance_prev, LPSTR cmdline, int cmd_count)
{
	RJD_UNUSED_PARAM(handle_instance_prev);
    RJD_UNUSED_PARAM(cmdline);
    RJD_UNUSED_PARAM(cmd_count);

	struct app_data = {0};

	struct rjd_window_environment env = {
		.userdata = &appdata,
		.argc = cmd_count,
		.argv = cmdline,
	};

	struct rjd_result result = rjd_window_enter_windowed_environment(create_params, env);
	if (!rjd_result_isok(result)) {
		printf("exiting with error: %s\n", result.error);
		return 1;
	}

	return 0;
}
