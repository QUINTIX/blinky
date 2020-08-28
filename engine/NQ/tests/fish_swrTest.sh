#!/bin/sh

cc -g -std=gnu11 -c -DNQ_HACK -DQBASEDIR="\"/Users/patrickquin/qk/content\"" \
	quake_swrTest.c -o bin/quake_swrTest.o \
	-I../../include -I../fisheye -I.. &&

cc -g -std=gnu11 -c -DNQ_HACK \
	../fisheye/fishcam.c -o bin/fishcam.o \
	-I../../include -I../fisheye -I.. &&

cc -g -std=c11 -c ../fisheye/fishlens.c -o bin/fishlens.o \
	-I../../include -I../fisheye &&

cc 	-L/usr/lib -L/usr/local/lib -lm -lcmocka -lsdl2 \
	./bin/fishcam.o  \
	./bin/fishlens.o \
	./bin/quake_swrTest.o \
	../../build/nqsw/mathlib.o \
	../../build/nqsw/zone.o \
	../../build/test/sys_t_unix.o \
	../../build/nqsw/common.o \
	../../build/nqsw/crc.o \
	../../build/nqsw/rb_tree.o \
	../../build/nqsw/shell.o \
	../../build/nqsw/cmd.o \
	../../build/nqsw/cvar.o \
	../../build/nqsw/snd_sdl.o \
	../../build/nqsw/snd_mix.o \
	../../build/nqsw/snd_mem.o \
	../../build/nqsw/snd_dma.o \
	../../build/nqsw/net_loop.o \
	../../build/nqsw/net_main.o \
	../../build/nqsw/net_bsd.o \
	../../build/nqsw/net_udp.o \
	../../build/nqsw/net_common.o \
	../../build/nqsw/net_dgrm.o \
	../../build/nqsw/world.o \
	../../build/nqsw/pr_*.o \
	../../build/nqsw/buildinfo.o \
	../../build/nqsw/host_cmd.o \
	../../build/nqsw/sv_*.o \
	../../build/nqsw/chase.o \
	../../build/nqsw/cl_*.o \
	../../build/nqsw/pcx.o \
	../../build/nqsw/screen.o \
	../../build/nqsw/d_*.o \
	../../build/nqsw/console.o \
	../../build/nqsw/nonintel.o \
	../../build/nqsw/sprite_model.o \
	../../build/nqsw/model.o \
	../../build/nqsw/r_*.o \
	../../build/nqsw/wad.o \
	../../build/nqsw/draw.o \
	../../build/nqsw/sbar.o \
	../../build/nqsw/host.o \
	../../build/nqsw/keys.o \
	../../build/nqsw/cd_null.o \
	../../build/nqsw/cd_common.o \
	../../build/nqsw/menu.o \
	../../build/nqsw/sdl_common.o \
	../../build/nqsw/in_sdl.o \
	../../build/nqsw/vid_mode.o \
	../../build/nqsw/vid_sdl.o \
	../../build/nqsw/view.o \
	../../build/nqsw/alias_model.o \
	-o bin/fish_swrTest.out &&

./bin/fish_swrTest.out