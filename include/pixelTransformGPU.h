#ifndef PIXELTRANSFORMGPU_H
#define PIXELTRANSFORMGPU_H

#include<format.h>

__global__ void iDCT_GPU(int* in,
			 unsigned char *out,
			 int stride,
			 int samples_x, int samples_y,
			 int num_DCT_blocks);
__global__ void iDCT_GPU_warp_shuffle(int* in,
			 unsigned char *out,
			 int stride,
			 int samples_x, int samples_y,
			 int num_DCT_blocks);

__host__ void iDCT_resample_colourTransform(JPGReader* jpg);
__host__ void upsampleAndColourTransformHybrid(JPGReader* jpg);
__host__ void upsampleAndColourTransformGPU(JPGReader* jpg);
__host__ void upsampleAndColourTransformCPU(JPGReader* jpg);


#endif // PIXELTRANSFORMGPU_H //
