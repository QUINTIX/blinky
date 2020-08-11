#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>
#include <stdio.h>
#include "qtypes.h"
#include "quakedef.h"
#include "sys.h"
#include "zone.h"
#include "host.h"

#include "fisheye.h"

#define stringify__(x) #x
#define stringify(x) stringify__(x)

const char* greeting = "===fisheye test===\n";
const char* logFile = "fishtest.log";

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
	struct MyState *myState = *state;
	quakeparms_t parms = *myState->params;
	free(parms.membase);
	free(myState->params);
	free(*state);
	return 0;
}

static void test_hello_world(void **state){
	struct MyState *myState = *state;
	const char* text = myState->testString;
	quakeparms_t parms = *myState->params;
	
	Sys_DebugLog(logFile, "%s- allocated 0x%X bytes at 0x%016llX\n", text,
		     parms.memsize, (long long)parms.membase);
	Sys_DebugLog(logFile, "- QBASEDIR is \"%s\"\n", parms.basedir);
}

int main(void) {
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_hello_world)
	};
	return cmocka_run_group_tests(tests, setup, teardown);
}
