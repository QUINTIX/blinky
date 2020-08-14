#include "qtypes.h"
#include <math.h>
#include <time.h>
#include "fishlens.h"

struct _lens_builder * start_lens_builder_clock_(
   struct _lens_builder *lens_builder)
{
   lens_builder->start_time = clock();
   return lens_builder;
}

qboolean is_lens_builder_time_up_(struct _lens_builder *lens_builder)
{
   clock_t time = clock() - lens_builder->start_time;
   float s = ((float)time) / CLOCKS_PER_SEC;
   return (s >= lens_builder->seconds_per_frame);
};


vec3_u latlon_to_ray_(const vec2_u latlon)
{
   vec_t clat = cos(latlon.latlon.lat);

   vec3_u ray = { .xyz = {
          .x = sin(latlon.latlon.lon) * clat,
          .y = sin(latlon.latlon.lat),
          .z = cos(latlon.latlon.lon) * clat
   }};
   return ray;
}

vec2_u ray_to_latlon_(const vec3_u ray)
{
   vec_t lengthY = sqrt(
      ray.xyz.x*ray.xyz.x + ray.xyz.z*ray.xyz.z
   );

   vec2_u latlon = { .latlon = {
      .lon = atan2(ray.xyz.x, ray.xyz.z),
      .lat = atan2(ray.xyz.y, lengthY)
   }};
   return latlon;
}

vec3_u plate_uv_to_ray_(const struct _globe *globe,
	int plate_index, const vec2_u uv)
{	
   // transform to image coordinates
   vec2_u uv_image = {.k =
      { uv.uv.u - 0.5 , - (uv.uv.v - 0.5) }
   };

   // put clear ray on the stack
   vec3_u outRay = {.vec = {0,0,0}};

   // get euclidean coordinate from texture uv
   VectorMA(outRay.vec,
      globe->plates[plate_index].dist,
      globe->plates[plate_index].forward,
      outRay.vec);

   VectorMA(outRay.vec, uv_image.uv.u,
      globe->plates[plate_index].right,
      outRay.vec);

   VectorMA(outRay.vec, uv_image.uv.v,
      globe->plates[plate_index].up, outRay.vec);

   VectorNormalize(outRay.vec);
   return outRay;
}

// convenience functions for forward map calculation:
// map lens to screen uv for a given globe plate
vec2_u lens_to_screen_uv_(lens_forward_t forward,
      const struct _globe *globe, int plate_index, const vec2_u uv)
{
   vec3_u ray = plate_uv_to_ray_(globe, plate_index, uv);
   return forward(ray);
}

// maps uv coordinate on a texture to a screen coordinate
point2d uv_to_screen_(const struct state_paq state,
      int plate_index, const vec2_u uv)
{
   struct _lens lens = *state.lens;
   vec2_u out_uv = lens_to_screen_uv_(state.forward, state.globe, plate_index, uv);
   point2d pt = {.k = {
          (int)(out_uv.uv.u/lens.scale + lens.width_px/2),
          (int)(-out_uv.uv.v/lens.scale + lens.height_px/2)
   }};

   return pt;
}


