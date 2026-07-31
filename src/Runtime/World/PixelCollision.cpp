#include "World/PixelCollision.h"
#include <cmath>

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(World)

namespace
{
	constexpr UInt32 kBitsPerRow = 64;
	constexpr UInt32 kRowWords = (kWorldW + 63) / 64;
}

PixelCollision::PixelCollision()
{
	mask_.resize(kWorldH * kRowWords, 0);
}

void PixelCollision::SetSolid(Int x, Int y, Bool solid)
{
	if (x < 0 || y < 0 || x >= (Int)kWorldW || y >= (Int)kWorldH)
		return;
	UInt32 word = (UInt32)y * kRowWords + (UInt32)x / kBitsPerRow;
	UInt64 bit = 1ull << ((UInt32)x % kBitsPerRow);
	if (solid)
		mask_[word] |= bit;
	else
		mask_[word] &= ~bit;
}

Bool PixelCollision::IsSolid(Int x, Int y) CONST
{
	if (x < 0 || y < 0 || x >= (Int)kWorldW || y >= (Int)kWorldH)
		return true;   // out of world = solid (bounds)
	UInt32 word = (UInt32)y * kRowWords + (UInt32)x / kBitsPerRow;
	UInt64 bit = 1ull << ((UInt32)x % kBitsPerRow);
	return (mask_[word] & bit) != 0;
}

void PixelCollision::Clear()
{
	std::fill(mask_.begin(), mask_.end(), 0ull);
}

PixelCollision::RayHit PixelCollision::Raycast(glm::vec2 from, glm::vec2 dir, Float32 max_dist) CONST
{
	RayHit result;
	glm::vec2 end = from + dir * max_dist;
	Float32 dx = end.x - from.x;
	Float32 dy = end.y - from.y;

	Int x = (Int)std::floor(from.x);
	Int y = (Int)std::floor(from.y);
	Int step_x = dx > 0.0f ? 1 : (dx < 0.0f ? -1 : 0);
	Int step_y = dy > 0.0f ? 1 : (dy < 0.0f ? -1 : 0);

	Float32 t_delta_x = dx != 0.0f ? std::fabs(1.0f / dx) : 1e30f;
	Float32 t_delta_y = dy != 0.0f ? std::fabs(1.0f / dy) : 1e30f;

	Float32 t_max_x = t_delta_x;
	Float32 t_max_y = t_delta_y;
	Float32 t = 0.0f;

	// Bound the loop by the world diagonal to prevent infinite loops.
	UInt32 max_steps = kWorldW + kWorldH;
	for (UInt32 i = 0; i < max_steps; ++i)
	{
		if (IsSolid(x, y))
		{
			result.hit = true;
			result.cell_x = x;
			result.cell_y = y;
			result.t = t;
			// Normal points against the axis with the smaller t (the axis
			// we just crossed).
			if (t_max_x < t_max_y)
				result.normal_x = -step_x;
			else
				result.normal_y = -step_y;
			return result;
		}
		if (t_max_x < t_max_y)
		{
			x += step_x;
			t = t_max_x;
			t_max_x += t_delta_x;
		}
		else
		{
			y += step_y;
			t = t_max_y;
			t_max_y += t_delta_y;
		}
		if (t > max_dist)
			break;
	}
	return result;   // miss
}

Bool PixelCollision::CircleOverlap(glm::vec2 center, Float32 radius) CONST
{
	Int x0 = (Int)std::floor(center.x - radius);
	Int x1 = (Int)std::floor(center.x + radius);
	Int y0 = (Int)std::floor(center.y - radius);
	Int y1 = (Int)std::floor(center.y + radius);
	if (x1 < 0 || y1 < 0 || x0 >= (Int)kWorldW || y0 >= (Int)kWorldH)
		return false;

	for (Int y = y0; y <= y1; ++y)
	{
		if (y < 0 || y >= (Int)kWorldH)
			continue;
		UInt32 word0 = (UInt32)y * kRowWords;
		for (Int x = x0; x <= x1; ++x)
		{
			if (x < 0 || x >= (Int)kWorldW)
				continue;
			if (IsSolid(x, y))
				return true;
		}
		(void)word0;
	}
	return false;
}

void PixelCollision::ResolveAABB(glm::vec2& pos, glm::vec2& vel, glm::vec2 half_extents) CONST
{
	// X axis first, then Y (discrete two-axis separation; fixed tick keeps
	// velocities small enough that no tunneling occurs at tick speeds).
	if (vel.x != 0.0f)
	{
		pos.x += vel.x;
		Int x0 = (Int)std::floor(pos.x - half_extents.x);
		Int x1 = (Int)std::floor(pos.x + half_extents.x);
		Int y0 = (Int)std::floor(pos.y - half_extents.y);
		Int y1 = (Int)std::floor(pos.y + half_extents.y);
		Bool blocked = false;
		for (Int yy = y0; yy <= y1 && !blocked; ++yy)
			for (Int xx = x0; xx <= x1 && !blocked; ++xx)
				if (IsSolid(xx, yy))
					blocked = true;
		if (blocked)
		{
			pos.x -= vel.x;
			vel.x = 0.0f;
		}
	}

	if (vel.y != 0.0f)
	{
		pos.y += vel.y;
		Int x0 = (Int)std::floor(pos.x - half_extents.x);
		Int x1 = (Int)std::floor(pos.x + half_extents.x);
		Int y0 = (Int)std::floor(pos.y - half_extents.y);
		Int y1 = (Int)std::floor(pos.y + half_extents.y);
		Bool blocked = false;
		for (Int yy = y0; yy <= y1 && !blocked; ++yy)
			for (Int xx = x0; xx <= x1 && !blocked; ++xx)
				if (IsSolid(xx, yy))
					blocked = true;
		if (blocked)
		{
			pos.y -= vel.y;
			vel.y = 0.0f;
		}
	}
}

MYRENDERER_END_NAMESPACE  // World
MYRENDERER_END_NAMESPACE  // MXRender