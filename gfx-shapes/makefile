ifeq ($(OS), Windows_NT)
	PLATFORM_FLAGS := -D RJD_GFX_BACKEND_D3D11=1
	PLATFORM_FILES := main_win32.c
	OUTPUT_FILE := sample.exe
else
	#SHELL_NAME := $(shell uname -s)
	#ifeq($(SHELL_NAME), Darwin) #OSX
	#endif
	#ifeq($(SHELL_NAME), Linux) #Linux
	#endif

	PLATFORM_FLAGS := -fsanitize=undefined -fsanitize=address
	PLATFORM_FILES := *.m -framework Foundation -framework AppKit -framework Metal -framework MetalKit
	PLATFORM_DEFINES := -D RJD_GFX_BACKEND_METAL=1 
	OUTPUT_FILE := sample
endif

# -Wno-unused-local-typedefs to suppress locally defined typedefs coming from RJD_STATIC_ASSERT
CFLAGS := --std=c11 -pedantic -Wall -Wextra -g -march=native -Wno-unused-local-typedefs 
DEFINES := -D RJD_ENABLE_ASSERT=1 -D RJD_ENABLE_LOGGING=1 $(PLATFORM_DEFINES)

all:
	gcc $(CFLAGS) $(DEFINES) $(PLATFORM_FLAGS) sample.c $(PLATFORM_FILES) -o $(OUTPUT_FILE)

tags:
	ctags -f tags *

clean:
	rm *.exe
	rm *.ilk
	rm *.obj
	rm *.pdb
	rm -r Debug
	rm *.stackdump

