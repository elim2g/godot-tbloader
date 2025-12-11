#include "surface_cache.h"
#include <godot_cpp/classes/time.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

SurfaceCache::SurfaceCache(std::shared_ptr<LMMapData> _map_data)
	: map_data(_map_data) {}

SurfaceCache::~SurfaceCache() {
	clear();
}

void SurfaceCache::build_cache() {
	// Reserve space upfront to prevent rehashing (which would invalidate pointers)
	// With lazy caching, we don't pre-populate, but we estimate typical needs
	cache.reserve(std::min(100, map_data->entity_count * map_data->texture_count));

	// Note: Actual caching happens lazily in get_surfaces() on first access
	// This avoids the O(n*m) upfront cost of pre-computing all (entity, texture) combinations
}

const LMSurfaces* SurfaceCache::get_surfaces(int entity_idx, int texture_idx) {
	EntityTextureCacheKey key{entity_idx, texture_idx};
	auto it = cache.find(key);

	if (it != cache.end()) {
		cache_hits++;
		return &it->second;
	}

	// Lazy caching: compute on first access if not found
	cache_misses++;
	const char* texture_name = map_data->textures[texture_idx].name;

	LMSurfaceGatherer gatherer(map_data);
	gatherer.surface_gatherer_set_entity_index_filter(entity_idx);
	gatherer.surface_gatherer_set_texture_filter(texture_name);

	auto start_time = godot::Time::get_singleton()->get_ticks_msec();
	gatherer.surface_gatherer_run();
	auto end_time = godot::Time::get_singleton()->get_ticks_msec();

	if (cache_misses == 1 || cache_misses % 50 == 0) {
		godot::UtilityFunctions::print(
			"[SurfaceCache] Gatherer run #", cache_misses,
			" (entity=", entity_idx, ", texture=", texture_idx,
			") took ", (end_time - start_time), "ms"
		);
	}

	// Only cache if surfaces were generated
	if (gatherer.out_surfaces.surface_count > 0) {
		LMSurfaces cached_surfaces;
		cached_surfaces.surface_count = gatherer.out_surfaces.surface_count;
		cached_surfaces.surfaces = gatherer.out_surfaces.surfaces;

		// Prevent gatherer from freeing this memory
		gatherer.out_surfaces.surfaces = nullptr;
		gatherer.out_surfaces.surface_count = 0;

		cache[key] = cached_surfaces;
		entries_cached++;
		return &cache[key];
	}

	return nullptr;
}

void SurfaceCache::clear() {
	for (auto& pair : cache) {
		LMSurfaces& surfs = pair.second;
		for (int s = 0; s < surfs.surface_count; ++s) {
			if (surfs.surfaces[s].vertices != nullptr) {
				free(surfs.surfaces[s].vertices);
			}
			if (surfs.surfaces[s].indices != nullptr) {
				free(surfs.surfaces[s].indices);
			}
		}
		if (surfs.surfaces != nullptr) {
			free(surfs.surfaces);
		}
	}
	cache.clear();
}

void SurfaceCache::print_diagnostics() const {
	int total_lookups = cache_hits + cache_misses;
	int hit_ratio = (total_lookups > 0) ? (100 * cache_hits) / total_lookups : 0;

	godot::UtilityFunctions::print(
		"[SurfaceCache] Entries cached: ", entries_cached,
		" | Hits: ", cache_hits,
		" | Misses: ", cache_misses,
		" | Total lookups: ", total_lookups,
		" | Hit ratio: ", hit_ratio, "%"
	);
}
