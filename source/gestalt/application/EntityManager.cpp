
#include "EntityManager.hpp"

#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/core.hpp>

#include <glm/gtx/matrix_decompose.hpp>

#include "Interface/IResourceManager.hpp"

namespace gestalt::application {

    void EntityManager::init(IGpu* gpu, IResourceManager* resource_manager,
                           IDescriptorLayoutBuilder* builder, Repository* repository,
                           FrameProvider* frame) {
    gpu_ = gpu;
    resource_manager_ = resource_manager;
    repository_ = repository;

    component_factory_->init(resource_manager_, repository_);
    asset_loader_->init(resource_manager_, component_factory_.get(), repository_);

    component_factory_->create_directional_light(
        glm::vec3(1.f, 0.957f, 0.917f), 5.f, glm::vec3(-0.216, 0.941, -0.257), get_root_entity());
    component_factory_->create_point_light(glm::vec3(1.0f), 5.0f, glm ::vec3(0.0, 6.0, 0.0), 100.f,
                                           get_root_entity());

    auto [main_cam, main_cam_node] = component_factory_->create_entity("Editor Camera");
    component_factory_->link_entity_to_parent(main_cam, root_entity_);
    component_factory_->add_free_fly_camera(glm::vec3(7, 1.8, -7), glm::vec3(0, 0, 0),
                                            glm::vec3(0, 1, 0), main_cam);

    auto [player, player_node] = component_factory_->create_entity("Player");
    component_factory_->link_entity_to_parent(player, root_entity_);
    component_factory_->add_first_person_camera(glm::vec3(7, 1.8, -7), player);

    material_system_ = std::make_unique<MaterialSystem>();
    material_system_->init(gpu_, resource_manager_, builder, repository_,
                           notification_manager_.get(), frame);
    light_system_ = std::make_unique<LightSystem>();
    light_system_->init(gpu_, resource_manager_, builder, repository_, notification_manager_.get(),
                        frame);
    camera_system_ = std::make_unique<CameraSystem>();
    camera_system_->init(gpu_, resource_manager_, builder, repository_, notification_manager_.get(),
                         frame);
    transform_system_ = std::make_unique<TransformSystem>();
    transform_system_->init(gpu_, resource_manager_, builder, repository_,
                            notification_manager_.get(), frame);
    mesh_system_ = std::make_unique<MeshSystem>();
    mesh_system_->init(gpu_, resource_manager_, builder, repository_, notification_manager_.get(),
                       frame);
    physics_system_ = std::make_unique<PhysicSystem>();
    physics_system_->init(gpu_, resource_manager_, builder, repository_,
                          notification_manager_.get(), frame);
  }

    void EntityManager::load_scene(const std::string& path) {
      asset_loader_->load_scene_from_gltf(path);
    }

    void EntityManager::cleanup() const {
      physics_system_->cleanup();
      mesh_system_->cleanup();
      transform_system_->cleanup();
      camera_system_->cleanup();
      light_system_->cleanup();
      material_system_->cleanup();

      repository_->transform_components.clear();
      repository_->camera_components.clear();
      repository_->light_components.clear();
      repository_->mesh_components.clear();
      repository_->scene_graph.clear();
    }

    NodeComponent& EntityManager::get_root_node() {
      return repository_->scene_graph.get(get_root_entity()).value();
    }

    void EntityManager::add_to_root(Entity entity, NodeComponent& node) {
      assert(entity != invalid_entity);
      get_root_node().children.push_back(entity);
      node.parent = get_root_entity();
    }

    void EntityManager::update_scene(const float delta_time, const UserInput& movement, const float aspect) {
      if (!scene_path_.empty()) {
        load_scene(scene_path_);
        scene_path_.clear();
      }
      resource_manager_->flush_loader();

      physics_system_->update(delta_time);
      material_system_->update();
      light_system_->update();
      camera_system_->update_cameras(delta_time, movement, aspect);
      camera_system_->update();
      transform_system_->update();
      mesh_system_->update();
    }

    void EntityManager::request_scene(const std::string& path) { scene_path_ = path; }

}  // namespace gestalt