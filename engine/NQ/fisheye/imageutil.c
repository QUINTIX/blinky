#include "qtypes.h"
#include "imageutil.h"
#include "fisheye.h"
#include "fishlens.h"
#include "zone.h"
#include "common.h"
#include "console.h"
#include <SDL_surface.h>
#include <SDL_image.h>

#define BITS_PER_PIXEL 8

static int find_closest_pal_index_(int r, int g, int b, const byte* pal_){
   byte* pal = (byte*)pal_;
   int i;
   int mindist = 256*256*256;
   int minindex = 0;
   for (i=0; i<256; ++i)
   {
      int dr = (int)pal[0]-r;
      int dg = (int)pal[1]-g;
      int db = (int)pal[2]-b;
      int dist = dr*dr+dg*dg+db*db;
      if (dist < mindist)
      {
         mindist = dist;
         minindex = i;
      }
      pal += 3;
   }
   return minindex;
}

byte* makePalmapForPlate(const byte *inPal, byte palleteLookup[256], 
       int plateIdx)
{
   int i;
   int percent = 256/6;
   int tint[3] = {0,0,0};
   switch(plateIdx){
      case 0:
            tint[0] = tint[1] = tint[2] = 255;
            break;
         case 1:
            tint[2] = 255;
            break;
         case 2:
            tint[0] = 255;
            break;
         case 3:
            tint[0] = tint[1] = 255;
            break;
         case 4:
            tint[0] = tint[2] = 255;
            break;
         case 5:
            tint[1] = tint[2] = 255;
            break;
   }
   
   byte* pal = (byte*)inPal;
   for (i=0; i<256; ++i)
   {
         int r = pal[0];
         int g = pal[1];
         int b = pal[2];

         r += percent * (tint[0] - r) >> 8;
         g += percent * (tint[1] - g) >> 8;
         b += percent * (tint[2] - b) >> 8;

         if (r < 0) r=0; if (r > 255) r=255;
         if (g < 0) g=0; if (g > 255) g=255;
         if (b < 0) b=0; if (b > 255) b=255;

         palleteLookup[i] = find_closest_pal_index_(r,g,b, inPal);

         pal += 3;
      }
   return palleteLookup;
}

// --------------------------------------------------------------------------------
// |                                                                              |
// |                           GLOBE SAVER FUNCTIONS                              |
// |                                                                              |
// --------------------------------------------------------------------------------

// write a plate
void WritePNGplate_(struct _globe* globe, ray_to_plate_index_t ray_to_plate,
      const byte* inPal, char *filename, int plate_index, int with_margins){
    int platesize = globe->platesize;
    byte *data = globe->pixels + plate_index * platesize * platesize;
    int width = platesize;
    int height = platesize;
    int rowbytes = platesize;
    byte *palette = (byte*)inPal;
   
    SDL_Surface *surf = SDL_CreateRGBSurfaceWithFormatFrom(data, width, height,
            BITS_PER_PIXEL, rowbytes, SDL_PIXELFORMAT_INDEX8);
    SDL_Palette *pal = makePaddedPalette(palette);
    SDL_SetSurfacePalette(surf, pal);
    
    SDL_Surface *maskedSurf = SDL_CreateRGBSurfaceWithFormat(0, width, height,
            BITS_PER_PIXEL, SDL_PIXELFORMAT_INDEX8);
    SDL_SetSurfacePalette(maskedSurf, pal);
    
    SDL_Rect getRekt = {.x=0,.y=0,.w=width,.h=height};
    SDL_BlitSurface(surf, &getRekt, maskedSurf, NULL);
    SDL_LockSurface(maskedSurf);
    byte* pix = (byte*)maskedSurf->pixels;
    for(int i=0; i < height; i++){
       for(int j=0; j < width; j++){
          const vec2_u uv = {.k = { (float)j/width, ((float)i)/height}};
          vec3_u ray = plate_uv_to_ray(globe, plate_index, uv);
          *pix = with_margins || plate_index == ray_to_plate(ray.vec)
                  ? *pix : 0xFE;
          pix++;
       }
    }
    SDL_UnlockSurface(maskedSurf);
    IMG_SavePNG(maskedSurf, filename);
   
    SDL_FreePalette(pal);
    SDL_FreeSurface(maskedSurf);
    SDL_FreeSurface(surf);
}

void dumppal(const byte* inPal){
   byte *pal = (byte*)inPal;
   FILE *pFile = fopen("palette","w");
   if (NULL == pFile) {
      Con_Printf("could not open \"palette\" for writing\n");
      return;
   }
   for (int i=0; i<256; ++i) {
      fprintf(pFile, "%d, %d, %d,\n",
            pal[0],pal[1],pal[2]);
      pal+=3;
   }
   fclose(pFile);
}

SDL_Palette* makePaddedPalette(byte* input){
   SDL_Palette* pal = SDL_AllocPalette(256);
   byte* curColor = input;
   byte* end = input + 256*3;
   for(int i=0;curColor < end; curColor+=3){
      Uint8 r = curColor[0];
      Uint8 g = curColor[1];
      Uint8 b = curColor[2];
      pal->colors[i++] = (SDL_Color){.r=r,.g=g,.b=b};
   }
   return pal;
}

void dumpTints(struct _lens* lens, char *filename){
   SDL_Surface *surf = SDL_CreateRGBSurfaceWithFormatFrom(
      lens->pixel_tints, lens->width_px, lens->height_px, 8, lens->width_px,
         SDL_PIXELFORMAT_INDEX8);
  
    SDL_Palette* pal = SDL_AllocPalette(256);
   
   pal->colors[0] = (SDL_Color){.r=255,.g=0,.b=0};
   pal->colors[1] = (SDL_Color){.r=255,.g=255,.b=0};
   pal->colors[2] = (SDL_Color){.r=0,.g=255,.b=0};
   pal->colors[3] = (SDL_Color){.r=0,.g=255,.b=255};
   pal->colors[4] = (SDL_Color){.r=0,.g=0,.b=255};
   pal->colors[5] = (SDL_Color){.r=255,.g=0,.b=255};
   pal->colors[255] = (SDL_Color){.r=0,.g=0,.b=0};
   
   SDL_SetSurfacePalette(surf, pal);
   
   IMG_SavePNG(surf, filename);

   SDL_FreePalette(pal);
   
   SDL_FreeSurface(surf);
}

//this makes assumptions about the arrangement of globe->pixels that will not remain valid
static uint32_t convertGlobePixelIndexToColor(
		const uint32_t globePixelIndex,
		const uint32_t width,
		const uint32_t area,
		const uint8_t mask,
		const SDL_PixelFormat* thanksIHateIt){
	float scale = (float) (UINT8_MAX) / (float)width;
	
	uint8_t plateNum = (uint8_t)(globePixelIndex / area);
	uint32_t platePixelIdx = globePixelIndex % area;
	uint32_t platePixelY = platePixelIdx/width;
	uint32_t platePixelX = platePixelIdx % width;
	
	uint8_t alpha = mask == 255 ? INT8_MAX : UINT8_MAX;
	
	uint32_t colorRaw = SDL_MapRGBA(thanksIHateIt,
					(uint8_t)(scale*platePixelX),
					(uint8_t)(scale*platePixelY),
					plateNum * 42, alpha);
	return colorRaw;
}

static SDL_Surface* genSurfaceFromLensIndicies(struct _lens* lens){
	SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormat(0,
		lens->width_px, lens->height_px, 32, SDL_PIXELFORMAT_RGBA8888);
	
	int screenArea = lens->width_px * lens->height_px;
	uint32_t globePlateWidth = lens->width_px < lens->height_px ?
		lens->width_px : lens->height_px;
	
	uint32_t area = globePlateWidth*globePlateWidth;
	
	SDL_LockSurface(surf);
	uint32_t* lensPixels = lens->pixels;
	uint32_t* surfPixels = surf->pixels;
	uint8_t* tintPixels = lens->pixel_tints;

	for(int i=0; i < screenArea; i++){
		surfPixels[i] = convertGlobePixelIndexToColor(
			lensPixels[i], globePlateWidth, area, tintPixels[i],
			surf->format
		);
	}
	
	SDL_UnlockSurface(surf);
	return surf;
}

void dumpIndicies(struct _lens* lens, const char* filename){
   SDL_Surface *surf = genSurfaceFromLensIndicies(lens);
   
   IMG_SavePNG(surf, filename);
   
   SDL_FreeSurface(surf);
}

void dumpIndiciesRaw(struct _lens* lens, const char* filename){
	size_t filesize = lens->width_px * lens->height_px * sizeof(*(lens->pixels));
	COM_WriteFile(filename, lens->pixels, (int)filesize);
}
