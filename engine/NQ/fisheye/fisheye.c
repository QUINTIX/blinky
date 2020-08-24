/*

	see fisheye.c.md

*/

#include "bspfile.h"
#include "client.h"
#include "console.h"
#include "cvar.h"
#include "draw.h"
#include "host.h"
#include "mathlib.h"
#include "quakedef.h"
#include "r_local.h"
#include "screen.h"
#include "sys.h"
#include "view.h"

#include "fisheye.h"
#include "fishmem.h"
#include "fishlens.h"
#include "fishScript.h"
#include "fishcmd.h"
#include "fishzoom.h"
#include "imageutil.h"

#include <time.h>

// -------------------------------------------------------------------------------- 
// |                                                                              |
// |                             VARIABLES                                        |
// |                                                                              |
// --------------------------------------------------------------------------------

// This is a globally accessible variable that determines if these fisheye features
// should be on.  It is used by other files for modifying behaviors that fisheye 
// depends on (e.g. square refdef, disabling water warp, hooking renderer).
qboolean fisheye_enabled;

// This is a globally accessible variable that is used to set the fov of each
// camera view that we render.
double fisheye_plate_fov;

// Lens computation is slow, so we don't want to block the game while its busy.
// Instead of dealing with threads, we are just limiting the time that the
// lens builder can work each frame.  It keeps track of its work between frames
// so it can resume without problems.  This allows the user to watch the lens
// pixels become visible as they are calculated.
static struct _lens_builder lens_builder;

static struct _globe globe;

static struct _lens lens;

static struct _zoom zoom;

static struct _rubix rubix;

static struct state_paq arg_consolidator;

// -------------------------------------------------------------------------------- 
// |                                                                              |
// |                      FUNCTION DECLARATIONS                                   |
// |                                                                              |
// --------------------------------------------------------------------------------

// public main functions
void F_Init(void);
void F_Shutdown(void);
void F_WriteConfig(FILE* f);
void F_RenderView(void);

// public introspection functions
struct _globe* F_getGlobe(void);
struct _lens* F_getLens(void);

// palette function
static void create_palmap(void);

// lens pixel setters
static void set_lensmap_grid(int lx, int ly, int px, int py, int plate_index);
static void set_lensmap_from_plate(int lx, int ly, int px, int py, int plate_index);
static void set_lensmap_from_plate_uv(int lx, int ly, double u, double v, int plate_index);
static void set_lensmap_from_ray(int lx, int ly, double sx, double sy, double sz);

// globe plate getters
static int ray_to_plate_index(vec3_t ray);
static qboolean ray_to_plate_uv(int plate_index, vec3_t ray, double *u, double *v);

// forward map getter/setter helpers
static void draw_quad(int *tl, int *tr, int *bl, int *br, int plate_index, int px, int py);

// lens builder resumers
static void resume_lensmap(void);
static qboolean resume_lensmap_inverse(void);
static qboolean resume_lensmap_forward(void);

// lens creators
static void create_lensmap_inverse(void);
static void create_lensmap_forward(void);
static void create_lensmap(void);

// renderers
static void render_lensmap(void);
static void render_plate(int plate_index, vec3_t forward, vec3_t right, vec3_t up);

// globe saver functions
static void WritePNGplate(char *filename, int plate_index, int with_margins);
static void save_globe(void);

//externalize exception throwing
static fisheye_status last_status;

void fe_throw(fisheye_status status){
    last_status = status;
};
void fe_clear(){
    last_status = FE_SUCCESS;
}
//might as well literally make it extern at this point. But I hate extern. And I'm stubborn.
#ifndef __wrap_F_getLastStatus
fisheye_status F_getLastStatus(){
	return last_status;
}
#endif //temporary until I figure out how to do this externally

// retrieves a pointer to a pixel in the video buffer
#define VBUFFER(x,y) (vid.buffer + (x) + (y)*vid.rowbytes)

// -------------------------------------------------------------------------------- 
// |                                                                              |
// |                        PUBLIC MAIN FUNCTIONS                                 |
// |                                                                              |
// --------------------------------------------------------------------------------

void F_Init(void)
{
   arg_consolidator = (struct state_paq){
        .forward = scriptToC_lens_forward,
        .inverse = scriptToC_lens_inverse, //currently unused from here
        .globe = &globe,
        .lens = &lens
   };
   lens_builder.working = false;
   lens_builder.seconds_per_frame = 1.0f / 60;

   rubix.enabled = false;

   F_scriptInit();

   F_init_commands(&zoom, &rubix);

   // create palette maps
   create_palmap();
}

void F_Shutdown(void)
{
   F_scriptShutdown();
}

void F_WriteConfig(FILE* f)
{
   fprintf(f,"fisheye %d\n", fisheye_enabled);
   fprintf(f,"f_lens \"%s\"\n", lens.name);
   fprintf(f,"f_globe \"%s\"\n", globe.name);
   fprintf(f,"f_rubixgrid %d %f %f\n", rubix.numcells, rubix.cell_size, rubix.pad_size);
   switch (zoom.type) {
      case ZOOM_FOV:     fprintf(f,"f_fov %d\n", zoom.fov); break;
      case ZOOM_VFOV:    fprintf(f,"f_vfov %d\n", zoom.fov); break;
      case ZOOM_COVER:   fprintf(f,"f_cover\n"); break;
      case ZOOM_CONTAIN: fprintf(f,"f_contain\n"); break;
      default: break;
   }
}

void F_RenderView(void)
{
   static int pwidth = -1;
   static int pheight = -1;

   // update screen size
   lens.width_px = scr_vrect.width;
   lens.height_px = scr_vrect.height;
   #define MIN(a,b) ((a) < (b) ? (a) : (b))
   int platesize = globe.platesize = MIN(lens.height_px, lens.width_px);
   int area = lens.width_px * lens.height_px;
   
   qboolean needNewBuffers = hasResizedOrRestarted(lens.width_px, lens.height_px);
   if(needNewBuffers){
      createOrReallocBuffers(&globe, &lens, area, platesize);
   }
   // recalculate lens
   if (needNewBuffers || zoom.changed || lens.changed || globe.changed) {
      memset(lens.pixels, 0, area*sizeof(*lens.pixels));
      memset(lens.pixel_tints, 255, area*sizeof(byte));

      // load lens again
      // (NOTE: this will be the second time this lens will be loaded in this frame if it has just changed)
      // (I'm just trying to force re-evaluation of lens variables that are dependent on globe variables (e.g. "lens_width = numplates" in debug.lua))
      lens.valid = F_load_lens(lens.name, &lens, &zoom, globe.numplates);
      if (!lens.valid) {
         strcpy(lens.name,"");
         Con_Printf("not a valid lens\n");
      }
      create_lensmap();
   }
   else if (lens_builder.working) {
      resume_lensmap();
   }

   // get the orientations required to render the plates
   vec3_t forward, right, up;
   AngleVectors(r_refdef.viewangles, forward, right, up);

   // do not do this every frame?
   extern int sb_lines;
   extern vrect_t scr_vrect;
   vrect_t vrect;
   vrect.x = 0;
   vrect.y = 0;
   vrect.width = vid.width;
   vrect.height = vid.height;
   R_SetVrect(&vrect, &scr_vrect, sb_lines);

   // render plates
   int i;
   for (i=0; i<globe.numplates; ++i)
   {
      if (globe.plates[i].display) {

         // set view to change plate FOV
         fisheye_plate_fov = globe.plates[i].fov;
         R_ViewChanged(&vrect, sb_lines, vid.aspect);

         // compute absolute view vectors
         // right = x
         // top = y
         // forward = z

         vec3_t r = { 0,0,0};
         VectorMA(r, globe.plates[i].right[0], right, r);
         VectorMA(r, globe.plates[i].right[1], up, r);
         VectorMA(r, globe.plates[i].right[2], forward, r);

         vec3_t u = { 0,0,0};
         VectorMA(u, globe.plates[i].up[0], right, u);
         VectorMA(u, globe.plates[i].up[1], up, u);
         VectorMA(u, globe.plates[i].up[2], forward, u);

         vec3_t f = { 0,0,0};
         VectorMA(f, globe.plates[i].forward[0], right, f);
         VectorMA(f, globe.plates[i].forward[1], up, f);
         VectorMA(f, globe.plates[i].forward[2], forward, f);

         render_plate(i, f, r, u);
      }
   }

   // save plates upon request from the "saveglobe" command
   if (globe.save.should) {
      save_globe();
   }

   // render our view
   Draw_TileClear(0, 0, vid.width, vid.height);
   render_lensmap();

   // store current values for change detection
   pwidth = lens.width_px;
   pheight = lens.height_px;

   // reset change flags
   lens.changed = globe.changed = zoom.changed = false;
}

// -------------------------------------------------------------------------------- 
// |                                                                              |
// |                           PALLETE FUNCTION                                  |
// |                                                                              |
// --------------------------------------------------------------------------------

static void create_palmap(void){
   const byte* pal = host_basepal;
   for (int j=0; j<MAX_PLATES; ++j){
      makePalmapForPlate(pal, globe.plates[j].palette, j);
   }
}

// -------------------------------------------------------------------------------- 
// |                                                                              |
// |                           GLOBE SAVER FUNCTIONS                              |
// |                                                                              |
// --------------------------------------------------------------------------------

// write a plate 
static void WritePNGplate(char *filename, int plate_index, int with_margins){
   const byte *palette = host_basepal;
   WritePNGplate_(&globe, ray_to_plate_index, palette, filename, plate_index, with_margins);
}

static void save_globe(void)
{
   int i;
   char pngname[32];

   globe.save.should = false;

    D_EnableBackBufferAccess();	// enable direct drawing of console to back

   for (i=0; i<globe.numplates; ++i) 
   {
      qsnprintf(pngname, 32, "%s%d.png", globe.save.name, i);
      WritePNGplate(pngname, i, globe.save.with_margins);

    Con_Printf("Wrote %s\n", pngname);
   }

    D_DisableBackBufferAccess();	// for adapters that can't stay mapped in
    //  for linear writes all the time
}

// -------------------------------------------------------------------------------- 
// |                                                                              |
// |                           LENS PIXEL SETTERS                                 |
// |                                                                              |
// --------------------------------------------------------------------------------

static void set_lensmap_grid(int lx, int ly, int px, int py, int plate_index)
{
   // designate the palette for this pixel
   // This will set the palette index map such that a grid is shown

   // (This is a block)
   //    |----|----|----|
   //    |    |    |    |
   //    |    |    |    |
   //    |----|----|----|
   //    |    |XXXXXXXXX|
   //    |    |XXXXXXXXX|
   //    |----|XXXXXXXXX|
   //    |    |XXXXXXXXX|
   //    |    |XXXXXXXXX|
   //    |----|----|----|
   double block_size = (rubix.pad_size + rubix.cell_size);

   // (Total number of units across)
   //    ---------------------------------------------------
   //    |    |    |    |    |    |    |    |    |    |    |
   //    |    |    |    |    |    |    |    |    |    |    |
   //    |----|----|----|----|----|----|----|----|----|----|
   double num_units = rubix.numcells * block_size + rubix.pad_size;

   // (the size of one unit)
   double unit_size_px = (double)globe.platesize / num_units;

   // convert pixel coordinates to units
   double ux = (double)px/unit_size_px;
   double uy = (double)py/unit_size_px;

   int ongrid =
      fmod(ux,block_size) < rubix.pad_size ||
      fmod(uy,block_size) < rubix.pad_size;

   if (!ongrid)
      *LENSPIXELTINT(lx,ly) = plate_index;
}

// set a pixel on the lensmap from plate coordinates
static void set_lensmap_from_plate(int lx, int ly, int px, int py, int plate_index)
{
   // check valid lens coordinates
   if (lx < 0 || lx >= lens.width_px || ly < 0 || ly >= lens.height_px) {
      return;
   }

   // check valid plate coordinates
   if (px <0 || px >= globe.platesize || py < 0 || py >= globe.platesize) {
      return;
   }

   // increase the number of times this side is used
   globe.plates[plate_index].display = 1;

   // map the lens pixel to this cubeface pixel
   *LENSPIXEL(lx,ly) = RELATIVE_GLOBEPIXEL(plate_index,px,py);

   set_lensmap_grid(lx,ly,px,py,plate_index);
}

// set a pixel on the lensmap from plate uv coordinates
static void set_lensmap_from_plate_uv(int lx, int ly, double u, double v, int plate_index)
{
   // convert to plate coordinates
   int px = (int)(u*globe.platesize);
   int py = (int)(v*globe.platesize);
   
   set_lensmap_from_plate(lx,ly,px,py,plate_index);
}

// set the (lx,ly) pixel on the lensmap to the (sx,sy,sz) view vector
static void set_lensmap_from_ray(int lx, int ly, double sx, double sy, double sz)
{
   vec3_t ray = {sx,sy,sz};

   // get plate index
   int plate_index = ray_to_plate_index(ray);
   if (plate_index < 0) {
      return;
   }

   // get texture coordinates
   double u,v;
   if (!ray_to_plate_uv(plate_index, ray, &u, &v)) {
      return;
   }

   // map lens pixel to plate pixel
   set_lensmap_from_plate_uv(lx,ly,u,v,plate_index);
}


// -------------------------------------------------------------------------------- 
// |                                                                              |
// |                           GLOBE PIXEL GETTERS                                |
// |                                                                              |
// --------------------------------------------------------------------------------

// retrieves the plate closest to the given ray
static int ray_to_plate_index(vec3_t ray)
{
   int plate_index = 0;
   script_refs lua_refs = *F_getScriptRef();
   vec3_u ray_;
   VectorCopy(ray, ray_.vec);

   if (lua_refs.globe_plate != -1) {
      int candValue = scriptToC_globe_plate(ray_);
      if (last_status ==FE_SUCCESS){
         return candValue;
      }
      fe_throw(NONSENSE_VALUE);
      return NONSENSE_VALUE; //TODO: don't use index for state
   }

   // maximum dotproduct 
   //  = minimum acos(dotproduct) 
   //  = minimum angle between vectors
   double max_dp = -2;

   int i;
   for (i=0; i<globe.numplates; ++i) {
      double dp = DotProduct(ray, globe.plates[i].forward);
      if (dp > max_dp) {
         max_dp = dp;
         plate_index = i;
      }
   }

   return plate_index;
}

static qboolean ray_to_plate_uv(int plate_index, vec3_t ray, double *u, double *v)
{
   // get ray in the plate's relative view frame
   double x = DotProduct(globe.plates[plate_index].right, ray);
   double y = DotProduct(globe.plates[plate_index].up, ray);
   double z = DotProduct(globe.plates[plate_index].forward, ray);

   // project ray to the texture
   double dist = globe.plates[plate_index].dist;
   *u = x/z*dist + 0.5;
   *v = -y/z*dist + 0.5;

   // return true if valid texture coordinates
   return *u>=0 && *u<=1 && *v>=0 && *v<=1;
}

// -------------------------------------------------------------------------------- 
// |                                                                              |
// |                           LENS BUILDER RESUMERS                              |
// |                                                                              |
// --------------------------------------------------------------------------------

static void resume_lensmap(void)
{
   if (lens.map_type == MAP_FORWARD) {
      lens_builder.working = resume_lensmap_forward();
   }
   else if (lens.map_type == MAP_INVERSE) {
      lens_builder.working = resume_lensmap_inverse();
   }
}

static qboolean resume_lensmap_inverse(void)
{
   // image coordinates
   double x,y;

   // lens coordinates
   int lx, *ly;

   start_lens_builder_clock_(&lens_builder);
   for(ly = &(lens_builder.inverse_state.ly); *ly >= 0; --(*ly))
   {
      // pause building if we have exceeded time allowed per frame
      if (is_lens_builder_time_up_(&lens_builder)) {
         return true; 
      }

      y = -(*ly-lens.height_px/2) * lens.scale;

      // calculate all the pixels in this row
      for(lx = 0;lx<lens.width_px;++lx)
      {
         x = (lx-lens.width_px/2) * lens.scale;

         // determine which light ray to follow
         vec3_u ray = scriptToC_lens_inverse((vec2_u){{x,y}});
         if (last_status == NO_VALUE_RETURNED) {
            continue;
         }
         else if (last_status == NONSENSE_VALUE) {
            return false;
         }

         // get the pixel belonging to the light ray
         set_lensmap_from_ray(lx,*ly,ray.vec[0],ray.vec[1],ray.vec[2]);
      }
   }

   // done building lens
   return false;
}

static qboolean resume_lensmap_forward(void)
{
   int *top = lens_builder.forward_state.top;
   int *bot = lens_builder.forward_state.bot;
   int *py = &(lens_builder.forward_state.py);
   int *plate_index = &(lens_builder.forward_state.plate_index);
   int platesize = globe.platesize;

   start_lens_builder_clock_(&lens_builder);
   for (; *plate_index < globe.numplates; ++(*plate_index))
   {
      int px;
      for (; *py >=0; --(*py)) {

         // pause building if we have exceeded time allowed per frame
         if (is_lens_builder_time_up_(&lens_builder)) {
            return true; 
         }

         // FIND ALL DESTINATION SCREEN COORDINATES FOR THIS TEXTURE ROW ********************

         // compute lower points
         if (*py == platesize-1) {
            double v = (*py + 0.5) / platesize;
            for (px = 0; px < platesize; ++px) {
               // compute left point
               if (px == 0) {
                  double u = (px - 0.5) / platesize;
                  point2d bot_ = uv_to_screen(arg_consolidator, *plate_index, (vec2_u){{u,v}});
                  bot[0] = bot_.xy.x; bot[1] = bot_.xy.y;
                   if (last_status == NO_VALUE_RETURNED){ continue;}
                   else if (last_status == NONSENSE_VALUE){ return false;}
               }
               // compute right point
               double u = (px + 0.5) / platesize;
               int index = 2*(px+1);
               point2d bot_ = uv_to_screen(arg_consolidator, *plate_index, (vec2_u){{u,v}});
               bot[index] = bot_.xy.x; bot[index+1] = bot_.xy.y;
                if (last_status == NO_VALUE_RETURNED) {continue;}
                else if (last_status == NONSENSE_VALUE) {return false;}
            }
         }
         else {
            // swap references so that the previous bottom becomes our current top
            int *temp = top;
            top = bot;
            bot = temp;
         }

         // compute upper points
         double v = (*py - 0.5) / platesize;
         for (px = 0; px < platesize; ++px) {
            // compute left point
            if (px == 0) {
               double u = (px - 0.5) / platesize;
               point2d top_ = uv_to_screen(arg_consolidator, *plate_index, (vec2_u){{u,v}});
               top[0] = top_.xy.x; top[1] = top_.xy.y;
               if (last_status == NO_VALUE_RETURNED) {continue;}
                else if (last_status == NONSENSE_VALUE) {return false;}
            }
            // compute right point
            double u = (px + 0.5) / platesize;
            int index = 2*(px+1);
            point2d top_ = uv_to_screen(arg_consolidator, *plate_index, (vec2_u){{u,v}});
            top[index] = top_.xy.x; top[index+1] = top_.xy.y;
             if (last_status == NO_VALUE_RETURNED) {continue;}
             else if (last_status == NONSENSE_VALUE) {return false;}
         }

         // DRAW QUAD FOR EACH PIXEL IN THIS TEXTURE ROW ***********************************

         v = ((double)*py)/platesize;
         for (px = 0; px < platesize; ++px) {
            
            // skip overlapping region of texture
            double u = ((double)px)/platesize;
            vec3_u ray = plate_uv_to_ray(&globe, *plate_index, (vec2_u){{u,v}});
            if (*plate_index != ray_to_plate_index(ray.vec)) {
               continue;
            }

            int index = 2*px;
            draw_quad(&top[index], &top[index+2], &bot[index], &bot[index+2], *plate_index,px,*py);
         }

      }

      // reset row position
      // (we have to do it here because it cannot be reset until it is done iterating)
      // (we cannot do it at the beginning because the function could be resumed at some middle row)
      *py = platesize-1;
   }

   free(top);
   free(bot);

   // done building lens
   return false;
}

// fills a quad on the lensmap using the given plate coordinate
static void draw_quad(int *tl, int *tr, int *bl, int *br,
      int plate_index, int px, int py)
{
   // array for quad corners in clockwise order
   int *p[] = { tl, tr, br, bl };

   // get bounds
   int x = tl[0], y = tl[1];
   int miny=y, maxy=y;
   int minx=x, maxx=x;
   int i;
   for (i=1; i<4; i++) {
      int tx = p[i][0];
      if (tx < minx) { minx = tx; }
      else if (tx > maxx) { maxx = tx; }

      int ty = p[i][1];
      if (ty < miny) { miny = ty; }
      else if (ty > maxy) { maxy = ty; }
   }

   // temp solution for keeping quads from wrapping around
   //    the boundaries of the image. I guess that quads
   //    will not get very big unless they're doing wrapping.
   // actual clipping will require knowledge of the boundary.
   const int maxdiff = 20;
   if (abs(minx-maxx) > maxdiff || abs(miny-maxy) > maxdiff) {
      return;
   }

   // pixel
   if (miny == maxy && minx == maxx) {
      set_lensmap_from_plate(x,y,px,py,plate_index);
      return;
   }

   // horizontal line
   if (miny == maxy) {
      int tx;
      for (tx=minx; tx<=maxx; ++tx) {
         set_lensmap_from_plate(tx,miny,px,py,plate_index);
      }
      return;
   }

   // vertical line
   if (minx == maxx) {
      int ty;
      for (ty=miny; ty<=maxy; ++ty) {
         set_lensmap_from_plate(x,ty,px,py,plate_index);
      }
      return;
   }

   // quad
   for (y=miny; y<=maxy; ++y) {

      // get x points
      int tx[2] = {minx,maxx};
      int txi=0; // tx index
      int j=3;
      for (i=0; i<4; ++i) {
         int ix = p[i][0], iy = p[i][1];
         int jx = p[j][0], jy = p[j][1];
         if ((iy < y && y <= jy) || (jy < y && y <= iy)) {
            double dy = jy-iy;
            double dx = jx-ix;
            tx[txi] = (int)(ix + (y-iy)/dy*dx);
            if (++txi == 2) break;
         }
         j=i;
      }

      // order x points
      if (tx[0] > tx[1]) {
         int temp = tx[0];
         tx[0] = tx[1];
         tx[1] = temp;
      }

      // sanity check on distance
      if (tx[1] - tx[0] > maxdiff)
      {
         Con_Printf("%d > maxdiff\n", tx[1]-tx[0]);
         return;
      }

      // draw horizontal line between x points
      for (x=tx[0]; x<=tx[1]; ++x) {
         set_lensmap_from_plate(x,y,px,py,plate_index);
      }
   }
}

// -------------------------------------------------------------------------------- 
// |                                                                              |
// |                           LENS CREATORS                                      |
// |                                                                              |
// --------------------------------------------------------------------------------

static void create_lensmap_inverse(void)
{
   // initialize progress state
   lens_builder.inverse_state.ly = lens.height_px-1;

   resume_lensmap();
}

static void create_lensmap_forward(void)
{
   // initialize progress state
   int *rowa = malloc((globe.platesize+1)*sizeof(int[2]));
   int *rowb = malloc((globe.platesize+1)*sizeof(int[2]));
   lens_builder.forward_state.top = rowa;
   lens_builder.forward_state.bot = rowb;
   lens_builder.forward_state.py = globe.platesize-1;
   lens_builder.forward_state.plate_index = 0;

   resume_lensmap();
}

static void create_lensmap(void)
{
   lens_builder.working = false;

   // render nothing if current lens or globe is invalid
   if (!lens.valid || !globe.valid)
      return;

   // test if this lens can support the current fov
   if (!calc_zoom(&lens, &zoom)) {
      Con_Printf("This lens could not be initialized.\n");
      return;
   }

   // clear the side counts
   int i;
   for (i=0; i<globe.numplates; i++) {
      globe.plates[i].display = 0;
   }

   // create lensmap
   if (lens.map_type == MAP_FORWARD) {
      create_lensmap_forward();
   }
   else if (lens.map_type == MAP_INVERSE) {
      create_lensmap_inverse();
   }
   else { // MAP_NONE
      Con_Printf("no inverse or forward map being used\n");
   }
}

// -------------------------------------------------------------------------------- 
// |                                                                              |
// |                           LENS RENDERERS                                     |
// |                                                                              |
// --------------------------------------------------------------------------------

// draw the lensmap to the vidbuffer
static void render_lensmap(void)
{
   uint32_t *lmap = lens.pixels;
   byte *pmap = lens.pixel_tints;
   int x, y;
   for(y=0; y<lens.height_px; y++)
      for(x=0; x<lens.width_px; x++,lmap++,pmap++)
         if (*lmap) {
            int lx = x+scr_vrect.x;
            int ly = y+scr_vrect.y;
            byte* pixAddr = globe.pixels + *lmap;
            if (rubix.enabled) {
               int i = *pmap;
              
               *VBUFFER(lx,ly) = i != 255 ? globe.plates[i].palette[*pixAddr] : *pixAddr;
            }
            else {
               *VBUFFER(lx,ly) = *pixAddr;
            }
         }
}

// render a specific plate
static void render_plate(int plate_index, vec3_t forward, vec3_t right, vec3_t up) 
{
    byte *pixels = globe.pixels + RELATIVE_GLOBEPIXEL(plate_index, 0, 0);

   // set camera orientation
   VectorCopy(forward, r_refdef.forward);
   VectorCopy(right, r_refdef.right);
   VectorCopy(up, r_refdef.up);

   // render view
   R_PushDlights();
   R_RenderView();

   // copy from vid buffer to cubeface, row by row
   byte *vbuffer = VBUFFER(scr_vrect.x,scr_vrect.y);
   int y;
   for(y = 0;y<globe.platesize;y++) {
      memcpy(pixels, vbuffer, globe.platesize);

      // advance to the next row
      vbuffer += vid.rowbytes;
      pixels += globe.platesize;
   }
}

//Introspection function implementations
struct _globe* F_getGlobe(void){
	return &globe;
}

struct _lens* F_getLens(void){
	return &lens;
}

struct _lens_builder* F_getStatus(void){
   return &lens_builder;
}
// vim: et:ts=3:sts=3:sw=3
