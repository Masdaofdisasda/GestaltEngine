﻿
#include "ECSManager.hpp"

#include "AnimationSystem.hpp"
#include "CameraSystem.hpp"
#include "LightSystem.hpp"
#include "MaterialSystem.hpp"
#include "MeshSystem.hpp"
#include "PhysicSystem.hpp"
#include "TransformSystem.hpp"
#include "Interface/IResourceAllocator.hpp"
#include "Interface/IResourceManager.hpp"
#include "Resource Loading/AssetLoader.hpp"

namespace gestalt::application {

    void ECSManager::init(IGpu* gpu, IResourceManager* resource_manager, IResourceAllocator* resource_allocator,
                           IDescriptorLayoutBuilder* builder, Repository* repository,
                           FrameProvider* frame) {
    gpu_ = gpu;
    resource_manager_ = resource_manager;
    resource_allocator_ = resource_allocator;
    repository_ = repository;

      asset_loader_ = std::make_unique<AssetLoader>();
    component_factory_ = std::make_unique<ComponentFactory>();


    component_factory_->init(resource_manager_, repository_);
    asset_loader_->init(resource_manager_, component_factory_.get(), repository_);

    component_factory_->create_directional_light(
        glm::vec3(1.f, 0.957f, 0.917f), 5.f, glm::vec3(-0.216, 0.941, -0.257));
    component_factory_->create_point_light(glm::vec3(1.0f), 5.0f, glm ::vec3(0.0, 6.0, 0.0), 100.f);

    auto [main_cam, main_cam_node] = component_factory_->create_entity("Editor Camera");
    component_factory_->link_entity_to_parent(main_cam, root_entity_);
    component_factory_->add_free_fly_camera(glm::vec3(7, 1.8, -7), glm::vec3(0, 0, 0),
                                            glm::vec3(0, 1, 0), main_cam);


    auto [player, player_node] = component_factory_->create_entity("Player");
    component_factory_->link_entity_to_parent(player, root_entity_);
    component_factory_->add_first_person_camera(glm::vec3(7, 1.8, -7), player);
    repository_->transform_components[player].position = glm::vec3(0, 20.f, 0);
    component_factory_->create_physics_component(player, DYNAMIC, CapsuleCollider{.9f, 1.8f});

      auto [floor, floor_node] = component_factory_->create_entity("Floor");
      component_factory_->link_entity_to_parent(floor, root_entity_);
      component_factory_->create_physics_component(floor, STATIC,
                                                   BoxCollider{glm::vec3(1000.f, 0.1f, 1000.f)});

    material_system_ = std::make_unique<MaterialSystem>();
    material_system_->init(gpu_, resource_manager_, resource_allocator, builder, repository_,frame);
    light_system_ = std::make_unique<LightSystem>();
    light_system_->init(gpu_, resource_manager_, resource_allocator, builder, repository_,
                        frame);
    camera_system_ = std::make_unique<CameraSystem>();
    camera_system_->init(gpu_, resource_manager_, resource_allocator, builder, repository_,
                         frame);
    transform_system_ = std::make_unique<TransformSystem>();
    transform_system_->init(gpu_, resource_manager_, resource_allocator, builder, repository_,
                            frame);
    animation_system_ = std::make_unique<AnimationSystem>();
    animation_system_->init(gpu_, resource_manager_, resource_allocator, builder, repository_,
                            frame);
    mesh_system_ = std::make_unique<MeshSystem>();
    mesh_system_->init(gpu_, resource_manager_, resource_allocator, builder, repository_,
                       frame);
    physics_system_ = std::make_unique<PhysicSystem>();
    physics_system_->init(gpu_, resource_manager_, resource_allocator, builder, repository_, frame);
  }

  void ECSManager::set_active_camera(const Entity camera) const {
      camera_system_->set_active_camera(camera);
    }

  Entity ECSManager::get_active_camera() const { return camera_system_->get_active_camera(); }

    void ECSManager::cleanup() const {
      physics_system_->cleanup();
      mesh_system_->cleanup();
      transform_system_->cleanup();
      animation_system_->cleanup();
      camera_system_->cleanup();
      light_system_->cleanup();
      material_system_->cleanup();

      repository_->transform_components.clear();
      repository_->camera_components.clear();
      repository_->light_components.clear();
      repository_->mesh_components.clear();
      repository_->scene_graph.clear();
    }

    NodeComponent& ECSManager::get_root_node() {
      return repository_->scene_graph.get(get_root_entity()).value();
    }

    void ECSManager::add_to_root(Entity entity, NodeComponent& node) {
      assert(entity != invalid_entity);
      get_root_node().children.push_back(entity);
      node.parent = get_root_entity();
    }

    void ECSManager::update_scene(const float delta_time, const UserInput& movement, const float aspect) {
      if (!scene_path_.empty()) {
        asset_loader_->load_scene_from_gltf(scene_path_);
        scene_path_.clear();
      }
      resource_manager_->flush_loader();

      physics_system_->update(delta_time, movement, aspect);
      material_system_->update(delta_time, movement, aspect);
      light_system_->update(delta_time, movement, aspect);
      camera_system_->update(delta_time, movement, aspect);
      transform_system_->update(delta_time, movement, aspect);
      mesh_system_->update(delta_time, movement, aspect);
      animation_system_->update(delta_time, movement, aspect);
    }

    void ECSManager::request_scene(const std::filesystem::path& file_path) {
      scene_path_ = file_path;
    }

}  // namespace gestalt