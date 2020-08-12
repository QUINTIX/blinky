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

const static double TIME_TO_GIB = 6.25;
const static double HALF_A_SEC = 0.5;

struct MyState {
	const char *testString;
	quakeparms_t *params;
};

static int setup(void **state){
	struct MyState *myState = malloc(sizeof(struct MyState));
	
	myState -> testString = greeting;
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

static void test_post_init(void **state){
	struct MyState *myState = *state;
	const char* text = myState->testString;
	quakeparms_t parms = *myState->params;
	
	Sys_DebugLog(logFile, "%s- allocated 0x%X bytes at 0x%016llX\n", text,
		     parms.memsize, (long long)parms.membase);
	Sys_DebugLog(logFile, "- QBASEDIR is \"%s\"\n", parms.basedir);
}

static void runConsoleScript(double fakeDeltaTime){
	Host_Frame(fakeDeltaTime);
	Cbuf_InsertText("f_globe tetra; f_lense stereographic; f_fov 210; show_fps 1");
	Cbuf_Execute();
	SCR_CenterPrint("Warming up for tests. Do not exit.");
	Host_Frame(fakeDeltaTime);
}

static void test_warmup(void **state){
	
	double oldtime = Sys_DoubleTime() - 0.1;
	double deltatime = 0.1, newtime = 0.;
	
	runConsoleScript(deltatime);
	
	double starttime = Sys_DoubleTime();
	double endtime = starttime + TIME_TO_GIB;
	
	Sys_Printf("%0.3f seconds elapsed before full init\n", starttime-oldtime);
	while (oldtime < endtime){
		newtime = Sys_DoubleTime();
		deltatime = newtime - oldtime;
		
		if (deltatime > sys_ticrate.value * 2){
			oldtime = newtime; }
		else{ oldtime += deltatime; }
		
		Host_Frame(deltatime);
		if(endtime - oldtime < HALF_A_SEC){
			SCR_CenterPrint("Warmup nearly done.");
		}
	}
	
	Host_Frame(deltatime);
}

int main(void) {
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_post_init),
		cmocka_unit_test(test_warmup)
	};
	return cmocka_run_group_tests(tests, setup, teardown);
}
