#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>
#include <math.h>

#include "fisheye.h"
#include "fishlens.h"

static vec2_u mockForward(vec3_u input);

static int mockScriptState = 0;

const static vec_t EPSILON = 1./(0x40000);

void test_plate_uv_to_ray(void **state){
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
	
	vec3_u actualRay = plate_uv_to_ray(&globe, 0, uv);
	
	vec_t lengthSq = DotProduct(actualRay.vec, actualRay.vec);
	assert_float_equal(1.0, lengthSq, EPSILON);
}

void test_latlon_to_ray(void **state){
	(void)state;
	
	const vec2_u onEquator = { .latlon = {
		.lat = 0.,
		.lon = M_PI/4.
	}};
	
	vec3_u actualRay = latlon_to_ray(onEquator);
	
	float halfRoot2 = sqrtf(2)/2;
	
	assert_float_equal(halfRoot2, actualRay.xyz.x, EPSILON);
	assert_float_equal(0, actualRay.xyz.y, EPSILON);
	assert_float_equal(halfRoot2, actualRay.xyz.z, EPSILON);
}


void test_ray_to_latlon(void **state){
	(void)state;
	const vec3_u pole = {.vec = {0., -1., 0.} };
	
	vec2_u actualLatLon = ray_to_latlon(pole);
	
	assert_float_equal(actualLatLon.latlon.lat, -M_PI/2, EPSILON);
	assert_float_equal(actualLatLon.latlon.lon, 0., EPSILON);
}

void test_uv_to_screen(void **state){
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
	
	point2d screenPoint = uv_to_screen(paq, 0, uv);
	assert_in_range(screenPoint.xy.x, 0, lens.width_px);
	assert_int_equal(1, mockScriptState);
	mockScriptState = 0;
}

static vec2_u mockForward(vec3_u input){
	mockScriptState = 1;
	vec2_u retval = {.k={input.xyz.x, input.xyz.y}};
	return retval;
}
