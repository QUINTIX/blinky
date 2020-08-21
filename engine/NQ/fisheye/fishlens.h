#include <stdio.h>
#include <time.h>
#include "qtypes.h"
#include "mathlib.h"
#include "fisheye.h"

#ifndef FISHLENS_H_
#define FISHLENS_H_

// Lens computation is slow, so we don't want to block the game while its busy.
// Instead of dealing with threads, we are just limiting the time that the
// lens builder can work each frame.  It keeps track of its work between frames
// so it can resume without problems.  This allows the user to watch the lens
// pixels become visible as they are calculated.
struct _lens_builder
{
   qboolean working;
   clock_t start_time;
   float seconds_per_frame;
   struct _inverse_state
   {
      int ly;
   } inverse_state;
   struct _forward_state
   {
      int *top;
      int *bot;
      int plate_index;
      int py;
   } forward_state;
};

struct _lens_builder* F_getStatus(void);

typedef union _vec3_u{
  vec3_t vec;
  struct _vec3_s {
    vec_t x, y, z;
  } xyz;
} vec3_u;

typedef union _vec2_u{
  vec_t k[2];
  struct _vec3_uv {
     vec_t u, v;
  } uv;
  struct _vec3_latlon {
     vec_t lat, lon;
  } latlon;
  struct __xy{
     vec_t x, y;
  } xy;
} vec2_u;

typedef union _point{
   int k[2];
   struct _xy { int x, y; } xy;
} point2d;

typedef vec2_u (*lens_forward_t)(vec3_u ray);
typedef vec3_u (*lens_reverse_t)(vec2_u uv);

struct state_paq {
   lens_forward_t forward;
   lens_reverse_t inverse;
   const struct _globe *globe;
   const struct _lens *lens;
};

struct _lens_builder * start_lens_builder_clock_(
      struct _lens_builder *lens_builder);

qboolean is_lens_builder_time_up_(struct _lens_builder *lens_builder);

vec3_u latlon_to_ray_(const vec2_u latlon);

vec2_u ray_to_latlon_(const vec3_u ray);

vec3_u plate_uv_to_ray_(const struct _globe *globe,
      int plate_index, const vec2_u uv);

point2d uv_to_screen_(const struct state_paq state,
      int plate_index, const vec2_u uv);

#endif
