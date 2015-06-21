#include "shared.h"

VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc, VSRegisterFunction registerFunc, VSPlugin *plugin) {
	configFunc("com.vapoursynth.removegrainvs", "rgsf", "RemoveGrain VapourSynth Port", VAPOURSYNTH_API_VERSION, 1, plugin);
	registerFunc("RemoveGrain", "clip:clip;mode:int[];", removeGrainCreate, nullptr, plugin);
	registerFunc("Repair", "clip:clip;repairclip:clip;mode:int[];", repairCreate, nullptr, plugin);
	registerFunc("VerticalCleaner", "clip:clip;mode:int[];", verticalCleanerCreate, nullptr, plugin);
	registerFunc("Clense", "clip:clip;previous:clip:opt;next:clip:opt;planes:int[]:opt;", clenseCreate, reinterpret_cast<void *>(cmNormal), plugin);
	registerFunc("ForwardClense", "clip:clip;planes:int[]:opt;", clenseCreate, reinterpret_cast<void *>(cmForward), plugin);
	registerFunc("BackwardClense", "clip:clip;planes:int[]:opt;", clenseCreate, reinterpret_cast<void *>(cmBackward), plugin);
} 
