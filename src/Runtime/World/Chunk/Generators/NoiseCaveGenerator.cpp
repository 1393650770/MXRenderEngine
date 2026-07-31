#include "World/Chunk/Generators/NoiseCaveGenerator.h"
#include "World/CellPacking.h"
#include "World/MaterialRegistry.h"
#include "World/SimRNG.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(World)

namespace
{
	// Cheap deterministic value noise: smooth interpolation of hash corners.
	Float32 HashNoise(Float32 x, Float32 y, UInt32 seed)
	{
		Int ix = (Int)std::floor(x);
		Int iy = (Int)std::floor(y);
		Float32 fx = x - (Float32)ix;
		Float32 fy = y - (Float32)iy;
		fx = fx * fx * (3.0f - 2.0f * fx);   // smoothstep
		fy = fy * fy * (3.0f - 2.0f * fy);

		auto corner = [seed](Int cx, Int cy) -> Float32 {
			UInt32 h = SimRNG::HashU32((UInt32)cx, (UInt32)cy);
			h ^= seed;
			return (Float32)(WangHash(h) & 0xFFFFu) / 65535.0f;
		};
		Float32 v00 = corner(ix, iy);
		Float32 v10 = corner(ix + 1, iy);
		Float32 v01 = corner(ix, iy + 1);
		Float32 v11 = corner(ix + 1, iy + 1);
		Float32 vx0 = v00 + (v10 - v00) * fx;
		Float32 vx1 = v01 + (v11 - v01) * fx;
		return vx0 + (vx1 - vx0) * fy;
	}
}

void NoiseCaveGenerator::Generate(ChunkKey key, ChunkBaseline& out)
{
	UInt32 seed = SimRNG::HashU32(0xC0FFEEu, (UInt32)(key.cy * 100000 + key.cx));
	UInt8 stone = MaterialRegistry::GetStone();
	UInt8 empty = kMaterialEmpty;

	Float32 base_x = (Float32)(key.cx * kChunkSize);
	Float32 base_y = (Float32)(key.cy * kChunkSize);

	for (Int ly = 0; ly < kChunkSize; ++ly)
	{
		for (Int lx = 0; lx < kChunkSize; ++lx)
		{
			Float32 wx = base_x + (Float32)lx;
			Float32 wy = base_y + (Float32)ly;

			// Two-octave noise; carve caves where it exceeds 0.62.
			Float32 n1 = HashNoise(wx * 0.05f, wy * 0.05f, seed);
			Float32 n2 = HashNoise(wx * 0.13f + 100.0f, wy * 0.13f + 100.0f, seed ^ 0xABCD);
			Float32 n = n1 * 0.7f + n2 * 0.3f;

			UInt8 mat = (n > 0.62f) ? empty : stone;
			out.cells[(UInt32)(ly * kChunkSize + lx)] =
				PackCell(mat, (UInt8)kTempZeroOffset, 0, 0);
		}
	}
}

MYRENDERER_END_NAMESPACE  // World
MYRENDERER_END_NAMESPACE  // MXRender