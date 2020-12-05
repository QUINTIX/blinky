__Update:__ we are porting to Minecraft (see [MC Render 360](http://www.minecraftforum.net/forums/mapping-and-modding/minecraft-mods/2738385-render-360-unlimited-fov) and experiments at: https://github.com/shaunlebron/flex-fov) 

# Blinky

<img src="readme-img/blinky-rocket.png" align="right" width="220px"/>

Proof of concept to __put peripheral vision into games__ (without VR goggles).
Explore this interesting space by playing the _Quake demo_ with fisheyes,
panoramas, and cartographic projections.

__Latest Release - 1.4:__

<img src="readme-img/windows.png" height="16px"> [Download for Windows](https://github.com/shaunlebron/blinky/releases/download/1.4/blinky-1.4-windows.zip)  
<img src="readme-img/apple.png"   height="16px"> [Download for Mac](https://github.com/shaunlebron/blinky/releases/download/1.4/blinky-1.4-mac.zip)  
<img src="readme-img/linux.png"   height="16px"> [Building on Linux](https://github.com/shaunlebron/blinky/blob/master/BUILDING.md#linux)

(See also: [Using with full version of Quake](https://github.com/shaunlebron/blinky/wiki/Using-with-full-version-of-Quake))

__Videos__

- __[Quick walkthrough](http://youtu.be/f9v_XN7Wxh8)__ (from 1.3)
- [Speedrun montage](http://youtu.be/jQOJ3yCK8pI) (pre 1.0)

__Why?__ The standard projection used in all games was never intended for
wide-angle views, whereas the [Panini] projection was invented to be the best
representation of your natural ~170º wide vision.

[![old-and-new](readme-img/old-and-new.jpg)](https://raw.githubusercontent.com/shaunlebron/blinky/master/readme-img/old-and-new.jpg)

__How?__ To use non-standard projections like Panini, Blinky first snaps
multiple pictures around you to form a _Globe_ of pixels.  Then it projects
all those pixels to the screen using a _Lens_.  You can enable a _Rubix_
grid if you wish to visualize the mapping.  ([Quincuncial] projection shown below)

[![map](readme-img/map.gif)](https://raw.githubusercontent.com/shaunlebron/blinky/master/readme-img/map.gif)

__Try it:__ Blinky has shortcut keys for quickly trying several Globes and
Lenses.  If you want more control, use the [console
commands](#console-commands) or even [edit/create](#lua-scripts) your own
globes and lenses with Lua scripts.

[![keys](readme-img/keys.png)](https://raw.githubusercontent.com/shaunlebron/blinky/master/readme-img/keys.png)

__360º vision:__ Blinky has many fisheye and panoramic lenses from photography,
but it also has many map projections from cartography that will give you a full
360º view.

[![worlds](readme-img/worlds.jpg)](https://raw.githubusercontent.com/shaunlebron/blinky/master/readme-img/worlds.jpg)

__Performance vs Quality:__ Blinky has to render 6 views per frame when using a
_Cube_ globe.  So we provide lower poly globes as a way to balance quality and
performance.  Fewer renders means each view has to cover more area with less
resolution.

[![globes](readme-img/globes.jpg)](https://raw.githubusercontent.com/shaunlebron/blinky/master/readme-img/globes.jpg)

## Technical Details

Blinky is a modification of the famous __[Fisheye Quake]__.

- adds a Lua scripting environment for defining:
  - _Globes_ (for capturing the environment)
  - _Lenses_ (for projecting a wide-angle image)
- uses the cross-platform [TyrQuake] engine for Windows, Mac, and Linux

### Building from source

After some [manual setup](BUILDING.md) for your OS, you can build/play with:

```
$ ./build.sh
$ ./play.sh
```

### Console Commands

Press `~` to access the command console.  Use the `Tab` key for help completing
a partial command.

```sh
fisheye <0|1>      # enable/disable fisheye mode
f_help             # show quick start options
f_globe <name>     # choose a globe (affects picture quality and render speed)
f_lens <name>      # choose a lens (affects the shape of your view)

f_fov <degrees>    # zoom to a horizontal FOV
f_vfov <degrees>   # zoom to a vertical FOV
f_cover            # zoom in until screen is covered (some parts may be hidden)
f_contain          # zoom out until screen contains the entire image (if possible)

f_rubix            # display colored grid for each rendered view in the globe
f_saveglobe        # take screenshots of each globe face (environment map) as .png files
f_saverubix <file> # save image of colored grid to <file>.png
f_savelens <file>  # save visualization of lens->globe map to <file>.png
```

### Lua Scripts

To create/edit globes and lenses, check out the following guides:

- [Create a Globe](game/lua-scripts/globes)
- [Create a Lens](game/lua-scripts/lenses)

### Engine Code

- [engine/NQ/fisheye.c](engine/NQ/fisheye.c) - new engine code
- [engine patch](engine/fisheye.patch) - engine modifications

## Theory of Projections

[>> Interactive visualization for how projections work](http://shaunlebron.github.io/visualizing-projections)

[![theory](readme-img/theory.png)](http://shaunlebron.github.io/visualizing-projections)

## Future

I hope to apply this to modern graphics using frame buffers for
environment-capturing and pixel shaders for projection.  It would be
interesting to see its impact on performance.

If this modern method is performant enough, I think Panini/Stereographic could
easily become a standard for gamers demanding wide-angle video.  But if it is
not performant enough for live applications, I think it could still prove
useful in post-processed videos using something like [WolfCam].  For example,
spectators could benefit from wide-angle viewings of previously recorded
competitive matches or even [artistic montages][f33l].

## Thanks

This project would not exist without these people!

- __Wouter van Oortmerssen__ for creating & open-sourcing [Fisheye Quake]
- __Peter Weiden__ for creating [fisheye diagrams] on Wikipedia
- __Kevin Shanahan__ for creating/maintaining a cross-platform Quake engine, [TyrQuake]

## Contact

- email: <shaun.e.williams@gmail.com>
- twitter: [@shaunlebron](http://twitter.com/shaunlebron)

## License

- The [Lua scripts](game/lua-scripts) and [fisheye.c](engine/NQ/fisheye.c) are released under the MIT license.
- TyrQuake is released under GPL-2.


[Fisheye Quake]:http://strlen.com/gfxengine/fisheyequake/
[TyrQuake]:http://disenchant.net/tyrquake/
[Panini]: http://tksharpless.net/vedutismo/Pannini/
[Quincuncial]:http://en.wikipedia.org/wiki/Peirce_quincuncial_projection
[f33l]:https://www.dailymotion.com/video/x5483f2
[WolfCam]:https://github.com/brugal/wolfcamql
[fisheye diagrams]:http://en.wikipedia.org/wiki/Fisheye_lens#Mapping_function
