#include <stdio.h>

#include "m_argv.h"

#include "doomgeneric.h"

pixel_t* DG_ScreenBuffer = NULL;

void M_FindResponseFile(void);
void D_DoomMain (void);


void doomgeneric_Create(int argc, char **argv)
{
	DG_Log("doomgeneric_Create: start (argc=%d)", argc);
	for(int i = 0; i < argc; i++) {
		if(argv != NULL && argv[i] != NULL) {
			DG_Log("doomgeneric_Create: argv[%d]=\"%s\"", i, argv[i]);
		} else {
			DG_Log("doomgeneric_Create: argv[%d]=<null>", i);
		}
	}

	// save arguments
    myargc = argc;
    myargv = argv;
	DG_Log("doomgeneric_Create: arguments stored");

	M_FindResponseFile();
	DG_Log("doomgeneric_Create: response files processed");

	DG_ScreenBuffer = malloc(DOOMGENERIC_RESX * DOOMGENERIC_RESY * 4);
	if(DG_ScreenBuffer == NULL) {
		DG_Log("doomgeneric_Create: failed to allocate screen buffer (%dx%d)", DOOMGENERIC_RESX, DOOMGENERIC_RESY);
	} else {
		DG_Log("doomgeneric_Create: screen buffer allocated (%zu bytes)", (size_t)(DOOMGENERIC_RESX * DOOMGENERIC_RESY * 4));
	}

	DG_Init();
	DG_Log("doomgeneric_Create: DG_Init completed");

	D_DoomMain ();
	DG_Log("doomgeneric_Create: D_DoomMain returned");
}
