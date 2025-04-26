
#include "EntityComponentSystem.hpp"

#include "AnimationSystem.hpp"
#include "AudioSystem.hpp"
#include "CameraSystem.hpp"
#include "LightSystem.hpp"
#include "MaterialSystem.hpp"
#include "MeshSystem.hpp"
#include "PhysicSystem.hpp"
#include "RayTracingSystem.hpp"
#include "TransformSystem.hpp"
#include "UserInput.hpp"
#include "Events/EventBus.hpp"
#include "Resource Loading/AssetLoader.hpp"

namespace gestalt::application {

  EntityComponentSystem::EntityComponentSystem(IGpu& gpu, IResourceAllocator& resource_allocator,
                                               Repository& repository,
                                               EventBus& event_bus, FrameProvider& frame)
      : gpu_(gpu),
        repository_(repository),
        event_bus_(event_bus),
        component_factory_(repository, event_bus_),
        asset_loader_(resource_allocator, repository, component_factory_),
        material_system_(gpu_, resource_allocator, repository_, frame),
        camera_system_(gpu_, resource_allocator, repository_, frame, event_bus_),
        light_system_(gpu_, resource_allocator, repository_, event_bus_, frame),
        transform_system_(repository_, event_bus_),
        animation_system_(repository_, event_bus_),
        mesh_system_(gpu_, resource_allocator, repository_, frame),
        audio_system_(),
        physics_system_(gpu_, resource_allocator, repository_, frame, event_bus_),
        raytracing_system_(gpu_, resource_allocator, repository_, frame) {
    if (const std::string initial_scene = getInitialScene(); !initial_scene.empty()) {
      request_scene(std::filesystem::current_path() / "../../assets" / initial_scene);
    }
    component_factory_.create_directional_light(glm::vec3(1.f, 0.957f, 0.917f), 683.f,
                                                glm::vec3(0.216, -0.941, 0.257));
    component_factory_.create_point_light(glm::vec3(1.0f), 5.0f, glm ::vec3(0.0, 6.0, 0.0), 100.f);

    auto [main_cam, main_cam_node] = component_factory_.create_entity("Editor Camera");
    component_factory_.link_entity_to_parent(main_cam, root_entity_);
    component_factory_.add_free_fly_camera(glm::vec3(7, 1.8, -7), glm::vec3(0, 0, 0),
                                           glm::vec3(0, 1, 0), main_cam);

    // TODO: find out how to create initial TLAS and update it instead of this hack
    UserInput movement;
    update_scene(0.f, movement, 0.f);
  }

  EntityComponentSystem::~EntityComponentSystem() = default;

  void EntityComponentSystem::set_active_camera(const Entity camera) {
    camera_system_.set_active_camera(camera);
  }

  Entity EntityComponentSystem::get_active_camera() const {
    return camera_system_.get_active_camera();
  }

  void EntityComponentSystem::add_to_root(Entity entity, NodeComponent& node) {
    if (entity != invalid_entity) {
      throw std::runtime_error("Entity has invalid id");
    }
    repository_.scene_graph.find_mutable(get_root_entity())->children.push_back(entity);
    node.parent = get_root_entity();
  }

  void EntityComponentSystem::update_scene(const float delta_time, const UserInput& movement,
                                           const float aspect) {
    if (!scene_path_.empty()) {
      asset_loader_.load_scene_from_gltf(scene_path_);
      scene_path_.clear();
    }

    material_system_.update();
    camera_system_.update(delta_time, movement, aspect);
    light_system_.update();
    transform_system_.update();
    mesh_system_.update();
    animation_system_.update(delta_time);
    audio_system_.update();
    raytracing_system_.update();

    event_bus_.poll();

    scene_path_.clear();
  }

  void EntityComponentSystem::request_scene(const std::filesystem::path& file_path) {
    scene_path_ = file_path;
  }

}  // namespace gestalt::application