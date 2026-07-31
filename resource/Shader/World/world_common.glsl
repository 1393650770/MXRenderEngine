// Shared definitions for the GPU pixel world simulation chain.
// Included by each world_*.comp AFTER their #version + #extension lines.
// IMPORTANT: keep in sync with the C++ side:
//   - src/Runtime/World/PixelWorldConstants.h  (kWorldW/kWorldH/kMaxMaterials)
//   - src/Runtime/World/CellPacking.h          (bit layout)
//   - src/Runtime/World/PixelWorldRng.h        (wang_hash / rng_dice)
//
// Cell packing:  bit 31..24 flags | bit 23..16 life | bit 15..8 temp | bit 7..0 mat

// ---- world size (must match PixelWorldConstants.h) ----
#define WORLD_W 256u
#define WORLD_H 192u
#define WORLD_CELLS (WORLD_W * WORLD_H)

// ---- cell bit layout (must match CellPacking.h) ----
#define MAT_MASK  0xFFu
#define TEMP_SHIFT 8u
#define LIFE_SHIFT 16u
#define FLAG_SHIFT 24u

// ---- flag bits ----
#define FLAG_ACTIVE   0x01u
#define FLAG_BURNING  0x02u
#define FLAG_SLEEPING 0x04u

// ---- material slots (must match MaterialRegistry.cpp builtins) ----
#define MAT_EMPTY 0u
#define MAT_SAND  1u
#define MAT_WATER 2u
#define MAT_STONE 3u
#define MAT_WOOD  4u
#define MAT_FIRE  5u
#define MAT_EMBER 6u

// ---- edit event ops (must match ITerrainEditSink.h) ----
#define OP_WRITE  0u
#define OP_DEPOSIT 1u

// ---- deterministic RNG (must match PixelWorldRng.h, bit-identical) ----
uint wang_hash(uint key)
{
    key = (key ^ 61u) ^ (key >> 16);
    key = key + (key << 3u);
    key = key ^ (key >> 4u);
    key = key * 0x27d4eb2du;
    key = key ^ (key >> 15u);
    return key;
}

uint rng_dice(uint cell_index, uint tick, uint material_seed)
{
    uint key = cell_index ^ (tick * 0x9E3779B9u) ^ (material_seed * 0x85EBCA77u);
    return wang_hash(key) & 0x3FFu;
}

bool rng_hit(uint cell_index, uint tick, uint material_seed, uint chance_permille)
{
    return rng_dice(cell_index, tick, material_seed) < chance_permille;
}

// ---- cell helpers ----
uint pack_cell(uint mat, uint temp, uint life, uint flags)
{
    return (flags << FLAG_SHIFT) | (life << LIFE_SHIFT) | (temp << TEMP_SHIFT) | mat;
}

uint cell_mat(uint packed)  { return packed & MAT_MASK; }
uint cell_temp(uint packed) { return (packed >> TEMP_SHIFT) & 0xFFu; }
uint cell_life(uint packed) { return (packed >> LIFE_SHIFT) & 0xFFu; }
uint cell_flag(uint packed) { return (packed >> FLAG_SHIFT) & 0xFFu; }

uint cell_index(uint x, uint y) { return y * WORLD_W + x; }

// ---- simulation params (matches GpuPixelWorld sim_params buffer) ----
struct SimParams {
    uint tick;
    uint event_count;
    uint world_w;
    uint world_h;
    uint frame_index;
    uint _pad0;
    uint _pad1;
    uint _pad2;
};

// ---- material props entry (matches MaterialDef serialized layout) ----
struct MaterialProps {
    uint phase;      // 0 Empty 1 Powder 2 Liquid 3 Gas 4 Solid 5 Fire
    uint density;
    uint flammability;      // permille
    uint lifetime_max;
    vec4 color;
    uint chance_permille;
    uint _pad0;
    uint _pad1;
    uint _pad2;
};
