#define RJD_IMPL 1
#include "app.h"
#include <stdio.h>

int WINAPI WinMain(HINSTANCE handle_instance, HINSTANCE handle_instance_prev, LPSTR cmdline, int cmd_count)
{
	RJD_UNUSED_PARAM(handle_instance_prev);
	RJD_UNUSED_PARAM(cmd_count);

	struct app_data app = {0};

	const char* argv[] = { cmdline, 0};

	struct rjd_window_environment env = {
		.userdata = &app,
		.argc = rjd_countof(argv),
		.argv = argv,
		.win32 = {
			.hinstance = handle_instance,
		}
	};

	rjd_window_enter_windowed_environment(env, env_init, env_close);

	return 0;
}
