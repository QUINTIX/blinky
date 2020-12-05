#include "fishScript.h"
#include "fishlens.h"

void F_scriptInit(void){}

void F_scriptShutdown(void){}

const static vec_t EPSILON = 0.125/(0x4000000);

static script_refs scriptRef;

script_refs* F_getScriptRef(void){
	return &scriptRef;
};

qboolean F_load_lens(const char *name, struct _lens *lens, struct _zoom *zoom, int numplates){
	return true;
}

qboolean F_load_globe(const char *name, struct _globe *globe){
	return true;
}

void F_clear_lens(int numplates){}

void F_clear_globe(void){}

vec3_u scriptToC_lens_inverse(vec2_u xy){
	return (vec3_u){{xy.xy.x, xy.xy.y, 0}};
};

vec2_u scriptToC_lens_forward(vec3_u ray){
	vec_t z = ray.xyz.z;
	if(absf(z) > EPSILON){
		return (vec2_u){{ray.xyz.x/z,ray.xyz.y/z}};
	} else {
		return (vec2_u){{0,0}};
	}
}

int scriptToC_globe_plate(vec3_u ray){
	int xpositive = ray.xyz.x > 0 ? 4 : 0;
	int ypositive = ray.xyz.y > 0 ? 2 : 0;
	int zpositive = ray.xyz.z > 0 ? 1 : 0;
	return (xpositive | ypositive | zpositive) % 6;
}
