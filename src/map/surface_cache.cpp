#include "surface_cache.h"

SurfaceCache::SurfaceCache(std::shared_ptr<LMMapData> _map_data)
	: map_data(_map_data) {}

SurfaceCache::~SurfaceCache() {
	clear();
}

void SurfaceCache::build_cache() {
	// Reserve space upfront to prevent rehashing (which would invalidate pointers)
	cache.reserve(map_data->entity_count * map_data->texture_count);

	// Pre-compute surfaces for all (entity, texture) combinations
	for (int entity_idx = 0; entity_idx < map_data->entity_count; entity_idx++) {
		for (int texture_idx = 0; texture_idx < map_data->texture_count; texture_idx++) {
			const char* texture_name = map_data->textures[texture_idx].name;

			// Use existing gatherer to ensure identical output
			LMSurfaceGatherer gatherer(map_data);
			gatherer.surface_gatherer_set_entity_index_filter(entity_idx);
			gatherer.surface_gatherer_set_texture_filter(texture_name);
			gatherer.surface_gatherer_run();

			// Only cache if surfaces were generated
			if (gatherer.out_surfaces.surface_count > 0) {
				EntityTextureCacheKey key{entity_idx, texture_idx};

				// Take ownership of surfaces from gatherer
				LMSurfaces cached_surfaces;
				cached_surfaces.surface_count = gatherer.out_surfaces.surface_count;
				cached_surfaces.surfaces = gatherer.out_surfaces.surfaces;

				// Prevent gatherer from freeing this memory
				gatherer.out_surfaces.surfaces = nullptr;
				gatherer.out_surfaces.surface_count = 0;

				cache[key] = cached_surfaces;
			}
		}
	}
}

const LMSurfaces* SurfaceCache::get_surfaces(int entity_idx, int texture_idx) const {
	EntityTextureCacheKey key{entity_idx, texture_idx};
	auto it = cache.find(key);
	return (it != cache.end()) ? &it->second : nullptr;
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
