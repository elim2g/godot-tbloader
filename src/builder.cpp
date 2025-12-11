#include <builder.h>

#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/omni_light3d.hpp>
#include <godot_cpp/classes/area3d.hpp>
#include <godot_cpp/classes/collision_shape3d.hpp>
#include <godot_cpp/classes/shape3d.hpp>
#include <godot_cpp/classes/packed_scene.hpp>
#include <godot_cpp/classes/standard_material3d.hpp>
#include <godot_cpp/classes/convex_polygon_shape3d.hpp>
#include <godot_cpp/classes/concave_polygon_shape3d.hpp>
#include <godot_cpp/templates/vmap.hpp>
#include <godot_cpp/templates/hash_map.hpp>

#include <turnt_loader.h>
#include <scoped_timer.h>
#include <material_slot_map.h>

Builder::Builder(TurntLoader* loader)
{
	m_loader = loader;
	m_map = std::make_shared<LMMapData>();
}

Builder::~Builder()
{
}

void Builder::load_map(const String& path, bool generate_geometry)
{
	SCOPED_TIMER(LOAD_MAP);
	UtilityFunctions::print("Building map ", path);

	if (!FileAccess::file_exists(path)) {
		UtilityFunctions::printerr("Map file does not exist!");
		return;
	}

	// Parse the map from the file
	Ref<FileAccess> f = FileAccess::open(path, FileAccess::ModeFlags::READ);
	LMMapParser parser(m_map);
	{
		SCOPED_TIMER(PARSE_MAP);
		parser.load_from_godot_file(f);
	}

	{
		SCOPED_TIMER(CACHE_TEXTURES);
		load_and_cache_map_textures();
	}

	// <ELIM> Moved texture sizing to the moment textures are loaded to prevent hashtable lookup
	// We have to manually set the size of textures
	// {
	// 	SCOPED_TIMER(TEXTURE_SIZING);
	// 	for (int i = 0; i < m_map->texture_count; i++) {
	// 		auto& tex = m_map->textures[i];

	// 		auto res_texture = texture_from_name(tex.name);
	// 		if (res_texture != nullptr) {
	// 			tex.width = res_texture->get_width();
	// 			tex.height = res_texture->get_height();
	// 		} else {
	// 			// Make sure we don't divide by 0 and create NaN UV's
	// 			tex.width = 1;
	// 			tex.height = 1;
	// 		}
	// 	}
	// }
	// </ELIM>

	// Run geometry generator (this also generates UV's, so we do this last)
	if (generate_geometry)
	{
		SCOPED_TIMER(GEOGEN);
		LMGeoGenerator geogen(m_map);
		geogen.run();

		// Build surface cache for optimized mesh building
		{
			SCOPED_TIMER(BUILD_SURFACE_CACHE);
			m_surface_cache = std::make_unique<SurfaceCache>(m_map);
			m_surface_cache->build_cache();
		}
	}
}

void Builder::build_map()
{
	SCOPED_TIMER(BUILD_MAP);
	{
		SCOPED_TIMER(BUILD_ENTITIES);
		for (int i = 0; i < m_map->entity_count; i++) {
			auto& ent = m_map->entities[i];
			build_entity(i, ent, ent.get_property("classname"));
		}
	}

	// Print cache diagnostics
	if (m_surface_cache) {
		m_surface_cache->print_diagnostics();
	}
}

Node* Builder::build_worldspawn(int idx, LMEntity& ent, bool collision)
{
	// Create node for this entity
	auto container_node = memnew(Node3D());
	m_loader->add_child(container_node);
	container_node->set_owner(m_loader->get_owner());

	// Decide generated collision type
	ColliderType collider = ColliderType::None;
	ColliderShape collider_shape = ColliderShape::Concave;
	if (collision && m_loader->m_collision) {
		collider = ColliderType::Static;
		collider_shape = ColliderShape::Concave;
	}

	// Create mesh instance for worldspawn
	build_entity_mesh(idx, ent, container_node, collider, collider_shape);

	// Delete container if we added nothing to it
	if (container_node->get_child_count() == 0) {
		container_node->queue_free();
		return nullptr;
	}

	// Find name for entity
	const char* tb_name;
	if (!strcmp(ent.get_property("classname"), "worldspawn")) {
		tb_name = "Default Layer";
	} else {
		tb_name = ent.get_property("_tb_name", nullptr);
	}

	// Add container to loader
	if (tb_name != nullptr) {
		container_node->set_name(tb_name);
	}
	container_node->set_position(lm_transform(ent.center));

	return container_node;
}

Node* Builder::build_entity(int idx, LMEntity& ent, const String& classname)
{
	Node* newEntityNode = nullptr;

	if (classname == "worldspawn" || classname == "func_group") {
		// Skip worldspawn if the layer is hidden and the "skip hidden layers" option is checked
		if (m_loader->m_skip_hidden_layers) {
			bool is_visible = (ent.get_property_int("_tb_layer_hidden", 0) == 0);
			if (!is_visible) {
				return nullptr;
			}
		}

		{
			SCOPED_TIMER(BUILD_WORLDSPAWN);
			newEntityNode = build_worldspawn(idx, ent, true);
		}

	} else {
		// Load common entities if enabled
		if (m_loader->m_entity_common) {
			if (classname == "light") {
				newEntityNode = build_entity_light(idx, ent);
			} else if (classname == "area") {
				newEntityNode = build_entity_area(idx, ent);
			} else if (classname == "nocollision") {
				newEntityNode = build_worldspawn(idx, ent, false);
			}

			//TODO: More common entities
		}

		if (newEntityNode == nullptr) {
			// Still no entity? We're building a custom one
			newEntityNode = build_entity_custom(idx, ent, m_map->entity_geo[idx], classname);
		}
	}

	if (newEntityNode != nullptr) {
		// Load common properties
		if (ent.has_property("name")) {
			newEntityNode->set_name(ent.get_property("name"));
		}
	}

	return newEntityNode;
}

Node* Builder::build_entity_custom(int idx, LMEntity& ent, LMEntityGeometry& geo, const String& classname)
{
	// m_loader->m_entity_path => "res://entities/"
	// "info_player_start" => "info/player/start.tscn", "info/player_start.tscn", "info_player_start.tscn"
	// "thing" => "thing.tscn"

	auto resource_loader = ResourceLoader::get_singleton();

	auto arr = classname.split("_");
	for (int i = 0; i < arr.size(); i++) {
		String path = m_loader->m_entity_path + "/";
		for (int j = 0; j < arr.size(); j++) {
			if (j > 0) {
				if (j <= i) {
					path = path + "/";
				} else {
					path = path + "_";
				}
			}
			path = path + arr[j];
		}
		path = path + ".tscn";

		if (resource_loader->exists(path, "PackedScene")) {
			Ref<PackedScene> scene = resource_loader->load(path);
			if (scene == nullptr) {
				UtilityFunctions::printerr("Resource at path '", path, "' could not be loaded as a PackedScene by the resource loader!");
				return nullptr;
			}

			auto instance = scene->instantiate();
			m_loader->add_child(instance);
			instance->set_owner(m_loader->get_owner());

			if (instance->is_class("Node3D")) {
				set_entity_node_common((Node3D*)instance, ent);
				if (ent.brush_count > 0) {
					set_entity_brush_common(idx, (Node3D*)instance, ent);
				}
			}

			for (int j = 0; j < ent.property_count; j++) {
				auto& prop = ent.properties[j];

				auto var = instance->get(prop.key);
				switch (var.get_type()) {
					case Variant::BOOL: instance->set(prop.key, atoi(prop.value) == 1); break;
					case Variant::INT: instance->set(prop.key, (int64_t)atoll(prop.value)); break;
					case Variant::FLOAT: instance->set(prop.key, atof(prop.value)); break; //TODO: Locale?
					case Variant::STRING: instance->set(prop.key, prop.value); break;

					case Variant::STRING_NAME: instance->set(prop.key, StringName(prop.value));
					case Variant::NODE_PATH: instance->set(prop.key, NodePath(prop.value)); //TODO: More TrenchBroom focused node path conversion?

					case Variant::VECTOR2: {
						vec2 v = vec2_parse(prop.value);
						instance->set(prop.key, Vector2(v.x, v.y));
						break;
					}
					case Variant::VECTOR2I: {
						vec2 v = vec2_parse(prop.value);
						instance->set(prop.key, Vector2i((int)v.x, (int)v.y));
						break;
					}
					case Variant::VECTOR3: {
						vec3 v = vec3_parse(prop.value);
						instance->set(prop.key, Vector3(v.x, v.y, v.z));
						break;
					}
					case Variant::VECTOR3I: {
						vec3 v = vec3_parse(prop.value);
						instance->set(prop.key, Vector3i((int)v.x, (int)v.y, (int)v.z));
						break;
					}

					case Variant::COLOR: {
						vec3 v = vec3_parse(prop.value);
						instance->set(prop.key, Color(v.x / 255.0f, v.y / 255.0f, v.z / 255.0f));
						break;
					}
				}
			}

			return instance;
		}
	}

	// <ELIM> We don't support all entity types - these errors are meaningless
	// UtilityFunctions::printerr("Path to entity resource could not be resolved: ", classname);
	// </ELIM>
	return nullptr;
}

Node* Builder::build_entity_light(int idx, LMEntity& ent)
{
	auto light = memnew(OmniLight3D());

	light->set_bake_mode(Light3D::BAKE_STATIC);
	light->set_param(Light3D::PARAM_RANGE, ent.get_property_double("range", 10));
	light->set_param(Light3D::PARAM_ENERGY, ent.get_property_double("energy", 1));
	light->set_param(Light3D::PARAM_ATTENUATION, ent.get_property_double("attenuation", 1));
	light->set_param(Light3D::PARAM_SPECULAR, ent.get_property_double("specular", 0.5));
	set_entity_node_common(light, ent);

	vec3 color = ent.get_property_vec3("light_color", { 255, 255, 255 });
	light->set_color(Color(color.x / 255.0f, color.y / 255.0f, color.z / 255.0f));

	m_loader->add_child(light);
	light->set_owner(m_loader->get_owner());

	return light;
}

Node* Builder::build_entity_area(int idx, LMEntity& ent)
{
	Vector3 center = lm_transform(ent.center);

	// Gather surfaces for the area
	LMSurfaceGatherer surf_gather(m_map);
	surf_gather.surface_gatherer_set_entity_index_filter(idx);
	surf_gather.surface_gatherer_run();

	auto& surfs = surf_gather.out_surfaces;
	if (surfs.surface_count == 0) {
		return nullptr;
	}

	// Create the area
	auto area = memnew(Area3D());
	m_loader->add_child(area);
	area->set_owner(m_loader->get_owner());
	area->set_position(center);

	for (int i = 0; i < surfs.surface_count; i++) {
		auto& surf = surfs.surfaces[i];
		if (surf.vertex_count == 0) {
			continue;
		}

		// Create the mesh
		Ref<ArrayMesh> mesh = memnew(ArrayMesh());
		add_surface_to_mesh(mesh, surf);

		// Create collision shape for the area
		add_collider_from_mesh(area, mesh, ColliderShape::Concave);
	}

	return area;
}

void Builder::set_entity_node_common(Node3D* node, LMEntity& ent)
{
	// Target name
	auto targetname = ent.get_property("targetname", nullptr);
	if (targetname != nullptr) {
		node->set_name(targetname);
	}

	// Position
	if (ent.has_property("origin")) {
		Vector3 origin = lm_transform(ent.get_property_vec3("origin"));
		node->set_position(origin);
	}

	// Brush entities shouldn't be rotated as they are already in mesh space
	if (ent.brush_count == 0) {
		// Rotation
		double pitch = 0;
		double yaw = 0;
		double roll = 0;

		if (ent.has_property("angle")) {
			// "angle" is yaw rotation only
			yaw = ent.get_property_double("angle");

		} else if (ent.has_property("angles")) {
			// "angles" is "pitch yaw roll"
			vec3 angles = ent.get_property_vec3("angles");
			pitch = angles.x;
			yaw = angles.y;
			roll = angles.z;

		} else if (ent.has_property("mangle")) {
			vec3 mangle = ent.get_property_vec3("mangle");
			// "mangle" depends on whether the classname starts with "light"
			const char* classname = ent.get_property("classname");
			if (strstr(classname, "light") == classname) {
				// "yaw pitch roll", if classname starts with "light"
				yaw = mangle.x;
				pitch = mangle.y;
				roll = mangle.z;
			} else {
				// "pitch yaw roll", just like "angles"
				pitch = mangle.x;
				yaw = mangle.y;
				roll = mangle.z;
			}
		}

		node->set_rotation(Vector3(
			Math::deg_to_rad(-pitch),
			Math::deg_to_rad(yaw + 180),
			Math::deg_to_rad(-roll)
		));
	}
}

void Builder::set_entity_brush_common(int idx, Node3D* node, LMEntity& ent)
{
	// Position
	Vector3 center = lm_transform(ent.center);
	node->set_position(center);

	// Check what we actually need
	bool need_visual = node->is_class("Node3D");
	ColliderType need_collider = ColliderType::None;
	ColliderShape need_collider_shape = ColliderShape::Concave;

	if (node->is_class("RigidBody3D")) {
		// RigidBody3D requires convex collision meshes
		need_collider = ColliderType::Mesh;
		need_collider_shape = ColliderShape::Convex;

	} else if (node->is_class("Area3D")) {
		// Area3D works best with convex collision meshes
		need_collider = ColliderType::Mesh;
		need_collider_shape = ColliderShape::Convex;

	} else if (node->is_class("CollisionObject3D")) {
		// If it's not a dynamic body, we can just use a concave trimesh collider
		need_collider = ColliderType::Mesh;
		need_collider_shape = ColliderShape::Concave;
	}

	// Stop if we don't need to do anything
	if (!need_visual && need_collider == ColliderType::None) {
		UtilityFunctions::printerr("Brush entity class has no need for visual nor collision: ", node->get_class());
		return;
	}

	build_entity_mesh(idx, ent, node, need_collider, need_collider_shape);
}

Vector3 Builder::lm_transform(const vec3& v)
{
	vec3 sv = vec3_div_double(v, m_loader->m_inverse_scale);
	return Vector3(sv.y, sv.z, sv.x);
}

void Builder::add_collider_from_mesh(Node3D* node, Ref<ArrayMesh>& mesh, ColliderShape colshape)
{
	Ref<Shape3D> mesh_shape;
	switch (colshape) {
	case ColliderShape::Convex: mesh_shape = mesh->create_convex_shape(); break;
	case ColliderShape::Concave: mesh_shape = mesh->create_trimesh_shape(); break;
	}

	if (mesh_shape == nullptr) {
		UtilityFunctions::printerr("Unable to create collider shape from mesh!");
		return;
	}

	auto collision_shape = memnew(CollisionShape3D());
	collision_shape->set_shape(mesh_shape);
	node->add_child(collision_shape, true);
	collision_shape->set_owner(m_loader->get_owner());
}

void Builder::add_surface_to_mesh(Ref<ArrayMesh>& mesh, LMSurface& surf)
{
	// Pre-allocate arrays to avoid growth reallocations (critical for performance)
	PackedVector3Array vertices;
	vertices.resize(surf.vertex_count);

	PackedFloat32Array tangents;
	tangents.resize(surf.vertex_count * 4);

	PackedVector3Array normals;
	normals.resize(surf.vertex_count);

	PackedVector2Array uvs;
	uvs.resize(surf.vertex_count);

	PackedInt32Array indices;
	indices.resize(surf.index_count);

	// Fill pre-allocated arrays (no reallocation overhead)
	for (int k = 0; k < surf.vertex_count; k++) {
		auto& v = surf.vertices[k];

		vertices[k] = lm_transform(v.vertex);
		tangents[k * 4 + 0] = v.tangent.y;
		tangents[k * 4 + 1] = v.tangent.z;
		tangents[k * 4 + 2] = v.tangent.x;
		tangents[k * 4 + 3] = v.tangent.w;
		normals[k] = Vector3(v.normal.y, v.normal.z, v.normal.x);
		uvs[k] = Vector2(v.uv.u, v.uv.v);
	}

	// Fill indices array
	for (int k = 0; k < surf.index_count; k++) {
		indices[k] = surf.indices[k];
	}

	Array arrays;
	arrays.resize(Mesh::ARRAY_MAX);
	arrays[Mesh::ARRAY_VERTEX] = vertices;
	arrays[Mesh::ARRAY_TANGENT] = tangents;
	arrays[Mesh::ARRAY_NORMAL] = normals;
	arrays[Mesh::ARRAY_TEX_UV] = uvs;
	arrays[Mesh::ARRAY_INDEX] = indices;

	// Create mesh
	mesh->add_surface_from_arrays(Mesh::PRIMITIVE_TRIANGLES, arrays);
}

MeshInstance3D* Builder::build_entity_mesh(int idx, LMEntity& ent, Node3D* parent, ColliderType coltype, ColliderShape colshape)
{
	// Create instance name based on entity idx
	String instance_name = String("entity_{0}_geometry").format(Array::make(idx));

	auto mesh_instance = memnew(MeshInstance3D());
	auto material_slot_map = memnew(MaterialSlotMap());

	parent->add_child(mesh_instance);

	// Set the layers that the mesh instance will be rendered in
	mesh_instance->set_layer_mask(m_loader->get_visual_layer_mask());
	mesh_instance->set_owner(m_loader->get_owner());
	mesh_instance->set_name(instance_name);
	mesh_instance->add_child(material_slot_map);

	// Create mesh
	Ref<ArrayMesh> mesh = memnew(ArrayMesh());
	Ref<ArrayMesh> collision_mesh = memnew(ArrayMesh());

	// Give mesh to mesh instance
	mesh_instance->set_mesh(mesh);

	// Prepare material template
	auto material_template = m_loader->get_material_template();
	bool has_material_template = material_template.is_valid();
	VMap<String, Ref<Material>> material_template_map;

	{
		for (int i = 0; i < m_map->texture_count; i++) {
			LMTextureData tex = m_map->textures[i];

			// Create material
			Ref<Material> material;

			// Skip processing a surface when it's using the skip material
			if (tex.name == m_loader->get_skip_texture_name()) {
				continue;
			}

		// Attempt to load material
		material = material_from_name(tex.name);

		if (material == nullptr) {
			// Load texture
			auto res_texture = texture_from_name(tex.name);

			// Create material
			if (res_texture != nullptr) {
				Ref<Material> new_material;

				if (has_material_template) {
					// Duplicate and set texture for material template
					// Only creates one copy per texture; materials are reused using a map
					if (!material_template_map.has(tex.name)) {
						auto material_template_copy = material_template->duplicate();
						material_template_copy->set(m_loader->get_material_texture_path(), res_texture);
						material_template_map.insert(tex.name, material_template_copy);
					}
					new_material = material_template_map[tex.name];
				} else {
					// Generate new material if no template supplied
					Ref<StandardMaterial3D> new_standard_material = memnew(StandardMaterial3D());
					new_standard_material->set_texture(BaseMaterial3D::TEXTURE_ALBEDO, res_texture);
					if (m_loader->m_filter_nearest) {
						new_standard_material->set_texture_filter(BaseMaterial3D::TEXTURE_FILTER_NEAREST);
					}
					new_material = new_standard_material;
				}

				material = new_material;
			}
		}

		// Lookup cached surfaces (pre-computed during load_map)
		const LMSurfaces* surfs = nullptr;
		if (m_surface_cache) {
			surfs = m_surface_cache->get_surfaces(idx, i);
		}
		if (surfs == nullptr || surfs->surface_count == 0) {
			continue;
		}

		for (int j = 0; j < surfs->surface_count; j++) {
			auto& surf = surfs->surfaces[j];
			if (surf.vertex_count == 0) {
				continue;
			}

			// Add surface to collision mesh
			add_surface_to_mesh(collision_mesh, surf);

			// Skip if the texture specifies that we only want collision (invisible walls)
			if (tex.name == m_loader->get_clip_texture_name()) {
				continue;
			}

			// Add surface to visual mesh
			add_surface_to_mesh(mesh, surf);

			// Give mesh material
			const uint64_t surf_idx = mesh->get_surface_count()-1;
			if (material != nullptr) {
				mesh->surface_set_material(surf_idx, material);
			}
			// Map the surface index to the texture name so it can be changed programmatically at runtime
			mesh->surface_set_name(surf_idx, tex.name);
			material_slot_map->add_slot(tex.name);
		}
		}
	}

	// Unwrap UV2's if needed
	if (m_loader->m_lighting_unwrap_uv2) {
		{
			SCOPED_TIMER(LIGHTMAP_UNWRAP);
			mesh->lightmap_unwrap(mesh_instance->get_global_transform(), m_loader->m_lighting_unwrap_texel_size);
			mesh_instance->set_gi_mode(GeometryInstance3D::GI_MODE_STATIC);
		}
	}

	// Create collisions if needed
	if (!m_loader->m_skip_empty_meshes || collision_mesh->get_surface_count() > 0) {
		switch (coltype) {
		case ColliderType::Mesh:
			add_collider_from_mesh(parent, collision_mesh, colshape);
			break;

		case ColliderType::Static:
			StaticBody3D* static_body = memnew(StaticBody3D());
			static_body->set_name(String(mesh_instance->get_name()) + "_col");
			static_body->set_collision_layer(m_loader->get_collision_layer_mask());
			parent->add_child(static_body, true);
			static_body->set_owner(m_loader->get_owner());
			add_collider_from_mesh(static_body, collision_mesh, colshape);
			break;
		}
	}

	// Remove the empty mesh instances if enabled
	if (m_loader->m_skip_empty_meshes && mesh->get_surface_count() == 0) {
		parent->remove_child(mesh_instance);
	}

	return mesh_instance;
}

void Builder::load_and_cache_map_textures()
{
	m_loaded_map_textures.clear();

	// Setup a texture extension list that both Trenchbroom and Godot support
	constexpr int num_extensions = 9;
	constexpr const char* supported_extensions[num_extensions] = { "png", "dds", "tga", "jpg", "jpeg", "bmp", "webp", "exr", "hdr" };

	// Attempt to load and cache textures used by the map
	auto resource_loader = ResourceLoader::get_singleton();
	String tex_path;

	for (int tex_i = 0; tex_i < m_map->texture_count; tex_i++) {
		bool has_loaded_texture = false;
		LMTextureData& tex = m_map->textures[tex_i];

		// Find the texture with a supported extension - stop when it can be loaded
		for (int ext_i = 0; ext_i < num_extensions; ext_i++) {
			tex_path = texture_path(tex.name, supported_extensions[ext_i]);
			if (resource_loader->exists(tex_path, "CompressedTexture2D")) {
				m_loaded_map_textures[tex.name] = resource_loader->load(tex_path);
				has_loaded_texture = true;
				break;
			}
		}

		// <ELIM> Apply texture sizes as soon as they're loaded
		if (has_loaded_texture) {
			auto loaded_texture = m_loaded_map_textures[tex.name];
			tex.width = loaded_texture->get_width();
			tex.height = loaded_texture->get_height();
		}
		else {
			tex.width = 1;
			tex.height = 1;
		}

		// if (!has_loaded_texture && strcmp(tex.name, "__TB_empty") != 0) {
		// 	UtilityFunctions::printerr("Texture cannot be found or is unsupported! - ", m_loader->m_texture_path, tex.name);
		// 	if (m_loader->m_texture_path.is_empty()) {
		// 		UtilityFunctions::printerr("texture_path is empty");
		// 	}
		// }
		// </ELIM>
	}
}

String Builder::texture_path(const char* name, const char* extension)
{
	return m_loader->m_texture_path.path_join(String(name) + "." + extension);
}

String Builder::material_path(const char* name)
{
	auto resource_loader = ResourceLoader::get_singleton();
	auto root_path = m_loader->m_texture_path.path_join(name);

	if (resource_loader->exists(root_path + ".material")) {
		return root_path + ".material";
	} else if (resource_loader->exists(root_path + ".tres")) {
		return root_path + ".tres";
	}

	return "";
}

Ref<Texture2D> Builder::texture_from_name(const char* name)
{
	if (!m_loaded_map_textures.has(name)) {
		return nullptr;
	}
	return VariantCaster<Ref<Texture2D>>::cast(m_loaded_map_textures[name]);
}

Ref<Material> Builder::material_from_name(const char* name)
{
	auto path = material_path(name);

	auto resource_loader = ResourceLoader::get_singleton();
	if (!resource_loader->exists(path)) {
		return nullptr;
	}

	return resource_loader->load(path);
}
