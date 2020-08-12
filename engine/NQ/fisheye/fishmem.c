#include "console.h"
#include "zone.h"
#include "mathlib.h"

#include <stdio.h>
#include "fishmem.h"

void freeIfExtant(struct _globe* globe, struct _lens* lens);

void createOrReallocBuffers(struct _globe* globe, struct _lens* lens,
			int area, int platesize){
	
	freeIfExtant(globe, lens);
	
	globe->pixelHeapLowMark = Hunk_LowMark();
	globe->pixels = (byte*)Hunk_AllocName(platesize*platesize * MAX_PLATES
		* sizeof(byte), "globe_px");
	lens->pixelHeapLowMark = Hunk_LowMark();
	lens->pixels = (byte**)Hunk_AllocName(area*sizeof(byte*), "lens_ppx");
	lens->pixelTintsLowMark = Hunk_LowMark();
	lens->pixel_tints = (byte*)Hunk_AllocName(area*sizeof(byte), "tints_px");
	
	// the rude way
	if(!globe->pixels || !lens->pixels || !lens->pixel_tints) {
	   Con_Printf("Quake-Lenses: could not allocate enough memory\n");
	   exit(1);
	}
}

// since zone.h uses a stack based allocator, this must be done in reverse order
void freeIfExtant(struct _globe* globe, struct _lens* lens){
	if(lens->pixel_tints) Hunk_FreeToLowMark(lens->pixelTintsLowMark);
	if(lens->pixels) Hunk_FreeToLowMark(lens->pixelHeapLowMark);
	if(globe->pixels) Hunk_FreeToLowMark(globe->pixelHeapLowMark);
}


