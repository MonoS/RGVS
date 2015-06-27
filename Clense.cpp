/*
VapourSynth adaption by Fredrik Mellbin

Copyright(c) 2013 Victor Efimov

Permission is hereby granted, free of charge, to any person
obtaining a copy of this software and associated documentation
files(the "Software"), to deal in the Software without
restriction, including without limitation the rights to use,
copy, modify, merge, publish, distribute, sublicense, and / or sell
copies of the Software, and to permit persons to whom the
Software is furnished to do so, subject to the following
conditions :

The above copyright notice and this permission notice shall be
included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
NONINFRINGEMENT.IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
OTHER DEALINGS IN THE SOFTWARE.
*/

#include "shared.h"

#define CLENSE_RETERROR(x) do { vsapi->setError(out, (x)); vsapi->freeNode(d.cnode); vsapi->freeNode(d.pnode); vsapi->freeNode(d.nnode); return; } while (0)
#define CLAMP(value, lower, upper) do { if (value < lower) value = lower; else if (value > upper) value = upper; } while(0)

#include <emmintrin.h>
#include <immintrin.h>

typedef struct {
    VSNodeRef *cnode;
    VSNodeRef *pnode;
    VSNodeRef *nnode;
    const VSVideoInfo *vi;
    int mode;
    int process[3];
} ClenseData;


static void VS_CC clenseInit(VSMap *in, VSMap *out, void **instanceData, VSNode *node, VSCore *core, const VSAPI *vsapi) {
    ClenseData *d = static_cast<ClenseData *>(*instanceData);
    vsapi->setVideoInfo(d->vi, 1, node);
}

struct PlaneProc {
    template<typename T>
    static void clenseProcessPlane(T* VS_RESTRICT pDst, const T* VS_RESTRICT pSrc, const T* VS_RESTRICT pRef1, const T* VS_RESTRICT pRef2, int stride, int width, int height) {
        for (int y = 0; y < height; ++y) {
            int x;
            for (x = 0; x < (width / 8); x = x+8)
            {
                __m256 pSrc_8 = _mm256_load_ps(pSrc + x);
                __m256 pRef1_8 = _mm256_load_ps(pRef1 + x);
                __m256 pRef2_8 = _mm256_load_ps(pRef2 + x);

                __m256 dest = _mm256_min_ps(_mm256_max_ps(pSrc_8, _mm256_min_ps(pRef1_8, pRef2_8)), _mm256_max_ps(pRef1_8, pRef2_8));

                _mm256_store_ps((pDst + x), dest);
            }

            for(; x < width; ++x)
            {
                pDst[x] = std::min(std::max(pSrc[x], std::min(pRef1[x], pRef2[x])), std::max(pRef1[x], pRef2[x]));
            }

            pDst += stride;
            pSrc += stride;
            pRef1 += stride;
            pRef2 += stride;
        }
    }
};

struct PlaneProcFB {
    template<typename T>
    static void clenseProcessPlane(T* VS_RESTRICT pDst, const T* VS_RESTRICT pSrc, const T* VS_RESTRICT pRef1, const T* VS_RESTRICT pRef2, int stride, int width, int height) {

        __m256 Two = _mm256_set1_ps(2.0f);
        __m256 minLim = _mm256_set1_ps(std::numeric_limits<float>::min());
        __m256 maxLim = _mm256_set1_ps(std::numeric_limits<float>::max());

        for (int y = 0; y < height; ++y) {
            int x;


            for(x = 0; x < (width / 8);  x = x + 8)
            {
                __m256 pSrc_8 = _mm256_load_ps(pSrc + x);
                __m256 pRef1_8 = _mm256_load_ps(pRef1 + x);
                __m256 pRef2_8 = _mm256_load_ps(pRef2 + x);

                __m256 minref = _mm256_min_ps(pRef1_8, pRef2_8);
                __m256 maxref = _mm256_max_ps(pRef1_8, pRef2_8);

                __m256 lowref = _mm256_mul_ps(Two, minref);
                lowref = _mm256_sub_ps(lowref, pRef2_8);

                __m256 upref = _mm256_mul_ps(Two, maxref);
                upref = _mm256_sub_ps(upref, pRef2_8);

                __m256 value = pSrc_8;
                __m256 lower = _mm256_max_ps(lowref, minLim);
                __m256 upper = _mm256_max_ps(upref, maxLim);

                value = _mm256_min_ps(upper, _mm256_max_ps(lower, value));

                _mm256_store_ps(pDst + x, value);

            }

            for (; x < width; ++x) {
                T minref = std::min(pRef1[x], pRef2[x]);
                T maxref = std::max(pRef1[x], pRef2[x]);
                float lowref = minref * 2 - pRef2[x];
                float upref = maxref * 2 - pRef2[x];
                T src = pSrc[x];
                CLAMP(src, std::max<float>(lowref, std::numeric_limits<T>::min()), std::min<float>(upref, std::numeric_limits<T>::max()));
                pDst[x] = src;
            }

            pDst += stride;
            pSrc += stride;
            pRef1 += stride;
            pRef2 += stride;
        }
    }
};

template<typename T, typename Processor>
static const VSFrameRef *VS_CC clenseGetFrame(int n, int activationReason, void **instanceData, void **frameData, VSFrameContext *frameCtx, VSCore *core, const VSAPI *vsapi) {
    ClenseData *d = static_cast<ClenseData *>(*instanceData);

    if (activationReason == arInitial) {
        if (d->mode == cmNormal) {
            if (n >= 1 && (!d->vi->numFrames || n <= d->vi->numFrames - 2)) {
                *frameData = reinterpret_cast<void *>(1);
                vsapi->requestFrameFilter(n - 1, d->pnode, frameCtx);
                vsapi->requestFrameFilter(n, d->cnode, frameCtx);
                vsapi->requestFrameFilter(n + 1, d->nnode, frameCtx);
            } else {
                vsapi->requestFrameFilter(n, d->cnode, frameCtx);
            }
        } else if (d->mode == cmForward) {
            vsapi->requestFrameFilter(n, d->cnode, frameCtx);
            if (!d->vi->numFrames || n <= d->vi->numFrames - 3) {
                *frameData = reinterpret_cast<void *>(1);
                vsapi->requestFrameFilter(n + 1, d->cnode, frameCtx);
                vsapi->requestFrameFilter(n + 2, d->cnode, frameCtx);
            }
        } else if (d->mode == cmBackward) {
            if (n >= 2) {
                *frameData = reinterpret_cast<void *>(1);
                vsapi->requestFrameFilter(n - 2, d->cnode, frameCtx);
                vsapi->requestFrameFilter(n - 1, d->cnode, frameCtx);
            }
            vsapi->requestFrameFilter(n, d->cnode, frameCtx);
        }
    } else if (activationReason == arAllFramesReady) {
        const VSFrameRef *src = nullptr, *frame1 = nullptr, *frame2 = nullptr;

        if (!*frameData) // skip processing on first/last frames
            return vsapi->getFrameFilter(n, d->cnode, frameCtx);

        if (d->mode == cmNormal) {
            frame1 = vsapi->getFrameFilter(n - 1, d->pnode, frameCtx);
            src = vsapi->getFrameFilter(n, d->cnode, frameCtx);
            frame2 = vsapi->getFrameFilter(n + 1, d->nnode, frameCtx);
        } else if (d->mode == cmForward) {
            src = vsapi->getFrameFilter(n, d->cnode, frameCtx);
            frame1 = vsapi->getFrameFilter(n + 1, d->cnode, frameCtx);
            frame2 = vsapi->getFrameFilter(n + 2, d->cnode, frameCtx);
        } else if (d->mode == cmBackward) {
            frame2 = vsapi->getFrameFilter(n - 2, d->cnode, frameCtx);
            frame1 = vsapi->getFrameFilter(n - 1, d->cnode, frameCtx);
            src = vsapi->getFrameFilter(n, d->cnode, frameCtx);
        }

        const int pl[] = { 0, 1, 2 };
        const VSFrameRef *fr[] = { d->process[0] ? nullptr : src, d->process[1] ? nullptr : src, d->process[2] ? nullptr : src };

        VSFrameRef *dst = vsapi->newVideoFrame2(d->vi->format, d->vi->width, d->vi->height, fr, pl, src, core);

        int numPlanes = d->vi->format->numPlanes;
        for (int i = 0; i < numPlanes; i++) {
            if (d->process[i]) {
                Processor::template clenseProcessPlane<T>(
                    reinterpret_cast<T *>(vsapi->getWritePtr(dst, i)),
                    reinterpret_cast<const T *>(vsapi->getReadPtr(src, i)),
                    reinterpret_cast<const T *>(vsapi->getReadPtr(frame1, i)),
                    reinterpret_cast<const T *>(vsapi->getReadPtr(frame2, i)),
                    vsapi->getStride(dst, i)/sizeof(T),
                    vsapi->getFrameWidth(dst, i),
                    vsapi->getFrameHeight(dst, i));
            }
        }

        vsapi->freeFrame(src);
        vsapi->freeFrame(frame1);
        vsapi->freeFrame(frame2);

        return dst;
    }

    return nullptr;
}

static void VS_CC clenseFree(void *instanceData, VSCore *core, const VSAPI *vsapi) {
    ClenseData *d = static_cast<ClenseData *>(instanceData);
    vsapi->freeNode(d->cnode);
    vsapi->freeNode(d->pnode);
    vsapi->freeNode(d->nnode);
    delete d;
}

void VS_CC clenseCreate(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi) {
    ClenseData d = {};
    ClenseData *data;
    int err;
    int n, m, o;
    int i;

    d.mode = int64ToIntS(reinterpret_cast<intptr_t>(userData));
    d.cnode = vsapi->propGetNode(in, "clip", 0, nullptr);
    d.vi = vsapi->getVideoInfo(d.cnode);
    if (!isConstantFormat(d.vi))
        CLENSE_RETERROR("Clense: only constant format input supported");

    if (d.mode == cmNormal) {
        d.pnode = vsapi->propGetNode(in, "previous", 0, &err);
        if (err)
            d.pnode = vsapi->cloneNodeRef(d.cnode);
        d.nnode = vsapi->propGetNode(in, "next", 0, &err);
        if (err)
            d.nnode = vsapi->cloneNodeRef(d.cnode);
    }

    if (d.pnode && !isSameFormat(d.vi, vsapi->getVideoInfo(d.pnode)))
        CLENSE_RETERROR("Clense: previous clip doesn't have the same format as the main clip");

    if (d.nnode && !isSameFormat(d.vi, vsapi->getVideoInfo(d.nnode)))
        CLENSE_RETERROR("Clense: previous clip doesn't have the same format as the main clip");

    n = d.vi->format->numPlanes;
    m = vsapi->propNumElements(in, "planes");

    for (i = 0; i < 3; i++)
        d.process[i] = m <= 0;

    for (i = 0; i < m; i++) {
        o = int64ToIntS(vsapi->propGetInt(in, "planes", i, nullptr));

        if (o < 0 || o >= n)
            CLENSE_RETERROR("Clense: plane index out of range");

        if (d.process[o])
            CLENSE_RETERROR("Clense: plane specified twice");

        d.process[o] = 1;
    }

    VSFilterGetFrame getFrameFunc = nullptr;
      if (d.mode == cmNormal) {
          getFrameFunc = clenseGetFrame<float, PlaneProc>;
          }
      else {
          getFrameFunc = clenseGetFrame<float, PlaneProcFB>;
          }

    data = new ClenseData(d);

    vsapi->createFilter(in, out, "Clense", clenseInit, getFrameFunc, clenseFree, fmParallel, 0, data, core);
}
