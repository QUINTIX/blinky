#include "console.h"
#include "zone.h"
#include "mathlib.h"

#include <stdio.h>
#include "fishmem.h"

static int lastHighMark = 0;
static int postVideoHighMark = 0;
static size_t lastSize = 0;

const size_t MASK_LAST_BYTE_OF_ADDR = ~(0xFF);
const size_t PADDING_TO_ADD = 0x100;

static qboolean freeIfExtant(int mark, int lastHighMark, int lastSize);
static size_t padToNext256bytes(size_t unpadded);
static qboolean hasHighMemContracted(void);
static qboolean hasResized(int width, int height);

#define NIL -1

qboolean hasResizedOrRestarted(int width, int height){
   return hasResized(width, height) || hasHighMemContracted();
}

void createOrReallocBuffers(struct _globe* globe, struct _lens* lens,
      int screenArea, int plateSideLength){
   
   qboolean needNewHunk = freeIfExtant(Hunk_HighMark(), lastHighMark,
                                       (int)lastSize);
   if(!needNewHunk){
      return;
   }
   size_t globe_space = padToNext256bytes(
          plateSideLength * plateSideLength * MAX_PLATES * sizeof(*(globe->pixels)) );
     
   size_t lens_pixel_space = padToNext256bytes(
          screenArea * sizeof(*(lens->pixels)) );
   
   size_t pixel_tints_space = padToNext256bytes(
          screenArea * sizeof(*(lens->pixel_tints)) );
   
   int chonk_size = (int)(globe_space + lens_pixel_space + pixel_tints_space);
   
   postVideoHighMark = Hunk_HighMark();
   
   void* basePtr = Hunk_HighAllocName(chonk_size, "fisheye");
   void* lensPixelsPtr = basePtr + globe_space;
   void* tintsPtr = lensPixelsPtr + lens_pixel_space;
   
   globe->pixels = (byte*)basePtr;
   lens->pixels = (uint32_t*)lensPixelsPtr;
   lens->pixel_tints = (byte*)tintsPtr;
	
   lastHighMark = Hunk_HighMark();
   lastSize = lastHighMark - postVideoHighMark;
}

static qboolean freeIfExtant(
      int currentHighMark, int lastHighMark_, int lastSize_){
   
   qboolean needNewHunk = true;
   if(lastHighMark_ != 0 && currentHighMark > lastHighMark_){
      needNewHunk = false;
   } else if (currentHighMark == lastHighMark_) {
      Hunk_FreeToHighMark(lastHighMark_ - (int)lastSize_);
   }
   return needNewHunk;
}

static qboolean hasResized(int width, int height){
   static int prevWidth = NIL, prevHeight = NIL;
   
   qboolean hasResizedOrRestarted = prevWidth!=width || prevHeight!=height;
   prevWidth = width;
   prevHeight = height;
   
   return hasResizedOrRestarted;
}

static qboolean hasHighMemContracted(){
   return lastHighMark > Hunk_HighMark();
}

static size_t padToNext256bytes(size_t unpadded){
   return (unpadded & MASK_LAST_BYTE_OF_ADDR) + PADDING_TO_ADD;
}
