/*****************************************************************************

AvsFilterRemoveGrain/Repair16
Author: Laurent de Soras, 2012
Modified for VapourSynth by Fredrik Mellbin 2013

--- Legal stuff ---

This program is free software. It comes without any warranty, to
the extent permitted by applicable law. You can redistribute it
and/or modify it under the terms of the Do What The Fuck You Want
To Public License, Version 2, as published by Sam Hocevar. See
http://sam.zoy.org/wtfpl/COPYING for more details.

*Tab=3***********************************************************************/

#ifndef SHARED_H
#define SHARED_H

#include "VapourSynth.h"
#include "VSHelper.h"
#include <stdint.h>
#include <algorithm>
#include <cstdlib>
#include <emmintrin.h>
#include <immintrin.h>

#if defined(_MSC_VER)
#define _ALLOW_KEYWORD_MACROS
#define alignas(x) __declspec(align(x))
#define ALIGNED_ARRAY(decl, alignment) alignas(alignment) decl
#else
#ifndef __forceinline
#define __forceinline inline
#endif
#define ALIGNED_ARRAY(decl, alignment) __attribute__((aligned(16))) decl
#endif

static __forceinline __m256 clamp_8(__m256 lower,__m256 value, __m256 upper)
{
    return _mm256_min_ps(upper, _mm256_max_ps(lower, value));
}
template <class T>
static __forceinline T limit(T x, T mi, T ma)
{
	return ((x < mi) ? mi : ((x > ma) ? ma : x));
}

class LineProcAll {
public:
	static inline bool skip_line(int) { return (false); }
};
class LineProcEven {
public:
	static inline bool skip_line(int y) { return ((y & 1) != 0); }
};
class LineProcOdd {
public:
	static inline bool skip_line(int y) { return ((y & 1) == 0); }
};

enum cleanseMode {
	cmNormal,
	cmForward,
	cmBackward
};

void VS_CC removeGrainCreate(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi);
void VS_CC repairCreate(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi);
void VS_CC verticalCleanerCreate(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi);
void VS_CC clenseCreate(const VSMap *in, VSMap *out, void *userData, VSCore *core, const VSAPI *vsapi);

#endif

