
#define RJD_IMPL 1
#include "rjd_wrapped.h"
#include "entrypoint.h"
#include <stdio.h>

int main(int argc, const char* argv[]) 
{
	struct app_data app = {0};

	struct rjd_window_environment env = {
		.userdata = &app,
		.argc = argc,
		.argv = argv,
	};

	rjd_window_enter_windowed_environment(env, env_init);

	return 0;
}
