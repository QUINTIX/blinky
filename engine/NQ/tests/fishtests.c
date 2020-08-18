#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>
#include <stdio.h>
#include <math.h>
#include "qtypes.h"
#include "quakedef.h"
#include "sys.h"
#include "screen.h"
#include "host.h"
#include "cmd.h"
#include "zone.h"
#include <SDL_surface.h>
#include <SDL_image.h>

#include "fisheye.h"
#include "fishlens.h"

#define stringify__(x) #x
#define stringify(x) stringify__(x)

const char* greeting = "===fisheye test===\n";
const char* logFile = "fishtest.log";

const char* SMALLIFY = "vid_mode windowed; vid_width 640; vid_height 480; vid_restart;";
const char* BIGIFY = "vid_width 1024; vid_height 640; vid_restart;";

const static double TIME_TO_GIB = 6.25;
const static double HALF_A_SEC = 0.5;
double FAKE_DELTA_TIME = 0.1;
const static vec_t EPSILON = 1./(0x40000);

struct MyState {
	const char *testString;
	double lastFrameTime;
	int luaState;
	quakeparms_t *params;
};

static int setup(void **state){
	struct MyState *myState = malloc(sizeof(struct MyState));
	
	myState -> testString = greeting;
	myState -> lastFrameTime = FAKE_DELTA_TIME;
	myState -> params = malloc(sizeof(quakeparms_t));
	quakeparms_t *parms = myState->params;
	parms->argc = 0;
	parms->argv = NULL;
	parms->basedir = stringify(QBASEDIR);
	parms->memsize = (int)Memory_GetSize();
	parms->membase = malloc((*parms).memsize);
	
	*state = (void*)myState;
	
	Sys_Init();
	Host_Init(parms);
	
	return 0;
}

static int teardown(void **state){
	Host_Shutdown();
	struct MyState *myState = *state;
	quakeparms_t parms = *myState->params;
	free(parms.membase);
	free(myState->params);
	free(*state);
	return 0;
}

static double runHostForDuration(double duration){
	assert_true(duration > 0.);
	double oldtime = Sys_DoubleTime() - 0.1;
	double deltatime = 0.1, newtime = 0.;
	
	double starttime = Sys_DoubleTime();
	double endtime = starttime + duration;
	
	while (oldtime < endtime){
		newtime = Sys_DoubleTime();
		deltatime = newtime - oldtime;
		
		if (deltatime > sys_ticrate.value * 2){
			oldtime = newtime; }
		else{ oldtime += deltatime; }
		
		Host_Frame(deltatime);
	}
	return deltatime;
}

static double runHostUntilLensIsDone(double lastDuration){
      assert_true(lastDuration > 0.);
   double newtime = Sys_DoubleTime();
   double oldtime = newtime - lastDuration;
   double deltatime =lastDuration;
   struct _lens_builder* lensStatus = F_getStatus();
   
   while(lensStatus->working){
      newtime = Sys_DoubleTime();
      deltatime = newtime - oldtime;
      if (deltatime > sys_ticrate.value * 2){
              oldtime = newtime; }
      else{ oldtime += deltatime; }
      Host_Frame(deltatime);
   }
   return deltatime;
}

static void test_post_init(void **state){
	struct MyState *myState = *state;
	const char* text = myState->testString;
	quakeparms_t parms = *myState->params;
	
	Sys_DebugLog(logFile, "%s- allocated 0x%X bytes at 0x%016llX\n", text,
		     parms.memsize, (long long)parms.membase);
	Sys_DebugLog(logFile, "- QBASEDIR is \"%s\"\n", parms.basedir);
}

static void runConsoleSetupScript(double fakeDeltaTime){
	Host_Frame(fakeDeltaTime);
	Cbuf_InsertText("f_globe tetra; f_lense stereographic; f_fov 210; show_fps 1");
	Cbuf_Execute();
	SCR_CenterPrint("Warming up for tests. Do not exit.");
	Host_Frame(fakeDeltaTime);
}

static void dumpTints(struct _lens* lens){
   SDL_Surface *surf = SDL_CreateRGBSurfaceWithFormatFrom(
      lens->pixel_tints, lens->width_px, lens->height_px, 8, lens->width_px,
         SDL_PIXELFORMAT_INDEX8);
  
    SDL_Palette* pal = SDL_AllocPalette(256);
   
   pal->colors[0] = (SDL_Color){.r=255,.g=0,.b=0};
   pal->colors[1] = (SDL_Color){.r=255,.g=255,.b=0};
   pal->colors[2] = (SDL_Color){.r=0,.g=255,.b=0};
   pal->colors[3] = (SDL_Color){.r=0,.g=255,.b=255};
   pal->colors[4] = (SDL_Color){.r=0,.g=0,.b=255};
   pal->colors[5] = (SDL_Color){.r=255,.g=0,.b=255};
   pal->colors[255] = (SDL_Color){.r=0,.g=0,.b=0};
   
   SDL_SetSurfacePalette(surf, pal);
   
   IMG_SavePNG(surf, "tints.png");

   SDL_FreePalette(pal);
   
   SDL_FreeSurface(surf);
}

static void dumpIndicies(struct _lens* lens){
   SDL_Surface *surf = SDL_CreateRGBSurfaceWithFormatFrom(lens->pixels,
         lens->width_px, lens->height_px, 32, lens->width_px,
         SDL_PIXELFORMAT_BGRX8888);
   
   IMG_SavePNG(surf, "lens.png");
   
   SDL_FreeSurface(surf);
}

static void test_warmup(void **state){
	struct MyState *myState = *state;
	
	double starttime = Sys_DoubleTime();
	runConsoleSetupScript(FAKE_DELTA_TIME);
	double endtime = Sys_DoubleTime();
	
	Sys_Printf("%0.3f seconds elapsed before full init\n", endtime - starttime);
	runHostForDuration(TIME_TO_GIB);
	SCR_CenterPrint("Warmup nearly done.");
   Hunk_Check();
   Cbuf_AddText("hunk print\n");
	myState->lastFrameTime = runHostForDuration(HALF_A_SEC);
}

static void test_globe_malloc_in_range(void **state){
	struct MyState *myState = *state;
	quakeparms_t parms = *myState->params;
	
	const void* lowerAddr = parms.membase;
	const void* upperAddr = parms.membase + parms.memsize;
	
	struct _globe globe = *F_getGlobe();
	byte* lastPixel = globe.pixels +
	    (globe.platesize*globe.platesize * MAX_PLATES - 1);
	
	assert_in_range(lastPixel, lowerAddr, upperAddr);
	assert_in_range(globe.pixels, lowerAddr, upperAddr);
}

static void test_lens_malloc_in_range(void **state){
	struct MyState *myState = *state;
	quakeparms_t parms = *myState->params;
	
	const void* lowerAddr = parms.membase;
	const void* upperAddr = parms.membase + parms.memsize;
	
	struct _lens lens = *F_getLens();
	
	int area = lens.width_px * lens.height_px;

	byte* lastTintPixel = lens.pixel_tints + (area - 1);
	assert_in_range(lastTintPixel, lowerAddr, upperAddr);
	assert_in_range(lens.pixel_tints, lowerAddr, upperAddr);

	uint32_t* lastLensPixel = lens.pixels + (area - 1);
	assert_in_range(lens.pixels, lowerAddr, upperAddr);
	assert_in_range(lastLensPixel, lowerAddr, upperAddr);
	
}

static void test_can_change_window_size(void **state){
	struct MyState *myState = *state;
	
	Host_Frame(myState->lastFrameTime);
	
	Cbuf_InsertText(SMALLIFY); Cbuf_Execute();
	runHostForDuration(HALF_A_SEC);
	Cbuf_InsertText(BIGIFY); Cbuf_Execute();
	runHostForDuration(HALF_A_SEC);
	
	Cbuf_InsertText(SMALLIFY); Cbuf_Execute();
	runHostForDuration(HALF_A_SEC);
	Cbuf_InsertText(BIGIFY); Cbuf_Execute();
	runHostForDuration(HALF_A_SEC);

	myState->lastFrameTime = runHostForDuration(HALF_A_SEC);
}

static void test_restart_video_reallocs_fisheye_buffer(void **state){
	(void)state;
	
	Host_Frame(FAKE_DELTA_TIME);
	int expectedHighMarkAfterRestart = Hunk_HighMark();
	Cbuf_AddText("vid_restart\n");
	Host_Frame(FAKE_DELTA_TIME);
	int actualHighMarkAfterRestart = Hunk_HighMark();
	
	assert_int_equal(expectedHighMarkAfterRestart, actualHighMarkAfterRestart);
   Hunk_Check();
   Cbuf_AddText("hunk print\n");
   Host_Frame(FAKE_DELTA_TIME);
}

static void test_latlon_to_ray(void **state){
	(void)state;
	
	const vec2_u onEquator = { .latlon = {
		.lat = 0.,
		.lon = M_PI/4.
	}};
	
	vec3_u actualRay = latlon_to_ray_(onEquator);
	
	float halfRoot2 = sqrtf(2)/2;
	
	assert_float_equal(halfRoot2, actualRay.xyz.x, EPSILON);
	assert_float_equal(0, actualRay.xyz.y, EPSILON);
	assert_float_equal(halfRoot2, actualRay.xyz.z, EPSILON);
}

static void test_ray_to_latlon(void **state){
	(void)state;
	const vec3_u pole = {.vec = {0., -1., 0.} };
	
	vec2_u actualLatLon = ray_to_latlon_(pole);
	
	assert_float_equal(actualLatLon.latlon.lat, -M_PI/2, EPSILON);
	assert_float_equal(actualLatLon.latlon.lon, 0., EPSILON);
}

static void test_plate_uv_to_ray(void **state){
	(void)state;
	
	const vec2_u uv = {.uv = {.u=0.0, .v=0.5}};
	struct _globe globe;
   globe.plates[0].dist = 1.0;
	
	const vec3_u forward =	{.vec = {0., 0., 1.}};
	const vec3_u right =	{.vec = {1., 0., 0.}};
	const vec3_u up =	{.vec = {0., 1., 0.}};
	VectorCopy(forward.vec, globe.plates[0].forward);
	VectorCopy(right.vec, globe.plates[0].right);
	VectorCopy(up.vec, globe.plates[0].up);
	
	vec3_u actualRay = plate_uv_to_ray_(&globe, 0, uv);
	
	vec_t lengthSq = DotProduct(actualRay.vec, actualRay.vec);
	assert_float_equal(1.0, lengthSq, EPSILON);
}

static int mockScriptState = 0;

static vec2_u mockForward(vec3_u input){
	mockScriptState = 1;
	vec2_u retval = {.k={input.xyz.x, input.xyz.y}};
	return retval;
}

static void test_uv_to_screen(void **state){
	struct _globe globe;
	struct _lens lens;
	lens.width_px = 512;
	lens.height_px = 256;
	lens.scale = (float)lens.height_px;
	
	const vec3_u forward =	{.vec = {0., 0., 1.}};
	const vec3_u right =	{.vec = {1., 0., 0.}};
	const vec3_u up =	{.vec = {0., 1., 0.}};
	VectorCopy(forward.vec, globe.plates[0].forward);
	VectorCopy(right.vec, globe.plates[0].right);
	VectorCopy(up.vec, globe.plates[0].up);
	
	const struct state_paq paq = {
		.globe = &globe,
		.lens = &lens,
		.forward = mockForward
	};
	
	const vec2_u uv = {.uv = {.u=1.0, .v=1.0}};
	
	point2d screenPoint = uv_to_screen_(paq, 0, uv);
	assert_in_range(screenPoint.xy.x, 0, lens.width_px);
	assert_int_equal(1, mockScriptState);
	mockScriptState = 0;
}

static void test_save_rubix(void **state){
   struct MyState *myState = *state;
   Cbuf_AddText("f_globe tetra; f_lense panini; f_fov 270;\n");
   myState->lastFrameTime = runHostUntilLensIsDone(myState->lastFrameTime);
   
   struct _lens lens = *F_getLens();
   
   int colorCount[5] = {0,0,0,0,0};
   int tintPixelCount = lens.width_px * lens.height_px;
   
   for(int i=0; i < tintPixelCount; i++){
      byte tint = lens.pixel_tints[i];
      if(255 == tint){
         tint = 4;
      }
      assert_in_range(tint, 0, 4);
      colorCount[tint]++;
   }
   
   int maxPixelsPerFace = tintPixelCount/4;
   int minPixelsPerFace = 0x100;
   
   assert_in_range(colorCount[0], minPixelsPerFace, maxPixelsPerFace);
   assert_in_range(colorCount[1], minPixelsPerFace, maxPixelsPerFace);
   assert_in_range(colorCount[2], minPixelsPerFace, maxPixelsPerFace);
   
   dumpTints(&lens);
   dumpIndicies(&lens);
}

//will only pass in 64bit compilation. Fortunately macs only support 64bit
static void test_lens_pixel_index_is_not_absolute_pointer(void **state){
(void)state;
   struct _lens *lens = F_getLens();
   
   assert_true(sizeof(*lens->pixels) < sizeof(void*));
}


int main(void) {
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_post_init),
		
		cmocka_unit_test(test_latlon_to_ray),
		cmocka_unit_test(test_ray_to_latlon),
		cmocka_unit_test(test_plate_uv_to_ray),
		cmocka_unit_test(test_uv_to_screen),
		
		cmocka_unit_test(test_warmup),
		cmocka_unit_test(test_globe_malloc_in_range),
		cmocka_unit_test(test_lens_malloc_in_range),
		cmocka_unit_test(test_lens_pixel_index_is_not_absolute_pointer),

		cmocka_unit_test(test_restart_video_reallocs_fisheye_buffer),
		cmocka_unit_test(test_can_change_window_size),
         cmocka_unit_test(test_save_rubix)
	};
	return cmocka_run_group_tests(tests, setup, teardown);
}
