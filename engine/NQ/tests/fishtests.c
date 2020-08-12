#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>
#include <stdio.h>
#include "qtypes.h"
#include "quakedef.h"
#include "sys.h"
#include "screen.h"
#include "zone.h"
#include "host.h"
#include "cmd.h"

#include "fisheye.h"

#define stringify__(x) #x
#define stringify(x) stringify__(x)

const char* greeting = "===fisheye test===\n";
const char* logFile = "fishtest.log";

const char* SMALLIFY = "vid_mode windowed; vid_width 640; vid_height 480; vid_restart;";
const char* BIGIFY = "vid_width 1024; vid_height 640; vid_restart;";

const static double TIME_TO_GIB = 6.25;
const static double HALF_A_SEC = 0.5;
double FAKE_DELTA_TIME = 0.1;

struct MyState {
	const char *testString;
	double lastFrameTime;
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

static void test_warmup(void **state){
	struct MyState *myState = *state;
	
	double starttime = Sys_DoubleTime();
	runConsoleSetupScript(FAKE_DELTA_TIME);
	double endtime = Sys_DoubleTime();
	
	Sys_Printf("%0.3f seconds elapsed before full init\n", starttime-endtime);
	runHostForDuration(TIME_TO_GIB);
	SCR_CenterPrint("Warmup nearly done.");
	myState->lastFrameTime = runHostForDuration(HALF_A_SEC);
	
}

static void test_globe_malloc_in_range(void **state){
	struct MyState *myState = *state;
	quakeparms_t parms = *myState->params;
	
	const void* lowerAddr = parms.membase;
	const void* upperAddr = parms.membase + parms.memsize;
	
	struct _globe globe = *F_getGlobe();
	byte* lastPixel = globe.pixels +
		(globe.platesize*globe.platesize * MAX_PLATES - 1)
		* sizeof(*globe.pixels);
	
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
	byte** lastLensPixel = lens.pixels + (area - 1)*sizeof(*lens.pixels);
	assert_in_range(lastLensPixel, lowerAddr, upperAddr);
	assert_in_range(lens.pixels, lowerAddr, upperAddr);
	
	byte* lastTintPixel = lens.pixel_tints + (area - 1)*sizeof(*lens.pixel_tints);
	assert_in_range(lastTintPixel, lowerAddr, upperAddr);
	assert_in_range(lens.pixel_tints, lowerAddr, upperAddr);
}

static void test_can_change_window_size(void **state){
	struct MyState *myState = *state;
	
	Host_Frame(myState->lastFrameTime);
	Cbuf_InsertText(SMALLIFY); Cbuf_Execute();
	runHostForDuration(HALF_A_SEC);
	Cbuf_InsertText(BIGIFY); Cbuf_Execute();
	runHostForDuration(HALF_A_SEC);
	Cbuf_InsertText(SMALLIFY); Cbuf_Execute();
	myState->lastFrameTime = runHostForDuration(HALF_A_SEC);
}

int main(void) {
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_post_init),
		cmocka_unit_test(test_warmup),
		cmocka_unit_test(test_globe_malloc_in_range),
		cmocka_unit_test(test_lens_malloc_in_range),
		cmocka_unit_test(test_can_change_window_size)
	};
	return cmocka_run_group_tests(tests, setup, teardown);
}
