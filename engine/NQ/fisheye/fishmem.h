#include "fisheye.h"

#ifndef FISHMEM_H_
#define FISHMEM_H_

void createOrReallocBuffers(struct _globe* globe, struct _lens* lens,
		int area, int platesize);

#endif
