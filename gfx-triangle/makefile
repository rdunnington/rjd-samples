ifeq ($(OS), Windows_NT)
	PLATFORM_FLAGS :=
	PLATFORM_FILES := tests_rjd.c
	OUTPUT_FILE := sample.exe
else
	#SHELL_NAME := $(shell uname -s)
	#ifeq($(SHELL_NAME), Darwin) #OSX
	#endif
	#ifeq($(SHELL_NAME), Linux) #Linux
	#endif

	PLATFORM_FLAGS := -fsanitize=undefined -fsanitize=address  
	PLATFORM_FILES := *.m -framework Foundation -framework AppKit -framework Metal -framework MetalKit
	OUTPUT_FILE := sample
endif

all:
	# -Wno-unused-local-typedefs to suppress locally defined typedefs coming from RJD_STATIC_ASSERT
	gcc --std=c11 -pedantic -Wall -Wextra -g -march=native -Wno-unused-local-typedefs $(PLATFORM_FLAGS) sample.c $(PLATFORM_FILES) -o $(OUTPUT_FILE)

tags:
	ctags -f tags *

clean:
	rm *.exe
	rm *.ilk
	rm *.obj
	rm *.pdb
	rm -r Debug
	rm *.stackdump

