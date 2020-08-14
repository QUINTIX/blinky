#include "console.h"
#include "zone.h"
#include "mathlib.h"

#include <stdio.h>
#include "fishmem.h"

void freeIfExtant(struct _globe* globe, struct _lens* lens);

static int lastHighMark = 0;
static int postVideoHighMark = 0;

void createOrReallocBuffers(struct _globe* globe, struct _lens* lens,
			int area, int platesize){
   if(Hunk_HighMark() <= lastHighMark){
      Hunk_FreeToHighMark(postVideoHighMark);
   }
   
   postVideoHighMark = Hunk_HighMark();
   
   globe->pixels = (byte*)Hunk_HighAllocName(platesize*platesize * MAX_PLATES
          * sizeof(byte), "globe_px");
  
   lens->pixels = (byte**)Hunk_HighAllocName(area*sizeof(byte*), "lens_ppx");
  
   lens->pixel_tints = (byte*)Hunk_HighAllocName(area*sizeof(byte), "tints_px");
	
   lastHighMark = Hunk_HighMark();
}
