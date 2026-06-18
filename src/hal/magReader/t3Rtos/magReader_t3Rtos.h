#ifndef MAGREADER_T3RTOS_H_
#define MAGREADER_T3RTOS_H_

#ifdef DEVICE_TRENDITT3RTOS

#include "../magReader.h"

#define MAG_READER_TRACK_1_SIZE 80
#define MAG_READER_TRACK_2_SIZE 41
#define MAG_READER_TRACK_3_SIZE 108

OOP_CLASS(MagReaderT3Rtos) { OOP_EXTENDS(MagReader); };

OOP_CTOR(MagReaderT3Rtos);

#endif

#endif