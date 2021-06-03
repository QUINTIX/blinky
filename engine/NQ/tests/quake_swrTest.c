#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdlib.h>
#include <cmocka.h>
#include <math.h>
#include <stdarg.h>
#include <float.h>

#include "qtypes.h"
#include "quakedef.h"
#include "sys.h"
#include "screen.h"
#include "host.h"
#include "cmd.h"
#include "zone.h"
#include "vid.h"

#include "fishcam.h"
#define MAX_PRINTMSG 4096
//tests for using facade to software renderer



//mocks for weird fisheye dependencies
#ifdef FISHEYE_H_
#define __wrap_F_Init F_Init
#define __wrap_F_Shutdown F_Shutdown
#define __wrap_F_RenderView F_RenderView 
#endif
void __wrap_F_Init(void){}
void __wrap_F_Shutdown(void){}
void __wrap_F_RenderView(void){}

double fisheye_plate_fov;
qboolean fisheye_enabled;
static int setup(void** state);
static int teardown(void** state);

static struct MyState {
	double lastFrameTime;
	quakeparms_t *params;
} myState;

static SDL_Surface *surf;

static void test_create_surface_if_not_extant(void** state);
static void test_camera_directions_are_copied_over(void ** state);
static void test_surface_is_mapped_to_quake_backbuffer(const SDL_Surface* surf);

int main(void) {
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_create_surface_if_not_extant),
		cmocka_unit_test(test_camera_directions_are_copied_over)
	};

	return cmocka_run_group_tests(tests, setup, teardown);
}

static int setup(void** state){
	myState.lastFrameTime = 0.1f;
	myState.params = malloc(sizeof(quakeparms_t));
	quakeparms_t *parms = myState.params;
	parms->argc = 0;
	parms->argv = NULL;
	parms->basedir = QBASEDIR;
	parms->memsize = (int)Memory_GetSize();
	parms->membase = malloc((*parms).memsize);
	Sys_Init();
	Host_Init(parms);
	return 0;
}

static int teardown(void** state){
	if(!!surf){
		SDL_FreeSurface(surf);
	}
	Host_Shutdown();
	quakeparms_t *parms = myState.params;
	free(parms->membase);
	free(parms);
	return 0;
}

static void test_create_surface_if_not_extant(void** state){
	(void)state;

	CameraFacade facade = {
		.plateSize = vid.height
	};
	
	surf = rendererFacade(&facade, NULL);

	assert_non_null(surf);
	test_surface_is_mapped_to_quake_backbuffer(surf);
}

static void test_surface_is_mapped_to_quake_backbuffer(const SDL_Surface* surf_){
	SDL_Surface surface = *surf_;

	assert_int_equal(vid.width, surface.w);
	assert_int_equal(vid.height, surface.h);
	assert_ptr_equal(vid.buffer, surface.pixels);
}

static void test_camera_directions_are_copied_over(void ** state){
	(void)state;
	CameraFacade facade = {
		.plateSize = vid.height,
		.plateFov = 90,
		.eyeDirection = {
			.forward = {{1,0,0}},
			.right = {{0,1,0}},
			.upward = {{0,0,1}}
		}
	};

	surf = rendererFacade(&facade, surf);

	assert_memory_equal(r_refdef.forward, &facade.eyeDirection.forward,
			sizeof(vec3_t));
	
	assert_memory_equal(r_refdef.up, &facade.eyeDirection.upward,
			sizeof(vec3_t));
	
	assert_memory_equal(r_refdef.right, &facade.eyeDirection.right,
			sizeof(vec3_t));
}