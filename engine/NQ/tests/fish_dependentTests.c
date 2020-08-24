#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdlib.h>
#include <cmocka.h>
#include <math.h>
#include <stdarg.h>
#include <float.h>

#include "fishzoom.h"
#include "fishScript.h" //mocked via build settings
#include "console.h"

#define MAX_PRINTMSG 4096

//temporary until I figure out how to properly configure wrapping
#define __wrap_F_getScriptRef F_getScriptRef
#define __wrap_F_getLastStatus F_getLastStatus
#define __wrap_scriptToC_lens_forward scriptToC_lens_forward
#define __wrap_Con_Printf Con_Printf

script_refs* __wrap_F_getScriptRef(){
	return (script_refs*)mock();
}


#undef FISHEYE_H_
fisheye_status __wrap_F_getLastStatus(){
	return (fisheye_status)mock();
}
#define FISHEYE_H_

vec2_u __wrap_scriptToC_lens_forward(vec3_u ray){
	return *(vec2_u*)mock();
}

void __wrap_Con_Printf(const char* fmt, ...){
	va_list argpointer;
	va_start(argpointer, fmt);
	printf(fmt, argpointer);
	va_end(argpointer);
}

static void test_zoom_FOV(void **state);
static void test_zoom_VFOV(void **state);
static void test_zoom_CONTAIN(void **state);
static void test_zoom_COVER(void **state);

vec2_u mocked_Script_return  = {{1,2}};

int main(void) {
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_zoom_FOV),
		cmocka_unit_test(test_zoom_VFOV),
		cmocka_unit_test(test_zoom_CONTAIN),
		cmocka_unit_test(test_zoom_COVER)
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}

static void test_zoom_FOV(void **state){
   (void)state;
	script_refs mock_refs = {
		.lens_forward = 0,
		.globe_plate = 0
	};
	struct _lens lens = {
		.width_px = 256
	};
	struct _zoom zoom = {
		.changed = true,
		.max_fov = 180,
      .max_vfov = 180,
		.fov = 180,
		.type = ZOOM_FOV
	};

	will_return_always(__wrap_F_getScriptRef, &mock_refs);
	will_return_always(__wrap_F_getLastStatus, true);
	will_return_always(__wrap_scriptToC_lens_forward, &mocked_Script_return);

	qboolean result = calc_zoom_(&lens, &zoom);
	assert_true(result);
   assert_float_equal(mocked_Script_return.xy.x/(lens.width_px * 0.5), lens.scale, FLT_EPSILON);
}

static void test_zoom_VFOV(void **state){
   (void)state;
   script_refs mock_refs = {
      .lens_forward = 0,
      .globe_plate = 0
   };
   struct _lens lens = {
      .width_px = 256
   };
   struct _zoom zoom = {
      .changed = true,
      .max_fov = 180,
      .max_vfov = 180,
      .fov = 180,
      .type = ZOOM_VFOV
   };

   will_return_always(__wrap_F_getScriptRef, &mock_refs);
   will_return_always(__wrap_F_getLastStatus, true);
   will_return_always(__wrap_scriptToC_lens_forward, &mocked_Script_return);

   qboolean result = calc_zoom_(&lens, &zoom);
   assert_true(result);
   assert_float_equal(mocked_Script_return.xy.y/(lens.width_px * 0.5), lens.scale, FLT_EPSILON);
}

static void test_zoom_CONTAIN(void **state){
   (void)state;
   script_refs mock_refs = {
      .lens_forward = 0,
      .globe_plate = 0
   };
   struct _lens lens = {
      .width_px = 256,
      .height_px = 512,
      .width = 1.0,
      .height = 2.0
   };
   struct _zoom zoom = {
      .changed = true,
      .type = ZOOM_CONTAIN
   };

   will_return_always(__wrap_F_getScriptRef, &mock_refs);

   qboolean result = calc_zoom_(&lens, &zoom);
   assert_true(result);
}

static void test_zoom_COVER(void **state){
   (void)state;
   script_refs mock_refs = {
      .lens_forward = 0,
      .globe_plate = 0
   };
   struct _lens lens = {
      .width_px = 256,
      .height_px = 512,
      .width = 1.0,
      .height = 2.0
   };
   struct _zoom zoom = {
      .changed = true,
      .type = ZOOM_COVER
   };

   will_return_always(__wrap_F_getScriptRef, &mock_refs);

   qboolean result = calc_zoom_(&lens, &zoom);
   assert_true(result);
}

void Sys_Error(const char *error, ...)
{
    va_list argptr;
    char string[MAX_PRINTMSG];
    va_start(argptr, error);
    vsnprintf(string, sizeof(string), error, argptr);
    va_end(argptr);
    fprintf(stderr, "Error: %s\n", string);

    exit(1);
}
