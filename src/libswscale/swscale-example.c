/*
 * Copyright (C) 2003 Michael Niedermayer <michaelni@gmx.at>
 *
 * This file is part of FFmpeg.
 *
 * FFmpeg is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * FFmpeg is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with FFmpeg; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <stdarg.h>

#undef HAVE_AV_CONFIG_H
#include "libavutil/avutil.h"
#include "libavutil/lfg.h"
#include "swscale.h"
#include "swscale_internal.h"

#undef fprintf
#undef free
#undef malloc
#undef perror
#undef printf

static uint64_t getSSD(uint8_t *src1, uint8_t *src2, int stride1, int stride2, int w, int h){
    int x,y;
    uint64_t ssd=0;

//printf("%d %d\n", w, h);

    for (y=0; y<h; y++){
        for (x=0; x<w; x++){
            int d= src1[x + y*stride1] - src2[x + y*stride2];
            ssd+= d*d;
//printf("%d", abs(src1[x + y*stride1] - src2[x + y*stride2])/26 );
        }
//printf("\n");
    }
    return ssd;
}

// test by ref -> src -> dst -> out & compare out against ref
// ref & out are YV12
static int doTest(uint8_t *ref[4], int refStride[4], int w, int h, int srcFormat, int dstFormat,
                  int srcW, int srcH, int dstW, int dstH, int flags){
    uint8_t *src[4] = {0};
    uint8_t *dst[4] = {0};
    uint8_t *out[4] = {0};
    int srcStride[4], dstStride[4];
    int i;
    uint64_t ssdY, ssdU, ssdV, ssdA=0;
    struct SwsContext *srcContext = NULL, *dstContext = NULL,
                      *outContext = NULL;
    int res;

    res = 0;
    for (i=0; i<4; i++){
        // avoid stride % bpp != 0
        if (srcFormat==PIX_FMT_RGB24 || srcFormat==PIX_FMT_BGR24)
            srcStride[i]= srcW*3;
        else if (srcFormat==PIX_FMT_RGB48BE || srcFormat==PIX_FMT_RGB48LE)
            srcStride[i]= srcW*6;
        else
            srcStride[i]= srcW*4;

        if (dstFormat==PIX_FMT_RGB24 || dstFormat==PIX_FMT_BGR24)
            dstStride[i]= dstW*3;
        else if (dstFormat==PIX_FMT_RGB48BE || dstFormat==PIX_FMT_RGB48LE)
            dstStride[i]= dstW*6;
        else
            dstStride[i]= dstW*4;

        src[i]= (uint8_t*) malloc(srcStride[i]*srcH);
        dst[i]= (uint8_t*) malloc(dstStride[i]*dstH);
        out[i]= (uint8_t*) malloc(refStride[i]*h);
        if (!src[i] || !dst[i] || !out[i]) {
            perror("Malloc");
            res = -1;

            goto end;
        }
    }

    srcContext= sws_getContext(w, h, PIX_FMT_YUVA420P, srcW, srcH, srcFormat, flags, NULL, NULL, NULL);
    if (!srcContext) {
        fprintf(stderr, "Failed to get %s ---> %s\n",
                sws_format_name(PIX_FMT_YUVA420P),
                sws_format_name(srcFormat));
        res = -1;

        goto end;
    }
    dstContext= sws_getContext(srcW, srcH, srcFormat, dstW, dstH, dstFormat, flags, NULL, NULL, NULL);
    if (!dstContext) {
        fprintf(stderr, "Failed to get %s ---> %s\n",
                sws_format_name(srcFormat),
                sws_format_name(dstFormat));
        res = -1;

        goto end;
    }
    outContext= sws_getContext(dstW, dstH, dstFormat, w, h, PIX_FMT_YUVA420P, flags, NULL, NULL, NULL);
    if (!outContext) {
        fprintf(stderr, "Failed to get %s ---> %s\n",
                sws_format_name(dstFormat),
                sws_format_name(PIX_FMT_YUVA420P));
        res = -1;

        goto end;
    }
//    printf("test %X %X %X -> %X %X %X\n", (int)ref[0], (int)ref[1], (int)ref[2],
//        (int)src[0], (int)src[1], (int)src[2]);

    sws_scale(srcContext, ref, refStride, 0, h   , src, srcStride);
    sws_scale(dstContext, src, srcStride, 0, srcH, dst, dstStride);
    sws_scale(outContext, dst, dstStride, 0, dstH, out, refStride);

    ssdY= getSSD(ref[0], out[0], refStride[0], refStride[0], w, h);
    ssdU= getSSD(ref[1], out[1], refStride[1], refStride[1], (w+1)>>1, (h+1)>>1);
    ssdV= getSSD(ref[2], out[2], refStride[2], refStride[2], (w+1)>>1, (h+1)>>1);
    if (isALPHA(srcFormat) && isALPHA(dstFormat))
        ssdA= getSSD(ref[3], out[3], refStride[3], refStride[3], w, h);

    if (srcFormat == PIX_FMT_GRAY8 || dstFormat==PIX_FMT_GRAY8) ssdU=ssdV=0; //FIXME check that output is really gray

    ssdY/= w*h;
    ssdU/= w*h/4;
    ssdV/= w*h/4;
    ssdA/= w*h;

    printf(" %s %dx%d -> %s %4dx%4d flags=%2d SSD=%5"PRId64",%5"PRId64",%5"PRId64",%5"PRId64"\n",
           sws_format_name(srcFormat), srcW, srcH,
           sws_format_name(dstFormat), dstW, dstH,
           flags, ssdY, ssdU, ssdV, ssdA);
    fflush(stdout);

    end:

    sws_freeContext(srcContext);
    sws_freeContext(dstContext);
    sws_freeContext(outContext);

    for (i=0; i<4; i++){
        free(src[i]);
        free(dst[i]);
        free(out[i]);
    }

    return res;
}

static void selfTest(uint8_t *src[4], int stride[4], int w, int h){
    enum PixelFormat srcFormat, dstFormat;
    int srcW, srcH, dstW, dstH;
    int flags;

    for (srcFormat = 0; srcFormat < PIX_FMT_NB; srcFormat++) {
        for (dstFormat = 0; dstFormat < PIX_FMT_NB; dstFormat++) {
            printf("%s -> %s\n",
                   sws_format_name(srcFormat),
                   sws_format_name(dstFormat));
            fflush(stdout);

            srcW= w;
            srcH= h;
            for (dstW=w - w/3; dstW<= 4*w/3; dstW+= w/3){
                for (dstH=h - h/3; dstH<= 4*h/3; dstH+= h/3){
                    for (flags=1; flags<33; flags*=2) {
                        int res;

                        res = doTest(src, stride, w, h, srcFormat, dstFormat,
                                     srcW, srcH, dstW, dstH, flags);
                        if (res < 0) {
                            dstW = 4 * w / 3;
                            dstH = 4 * h / 3;
                            flags = 33;
                        }
                    }
                }
            }
        }
    }
}

#define W 96
#define H 96

int main(int argc, char **argv){
    uint8_t *rgb_data = malloc (W*H*4);
    uint8_t *rgb_src[3]= {rgb_data, NULL, NULL};
    int rgb_stride[3]={4*W, 0, 0};
    uint8_t *data = malloc (4*W*H);
    uint8_t *src[4]= {data, data+W*H, data+W*H*2, data+W*H*3};
    int stride[4]={W, W, W, W};
    int x, y;
    struct SwsContext *sws;
    AVLFG rand;

    sws= sws_getContext(W/12, H/12, PIX_FMT_RGB32, W, H, PIX_FMT_YUVA420P, 2, NULL, NULL, NULL);

    av_lfg_init(&rand, 1);

    for (y=0; y<H; y++){
        for (x=0; x<W*4; x++){
            rgb_data[ x + y*4*W]= av_lfg_get(&rand);
        }
    }
    sws_scale(sws, rgb_src, rgb_stride, 0, H, src, stride);

    selfTest(src, stride, W, H);

    return 123;
}
