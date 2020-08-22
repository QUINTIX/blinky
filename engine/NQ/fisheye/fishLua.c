#include "mathlib.h"
#include "quakedef.h"
#include "console.h"
#include "common.h"
#include <stdlib.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>

#include "fisheye.h"
#include "fishlens.h"
#include "fishScript.h"

// c->lua (c functions for use in lua)
static int CtoLUA_latlon_to_ray(lua_State *L);
static int CtoLUA_ray_to_latlon(lua_State *L);
static int CtoLUA_plate_to_ray(lua_State *L);

// lua helpers
static qboolean lua_func_exists(const char* name);
static qboolean lua_loadAPlate(int i, struct _globe* globe);

// the Lua state pointer
static lua_State *lua;

// lua reference indexes (for reference lua functions)
static script_refs scriptRef;

void* F_getStateHolder() {
   return (void*)lua;
} //I hate this oh so very much.

script_refs* F_getScriptRef() {
   return &scriptRef;
}

void F_scriptInit() {
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

void F_scriptShutdown(void) {
   if(NULL != lua)
      lua_close(lua);
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
   vec3_u ray = latlon_to_ray((vec2_u){{lat,lon}});
   lua_pushnumber(L, ray.xyz.x);
   lua_pushnumber(L, ray.xyz.y);
   lua_pushnumber(L, ray.xyz.z);
   return 3;
}

static int CtoLUA_ray_to_latlon(lua_State *L)
{
   double rx = luaL_checknumber(L, 1);
   double ry = luaL_checknumber(L, 2);
   double rz = luaL_checknumber(L, 3);

   vec3_u ray = {.vec={rx,ry,rz}};
   vec2_u latlon = ray_to_latlon(ray);

   lua_pushnumber(L, latlon.latlon.lat);
   lua_pushnumber(L, latlon.latlon.lon);
   return 2;
}

static int CtoLUA_plate_to_ray(lua_State *L)
{
   struct _globe* globe = F_getGlobe();
   int plate_index = luaL_checknumber(L,1);
   double u = luaL_checknumber(L,2);
   double v = luaL_checknumber(L,3);
   vec2_u uv = {{u,v}};
   if (plate_index < 0 || plate_index >= globe->numplates) {
      lua_pushnil(L);
      return 1;
   }

   vec3_u ray_ = plate_uv_to_ray(globe, plate_index, uv);
    
   lua_pushnumber(L, ray_.xyz.x);
   lua_pushnumber(L, ray_.xyz.y);
   lua_pushnumber(L, ray_.xyz.z);
   return 3;
}

// -------------------------------------------------------------------------------- 
// |                                                                              |
// |                 Lua->C (lua functions for use in c)                          |
// |                                                                              |
// --------------------------------------------------------------------------------
vec3_u scriptToC_lens_inverse(vec2_u xy)
{
   vec3_u retval;

   int top = lua_gettop(lua);
   lua_rawgeti(lua, LUA_REGISTRYINDEX, scriptRef.lens_inverse);
   lua_pushnumber(lua, xy.xy.x);
   lua_pushnumber(lua, xy.xy.y);
   lua_call(lua, 2, LUA_MULTRET);

   int numret = lua_gettop(lua) - top;

   switch(numret) {
      case 3:
         if (lua_isnumber(lua,-3) && lua_isnumber(lua,-2) && lua_isnumber(lua,-1)) {
            retval = (vec3_u){{
               lua_tonumber(lua, -3),
               lua_tonumber(lua, -2),
               lua_tonumber(lua, -1)
            }};
            VectorNormalize(retval.vec);
            fe_clear();
         } else {
            Con_Printf("lens_inverse returned a non-number value for x,y,z\n");
            fe_throw(NONSENSE_VALUE);
            retval = (vec3_u){{0,0,0}};
         }
       break;

      case 1:
         if (lua_isnil(lua,-1)) {
            fe_throw(NO_VALUE_RETURNED);
         }
         else {
            fe_throw(NONSENSE_VALUE);
            Con_Printf("lens_inverse returned a single non-nil value\n");
         }
         retval = (vec3_u){{0,0,0}};
         break;

      default:
         Con_Printf("lens_inverse returned %d values instead of 3\n", numret);
         fe_clear();
         retval = (vec3_u){{0,0,0}};
   }

   lua_pop(lua, numret);
   return retval;
}

vec2_u scriptToC_lens_forward(vec3_u ray)
{
   vec2_u retval;

   int top = lua_gettop(lua);
   lua_rawgeti(lua, LUA_REGISTRYINDEX, scriptRef.lens_forward);
   lua_pushnumber(lua,ray.xyz.x);
   lua_pushnumber(lua,ray.xyz.y);
   lua_pushnumber(lua,ray.xyz.z);
   lua_call(lua, 3, LUA_MULTRET);

   int numret = lua_gettop(lua) - top;

      switch (numret) {
      case 2:
         if (lua_isnumber(lua,-2) && lua_isnumber(lua,-1)) {
            retval = (vec2_u){.xy ={
               .x= lua_tonumber(lua, -2),
               .y= lua_tonumber(lua, -1)}};
            fe_clear();
         }
         else {
            Con_Printf("lens_forward returned a non-number value for x,y\n");
            fe_throw(NONSENSE_VALUE);
            retval = (vec2_u){{0,0}};
         }
         break;

      case 1:
         if (lua_isnil(lua,-1)) {
            fe_throw(NO_VALUE_RETURNED);
            retval = (vec2_u){{0,0}};
         }
         else {
            fe_throw(NONSENSE_VALUE);
            Con_Printf("lens_forward returned a single non-nil value\n");
            retval = (vec2_u){{0,0}};
         }
         break;

      default:
         Con_Printf("lens_forward returned %d values instead of 2\n", numret);
         fe_throw(NONSENSE_VALUE);
            retval = (vec2_u){{0,0}};
   }

   lua_pop(lua,numret);

   return retval;
}

int scriptToC_globe_plate(vec3_u ray) {
   lua_rawgeti(lua, LUA_REGISTRYINDEX, scriptRef.globe_plate);
   lua_pushnumber(lua, (double)ray.xyz.x);
   lua_pushnumber(lua, (double)ray.xyz.y);
   lua_pushnumber(lua, (double)ray.xyz.z);
   lua_call(lua, 3, LUA_MULTRET);

   if (!lua_isnumber(lua, -1))
   {
      lua_pop(lua,1);
      fe_throw(NO_VALUE_RETURNED);
      return 0;
   }

   int plate = (int)lua_tointeger(lua,-1);
   lua_pop(lua,1);
   fe_clear();
   return plate;
}

#define CLEARVAR(var) lua_pushnil(lua); lua_setglobal(lua, var);

// used to clear the state when switching lenses
void F_clear_lens(int numplates) {
   CLEARVAR("map");
   CLEARVAR("max_fov");
   CLEARVAR("max_vfov");
   CLEARVAR("lens_width");
   CLEARVAR("lens_height");
   CLEARVAR("lens_inverse");
   CLEARVAR("lens_forward");
   CLEARVAR("onload");

   // set "numplates" var
   lua_pushinteger(lua, numplates);
   lua_setglobal(lua, "numplates");
}

void F_clear_globe(void) {
   CLEARVAR("plates");
   CLEARVAR("globe_plate");
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
// |                    Lua state management functions                            |
// |                                                                              |
// --------------------------------------------------------------------------------

qboolean F_load_lens(const char *name, struct _lens *lens, struct _zoom *zoom, int numplates)
{
   // clear Lua variables
   F_clear_lens(numplates);

   // set full filename
   char filename[0x100];
   snprintf(filename, sizeof(filename), "%s/lua-scripts/lenses/%s.lua",
      com_basedir, name);

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
   lens->map_type = MAP_NONE;
   scriptRef.lens_forward = scriptRef.lens_inverse = -1;

   // check if the inverse map function is provided
   lua_getglobal(lua, "lens_inverse");
   if (!lua_isfunction(lua,-1)) {
      // Con_Printf("lens_inverse is not found\n");
      lua_pop(lua,1); // pop lens_inverse
   }
   else {
      scriptRef.lens_inverse = luaL_ref(lua, LUA_REGISTRYINDEX);
      lens->map_type = MAP_INVERSE;
   }

   // check if the forward map function is provided
   lua_getglobal(lua, "lens_forward");
   if (!lua_isfunction(lua,-1)) {
      // Con_Printf("lens_forward is not found\n");
      lua_pop(lua,1); // pop lens_forward
   }
   else {
      scriptRef.lens_forward = luaL_ref(lua, LUA_REGISTRYINDEX);
      if (lens->map_type == MAP_NONE) {
         lens->map_type = MAP_FORWARD;
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
         lens->map_type = MAP_INVERSE;
      }
      else if (!strcmp(funcname, "lens_forward")) {
         lens->map_type = MAP_FORWARD;
      }
      else {
         Con_Printf("Unsupported map function: %s\n", funcname);
         lua_pop(lua, 1); // pop map
         return false;
      }
   }
   lua_pop(lua,1); // pop map

   lua_getglobal(lua, "max_fov");
   zoom->max_fov = (int)lua_isnumber(lua,-1) ? lua_tonumber(lua,-1) : 0;
   lua_pop(lua,1); // pop max_fov

   lua_getglobal(lua, "max_vfov");
   zoom->max_vfov = (int)lua_isnumber(lua,-1) ? lua_tonumber(lua,-1) : 0;
   lua_pop(lua,1); // pop max_vfov

   lua_getglobal(lua, "lens_width");
   lens->width = lua_isnumber(lua,-1) ? lua_tonumber(lua,-1) : 0;
   lua_pop(lua,1); // pop lens_width

   lua_getglobal(lua, "lens_height");
   lens->height = lua_isnumber(lua,-1) ? lua_tonumber(lua,-1) : 0;
   lua_pop(lua,1); // pop lens_height

   return true;
}

qboolean F_load_globe(const char *name, struct _globe *globe) {
   // clear Lua variables
   F_clear_globe();

   // set full filename
   char filename[0x100];
   snprintf(filename, sizeof(filename), "%s/lua-scripts/globes/%s.lua",
      com_basedir, name);

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
   scriptRef.globe_plate = -1;
   if (lua_func_exists("globe_plate"))
   {
      lua_getglobal(lua, "globe_plate");
      scriptRef.globe_plate = luaL_ref(lua, LUA_REGISTRYINDEX);
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
   for (lua_pushnil(lua); lua_next(lua,-2); lua_pop(lua,1), ++i) {
      if(!lua_loadAPlate(i, globe)){
          return false;}
   }
   lua_pop(lua, 1); // pop plates

   globe->numplates = i;

   return true;
}

static qboolean lua_loadAPlate(int i, struct _globe* globe) {
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
   for (int j=0; j<3; ++j) {
      lua_rawgeti(lua, -1, j+1);
      if (!lua_isnumber(lua,-1))
      {
         Con_Printf("plate %d: forward vector: element %d not a number\n", i+1, j+1);
         lua_pop(lua, 4); // pop element, vector, plate, and plates
         return false;
      }
      (*globe).plates[i].forward[j] = lua_tonumber(lua,-1);
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
   for (int j=0; j<3; ++j) {
      lua_rawgeti(lua, -1, j+1);
      if (!lua_isnumber(lua,-1))
      {
         Con_Printf("plate %d: up vector: element %d not a number\n", i+1, j+1);
         lua_pop(lua, 4); // pop element, vector, plate, and plates
         return false;
      }
      (*globe).plates[i].up[j] = lua_tonumber(lua,-1);
      lua_pop(lua,1); // pop element
   }
   lua_pop(lua, 1); // pop up vector

   // calculate right vector (and correct up vector)
   CrossProduct((*globe).plates[i].up, (*globe).plates[i].forward, (*globe).plates[i].right);
   CrossProduct((*globe).plates[i].forward, (*globe).plates[i].right, (*globe).plates[i].up);

   // get fov
   lua_rawgeti(lua,-1,3);
   if (!lua_isnumber(lua,-1))
   {
      Con_Printf("plate %d: fov not a number\n", i+1);
   }
   (*globe).plates[i].fov = lua_tonumber(lua,-1) * M_PI / 180;
   lua_pop(lua, 1); // pop fov

   if ((*globe).plates[i].fov <= 0)
   {
      Con_Printf("plate %d: fov must > 0\n", i+1);
      return false;
   }

   // calculate distance to camera
   (*globe).plates[i].dist = 0.5/tan((*globe).plates[i].fov/2);
   
   return true;
}
