FISHEYE.C
=========

   This is a fisheye addon based on Fisheye Quake.  It renders up to 6 camera views
   per frame, and melds them together to allow a Field of View (FoV) greater than 180 degrees:
```
             ---------
             |       |
             | UP    |                          -----------------------------
             |       |                          |\--         UP          --/|
             ---------                          |   \---             ---/   |
   --------- --------- --------- ---------      |       \-----------/       |
   |       | |       | |       | |       |      |        |         |        |
   | LEFT  | | FRONT | | RIGHT | | BACK  | ---> |  LEFT  |  FRONT  | RIGHT  |
   |       | |       | |       | |       |      |        |         |        |
   --------- --------- --------- ---------      |       /-----------\       |
          ^  ---------                          |   /---             ---\   |
          |  |       |                          |/--        DOWN         --\|
         90º | DOWN  |                          -----------------------------
          |  |       |                          <---------- +180º ---------->
          v  ---------
             <--90º-->

         (a GLOBE controls the separate             (a LENS controls how the
          camera views to render)                  views are melded together)
```

   To enable this fisheye rendering, enter the command:
   ```
   ] fisheye 1
   ```

   To resume the standard view, enter the command:
   ```
   ] fisheye 0
   ```

GLOBES
------

   The multiple camera views are controlled by a "globe" script.  It contains a
   "plates" array, with each element containing a single camera's forward vector,
   up vector, and fov. Together, the plates should form a complete globe around
   the player.
```
   Coordinate System:
      
               +Y = up
                  ^
                  |
                  |
                  |    / +Z = forward
                  |   /
                  |  /
                  | /
                  0------------------> +X = right
```

   NOTE: Plate coordinates are relative to the camera's frame.  They are NOT absolute coordinates.

   For example, this is the default globe (globes/cube.lua):

```
   plates = {
      { { 0, 0, 1 }, { 0, 1, 0 }, 90 }, -- front
      { { 1, 0, 0 }, { 0, 1, 0 }, 90 }, -- right
      { { -1, 0, 0 }, { 0, 1, 0 }, 90 }, -- left
      { { 0, 0, -1 }, { 0, 1, 0 }, 90 }, -- back
      { { 0, 1, 0 }, { 0, 0, -1 }, 90 }, -- top
      { { 0, -1, 0 }, { 0, 0, 1 }, 90 } -- bottom
   }
```

```
             ---------
             |       |
             | UP    |
             |       |
             ---------
   --------- --------- --------- ---------
   |       | |       | |       | |       |
   | LEFT  | | FRONT | | RIGHT | | BACK  |
   |       | |       | |       | |       |
   --------- --------- --------- ---------
          ^  ---------
          |  |       |
         90º | DOWN  |
          |  |       |
          v  ---------
             <--90º-->
```

   To use this globe, enter the command:

   ```
   ] f_globe cube
   ```

   There are other included globes:

   - trism: a triangular prism with 5 views
   - tetra: a tetrahedron with 4 views
   - fast:  2 overlaid views in the same direction (90 and 160 degrees)

LENSES
------

   The camera views are melded together by a "lens" script.
   This is done with either a "forward" or "inverse" mapping.

```
             ---------
             |       |
             |       |                              -----------------------------
             |       |                              |\--                     --/|
             ---------                      FORWARD |   \---             ---/   |
   --------- --------- --------- ---------  ------> |       \-----------/       |
   |       | |       | |       | |       |          |        |         |        |
   |       | |       | |       | |       |          |        |         |        |
   |       | |       | |       | |       |          |        |         |        |
   --------- --------- --------- ---------  <------ |       /-----------\       |
             ---------                      INVERSE |   /---             ---\   |
             |       |                              |/--                     --\|
             |       |                              -----------------------------
             |       |
             ---------
```

   A "FORWARD" mapping does GLOBE -> LENS.
   An "INVERSE" mapping does LENS -> GLOBE (this is faster!).

   GLOBE COORDINATES:

      (you can use any of the following coord systems to get a globe pixel):

      - direction vector

```
               +Y = up
                  ^
                  |
                  |
                  |    / +Z = forward
                  |   /
                  |  /
                  | /
                  0------------------> +X = right
```

      - latitude/longitude (spherical degrees)

```
               +latitude (degrees up)
                  ^
                  |
                  |
                  |
                  |
                  |
                  |
                  0------------------> +longitude (degrees right)
```

      - plate index & uv (e.g. plate=1, u=0.5, v=0.5 to get the center pixel of plate 1)

```
                  0----------> +u (max 1)
                  | ---------
                  | |       |
                  | |       |
                  | |       |
                  | ---------
                  V
                  +v (max 1)
```

```
   LENS COORDINATES:

                 +Y
                  ^
                  |
                  |
                  |
                  |
                  |
                  |
                  0----------------> +X
```

ZOOMING
-------

   To control how much of the resulting lens image we can see on screen,
   we scale it such that the screen aligns with certain points on the lens' axes.

   For example, suppose we have a LENS image below.
   The (X) corresponds to the point at longitude=(FOV/2)º latitude=0º.
   We flush the screen edge to this point to achieve the desired FOV zoom.

```
   -------------------------------------------------------------------------
   | LENS IMAGE                        ^                                   |
   |                                   |                                   |
   |                                   |                                   |
   |                 ------------------|-------------------                |
   |                 | SCREEN          |                  |                |
   |                 | (90º FOV)       |                  |                |
   |                 |                 |                  |                |
   |                 |                 0------------------X--------------> |
   |                 |                                    |\               |
   |                 |                                    | \              |
   |                 |                                    |  \ point at    |
   |                 --------------------------------------    lon = 45º   |
   |                                                           lat = 0º    |
   |                                                                       |
   |                                                                       |
   -------------------------------------------------------------------------
```

The process is similar when we want a vertical FOV:

```
   -------------------------------------------------------------------------
   | LENS IMAGE                        ^                                   |
   |                                   |                                   |
   |                                   |                                   |
   |                 ------------------X-------------------                |
   |                 | SCREEN          |\   point at      |                |
   |                 | (90º vertical)  | \  lon = 0º      |                |
   |                 |                 |    lat = 45º     |                |
   |                 |                 0---------------------------------> |
   |                 |                                    |                |
   |                 |                                    |                |
   |                 |                                    |                |
   |                 --------------------------------------                |
   |                                                                       |
   |                                                                       |
   |                                                                       |
   -------------------------------------------------------------------------
```

We can also zoom the lens image such that its BOUNDARIES are flush with the screen.


LUA DETAILS
-----------
   
   Variables/Functions you provide:

      Globe:
      - plates (array of [forward, up, fov] objects)
      - globe_plate (function (x,y,z) -> index)

      Lens:

         MAPPING FUNCTIONS
         - lens_forward (function (x,y,z) -> (x,y))
         - lens_inverse (function (x,y) -> (x,y,z))

         BOUNDARIES
         - lens_width (double)
         - lens_height (double)
         - max_fov (int)
         - max_vfov (int)

         (optional command to be called when lens is loaded)
         - onload (string)

   Variables/Functions provided to you:
   
      - numplates (int)
      - latlon_to_ray (function (lat,lon) -> (x,y,z))
      - ray_to_latlon (function (x,y,z) -> (lat,lon))
      - plate_to_ray (function (i,u,v) -> (x,y,z))