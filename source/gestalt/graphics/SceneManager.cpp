
#include <SceneManger.h>

#include "vk_types.h"

#include <fastgltf/glm_element_traits.hpp>
#include <fastgltf/core.hpp>

#include <glm/gtx/matrix_decompose.hpp>

namespace gestalt {
  namespace application {

    using namespace foundation;

    void SceneManager::init(const graphics::Gpu& gpu,
                            const std::shared_ptr<graphics::ResourceManager>& resource_manager,
                            const std::shared_ptr<Repository>& repository) {
      gpu_ = gpu;
      resource_manager_ = resource_manager;
      repository_ = repository;

      component_factory_->init(resource_manager_, repository);
      asset_loader_->init(resource_manager_, component_factory_, repository_);

      resource_manager_->init_default_data();

      resource_manager_->load_and_process_cubemap(R"(..\..\assets\san_giuseppe_bridge_4k.hdr)");

      // TODO add lights from gltf to resource manager

      const size_t camera_index = repository_->cameras.add(CameraData{});
      repository_->camera_components.add(component_factory_->create_entity_node().first,
                                         CameraComponent{camera_index});

      component_factory_->create_directional_light(
          glm::vec3(1.f, 0.957f, 0.917f), 5.f, glm::vec3(-0.216, 0.941, -0.257), get_root_entity());
      component_factory_->create_point_light(glm::vec3(1.0f), 5.0f, glm ::vec3(0.0, 6.0, 0.0),
                                             get_root_entity());

      light_system_ = std::make_unique<LightSystem>();
      light_system_->init(gpu, resource_manager, repository);
      transform_system_ = std::make_unique<TransformSystem>();
      transform_system_->init(gpu, resource_manager, repository);
      render_system_ = std::make_unique<RenderSystem>();
      render_system_->init(gpu, resource_manager, repository);
    }

    void SceneManager::load_scene(const std::string& path) {
      size_t mesh_offset = repository_->meshes.size();
      const auto nodes = asset_loader_->load_scene_from_gltf(path);
      build_scene_graph(nodes, mesh_offset);
    }

    void SceneManager::create_entities(std::vector<fastgltf::Node> nodes,
                                       const size_t& mesh_offset) {
      for (fastgltf::Node& node : nodes) {
        if (node.lightIndex.has_value()) {
          // TODO
        }

        const auto [entity, node_component]
            = component_factory_->create_entity_node(std::string(node.name));

        if (node.meshIndex.has_value()) {
          component_factory_->add_mesh_component(entity, mesh_offset + *node.meshIndex);
        }

        if (std::holds_alternative<fastgltf::TRS>(node.transform)) {
          const auto& trs = std::get<fastgltf::TRS>(node.transform);

          glm::vec3 translation(trs.translation[0], trs.translation[1], trs.translation[2]);
          glm::quat rotation(trs.rotation[3], trs.rotation[0], trs.rotation[1], trs.rotation[2]);
          float scale = (trs.scale[0] + trs.scale[1] + trs.scale[2])
                        / 3.f;  // TODO handle non-uniform scale

          component_factory_->update_transform_component(entity, translation, rotation, scale);
        }
      }
    }

    void SceneManager::build_hierarchy(std::vector<fastgltf::Node> nodes,
                                       const size_t& node_offset) {
      for (int i = 0; i < nodes.size(); i++) {
        fastgltf::Node& node = nodes[i];
        entity parent_entity = node_offset + i;
        auto& scene_object = repository_->scene_graph.get(parent_entity).value().get();

        for (auto& c : node.children) {
          entity child_entity = node_offset + c;
          auto& child = repository_->scene_graph.get(child_entity).value().get();
          scene_object.children.push_back(child_entity);
          child.parent = parent_entity;
        }
      }
    }

    void SceneManager::link_orphans_to_root() {
      for (auto& [entity, node] : repository_->scene_graph.components()) {
        if (entity == get_root_entity()) {
          continue;
        }

        if (node.parent == invalid_entity) {
          get_root_node().children.push_back(entity);
          node.parent = get_root_entity();
        }
      }
    }

    void SceneManager::build_scene_graph(const std::vector<fastgltf::Node>& nodes,
                                         const size_t& mesh_offset) {
      const size_t node_offset = repository_->scene_graph.size();

      create_entities(nodes, mesh_offset);
      build_hierarchy(nodes, node_offset);
      link_orphans_to_root();
    }

    void SceneManager::cleanup() {}

    entity ComponentArchetypeFactory::create_entity() { return next_entity_id_++; }

    std::pair<entity, std::reference_wrapper<NodeComponent>>
    ComponentArchetypeFactory::create_entity_node(std::string node_name) {
      const entity new_entity = create_entity();

      if (node_name.empty()) {
        node_name = "entity_" + std::to_string(new_entity);
      }
      const NodeComponent node = {
          .name = node_name + "_" + std::to_string(new_entity),
      };

      create_transform_component(new_entity, glm::vec3(0));
      repository_->scene_graph.add(new_entity, node);

      fmt::print("created entity {}\n", new_entity);

      return std::make_pair(new_entity, std::ref(repository_->scene_graph.get(new_entity)->get()));
    }

    void ComponentArchetypeFactory::create_transform_component(const entity entity,
                                                               const glm::vec3& position,
                                                               const glm::quat& rotation,
                                                               const float& scale) const {
      repository_->transform_components.add(entity, TransformComponent(position, rotation, scale));
      auto& transform = repository_->transform_components.get(entity)->get();
      transform.matrix = repository_->model_matrices.add(glm::mat4(1.0));
    }

    void ComponentArchetypeFactory::update_transform_component(const entity entity,
                                                               const glm::vec3& position,
                                                               const glm::quat& rotation,
                                                               const float& scale) {
      const std::optional<std::reference_wrapper<TransformComponent>> transform_optional
          = repository_->transform_components.get(entity);

      if (transform_optional.has_value()) {
        auto& transform = transform_optional->get();

        transform.position = position;
        transform.rotation = rotation;
        transform.scale = scale;
        transform.is_dirty = true;
      }
    }

    NodeComponent& SceneManager::get_root_node() {
      return repository_->scene_graph.get(get_root_entity()).value();
    }

    void ComponentArchetypeFactory::add_mesh_component(const entity entity,
                                                       const size_t mesh_index) {
      assert(entity != invalid_entity);

      repository_->mesh_components.add(entity, MeshComponent{mesh_index});
    }

    void ComponentArchetypeFactory::add_camera_component(const entity entity,
                                                         const CameraComponent& camera) {
      assert(entity != invalid_entity);

      repository_->camera_components.add(entity, camera);
    }

    entity ComponentArchetypeFactory::create_directional_light(const glm::vec3& color,
                                                               const float intensity,
                                                               const glm::vec3& direction,
                                                               entity parent) {
      auto [entity, node] = create_entity_node("directional_light");
      link_entity_to_parent(entity, parent);

      auto& transform = repository_->transform_components.get(entity).value().get();
      transform.rotation = glm::quat(glm::lookAt(glm::vec3(0), direction, glm::vec3(0, 1, 0)));

      const LightComponent light{
          .type = LightType::kDirectional,
          .color = color,
          .intensity = intensity,
      };

      repository_->light_components.add(entity, light);
      const size_t matrix_id = repository_->light_view_projections.add(glm::mat4(1.0));
      repository_->light_components.get(entity).value().get().light_view_projections.push_back(
          matrix_id);

      return entity;
    }

    entity ComponentArchetypeFactory::create_spot_light(
        const glm::vec3& color, const float intensity, const glm::vec3& direction,
        const glm::vec3& position, const float innerCone, const float outerCone, entity parent) {
      auto [entity, node] = create_entity_node("spot_light");
      link_entity_to_parent(entity, parent);

      auto& transform = repository_->transform_components.get(entity).value().get();
      transform.rotation = glm::quat(glm::lookAt(glm::vec3(0), direction, glm::vec3(0, 1, 0)));
      transform.position = position;

      const LightComponent light{
          .type = LightType::kSpot,
          .color = color,
          .intensity = intensity,
          .inner_cone = innerCone,
          .outer_cone = outerCone,
      };

      repository_->light_components.add(entity, light);
      const size_t matrix_id = repository_->light_view_projections.add(glm::mat4(1.0));
      repository_->light_components.get(entity).value().get().light_view_projections.push_back(
          matrix_id);

      return entity;
    }

    entity ComponentArchetypeFactory::create_point_light(const glm::vec3& color,
                                                         const float intensity,
                                                         const glm::vec3& position, entity parent) {
      auto [entity, node] = create_entity_node("point_light");
      link_entity_to_parent(entity, parent);

      auto& transform = repository_->transform_components.get(entity).value().get();
      transform.position = position;

      const LightComponent light{
          .type = LightType::kPoint,
          .color = color,
          .intensity = intensity,
      };

      repository_->light_components.add(entity, light);
      for (int i = 0; i < 6; i++) {
        const size_t matrix_id = repository_->light_view_projections.add(glm::mat4(1.0));
        repository_->light_components.get(entity).value().get().light_view_projections.push_back(
            matrix_id);
      }
      return entity;
    }

    void ComponentArchetypeFactory::link_entity_to_parent(const entity child, const entity parent) {
      if (child == parent) {
        return;
      }

      const auto& parent_node = repository_->scene_graph.get(parent);
      const auto& child_node = repository_->scene_graph.get(child);

      if (parent_node.has_value() && child_node.has_value()) {
        parent_node->get().children.push_back(child);
        child_node->get().parent = parent;
      }
    }

    void SceneManager::add_to_root(entity entity, NodeComponent& node) {
      assert(entity != invalid_entity);
      get_root_node().children.push_back(entity);
      node.parent = get_root_entity();
    }

    void SceneManager::update_scene() {
      if (!scene_path_.empty()) {
        load_scene(scene_path_);
        scene_path_.clear();
      }

      light_system_->update();
      transform_system_->update();
      render_system_->update();
    }

    void SceneManager::request_scene(const std::string& path) { scene_path_ = path; }

    void ComponentArchetypeFactory::init(const std::shared_ptr<graphics::ResourceManager>& resource_manager,
                                         const std::shared_ptr<Repository>& repository) {
      resource_manager_ = resource_manager;
      repository_ = repository;

      auto [entity, root_node] = create_entity_node();
      root_node.get().name = "root";
    }

  }  // namespace application
}  // namespace gestalt