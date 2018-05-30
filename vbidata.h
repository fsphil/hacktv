
#ifndef _VBIDATA_H
#define _VBIDATA_H

#define VBIDATA_FILTER_RC (0)

#define VBIDATA_LSB_FIRST (0)
#define VBIDATA_MSB_FIRST (1)

extern int16_t *vbidata_init(unsigned int swidth, unsigned int dwidth, int16_t level, int filter, double beta);
extern void     vbidata_render_nrz(const int16_t *lut, const uint8_t *src, int offset, size_t length, int order, int16_t *dst, size_t step);

#endif

