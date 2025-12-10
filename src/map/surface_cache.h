#ifndef SURFACE_CACHE_H
#define SURFACE_CACHE_H

#include "surface_gatherer.h"
#include "map_data.h"
#include <memory>
#include <unordered_map>

// Key for looking up cached surfaces
struct EntityTextureCacheKey {
	int entity_idx;
	int texture_idx;

	bool operator==(const EntityTextureCacheKey& other) const {
		return entity_idx == other.entity_idx && texture_idx == other.texture_idx;
	}
};

// Hash function for cache key
struct EntityTextureCacheKeyHash {
	std::size_t operator()(const EntityTextureCacheKey& k) const {
		return std::hash<int>()(k.entity_idx) ^ (std::hash<int>()(k.texture_idx) << 1);
	}
};

class SurfaceCache {
public:
	std::shared_ptr<LMMapData> map_data;

	// Cache: (entity_idx, texture_idx) -> LMSurfaces
	std::unordered_map<EntityTextureCacheKey, LMSurfaces, EntityTextureCacheKeyHash> cache;

	SurfaceCache(std::shared_ptr<LMMapData> _map_data);
	~SurfaceCache();

	// Pre-populate cache for all (entity, texture) combinations
	void build_cache();

	// Lookup cached surfaces (returns nullptr if not found)
	const LMSurfaces* get_surfaces(int entity_idx, int texture_idx) const;

	// Free all cached memory
	void clear();
};

#endif // SURFACE_CACHE_H
