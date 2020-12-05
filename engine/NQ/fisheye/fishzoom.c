
#include "qtypes.h"
#include "fishlens.h"
#include "fisheye.h"
#include "fishScript.h"
#include "fishzoom.h"
#include "console.h"

qboolean calc_zoom(struct _lens *lens, struct _zoom *zoom)
{
   // clear lens scale
   (*lens).scale = -1;
   script_refs lua_refs = *F_getScriptRef();
   if ((*zoom).type == ZOOM_FOV || (*zoom).type == ZOOM_VFOV)
   {
      // check FOV limits
      if ((*zoom).max_fov <= 0 || (*zoom).max_vfov <= 0)
      {
         Con_Printf("max_fov & max_vfov not specified, try \"f_cover\"\n");
         return false;
      }
      else if ((*zoom).type == ZOOM_FOV && (*zoom).fov > (*zoom).max_fov) {
         Con_Printf("fov must be less than %d\n", (*zoom).max_fov);
         return false;
      }
      else if ((*zoom).type == ZOOM_VFOV && (*zoom).fov > (*zoom).max_vfov) {
         Con_Printf("vfov must be less than %d\n", (*zoom).max_vfov);
         return false;
      }

      // try to scale based on FOV using the forward map
      if (lua_refs.lens_forward != -1) {
         double fovr = (*zoom).fov * M_PI / 180;
         if ((*zoom).type == ZOOM_FOV) {
            vec3_u ray_ = latlon_to_ray((vec2_u){{0, fovr*0.5}});
            vec2_u xy = scriptToC_lens_forward(ray_);
            if (FE_SUCCESS == F_getLastStatus()) {
                (*lens).scale = xy.xy.x / ((*lens).width_px * 0.5);
            }
            else {
               Con_Printf("ray_to_xy did not return a valid r value for determining FOV scale\n");
               return false;
            }
         }
         else if ((*zoom).type == ZOOM_VFOV) {
            vec3_u ray_ = latlon_to_ray((vec2_u){{fovr*0.5,0}});
            vec2_u xy = scriptToC_lens_forward(ray_);
            if (FE_SUCCESS == F_getLastStatus()) {
                (*lens).scale = xy.xy.y / ((*lens).height_px * 0.5);
            }
            else {
               Con_Printf("ray_to_xy did not return a valid r value for determining FOV scale\n");
               return false;
            }
         }
      }
      else
      {
         Con_Printf("Please specify a forward mapping function in your script for FOV scaling\n");
         return false;
      }
   }
   else if ((*zoom).type == ZOOM_CONTAIN || (*zoom).type == ZOOM_COVER) { // scale based on fitting

      double fit_width_scale = (*lens).width / (*lens).width_px;
      double fit_height_scale = (*lens).height / (*lens).height_px;

      qboolean width_provided = ((*lens).width > 0);
      qboolean height_provided = ((*lens).height > 0);

      if (!width_provided && height_provided) {
         (*lens).scale = fit_height_scale;
      }
      else if (width_provided && !height_provided) {
         (*lens).scale = fit_width_scale;
      }
      else if (!width_provided && !height_provided) {
         Con_Printf("neither lens_height nor lens_width are valid/specified.  Try f_fov instead.\n");
         return false;
      }
      else {
         double lens_aspect = (*lens).width / (*lens).height;
         double screen_aspect = (double)(*lens).width_px / (*lens).height_px;
         qboolean lens_wider = lens_aspect > screen_aspect;

         if ((*zoom).type == ZOOM_CONTAIN) {
            (*lens).scale = lens_wider ? fit_width_scale : fit_height_scale;
         }
         else if ((*zoom).type == ZOOM_COVER) {
            (*lens).scale = lens_wider ? fit_height_scale : fit_width_scale;
         }
      }
   }

   // validate scale
   if ((*lens).scale <= 0) {
      Con_Printf("init returned a scale of %f, which is  <= 0\n", (*lens).scale);
      return false;
   }

   return true;
}
