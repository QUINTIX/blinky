#include "mathlib.h"
#include "fishlens.h"
#include <SDL2/SDL_surface.h>

/* meant to externalize part of render_plate in fisheye.c
 * currently is untested, dead code */

typedef struct _basisVectors {
	vec3_u forward;
	vec3_u right;
	vec3_u upward;
} BasisVectors;

typedef struct _CameraFacade {
	BasisVectors eyeDirection;
	BasisVectors headDirection; // for normalizing sprite and sky directions
	vec_t plateFov;
	vec2_u *shutterShape; // for culling geometery outside non-square plates
	int32_t shutterShapeEdges;
	int32_t plateSize; // round out to 192 bytes
} CameraFacade;

SDL_Surface* rendererFacade(
	CameraFacade *cameraFacade,
	SDL_Surface *plateBackBuffer);
