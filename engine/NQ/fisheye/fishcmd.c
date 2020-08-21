#include <lua.h>
#include <stdio.h>

#include "cmd.h"
#include "common.h"
#include "console.h"
#include "fisheye.h"
#include "fishcmd.h"
#include "host.h"
#include "imageutil.h"
#include "vid.h"
#include "zone.h"

// console commands
static void cmd_fisheye(void);
static void cmd_help(void);
static void cmd_lens(void);
static void cmd_globe(void);
static void cmd_fov(void);
static void cmd_vfov(void);
static void cmd_dumppal(void);
static void cmd_rubix(void);
static void cmd_rubixgrid(void);
static void cmd_cover(void);
static void cmd_contain(void);
static void cmd_saveglobe(void);
static void cmd_shortcutkeys(void);
static void cmd_saverubix(void);
static void cmd_savelens(void);

// console autocomplete helpers
static struct stree_root * cmdarg_lens(const char *arg);
static struct stree_root * cmdarg_globe(const char *arg);

//TODO: isolate global vars and state; Follow tell, don't ask
static struct _zoom *zoom = NULL;
static struct _lens *lens = NULL;
static struct _globe *globe = NULL;
static struct _rubix *rubix = NULL;
static lua_State *lua = NULL;

qboolean shortcutkeys_enabled;

void F_init_commands(
	struct _zoom *zoom_,
        struct _rubix *rubix_,
	lua_State *lua_
){
   zoom = zoom_;
   rubix = rubix_;
   lens = F_getLens();
   globe = F_getGlobe();
   
   lua = lua_;
   
   Cmd_AddCommand("fisheye", cmd_fisheye);
   Cmd_AddCommand("f_help", cmd_help);
   Cmd_AddCommand("f_dumppal", cmd_dumppal);
   Cmd_AddCommand("f_rubix", cmd_rubix);
   Cmd_AddCommand("f_rubixgrid", cmd_rubixgrid);
   Cmd_AddCommand("f_cover", cmd_cover);
   Cmd_AddCommand("f_contain", cmd_contain);
   Cmd_AddCommand("f_fov", cmd_fov);
   Cmd_AddCommand("f_vfov", cmd_vfov);
   Cmd_AddCommand("f_lens", cmd_lens);
   Cmd_SetCompletion("f_lens", cmdarg_lens);
   Cmd_AddCommand("f_globe", cmd_globe);
   Cmd_SetCompletion("f_globe", cmdarg_globe);
   Cmd_AddCommand("f_saveglobe", cmd_saveglobe);
   Cmd_AddCommand("f_shortcutkeys", cmd_shortcutkeys);
   Cmd_AddCommand("f_saverubix", cmd_saverubix);
   Cmd_AddCommand("f_dumplens", cmd_savelens);
}

static void clear_zoom(void)
{
   (*zoom).type = ZOOM_NONE;
   (*zoom).fov = 0;
   (*zoom).changed = true; // trigger change
}

static void print_zoom(void)
{
   Con_Printf("Zoom currently: ");
   switch ((*zoom).type) {
      case ZOOM_FOV:     Con_Printf("f_fov %.2d", (*zoom).fov); break;
      case ZOOM_VFOV:    Con_Printf("f_vfov %.2d", (*zoom).fov); break;
      case ZOOM_COVER:   Con_Printf("f_cover"); break;
      case ZOOM_CONTAIN: Con_Printf("f_contain"); break;
      default:           Con_Printf("none");
   }
   Con_Printf("\n");
}


static void cmd_shortcutkeys(void)
{
   shortcutkeys_enabled = !shortcutkeys_enabled;
   if (shortcutkeys_enabled) {
      Con_Printf("Enabled Fisheye shortcut keys: 1-9 = Lenses, Y,U,I,O,P = Globes\n");
      Cmd_ExecuteString("bind 1 \"f_lens panini\"", src_command);
      Cmd_ExecuteString("bind 2 \"f_lens stereographic\"", src_command);
      Cmd_ExecuteString("bind 3 \"f_lens hammer\"", src_command);
      Cmd_ExecuteString("bind 4 \"f_lens winkeltripel\"", src_command);
      Cmd_ExecuteString("bind 5 \"f_lens fisheye1\"", src_command);
      Cmd_ExecuteString("bind 6 \"f_lens mercator\"", src_command);
      Cmd_ExecuteString("bind 7 \"f_lens quincuncial\"", src_command);
      Cmd_ExecuteString("bind 8 \"f_lens cube\"", src_command);
      Cmd_ExecuteString("bind 9 \"f_lens debug\"", src_command);
      Cmd_ExecuteString("bind y \"f_globe cube\"", src_command);
      Cmd_ExecuteString("bind u \"f_globe cube_edge\"", src_command);
      Cmd_ExecuteString("bind i \"f_globe trism\"", src_command);
      Cmd_ExecuteString("bind o \"f_globe tetra\"", src_command);
      Cmd_ExecuteString("bind p \"f_globe fast\"", src_command);
   }
   else {
      Con_Printf("Disabled Fisheye shortcut keys\n");
      Cmd_ExecuteString("bind 1 \"impulse 1\"", src_command);
      Cmd_ExecuteString("bind 2 \"impulse 2\"", src_command);
      Cmd_ExecuteString("bind 3 \"impulse 3\"", src_command);
      Cmd_ExecuteString("bind 4 \"impulse 4\"", src_command);
      Cmd_ExecuteString("bind 5 \"impulse 5\"", src_command);
      Cmd_ExecuteString("bind 6 \"impulse 6\"", src_command);
      Cmd_ExecuteString("bind 7 \"impulse 7\"", src_command);
      Cmd_ExecuteString("bind 8 \"impulse 8\"", src_command);
      Cmd_ExecuteString("unbind 9", src_command);
      Cmd_ExecuteString("unbind y", src_command);
      Cmd_ExecuteString("unbind u", src_command);
      Cmd_ExecuteString("unbind i", src_command);
      Cmd_ExecuteString("unbind o", src_command);
      Cmd_ExecuteString("unbind p", src_command);
   }
}

static void cmd_help(void)
{
   Con_Printf("-----------------------------\n");
   Con_Printf("Welcome to the FISHEYE ADDON!\n");
   Con_Printf("-> fisheye 1    (ENABLE)\n");
   Con_Printf("-> fisheye 0    (DISABLE)\n");
   Con_Printf("\n");
   Con_Printf("-> f_lens <tab>    (CHANGE LENS)\n");
   Con_Printf("-> f_fov <degrees> (SET FOV)\n");
   Con_Printf("\n");
   Con_Printf("-> f_<tab>         (MORE COMMANDS)\n");
   Con_Printf("-----------------------------\n");
}

static void cmd_fov(void)
{
   if (Cmd_Argc() < 2) { // no fov given
      Con_Printf("f_fov <degrees>: set horizontal FOV\n");
      print_zoom();
      return;
   }

   clear_zoom();

   (*zoom).type = ZOOM_FOV;
   (*zoom).fov = (int)Q_atof(Cmd_Argv(1)); // will return 0 if not valid
}

static void cmd_vfov(void)
{
   if (Cmd_Argc() < 2) { // no fov given
      Con_Printf("f_vfov <degrees>: set vertical FOV\n");
      print_zoom();
      return;
   }

   clear_zoom();

   (*zoom).type = ZOOM_VFOV;
   (*zoom).fov = (int)Q_atof(Cmd_Argv(1)); // will return 0 if not valid
}

static void cmd_dumppal(void)
{
   dumppal(host_basepal);
}

static void cmd_rubix(void)
{
   (*rubix).enabled = !(*rubix).enabled;
   Con_Printf("Rubix is %s\n", (*rubix).enabled ? "ON" : "OFF");
}

static void cmd_rubixgrid(void)
{
   if (Cmd_Argc() == 4) {
      (*rubix).numcells = Q_atof(Cmd_Argv(1));
      (*rubix).cell_size = Q_atof(Cmd_Argv(2));
      (*rubix).pad_size = Q_atof(Cmd_Argv(3));
      (*lens).changed = true; // need to recompute lens to update grid
   }
   else {
      Con_Printf("RubixGrid <numcells> <cellsize> <padsize>\n");
      Con_Printf("   numcells (default 10) = %d\n", (*rubix).numcells);
      Con_Printf("   cellsize (default  4) = %f\n", (*rubix).cell_size);
      Con_Printf("   padsize  (default  1) = %f\n", (*rubix).pad_size);
   }
}

// lens command
static void cmd_lens(void)
{
   if (Cmd_Argc() < 2) { // no lens name given
      Con_Printf("f_lens <name>: use a new lens\n");
      Con_Printf("Currently: %s\n", (*lens).name);
      return;
   }

   // trigger change
   (*lens).changed = true;

   // get name
   strcpy((*lens).name, Cmd_Argv(1));

   // Display name
   Con_Printf("f_lens %s", (*lens).name);

   // load lens
   (*lens).valid = LUA_load_lens();
   if (!(*lens).valid) {
      strcpy((*lens).name,"");
      Con_Printf("not a valid lens\n");
   }

   // execute the lens' onload command string if given
   // (this is to provide a user-friendly default view of the lens (e.g. "f_fov 180"))
   lua_getglobal(lua, "onload");
   if (lua_isstring(lua, -1))
   {
      const char* onload = lua_tostring(lua, -1);
      Cmd_ExecuteString(onload, src_command);

      // display onload
      Con_Printf("; %s\n", onload);
   }
   else {
      // fail silently for now, resulting from two cases:
      // 1. onload is nil (undefined)
      // 2. onload is not a string
      Con_Printf("\n");
   }
   lua_pop(lua, 1); // pop "onload"
}

// autocompletion for lens names
static struct stree_root * cmdarg_lens(const char *arg)
{
   struct stree_root *root;

   root = Z_Malloc(sizeof(struct stree_root));
   if (root) {
      *root = STREE_ROOT;

      STree_AllocInit();
      COM_ScanDir(root, "../lua-scripts/lenses", arg, ".lua", true);
   }
   return root;
}

static void cmd_cover(void)
{
   clear_zoom();
   (*zoom).type = ZOOM_COVER;
}

static void cmd_contain(void)
{
   clear_zoom();
   (*zoom).type = ZOOM_CONTAIN;
}

static void cmd_fisheye(void)
{
   if (Cmd_Argc() < 2) {
      Con_Printf("Currently: ");
      Con_Printf("fisheye %d\n", fisheye_enabled);
      Con_Printf("\nTry F_HELP for more options and commands.\n");
      return;
   }
   fisheye_enabled = Q_atoi(Cmd_Argv(1)); // will return 0 if not valid
   vid.recalc_refdef = true;
}

static void cmd_saveglobe(void)
{
   if (Cmd_Argc() < 2) { // no file name given
      Con_Printf("f_saveglobe <name> [full flag=0]: screenshot the globe plates\n");
      return;
   }

   strncpy((*globe).save.name, Cmd_Argv(1), 32);

   if (Cmd_Argc() >= 3) {
      (*globe).save.with_margins = Q_atoi(Cmd_Argv(2));
   }
   else {
      (*globe).save.with_margins = 0;
   }
   (*globe).save.should = true; //TODO: remove out-of-band signalling
}

static void cmd_saverubix(void){
   if (Cmd_Argc() < 2) { // no file name given
        Con_Printf("f_saverubix <name> [full flag=0]: screenshot \"rubix\"\n");
        return;
   }
   
   char filetitle[32];
   strncpy(filetitle, Cmd_Argv(1), 32);
   filetitle[31] = 0;
   
   char filename[37];
   snprintf(filename, 37, "%s.png", filetitle);
   
   dumpTints(lens, filename);
   
   Con_Printf("rubix saved to %s\n", filename);
}

static void cmd_savelens(void){
   if (Cmd_Argc() < 2) { // no file name given
        Con_Printf("f_dumplens <name> [full flag=0]: dump lens to image\n");
        return;
   }
   
   char filetitle[32];
   strncpy(filetitle, Cmd_Argv(1), 32);
   filetitle[31] = 0;
   
   char filename[37];
   snprintf(filename, 37, "%s.png", filetitle);
   
   dumpIndicies(lens, filename);
   
   Con_Printf("lens indicies saved to %s\n", filename);
}

static void cmd_globe(void)
{
   if (Cmd_Argc() < 2) { // no globe name given
      Con_Printf("f_globe <name>: use a new globe\n");
      Con_Printf("Currently: %s\n", (*globe).name);
      return;
   }

   // trigger change
   (*globe).changed = true;

   // get name
   strcpy((*globe).name, Cmd_Argv(1));

   // display name
   Con_Printf("f_globe %s\n", (*globe).name);

   // load globe
   (*globe).valid = LUA_load_globe();
   if (!(*globe).valid) {
      strcpy((*globe).name,"");
      Con_Printf("not a valid globe\n");
   }
}

// autocompletion for globe names
static struct stree_root * cmdarg_globe(const char *arg)
{
   struct stree_root *root;

   root = Z_Malloc(sizeof(struct stree_root));
   if (root) {
      *root = STREE_ROOT;

      STree_AllocInit();
      COM_ScanDir(root, "../lua-scripts/globes", arg, ".lua", true);
   }
   return root;
}
