project('blinky', 'c',
  version : '0.67',
  default_options : ['warning_level=3'])

TYR_GIT = run_command(
    'git', 'describe', '--dirty'
).stdout().strip()
message('git reported version is @0@'.format(TYR_GIT))

TYR_RELEASE = 'v@0@'.format(meson.project_version())
TYR_VERSION_NUM = meson.project_version()
TYR_VERSION_TIME = run_command(
    'git', 'show', '-s', '--pretty=\'%at\''
).stdout().strip().strip('\'').to_int()

iconSrcDir = './icons/tyrquake-1024x1024.png'
iconDestDir = './include/tyrquake_icon_128.h'

includes = include_directories(
        './include',
        './NQ',
        './NQ/fisheye/'
)

# currently depends on MSYS2
win_src = files(
        'NQ/conproc.c',
        'NQ/net_win.c',
        'NQ/net_wins.c',
        'common/cd_win.c',
        'common/sys_win.c',
)

linux_src = files(
        'common/cd_linux.c',
        'common/sys_unix.c',
)

bsd_src = files(
        'common/cd_bsd.c',
        'common/sys_unix.c',
        'NQ/net_bsd.c'
)

mac64_src = files(
        'common/cd_null.c',
        'common/sys_unix.c',
        'NQ/net_bsd.c'
)

dummythicc_src = files(
        'NQ/net_none.c',
        'common/cd_null.c',
        'common/sys_null.c'
)

# currently broken
gl_src = files(
        'common/vid_glx.c',
        'common/drawhulls.c',
        'common/gl_draw.c',
        'common/gl_extensions.c',
        'common/gl_mesh.c',
        'common/gl_rmain.c',
        'common/gl_rmisc.c',
        'common/gl_rsurf.c',
        'common/gl_textures.c',
        'common/gl_warp.c',
        'common/qpic.c'
)

# software renderer w/o assembly
sw_src = files(
        'common/d_edge.c',
        'common/d_fill.c',
        'common/d_init.c',
        'common/d_modech.c',
        'common/d_part.c',
        'common/d_polyse.c',
        'common/d_scan.c',
        'common/d_sky.c',
        'common/d_sprite.c',
        'common/d_surf.c',
        'common/d_vars.c',
        'common/draw.c',
        'common/r_aclip.c',
        'common/r_alias.c',
        'common/r_bsp.c',
        'common/r_draw.c',
        'common/r_edge.c',
        'common/r_lerp.c',
        'common/r_light.c',
        'common/r_main.c',
        'common/r_misc.c',
        'common/r_model.c',
        'common/r_sky.c',
        'common/r_sprite.c',
        'common/r_surf.c',
        'common/r_vars.c',
        'common/nonintel.c'
)

sw_ASM_src = files(
        'common/d_draw.S',
        'common/d_parta.S',
        'common/d_scana.S',
        'common/d_varsa.S',
        'common/modela.S',
        'common/r_aliasa.S',
        'common/r_edgea.S',
        'common/surf8.S',
        'common/d_draw16.S',
        'common/d_polysa.S',
        'common/d_spr8.S',
        'common/r_aclipa.S',
        'common/r_drawa.S',
        'common/r_varsa.S',
        'common/surf16.S'
)

# unused, as "experimental" as SDL support may be, fisheye depends on SDL now
nonSDL_win = files(
        'common/in_win.c',
        'common/snd_win.c',
        'common/vid_win.c'
)
nonSDL_linux = files(
        'common/vid_x.c',
        'common/x11_core.c',
        'common/in_x11.c',
        'common/snd_oss.c',
        'common/snd_sndio.c'
)
nonSDL_unknown = files(
        'common/vid_sgl.c',
        'common/vid_wgl.c'
)
nonSDL_dummy = files(
        'common/snd_null.c',
        'common/vid_null.c',
        'common/in_null.c'
)
# end unused

SDL_src = files(
        'common/in_sdl.c',
        'common/snd_sdl.c',
        'common/vid_sdl.c'
)

NQ_src = files(
        'NQ/chase.c',
        'NQ/cl_demo.c',
        'NQ/cl_input.c',
        'NQ/cl_main.c',
        'NQ/cl_parse.c',
        'NQ/cl_tent.c',
        'NQ/host.c',
        'NQ/host_cmd.c',
        'NQ/net_common.c',
        'NQ/net_dgrm.c',
        'NQ/net_loop.c',
        'NQ/net_main.c',
        'NQ/net_udp.c',
        'NQ/sbar.c',
        'NQ/sv_main.c',
        'NQ/sv_user.c',
        'NQ/view.c'
)

common_nonasm_src = files(
        'common/mathlib.c',
        'common/snd_mix.c'
)

common_ASM_src = files(
        'common/math.S',
        'common/snd_mixa.S'
)

common_src = files(
        'common/alias_model.c',
        'common/buildinfo.c',
        'common/cd_common.c',
        'common/cmd.c',
        'common/common.c',
        'common/console.c',
        'common/crc.c',
        'common/cvar.c',
        'common/keys.c',
        'common/menu.c',
        'common/model.c',
        'common/pcx.c',
        'common/pr_cmds.c',
        'common/pr_edict.c',
        'common/pr_exec.c',
        'common/r_efrag.c',
        'common/r_part.c',
        'common/rb_tree.c',
        'common/screen.c',
        'common/sdl_common.c',
        'common/shell.c',
        'common/snd_dma.c',
        'common/snd_mem.c',
        'common/sprite_model.c',
        'common/sv_move.c',
        'common/sv_phys.c',
        'common/vid_mode.c',
        'common/wad.c',
        'common/world.c',
        'common/zone.c'
)

fisheye_src = files(
        'NQ/fisheye/fishLua.c',
        'NQ/fisheye/fishcam.c',
        'NQ/fisheye/fishcmd.c',
        'NQ/fisheye/fisheye.c',
        'NQ/fisheye/fishlens.c',
        'NQ/fisheye/fishmem.c',
        'NQ/fisheye/fishzoom.c',
        'NQ/fisheye/imageutil.c'
)

# test_deps = [dependency('cmocka')]

client_deps = [
        dependency('SDL2', required: true),
        dependency('SDL2_image', required: true),
        dependency('lua', required: true)
]

executable('blinky',
        NQ_src, common_src, common_nonasm_src,
        mac64_src, sw_src, SDL_src, fisheye_src,
        include_directories : includes,
        dependencies : client_deps,
        install : true,
        c_args : [
            '-DNQ_HACK',
            '-DTYR_VERSION=@0@'.format(TYR_VERSION_NUM),
            '-DTYR_VERSION_TIME=@0@LL'.format(TYR_VERSION_TIME)
        ])
