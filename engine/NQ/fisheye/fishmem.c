#include "console.h"
#include "zone.h"
#include "mathlib.h"

#include <stdio.h>
#include "fishmem.h"

void freeIfExtant(struct _globe* globe, struct _lens* lens);

static int lastHighMark = 0;
static int postVideoHighMark = 0;
static size_t lastSize = 0;

void createOrReallocBuffers(struct _globe* globe, struct _lens* lens,
			int area, int platesize){
   
   if(lastHighMark != 0 && Hunk_HighMark() > lastHighMark){
      return;
   } else if (Hunk_HighMark() == lastHighMark) {
      Hunk_FreeToHighMark(lastHighMark - (int)lastSize);
   }
   
   size_t globe_space = platesize * platesize * MAX_PLATES *
         sizeof(*(globe->pixels));
   globe_space &= 0xFFFFFFFFFF00;
   globe_space += 0x400;
     
   size_t lens_pixel_space = area * sizeof(*(lens->pixels));
   lens_pixel_space &= 0xFFFFFFFFFF00;
   globe_space += 0x400;
     
   size_t pixel_tints_space = area * sizeof(*(lens->pixel_tints));
   pixel_tints_space &= 0xFFFFFFFFFF00;
   pixel_tints_space += 400;
   
   int chonk_size = (int)(globe_space + lens_pixel_space + pixel_tints_space);
   
   postVideoHighMark = Hunk_HighMark();
   
   void* basePtr = Hunk_HighAllocName(chonk_size, "fisheye");
   
   void* lensPixelsPtr = basePtr + globe_space;
   
   void* tintsPtr = lensPixelsPtr + lens_pixel_space;
   
   globe->pixels = (byte*)basePtr;
  
   lens->pixels = (byte**)lensPixelsPtr;
   
   lens->pixel_tints = (byte*)tintsPtr;
	
   lastHighMark = Hunk_HighMark();
   
   lastSize = postVideoHighMark - lastHighMark;
}
