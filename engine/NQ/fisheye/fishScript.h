// a reusable interface for the accessing a script interpreter
#include "fisheye.h"
#include "fishlens.h"

#ifndef FISHSCRIPT_H
#define FISHSCRIPT_H

typedef struct _lua_refs {
   int lens_forward;
   int lens_inverse;
   int globe_plate;
} script_refs;

void F_scriptInit(void);

void F_scriptShutdown(void);

qboolean F_load_lens(const char *name, struct _lens *lens, struct _zoom *zoom, int numplates);

qboolean F_load_globe(const char *name, struct _globe *globe);

void F_clear_lens(int numplates);

void F_clear_globe(void);

vec3_u scriptToC_lens_inverse(vec2_u xy);

vec2_u scriptToC_lens_forward(vec3_u ray);

int scriptToC_globe_plate(vec3_u ray);

void* F_getStateHolder(void); //thanks, I hate it

script_refs* F_getScriptRef(void);

#endif
