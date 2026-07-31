#pragma once
#ifndef _MATERIAL_REGISTRY_
#define _MATERIAL_REGISTRY_

#include "Core/ConstDefine.h"
#include "World/MaterialDef.h"
#include "World/PixelWorldConstants.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(World)

// Static material registry. Built-in materials are registered at first use;
// new materials can be added at runtime via Register (data-driven design -
// cells store the 8-bit slot index, so adding a material never invalidates
// existing cells).
MYRENDERER_BEGIN_CLASS(MaterialRegistry)
#pragma region METHOD
public:
	// Registers a definition, assigns slot index (stable_id == slot).
	// Returns slot, or kMaterialEmpty if registry is full.
	static UInt8 METHOD(Register)(CONST MaterialDef& def);

	// Returns the definition for a slot index. Slot 0 is Empty.
	static CONST MaterialDef& METHOD(Get)(UInt8 slot);

	static UInt32 METHOD(GetCount)();
	static CONST MaterialDef* METHOD(Data)();

	// Built-in materials (registered lazily on first access)
	static UInt8 METHOD(GetEmpty)();
	static UInt8 METHOD(GetSand)();
	static UInt8 METHOD(GetWater)();
	static UInt8 METHOD(GetStone)();
	static UInt8 METHOD(GetWood)();
	static UInt8 METHOD(GetFire)();
	static UInt8 METHOD(GetEmber)();

protected:

private:
#pragma endregion

#pragma region MEMBER
public:

protected:

private:
#pragma endregion
MYRENDERER_END_CLASS

MYRENDERER_END_NAMESPACE  // World
MYRENDERER_END_NAMESPACE  // MXRender

#endif // _MATERIAL_REGISTRY_