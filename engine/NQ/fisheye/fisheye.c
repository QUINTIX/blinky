/*

	see fisheye.c.md

*/

#include "bspfile.h"
#include "client.h"
#include "cmd.h"
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
#include "fishcmd.h"
#include "imageutil.h"

#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

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

// the Lua state pointer
static lua_State *lua;

// lua reference indexes (for reference lua functions)
static struct _lua_refs {
   int lens_forward;
   int lens_inverse;
   int globe_plate;
} lua_refs;

static struct _globe globe;

static struct _lens lens;

static struct _zoom zoom;

static struct _rubix rubix;

static fisheye_status last_status;

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

// lens builder timing functions
static void start_lens_builder_clock(void);
static qboolean is_lens_builder_time_up(void);

// palette function
static void create_palmap(void);

// lua initializer
static void init_lua(void);

// c->lua (c functions for use in lua)
static int CtoLUA_latlon_to_ray(lua_State *L);
static int CtoLUA_ray_to_latlon(lua_State *L);
static int CtoLUA_plate_to_ray(lua_State *L);

// lua->c (lua functions for use in c)
static int LUAtoC_lens_inverse(double x, double y, vec3_t ray);
static vec3_u LUAtoC_lens_inverse_(vec2_u xy);
static int LUAtoC_lens_forward(vec3_t ray, double *x, double *y);
static vec2_u LUAtoC_lens_forward_(vec3_u ray);

static int LUAtoC_globe_plate(vec3_t ray, int *plate);

// functions to manage the data and functions in the Lua interpreter state
static void LUA_clear_lens(void);
static void LUA_clear_globe(void);

// lua helpers
static qboolean lua_func_exists(const char* name);

// zoom functions
static qboolean calc_zoom(void);

// lens pixel setters
static void set_lensmap_grid(int lx, int ly, int px, int py, int plate_index);
static void set_lensmap_from_plate(int lx, int ly, int px, int py, int plate_index);
static void set_lensmap_from_plate_uv(int lx, int ly, double u, double v, int plate_index);
static void set_lensmap_from_ray(int lx, int ly, double sx, double sy, double sz);

// globe plate getters
static int ray_to_plate_index(vec3_t ray);
static qboolean ray_to_plate_uv(int plate_index, vec3_t ray, double *u, double *v);

// pure coordinate convertors
static void latlon_to_ray(double lat, double lon, vec3_t ray);
static void ray_to_latlon(vec3_t ray, double *lat, double *lon);
static void plate_uv_to_ray(int plate_index, double u, double v, vec3_t ray);

// forward map getter/setter helpers
static int uv_to_screen(int plate_index, double u, double v, int *lx, int *ly);
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
        .forward = LUAtoC_lens_forward_,
        .inverse = LUAtoC_lens_inverse_,
        .globe = &globe,
        .lens = &lens
   };
   lens_builder.working = false;
   lens_builder.seconds_per_frame = 1.0f / 60;

   rubix.enabled = false;

   init_lua();

   F_init_commands(&zoom, &rubix, lua);

   // defaults
   Cmd_ExecuteString("fisheye 1", src_command);
   Cmd_ExecuteString("f_globe cube", src_command);
   Cmd_ExecuteString("f_lens panini", src_command);
   Cmd_ExecuteString("f_fov 180", src_command);
   Cmd_ExecuteString("f_rubixgrid 10 4 1", src_command);

   // create palette maps
   create_palmap();
}

void F_Shutdown(void)
{
   if(NULL != lua)
       lua_close(lua);
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
      lens.valid = LUA_load_lens();
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
// |                        LENS BUILD TIMING FUNCTIONS                           |
// |                                                                              |
// --------------------------------------------------------------------------------

static void start_lens_builder_clock(void) {
   start_lens_builder_clock_(&lens_builder);
}
static qboolean is_lens_builder_time_up(void) {
   return is_lens_builder_time_up_(&lens_builder);
}

// -------------------------------------------------------------------------------- 
// |                                                                              |
// |                           PALLETE FUNCTION                                  |
// |                                                                              |
// --------------------------------------------------------------------------------

static void create_palmap(void){
   const byte* pal = host_basepal;
   for (int j=0; j<MAX_PLATES; ++j){
      makePalmapForPlate_(pal, globe.plates[j].palette, j);
   }
}

// -------------------------------------------------------------------------------- 
// |                                                                              |
// |                    PURE COORDINATE CONVERTERS                                |
// |                                                                              |
// --------------------------------------------------------------------------------

static void latlon_to_ray(double lat, double lon, vec3_t ray)
{
   vec3_u ray_ = latlon_to_ray_((vec2_u){.k={lat,lon}});
   VectorCopy(ray_.vec, ray);
}

static void ray_to_latlon(vec3_t ray, double *lat, double *lon)
{
   vec3_u ray_ = {.vec = *ray};
   vec2_u latlon = ray_to_latlon_(ray_);
   *lon = latlon.latlon.lon; *lat = latlon.latlon.lat;
}

static void plate_uv_to_ray(int plate_index, double u, double v, vec3_t ray)
{
   vec2_u uv = {.k={(vec_t)u,(vec_t)v}};
   vec3_u ray_ = plate_uv_to_ray_(&globe, plate_index, uv);
   VectorCopy(ray_.vec, ray);
}

// -------------------------------------------------------------------------------- 
// |                                                                              |
// |                           LUA INITIALIZER                                    |
// |                                                                              |
// --------------------------------------------------------------------------------

static void init_lua(void)
{
   // create Lua state
   lua = luaL_newstate();

   // open Lua standard libraries
   luaL_openlibs(lua);

   char aliases[] = 
      "cos = math.cos\n"
      "sin = math.sin\n"
      "tan = math.tan\n"
      "asin = math.asin\n"
      "acos = math.acos\n"
      "atan = math.atan\n"
      "atan2 = math.atan2\n"
      "sinh = math.sinh\n"
      "cosh = math.cosh\n"
      "tanh = math.tanh\n"
      "log = math.log\n"
      "log10 = math.log10\n"
      "abs = math.abs\n"
      "sqrt = math.sqrt\n"
      "exp = math.exp\n"
      "pi = math.pi\n"
      "tau = math.pi*2\n"
      "pow = math.pow\n";

   int error = luaL_loadbuffer(lua, aliases, strlen(aliases), "aliases") ||
      lua_pcall(lua, 0, 0, 0);
   if (error) {
      fprintf(stderr, "%s", lua_tostring(lua, -1));
      lua_pop(lua, 1);  // pop error message from the stack
   }

   lua_pushcfunction(lua, CtoLUA_latlon_to_ray);
   lua_setglobal(lua, "latlon_to_ray");

   lua_pushcfunction(lua, CtoLUA_ray_to_latlon);
   lua_setglobal(lua, "ray_to_latlon");

   lua_pushcfunction(lua, CtoLUA_plate_to_ray);
   lua_setglobal(lua, "plate_to_ray");
}

// -------------------------------------------------------------------------------- 
// |                                                                              |
// |                           ZOOM FUNCTIONS                                     |
// |                                                                              |
// --------------------------------------------------------------------------------

static qboolean calc_zoom(void)
{
   // clear lens scale
   lens.scale = -1;

   if (zoom.type == ZOOM_FOV || zoom.type == ZOOM_VFOV)
   {
      // check FOV limits
      if (zoom.max_fov <= 0 || zoom.max_vfov <= 0)
      {
         Con_Printf("max_fov & max_vfov not specified, try \"f_cover\"\n");
         return false;
      }
      else if (zoom.type == ZOOM_FOV && zoom.fov > zoom.max_fov) {
         Con_Printf("fov must be less than %d\n", zoom.max_fov);
         return false;
      }
      else if (zoom.type == ZOOM_VFOV && zoom.fov > zoom.max_vfov) {
         Con_Printf("vfov must be less than %d\n", zoom.max_vfov);
         return false;
      }

      // try to scale based on FOV using the forward map
      if (lua_refs.lens_forward != -1) {
         vec3_t ray;
         double x=1.,y=1.;
         double fovr = zoom.fov * M_PI / 180;
         if (zoom.type == ZOOM_FOV) {
            vec3_u ray_ = latlon_to_ray_((vec2_u){0, fovr*0.5});
            VectorCopy(ray_.vec, ray);
            if (LUAtoC_lens_forward(ray,&x,&y)) {
               lens.scale = x / (lens.width_px * 0.5);
            }
            else {
               Con_Printf("ray_to_xy did not return a valid r value for determining FOV scale\n");
               return false;
            }
         }
         else if (zoom.type == ZOOM_VFOV) {
            vec3_u ray_ =latlon_to_ray_((vec2_u){fovr*0.5,0});
            VectorCopy(ray_.vec, ray);
            if (LUAtoC_lens_forward(ray,&x,&y)) {
               lens.scale = y / (lens.height_px * 0.5);
            }
            else {
               Con_Printf("ray_to_xy did not return a valid r value for determining FOV scale\n");
               return false;
            }
         }
      }
      else
      {
         Con_Printf("Please specify a forward mapping function in your script for FOV scaling\n");
         return false;
      }
   }
   else if (zoom.type == ZOOM_CONTAIN || zoom.type == ZOOM_COVER) { // scale based on fitting

      double fit_width_scale = lens.width / lens.width_px;
      double fit_height_scale = lens.height / lens.height_px;

      qboolean width_provided = (lens.width > 0);
      qboolean height_provided = (lens.height > 0);

      if (!width_provided && height_provided) {
         lens.scale = fit_height_scale;
      }
      else if (width_provided && !height_provided) {
         lens.scale = fit_width_scale;
      }
      else if (!width_provided && !height_provided) {
         Con_Printf("neither lens_height nor lens_width are valid/specified.  Try f_fov instead.\n");
         return false;
      }
      else {
         double lens_aspect = lens.width / lens.height;
         double screen_aspect = (double)lens.width_px / lens.height_px;
         qboolean lens_wider = lens_aspect > screen_aspect;

         if (zoom.type == ZOOM_CONTAIN) {
            lens.scale = lens_wider ? fit_width_scale : fit_height_scale;
         }
         else if (zoom.type == ZOOM_COVER) {
            lens.scale = lens_wider ? fit_height_scale : fit_width_scale;
         }
      }
   }

   // validate scale
   if (lens.scale <= 0) {
      Con_Printf("init returned a scale of %f, which is  <= 0\n", lens.scale);
      return false;
   }

   return true;
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
// |              C->Lua (c functions for use in lua)                             |
// |                                                                              |
// --------------------------------------------------------------------------------

static int CtoLUA_latlon_to_ray(lua_State *L)
{
   double lat = luaL_checknumber(L,1);
   double lon = luaL_checknumber(L,2);
   vec3_t ray;
   latlon_to_ray(lat,lon,ray);
   lua_pushnumber(L, ray[0]);
   lua_pushnumber(L, ray[1]);
   lua_pushnumber(L, ray[2]);
   return 3;
}

static int CtoLUA_ray_to_latlon(lua_State *L)
{
   double rx = luaL_checknumber(L, 1);
   double ry = luaL_checknumber(L, 2);
   double rz = luaL_checknumber(L, 3);

   vec3_t ray = {rx,ry,rz};
   double lat,lon;
   ray_to_latlon(ray,&lat,&lon);

   lua_pushnumber(L, lat);
   lua_pushnumber(L, lon);
   return 2;
}

static int CtoLUA_plate_to_ray(lua_State *L)
{
   int plate_index = luaL_checknumber(L,1);
   double u = luaL_checknumber(L,2);
   double v = luaL_checknumber(L,3);
   vec3_t ray;
   if (plate_index < 0 || plate_index >= globe.numplates) {
      lua_pushnil(L);
      return 1;
   }

   plate_uv_to_ray(plate_index,u,v,ray);
   lua_pushnumber(L, ray[0]);
   lua_pushnumber(L, ray[1]);
   lua_pushnumber(L, ray[2]);
   return 3;
}

// -------------------------------------------------------------------------------- 
// |                                                                              |
// |                 Lua->C (lua functions for use in c)                          |
// |                                                                              |
// --------------------------------------------------------------------------------

static fisheye_status LUAtoC_lens_inverse(double x, double y, vec3_t ray)
{
   int top = lua_gettop(lua);
   lua_rawgeti(lua, LUA_REGISTRYINDEX, lua_refs.lens_inverse);
   lua_pushnumber(lua, x);
   lua_pushnumber(lua, y);
   lua_call(lua, 2, LUA_MULTRET);

   int numret = lua_gettop(lua) - top;
   fisheye_status status;

   switch(numret) {
      case 3:
         if (lua_isnumber(lua,-3) && lua_isnumber(lua,-2) && lua_isnumber(lua,-1)) {
            ray[0] = lua_tonumber(lua, -3);
            ray[1] = lua_tonumber(lua, -2);
            ray[2] = lua_tonumber(lua, -1);
            VectorNormalize(ray);
            status = FE_SUCCESS;
         }
         else {
            Con_Printf("lens_inverse returned a non-number value for x,y,z\n");
            status = NONSENSE_VALUE;
         }
         break;

      case 1:
         if (lua_isnil(lua,-1)) {
            status = NO_VALUE_RETURNED;
         }
         else {
            status = NONSENSE_VALUE;
            Con_Printf("lens_inverse returned a single non-nil value\n");
         }
         break;

      default:
         Con_Printf("lens_inverse returned %d values instead of 3\n", numret);
         status = FE_SUCCESS;
   }

   lua_pop(lua, numret);
   return status;
}

static fisheye_status LUAtoC_lens_forward(vec3_t ray, double *x, double *y)
{
   int top = lua_gettop(lua);
   lua_rawgeti(lua, LUA_REGISTRYINDEX, lua_refs.lens_forward);
   lua_pushnumber(lua,ray[0]);
   lua_pushnumber(lua,ray[1]);
   lua_pushnumber(lua,ray[2]);
   lua_call(lua, 3, LUA_MULTRET);

   int numret = lua_gettop(lua) - top;
   fisheye_status status;

   switch (numret) {
      case 2:
         if (lua_isnumber(lua,-2) && lua_isnumber(lua,-1)) {
            *x = lua_tonumber(lua, -2);
            *y = lua_tonumber(lua, -1);
            status = FE_SUCCESS;
         }
         else {
            Con_Printf("lens_forward returned a non-number value for x,y\n");
            status = NONSENSE_VALUE;
         }
         break;

      case 1:
         if (lua_isnil(lua,-1)) {
            status = NO_VALUE_RETURNED;
         }
         else {
            status = NONSENSE_VALUE;
            Con_Printf("lens_forward returned a single non-nil value\n");
         }
         break;

      default:
         Con_Printf("lens_forward returned %d values instead of 2\n", numret);
         status = NONSENSE_VALUE;
   }

   lua_pop(lua,numret);
   return status;
}

//out variables should be darned to hecc
static vec2_u LUAtoC_lens_forward_(vec3_u ray){
    double u, v;
    last_status = LUAtoC_lens_forward(ray.vec, &u, &v);
    return (vec2_u){u,v};
}

static vec3_u LUAtoC_lens_inverse_(vec2_u xy){
    vec3_u ray;
    last_status = LUAtoC_lens_inverse(xy.xy.x, xy.xy.y, ray.vec);
    return ray;
}

static fisheye_status LUAtoC_globe_plate(vec3_t ray, int *plate)
{
   lua_rawgeti(lua, LUA_REGISTRYINDEX, lua_refs.globe_plate);
   lua_pushnumber(lua, ray[0]);
   lua_pushnumber(lua, ray[1]);
   lua_pushnumber(lua, ray[2]);
   lua_call(lua, 3, LUA_MULTRET);

   if (!lua_isnumber(lua, -1))
   {
      lua_pop(lua,1);
      return NO_VALUE_RETURNED;
   }

   *plate = (int)lua_tointeger(lua,-1);
   lua_pop(lua,1);
   return FE_SUCCESS;
}

// -------------------------------------------------------------------------------- 
// |                                                                              |
// |                    Lua state management functions                            |
// |                                                                              |
// --------------------------------------------------------------------------------

qboolean LUA_load_lens(void)
{
   // clear Lua variables
   LUA_clear_lens();

   // set full filename
   char filename[100];
   sprintf(filename,"%s/lua-scripts/lenses/%s.lua", com_basedir, lens.name);

   // check if loaded correctly
   int errcode = 0;
   if ((errcode=luaL_loadfile(lua, filename))) {
      Con_Printf("could not loadfile (%d) \nERROR: %s", errcode, lua_tostring(lua,-1));
      lua_pop(lua,1); // pop error message
      return false;
   }
   else {
      if ((errcode=lua_pcall(lua, 0, 0, 0))) {
         Con_Printf("could not pcall (%d) \nERROR: %s", errcode, lua_tostring(lua,-1));
         lua_pop(lua,1); // pop error message
         return false;
      }
   }

   // clear current maps
   lens.map_type = MAP_NONE;
   lua_refs.lens_forward = lua_refs.lens_inverse = -1;

   // check if the inverse map function is provided
   lua_getglobal(lua, "lens_inverse");
   if (!lua_isfunction(lua,-1)) {
      // Con_Printf("lens_inverse is not found\n");
      lua_pop(lua,1); // pop lens_inverse
   }
   else {
      lua_refs.lens_inverse = luaL_ref(lua, LUA_REGISTRYINDEX);
      lens.map_type = MAP_INVERSE;
   }

   // check if the forward map function is provided
   lua_getglobal(lua, "lens_forward");
   if (!lua_isfunction(lua,-1)) {
      // Con_Printf("lens_forward is not found\n");
      lua_pop(lua,1); // pop lens_forward
   }
   else {
      lua_refs.lens_forward = luaL_ref(lua, LUA_REGISTRYINDEX);
      if (lens.map_type == MAP_NONE) {
         lens.map_type = MAP_FORWARD;
      }
   }

   // get map function preference if provided
   lua_getglobal(lua, "map");
   if (lua_isstring(lua, -1))
   {
      // get desired map function name
      const char* funcname = lua_tostring(lua, -1);

      // check for valid map function name
      if (!strcmp(funcname, "lens_inverse")) {
         lens.map_type = MAP_INVERSE;
      }
      else if (!strcmp(funcname, "lens_forward")) {
         lens.map_type = MAP_FORWARD;
      }
      else {
         Con_Printf("Unsupported map function: %s\n", funcname);
         lua_pop(lua, 1); // pop map
         return false;
      }
   }
   lua_pop(lua,1); // pop map

   lua_getglobal(lua, "max_fov");
   zoom.max_fov = (int)lua_isnumber(lua,-1) ? lua_tonumber(lua,-1) : 0;
   lua_pop(lua,1); // pop max_fov

   lua_getglobal(lua, "max_vfov");
   zoom.max_vfov = (int)lua_isnumber(lua,-1) ? lua_tonumber(lua,-1) : 0;
   lua_pop(lua,1); // pop max_vfov

   lua_getglobal(lua, "lens_width");
   lens.width = lua_isnumber(lua,-1) ? lua_tonumber(lua,-1) : 0;
   lua_pop(lua,1); // pop lens_width

   lua_getglobal(lua, "lens_height");
   lens.height = lua_isnumber(lua,-1) ? lua_tonumber(lua,-1) : 0;
   lua_pop(lua,1); // pop lens_height

   return true;
}

qboolean LUA_load_globe(void)
{
   // clear Lua variables
   LUA_clear_globe();

   // set full filename
   char filename[100];
   sprintf(filename, "%s/lua-scripts/globes/%s.lua",com_basedir,globe.name);

   // check if loaded correctly
   int errcode = 0;
   if ((errcode=luaL_loadfile(lua, filename))) {
      Con_Printf("could not loadfile (%d) \nERROR: %s", errcode, lua_tostring(lua,-1));
      lua_pop(lua,1); // pop error message
      return false;
   }
   else {
      if ((errcode=lua_pcall(lua, 0, 0, 0))) {
         Con_Printf("could not pcall (%d) \nERROR: %s", errcode, lua_tostring(lua,-1));
         lua_pop(lua,1); // pop error message
         return false;
      }
   }

   // check for the globe_plate function
   lua_refs.globe_plate = -1;
   if (lua_func_exists("globe_plate"))
   {
      lua_getglobal(lua, "globe_plate");
      lua_refs.globe_plate = luaL_ref(lua, LUA_REGISTRYINDEX);
   }

   // load plates array
   lua_getglobal(lua, "plates");
   if (!lua_istable(lua,-1) || lua_rawlen(lua,-1) < 1)
   {
      Con_Printf("plates must be an array of one or more elements\n");
      lua_pop(lua, 1); // pop plates
      return false;
   }

   // iterate plates
   int i = 0;
   int j;
   for (lua_pushnil(lua); lua_next(lua,-2); lua_pop(lua,1), ++i)
   {
      // get forward vector
      lua_rawgeti(lua, -1, 1);

      // verify table of length 3
      if (!lua_istable(lua,-1) || lua_rawlen(lua,-1) != 3 )
      {
         Con_Printf("plate %d: forward vector is not a 3d vector\n", i+1);
         lua_pop(lua, 3); // pop forward vector, plate, and plates
         return false;
      }

      // get forward vector elements
      for (j=0; j<3; ++j) {
         lua_rawgeti(lua, -1, j+1);
         if (!lua_isnumber(lua,-1))
         {
            Con_Printf("plate %d: forward vector: element %d not a number\n", i+1, j+1);
            lua_pop(lua, 4); // pop element, vector, plate, and plates
            return false;
         }
         globe.plates[i].forward[j] = lua_tonumber(lua,-1);
         lua_pop(lua, 1); // pop element
      }
      lua_pop(lua,1); // pop forward vector

      // get up vector
      lua_rawgeti(lua, -1, 2);

      // verify table of length 3
      if (!lua_istable(lua,-1) || lua_rawlen(lua,-1) != 3 )
      {
         Con_Printf("plate %d: up vector is not a 3d vector\n", i+1);
         lua_pop(lua, 3); // pop forward vector, plate, and plates
         return false;
      }

      // get up vector elements
      for (j=0; j<3; ++j) {
         lua_rawgeti(lua, -1, j+1);
         if (!lua_isnumber(lua,-1))
         {
            Con_Printf("plate %d: up vector: element %d not a number\n", i+1, j+1);
            lua_pop(lua, 4); // pop element, vector, plate, and plates
            return false;
         }
         globe.plates[i].up[j] = lua_tonumber(lua,-1);
         lua_pop(lua,1); // pop element
      }
      lua_pop(lua, 1); // pop up vector

      // calculate right vector (and correct up vector)
      CrossProduct(globe.plates[i].up, globe.plates[i].forward, globe.plates[i].right);
      CrossProduct(globe.plates[i].forward, globe.plates[i].right, globe.plates[i].up);

      // get fov
      lua_rawgeti(lua,-1,3);
      if (!lua_isnumber(lua,-1))
      {
         Con_Printf("plate %d: fov not a number\n", i+1);
      }
      globe.plates[i].fov = lua_tonumber(lua,-1) * M_PI / 180;
      lua_pop(lua, 1); // pop fov

      if (globe.plates[i].fov <= 0)
      {
         Con_Printf("plate %d: fov must > 0\n", i+1);
         return false;
      }

      // calculate distance to camera
      globe.plates[i].dist = 0.5/tan(globe.plates[i].fov/2);
   }
   lua_pop(lua, 1); // pop plates

   globe.numplates = i;

   return true;
}

#define CLEARVAR(var) lua_pushnil(lua); lua_setglobal(lua, var);

// used to clear the state when switching lenses
static void LUA_clear_lens(void)
{
   CLEARVAR("map");
   CLEARVAR("max_fov");
   CLEARVAR("max_vfov");
   CLEARVAR("lens_width");
   CLEARVAR("lens_height");
   CLEARVAR("lens_inverse");
   CLEARVAR("lens_forward");
   CLEARVAR("onload");

   // set "numplates" var
   lua_pushinteger(lua, globe.numplates);
   lua_setglobal(lua, "numplates");
}

// used to clear the state when switching globes
static void LUA_clear_globe(void)
{
   CLEARVAR("plates");
   CLEARVAR("globe_plate");

   globe.numplates = 0;
}

#undef CLEARVAR

static qboolean lua_func_exists(const char* name)
{
   lua_getglobal(lua, name);
   int exists = lua_isfunction(lua,-1);
   lua_pop(lua, 1); // pop name
   return exists;
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

   if (lua_refs.globe_plate != -1) {
      // use user-defined plate selection function
      if (LUAtoC_globe_plate(ray, &plate_index)) {
         return plate_index;
      }
      last_status = NONSENSE_VALUE;
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
   double dist = 0.5 / tan(globe.plates[plate_index].fov/2);
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

   start_lens_builder_clock();
   for(ly = &(lens_builder.inverse_state.ly); *ly >= 0; --(*ly))
   {
      // pause building if we have exceeded time allowed per frame
      if (is_lens_builder_time_up()) {
         return true; 
      }

      y = -(*ly-lens.height_px/2) * lens.scale;

      // calculate all the pixels in this row
      for(lx = 0;lx<lens.width_px;++lx)
      {
         x = (lx-lens.width_px/2) * lens.scale;

         // determine which light ray to follow
         vec3_t ray;
         fisheye_status status = LUAtoC_lens_inverse(x,y,ray);
         if (status == NO_VALUE_RETURNED) {
            continue;
         }
         else if (status == NONSENSE_VALUE) {
            return false;
         }

         // get the pixel belonging to the light ray
         set_lensmap_from_ray(lx,*ly,ray[0],ray[1],ray[2]);
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

   start_lens_builder_clock();
   for (; *plate_index < globe.numplates; ++(*plate_index))
   {
      int px;
      for (; *py >=0; --(*py)) {

         // pause building if we have exceeded time allowed per frame
         if (is_lens_builder_time_up()) {
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
                  fisheye_status status = uv_to_screen(*plate_index, u, v, &bot[0], &bot[1]);
                   if (status == NO_VALUE_RETURNED){ continue;}
                   else if (status == NONSENSE_VALUE){ return false;}
               }
               // compute right point
               double u = (px + 0.5) / platesize;
               int index = 2*(px+1);
               fisheye_status status = uv_to_screen(*plate_index, u, v, &bot[index], &bot[index+1]);
                if (status == NO_VALUE_RETURNED) {continue;}
                else if (status == NONSENSE_VALUE) {return false;}
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
               fisheye_status status = uv_to_screen(*plate_index, u, v, &top[0], &top[1]);
                if (status == NO_VALUE_RETURNED) {continue;}
                else if (status == NONSENSE_VALUE) {return false;}
            }
            // compute right point
            double u = (px + 0.5) / platesize;
            int index = 2*(px+1);
            fisheye_status status = uv_to_screen(*plate_index, u, v, &top[index], &top[index+1]);
             if (status == NO_VALUE_RETURNED) {continue;}
             else if (status == NONSENSE_VALUE) {return false;}
         }

         // DRAW QUAD FOR EACH PIXEL IN THIS TEXTURE ROW ***********************************

         v = ((double)*py)/platesize;
         for (px = 0; px < platesize; ++px) {
            
            // skip overlapping region of texture
            double u = ((double)px)/platesize;
            vec3_t ray;
            plate_uv_to_ray(*plate_index, u, v, ray);
            if (*plate_index != ray_to_plate_index(ray)) {
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

// -------------------------------------------------------------------------------- 
// |                                                                              |
// |                  FORWARD MAP GETTER/SETTER HELPERS                           |
// |                                                                              |
// --------------------------------------------------------------------------------

// convenience function for forward map calculation:
//    maps uv coordinate on a texture to a screen coordinate
static int uv_to_screen(int plate_index, double u, double v, int *lx, int *ly)
{
    vec2_u uv = {u,v};
    point2d screenPoint = uv_to_screen_(arg_consolidator, plate_index, uv);
    *lx = screenPoint.xy.x;
    *ly = screenPoint.xy.y;

   return last_status;
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
   if (!calc_zoom()) {
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
   byte *pixels = GLOBEPIXEL(plate_index, 0, 0);

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
