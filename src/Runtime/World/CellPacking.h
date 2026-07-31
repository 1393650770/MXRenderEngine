#pragma once
#ifndef _CELL_PACKING_
#define _CELL_PACKING_

// 32-bit packed cell layout shared by CPU (C++) and GPU (GLSL):
//
//   bit 31..24  flags8   bit 23..16  life8   bit 15..8  temp8   bit 7..0  mat8
//
//   mat8   - material registry slot index (0 = Empty, reserved)
//   temp8  - offset-encoded temperature: temp8 = clamp(round(C/2)+128, 0, 255)
//            decode: C = (temp8 - 128) * 2  (range -256..+254 C)
//   life8  - burn/lifetime countdown in ticks (255 max, decremented per tick)
//   flags8 - bit0 ACTIVE (simulate this tick), bit1 BURNING,
//            bit2 SLEEPING (reserved), bit3..7 reserved
//
// The packed value is directly usable as an atomicCompSwap operand (32-bit).
// GLSL side: resource/Shader/World/world_common.glsl keeps identical macros.

#include "Core/ConstDefine.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(World)

// ---- flag bits ----
static CONST UInt32 kCellFlagActive = 0x01;
static CONST UInt32 kCellFlagBurning = 0x02;
static CONST UInt32 kCellFlagSleeping = 0x04;

// ---- temp encoding ----
static CONST Int kTempZeroOffset = 128;   // temp8 == 128 means 0 C
static CONST Float32 kTempScale = 2.0f;     // 1 temp8 unit == 2 C

FORCEINLINE UInt32 PackCell(UInt8 mat, UInt8 temp, UInt8 life, UInt8 flags)
{
	return ((UInt32)flags << 24) | ((UInt32)life << 16) | ((UInt32)temp << 8) | (UInt32)mat;
}

FORCEINLINE UInt8 UnpackMat(UInt32 packed)
{
	return (UInt8)(packed & 0xFF);
}

FORCEINLINE UInt8 UnpackTemp(UInt32 packed)
{
	return (UInt8)((packed >> 8) & 0xFF);
}

FORCEINLINE UInt8 UnpackLife(UInt32 packed)
{
	return (UInt8)((packed >> 16) & 0xFF);
}

FORCEINLINE UInt8 UnpackFlag(UInt32 packed)
{
	return (UInt8)((packed >> 24) & 0xFF);
}

FORCEINLINE Float32 DecodeTempCelsius(UInt8 temp8)
{
	return ((Int)temp8 - kTempZeroOffset) * kTempScale;
}

FORCEINLINE UInt8 EncodeTempCelsius(Float32 celsius)
{
	Int v = (Int)(celsius / kTempScale) + kTempZeroOffset;
	if (v < 0) v = 0;
	if (v > 255) v = 255;
	return (UInt8)v;
}

MYRENDERER_END_NAMESPACE  // World
MYRENDERER_END_NAMESPACE  // MXRender

#endif // _CELL_PACKING_