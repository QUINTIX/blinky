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
	va_list argptr;
	char msg[MAX_PRINTMSG];
	static qboolean inupdate;

	va_start(argptr, fmt);
	vsnprintf(msg, sizeof(msg), fmt, argptr);
	va_end(argptr);

	printf("%s", msg);
}

static int setup(void **state);
static void test_zoom_FOV(void **state);
static void test_zoom_VFOV(void **state);
static void test_zoom_COVER(void **state);

static void test_zoom_CONTAIN(void **state);
static void test_zoom_CONTAIN_width(void **state);
static void test_zoom_CONTAIN_height(void **state);
static void test_zoom_CONTAIN_no_wh(void **state);
static qboolean test_zoom_contain(vec2_u widthHeight);

static void test_zoom_NO_MAX_FOV(void **state);
static void test_zoom_NO_MAX_VFOV(void **state);
static void test_zoom_NO_MAX_VFOVorFOV(void **state);

static void test_zoom_x_toobig(int zoomtype);
static void test_zoom_FOV_toobig(void **state);
static void test_zoom_VFOV_toobig(void **state);

static void test_zoom_x_invalid(int zoomtype);
static void test_zoom_FOV_invalid(void **state);
static void test_zoom_VFOV_invalid(void **state);

static void test_zoom_no_forward(void **state);

static void test_zoom_scale_negative(void **state);

static struct _lens givenAGenricLensAndRefs(void);

static vec2_u mocked_Script_return= {{1,2}};

static script_refs mock_refs = {
	.lens_forward = 0,
	.globe_plate = 0
};

int main(void) {
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_zoom_FOV),
		cmocka_unit_test(test_zoom_VFOV),
		cmocka_unit_test(test_zoom_COVER),
		cmocka_unit_test(test_zoom_CONTAIN),
		cmocka_unit_test(test_zoom_CONTAIN_width),
		cmocka_unit_test(test_zoom_CONTAIN_height),
		cmocka_unit_test(test_zoom_CONTAIN_no_wh),
		cmocka_unit_test(test_zoom_NO_MAX_FOV),
		cmocka_unit_test(test_zoom_NO_MAX_VFOV),
		cmocka_unit_test(test_zoom_NO_MAX_VFOVorFOV),
		cmocka_unit_test(test_zoom_FOV_toobig),
		cmocka_unit_test(test_zoom_VFOV_toobig),
		cmocka_unit_test(test_zoom_FOV_invalid),
		cmocka_unit_test(test_zoom_VFOV_invalid),
		cmocka_unit_test(test_zoom_no_forward),
		cmocka_unit_test(test_zoom_scale_negative)
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}

static void test_zoom_FOV(void **state){
	(void)state;
	struct _lens lens = givenAGenricLensAndRefs();
	struct _zoom zoom = {
		.changed = true,
		.max_fov = 180,
		.max_vfov = 180,
		.fov = 180,
		.type = ZOOM_FOV
	};

	will_return_always(__wrap_F_getLastStatus, FE_SUCCESS);
	will_return_always(__wrap_scriptToC_lens_forward, &mocked_Script_return);

	qboolean result = calc_zoom(&lens, &zoom);
	assert_true(result);
	assert_float_equal(mocked_Script_return.xy.x/(lens.width_px * 0.5), lens.scale, FLT_EPSILON);
}

static void test_zoom_VFOV(void **state){
	(void)state;
	struct _lens lens = givenAGenricLensAndRefs();
	struct _zoom zoom = {
		.changed = true,
		.max_fov = 180,
		.max_vfov = 180,
		.fov = 180,
		.type = ZOOM_VFOV
	};

	will_return_always(__wrap_F_getLastStatus, FE_SUCCESS);
	will_return_always(__wrap_scriptToC_lens_forward, &mocked_Script_return);

	qboolean result = calc_zoom(&lens, &zoom);
	assert_true(result);
	assert_float_equal(mocked_Script_return.xy.y/(lens.width_px * 0.5), lens.scale, FLT_EPSILON);
}

static void test_zoom_CONTAIN(void **state){
	(void)state; 
	assert_true( test_zoom_contain((vec2_u){{1.0, 1.0}}) );
}
static void test_zoom_CONTAIN_width(void **state){
	(void)state; 
	assert_true( test_zoom_contain((vec2_u){{1.0, 0.0}}) );
}
static void test_zoom_CONTAIN_height(void **state){
	(void)state; 
	assert_true( test_zoom_contain((vec2_u){{0.0, 1.0}}) );
}
static void test_zoom_CONTAIN_no_wh(void **state){
	(void)state; 
	assert_false( test_zoom_contain((vec2_u){{0.0, 0.0}}) );
}
static qboolean test_zoom_contain(vec2_u widthHeight){
	struct _lens lens = {
		.width_px = 256,
		.height_px = 512,
		.width = widthHeight.xy.x,
		.height = widthHeight.xy.y
	};
	struct _zoom zoom = {
		.changed = true,
		.type = ZOOM_CONTAIN
	};

	will_return_always(__wrap_F_getScriptRef, &mock_refs);

	return  calc_zoom(&lens, &zoom);
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

	qboolean result = calc_zoom(&lens, &zoom);
	assert_true(result);
}

static void test_zoom_NO_MAX_FOV(void **state){
	(void)state;
	struct _lens lens = givenAGenricLensAndRefs();
	struct _zoom zoom = {
		.changed = true,
		.max_fov = 0,
		.max_vfov = 180,
		.fov = 180,
		.type = ZOOM_FOV
	};
	
	qboolean result = calc_zoom(&lens, &zoom);
	
	assert_false(result);
}

static void test_zoom_NO_MAX_VFOV(void **state){
	(void)state;
	struct _lens lens = givenAGenricLensAndRefs();
	
	struct _zoom zoom = {
		.changed = true,
		.max_fov = 180,
		.max_vfov = 0,
		.fov = 180,
		.type = ZOOM_VFOV
	};
	
	qboolean result = calc_zoom(&lens, &zoom);
	assert_false(result);
}

static void test_zoom_NO_MAX_VFOVorFOV(void **state){
	(void)state;
	struct _lens lens = givenAGenricLensAndRefs();
	
	struct _zoom zoom = {
		.changed = true,
		.max_fov = 0,
		.max_vfov = 0,
		.fov = 180,
		.type = ZOOM_VFOV
	};
	
	qboolean result = calc_zoom(&lens, &zoom);
	assert_false(result);
}

static void test_zoom_FOV_toobig(void **state){
	(void)state; test_zoom_x_toobig(ZOOM_FOV); }
static void test_zoom_VFOV_toobig(void **state){
	(void)state; test_zoom_x_toobig(ZOOM_VFOV); }

static void test_zoom_x_toobig(int zoomtype){
	struct _lens lens = givenAGenricLensAndRefs();

	struct _zoom zoom = {
		.changed = true,
		.max_fov = zoomtype == ZOOM_FOV ? 179 : 180,
		.max_vfov = zoomtype == ZOOM_VFOV ? 179 : 180,
		.fov = 180,
		.type = zoomtype
	};

	qboolean result = calc_zoom(&lens, &zoom);
	assert_false(result);
}

static void test_zoom_FOV_invalid(void **state){
	(void)state; test_zoom_x_invalid(ZOOM_FOV); }
static void test_zoom_VFOV_invalid(void **state){
	(void)state; test_zoom_x_invalid(ZOOM_VFOV); }

static void test_zoom_x_invalid(int zoomtype){
	struct _zoom zoom = {
		.changed = true,
		.max_fov = 180,
		.max_vfov = 180,
		.fov = 179,
	};
	zoom.type = zoomtype;

	struct _lens lens = givenAGenricLensAndRefs();
	will_return_always(__wrap_scriptToC_lens_forward, &mocked_Script_return);
	will_return_always(__wrap_F_getLastStatus, NO_VALUE_RETURNED);

	qboolean result = calc_zoom(&lens, &zoom);
	assert_false(result);
}

static void test_zoom_no_forward(void **state){
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
	static script_refs refs_noForward = {
		.lens_forward = -1,
		.globe_plate = 0
	};
	will_return_always(__wrap_F_getScriptRef, &refs_noForward);

	qboolean result = calc_zoom(&lens, &zoom);
	assert_false(result);
}

static void test_zoom_scale_negative(void **state){
	struct _lens lens = givenAGenricLensAndRefs();
	struct _zoom zoom = {
		.changed = true,
		.max_fov = 180,
		.max_vfov = 180,
		.fov = 180,
		.type = ZOOM_FOV
	};
	vec2_u willZoomNegative = {{-1,-1}};
	will_return_always(__wrap_F_getLastStatus, FE_SUCCESS);
	will_return_always(__wrap_scriptToC_lens_forward, &willZoomNegative);

	qboolean result = calc_zoom(&lens, &zoom);
	assert_false(result);
}

static struct _lens givenAGenricLensAndRefs(){
	will_return_always(__wrap_F_getScriptRef, &mock_refs);
	return (struct _lens){
		.width_px = 256,
		.height_px = 256,
		.width = 1,
		.height = 1,
	};
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
