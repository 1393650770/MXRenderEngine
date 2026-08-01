#include "World/MaterialRegistry.h"

MYRENDERER_BEGIN_NAMESPACE(MXRender)
MYRENDERER_BEGIN_NAMESPACE(World)

namespace
{
	Vector<MaterialDef>& GetTable()
	{
		static Vector<MaterialDef> s_table;
		return s_table;
	}

	// lazily build the built-in table exactly once
	void EnsureBuiltins()
	{
		auto& table = GetTable();
		if (!table.empty())
			return;

		// slot 0: Empty (reserved)
		MaterialDef empty;
		empty.stable_id = 0;
		empty.name = "Empty";
		empty.phase = MaterialPhase::Empty;
		empty.color = glm::vec3(0.15f, 0.15f, 0.18f);  // dark gray - distinguishable from render failure
		table.push_back(empty);

		// slot 1: Sand (powder)
		MaterialDef sand;
		sand.stable_id = 1;
		sand.name = "Sand";
		sand.phase = MaterialPhase::Powder;
		sand.density = 3;
		sand.color = glm::vec3(0.87f, 0.72f, 0.43f);
		sand.flammability = 0;
		table.push_back(sand);

		// slot 2: Water (liquid)
		MaterialDef water;
		water.stable_id = 2;
		water.name = "Water";
		water.phase = MaterialPhase::Liquid;
		water.density = 2;
		water.color = glm::vec3(0.16f, 0.44f, 0.87f);
		table.push_back(water);

		// slot 3: Stone (solid)
		MaterialDef stone;
		stone.stable_id = 3;
		stone.name = "Stone";
		stone.phase = MaterialPhase::Solid;
		stone.density = 10;
		stone.color = glm::vec3(0.48f, 0.48f, 0.52f);
		table.push_back(stone);

		// slot 4: Wood (solid, flammable)
		MaterialDef wood;
		wood.stable_id = 4;
		wood.name = "Wood";
		wood.phase = MaterialPhase::Solid;
		wood.density = 8;
		wood.color = glm::vec3(0.55f, 0.37f, 0.17f);
		wood.flammability = 12;          // 12 permille per tick
		wood.lifetime_max = 60;          // burns for 60 ticks
		wood.chance_permille = 12;
		table.push_back(wood);

		// slot 5: Fire (flame, lifetime-limited)
		MaterialDef fire;
		fire.stable_id = 5;
		fire.name = "Fire";
		fire.phase = MaterialPhase::Fire;
		fire.density = 1;
		fire.color = glm::vec3(1.0f, 0.45f, 0.1f);
		fire.lifetime_max = 8;
		table.push_back(fire);

		// slot 6: Ember (cooling ash)
		MaterialDef ember;
		ember.stable_id = 6;
		ember.name = "Ember";
		ember.phase = MaterialPhase::Powder;
		ember.density = 2;
		ember.color = glm::vec3(0.35f, 0.3f, 0.25f);
		table.push_back(ember);
	}
}

UInt8 MaterialRegistry::Register(CONST MaterialDef& def)
{
	auto& table = GetTable();
	EnsureBuiltins();

	if (table.size() >= kMaxMaterials)
		return kMaterialEmpty;

	UInt8 slot = (UInt8)table.size();
	MaterialDef copy = def;
	copy.stable_id = slot;              // invariant: stable_id == slot index
	table.push_back(copy);
	return slot;
}

CONST MaterialDef& MaterialRegistry::Get(UInt8 slot)
{
	EnsureBuiltins();
	auto& table = GetTable();
	if (slot >= table.size())
		return table[0];                // out of range -> Empty
	return table[slot];
}

UInt32 MaterialRegistry::GetCount()
{
	EnsureBuiltins();
	return (UInt32)GetTable().size();
}

CONST MaterialDef* MaterialRegistry::Data()
{
	EnsureBuiltins();
	return GetTable().data();
}

Bool MaterialRegistry::IsSolid(UInt8 slot)
{
	return Get(slot).phase == MaterialPhase::Solid;
}

UInt8 MaterialRegistry::GetEmpty() { return 0; }
UInt8 MaterialRegistry::GetSand() { return 1; }
UInt8 MaterialRegistry::GetWater() { return 2; }
UInt8 MaterialRegistry::GetStone() { return 3; }
UInt8 MaterialRegistry::GetWood() { return 4; }
UInt8 MaterialRegistry::GetFire() { return 5; }
UInt8 MaterialRegistry::GetEmber() { return 6; }

MYRENDERER_END_NAMESPACE  // World
MYRENDERER_END_NAMESPACE  // MXRender