#include <stdio.h>
#include "mathlib.h"
#ifndef FISHEYE_H_
#define FISHEYE_H_

extern qboolean fisheye_enabled;

void F_Init(void);
void F_Shutdown(void);
void F_RenderView(void);
void F_WriteConfig(FILE *f);

struct _globe {

   // name of the current globe
   char name[50];

   // indicates if the current globe is valid
   qboolean valid;

   // indiciates if the lens has changed and needs updating
   qboolean changed;

   // the environment map
   // a large array of pixels that hold all rendered views
   byte *pixels;
   int pixelHeapLowMark;
   
   // retrieves the _realative index_ of a pixel in the platemap
   #define RELATIVE_GLOBEPIXEL(plate, x, y) ((plate)*(globe.platesize)*(globe.platesize) + (x) + (y)*(globe.platesize))
   // retrieves a pointer to a pixel in the platemap
   #define GLOBEPIXEL(plate,x,y) (globe.pixels + RELATIVE_GLOBEPIXEL(plate, x, y))

   // globe plates
   #define MAX_PLATES 6
   struct {
      vec3_t forward;
      vec3_t right;
      vec3_t up;
      vec_t fov;
      vec_t dist;
      byte palette[256];
      int display;
   } plates[MAX_PLATES];

   // number of plates used by the current globe
   int numplates;

   // size of each rendered square plate in the vid buffer
   int platesize;

   // set when we want to save each globe plate
   // (make sure they are visible (i.e. current lens is using all plates))
   struct {
      qboolean should;
      int with_margins;
      char name[32];
   } save;

};

struct _globe* F_getGlobe(void);

struct _lens {

   // boolean signaling if the lens is properly loaded
   qboolean valid;

   // boolean signaling if the lens has changed and needs updating
   qboolean changed;

   // name of the current lens
   char name[50];

   // the type of map projection (inverse/forward)
   enum { MAP_NONE, MAP_INVERSE, MAP_FORWARD } map_type;

   // size of the lens image in its arbitrary units
   double width, height;

   // controls the zoom of the lens image
   // (scale = units per pixel)
   double scale;

   // pixel size of the lens view (it is equal to the screen size below):
   //    ------------------
   //    |                |
   //    |   ---------- ^ |
   //    |   |        | | |
   //    |   | screen | h |
   //    |   |        | | |
   //    |   ---------- v |
   //    |   <---w---->   |
   //    |----------------|
   //    |   status bar   |
   //    |----------------|
   int width_px, height_px;

   // array of pointers (*) to plate pixels
   // (the view constructed by the lens)
   //
   //    **************************    ^
   //    **************************    |
   //    **************************    |
   //    **************************  height_px
   //    **************************    |
   //    **************************    |
   //    **************************    v
   //
   //    <------- width_px ------->
   //
   byte **pixels;
   int pixelHeapLowMark;

   // retrieves a pointer to a lens pixel
   #define LENSPIXEL(x,y) (lens.pixels + (x) + (y)*lens.width_px)

   // a color tint index (i) for each pixel (255 = no filter)
   // (new color = globe.plates[i].palette[old color])
   // (used for displaying transparent colored overlays over certain pixels)
   //
   //    iiiiiiiiiiiiiiiiiiiiiiiiii    ^
   //    iiiiiiiiiiiiiiiiiiiiiiiiii    |
   //    iiiiiiiiiiiiiiiiiiiiiiiiii    |
   //    iiiiiiiiiiiiiiiiiiiiiiiiii  height_px
   //    iiiiiiiiiiiiiiiiiiiiiiiiii    |
   //    iiiiiiiiiiiiiiiiiiiiiiiiii    |
   //    iiiiiiiiiiiiiiiiiiiiiiiiii    v
   //
   //    <------- width_px ------->
   //
   byte *pixel_tints;
   int pixelTintsLowMark;

   // retrieves a pointer to a lens pixel tint
   #define LENSPIXELTINT(x,y) (lens.pixel_tints + (x) + (y)*lens.width_px)

};

struct _lens* F_getLens(void);

#endif
