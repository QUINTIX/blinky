#include "qtypes.h"
#include "fisheye.h"

byte* makePalmapForPlate_(const byte* inPal, byte palleteLookup[256],
     int plateIdx);

void WritePCXplate_(struct _globe* globe,
        ray_to_plate_index_t ray_to_plate, const byte* inPal,
	char *filename, int plate_index, int with_margins);

void dumppal(const byte* inPal);
