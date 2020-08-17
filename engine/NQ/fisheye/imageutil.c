#include "qtypes.h"
#include "imageutil.h"
#include "fisheye.h"
#include "fishlens.h"
#include "zone.h"
#include "common.h"
#include "console.h"
#include "pcx.h"	//unwanted dependency

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

byte* makePalmapForPlate_(const byte *inPal, byte palleteLookup[256], 
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

// TODO: excise any and all references to PCX; use external library for png instead
// --------------------------------------------------------------------------------
// |                                                                              |
// |                           GLOBE SAVER FUNCTIONS                              |
// |                                                                              |
// --------------------------------------------------------------------------------

// copied from WritePCXfile in NQ/screen.c
// write a plate
void WritePCXplate_(struct _globe* globe, ray_to_plate_index_t ray_to_plate,
      const byte* inPal, char *filename, int plate_index, int with_margins){
// parameters from WritePCXfile
    int platesize = globe->platesize;
    byte *data = globe->pixels + plate_index * platesize * platesize;
    int width = platesize;
    int height = platesize;
    int rowbytes = platesize;
    byte *palette = inPal;

    int i, j, length;
    pcx_t *pcx;
    unsigned char* pack;

    pcx = Hunk_TempAlloc(width * height * 2 + 1000);
    if (pcx == NULL) {
	Con_Printf("SCR_ScreenShot_f: not enough memory\n");
	return;
    }

    pcx->version = 5;		// 256 color
    pcx->encoding = 1;		// uncompressed
    pcx->bits_per_pixel = 8;	// 256 color
    pcx->xmin = 0;
    pcx->ymin = 0;
    pcx->xmax = LittleShort((short)(width - 1));
    pcx->ymax = LittleShort((short)(height - 1));
    pcx->hres = LittleShort((short)width);
    pcx->vres = LittleShort((short)height);
    memset(pcx->palette, 0, sizeof(pcx->palette));
    pcx->color_planes = 1;	// chunky image
    pcx->bytes_per_line = LittleShort((short)width);
    pcx->palette_type = LittleShort(2);	// not a grey scale

// pack the image
    pack = &pcx->data;

    for (i = 0; i < height; i++) {
        double v = ((double)i)/height;
	for (j = 0; j < width; j++) {
           double u = ((double)j)/width;
           const vec2_u uv = {.k = {u,v}};
            //
           vec3_u ray = plate_uv_to_ray_(globe, plate_index, uv);
           byte col = with_margins || plate_index == ray_to_plate(ray.vec) ? *data : 0xFE;

           if ((col & 0xc0) == 0xc0) {
              *pack++ = 0xc1;
           }
           *pack = col;

           pack++;
           data++;
      }

	data += rowbytes - width;
    }

// write the palette
    *pack++ = 0x0c;		// palette ID byte
    for (i = 0; i < 768; i++)
	*pack++ = *palette++;

// write output file
    length = pack - (byte *)pcx;
    COM_WriteFile(filename, pcx, length);
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
