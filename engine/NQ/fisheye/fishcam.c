#include "mathlib.h"
#include "fishlens.h"
#include "fishcam.h"
#include "sdl_common.h"
#include "vid.h"
#include "host.h"
#include <stdio.h>
#include <SDL2/SDL.h>

static SDL_Surface* getSurfaceFromVidBuffer();
static void copyCameraDirections(CameraFacade *cameraFacade);

SDL_Surface* rendererFacade(
		CameraFacade *cameraFacade,
		SDL_Surface *plateBackBuffer){ 
	
	SDL_Surface* local;

//callee is responsible for destroying this SDL_Surface when the viewport dimensions change
	if(!!!plateBackBuffer){
		local = getSurfaceFromVidBuffer();
	} else {
		local = plateBackBuffer;
	}

	if(!!!local){
		printf("%s", SDL_GetError());
	}

	copyCameraDirections(cameraFacade);
	R_PushDlights();
	R_RenderView();

	return local;
}

static void copyCameraDirections(CameraFacade *cameraFacade){
	VectorCopy(cameraFacade->eyeDirection.upward.vec, r_refdef.up);
	VectorCopy(cameraFacade->eyeDirection.forward.vec, r_refdef.forward);
	VectorCopy(cameraFacade->eyeDirection.right.vec, r_refdef.right);
}

static SDL_Surface* getSurfaceFromVidBuffer(){
	uint32_t pixelformat = sizeof(pixel_t) == 1 ? SDL_PIXELFORMAT_INDEX8 :
		 sdl_desktop_format->format;
	
	return SDL_CreateRGBSurfaceWithFormatFrom(
			vid.buffer, vid.width, vid.height, sizeof(pixel_t)*8,
				vid.rowbytes, pixelformat
		);
}