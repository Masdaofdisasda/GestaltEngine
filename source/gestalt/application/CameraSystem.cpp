#include <EngineConfig.hpp>

#include "RenderConfig.hpp"
#include "SceneSystem.hpp"

namespace gestalt::application {

    void CameraSystem::prepare() {
      free_fly_camera_ = std::make_unique<FreeFlyCamera>();
      free_fly_camera_->init(glm::vec3(7, 1.8, -7), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
      active_camera_ = std::make_unique<Camera>();
      active_camera_->init(*free_fly_camera_);

      PerFrameDataBuffers per_frame_data_buffers{};

      descriptor_layout_builder_->clear();
      per_frame_data_buffers.descriptor_layout
          = descriptor_layout_builder_->add_binding(
                0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                             VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT)
                .build(gpu_->getDevice());

      for (int i = 0; i < getFramesInFlight(); ++i) {
        per_frame_data_buffers.uniform_buffers[i] = resource_manager_->create_buffer(
            sizeof(PerFrameData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
        per_frame_data_buffers.descriptor_sets[i] = resource_manager_->get_descriptor_pool()->allocate(
            gpu_->getDevice(), per_frame_data_buffers.descriptor_layout);

        writer_->clear();
        writer_->write_buffer(0, per_frame_data_buffers.uniform_buffers[i].buffer,
                             sizeof(PerFrameData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        writer_->update_set(gpu_->getDevice(), per_frame_data_buffers.descriptor_sets[i]);
      }

      repository_->register_buffer(per_frame_data_buffers);
    }

    void CameraSystem::update_cameras(const float delta_time, const Movement& movement, float aspect) {
      aspect_ = aspect;
              active_camera_->update(delta_time, movement);
    }

    glm::vec4 normalizePlane(glm::vec4 p) { return p / length(glm::vec3(p)); }

    void CameraSystem::update() {
      glm::mat4 view = active_camera_->get_view_matrix();

      // camera projection
      glm::mat4 projection = glm::perspective(glm::radians(70.f), aspect_, 0.1f, 1000.f);
      glm::mat4 projection_t = transpose(projection);

      // invert the Y direction on projection matrix so that we are more similar
      // to opengl and gltf axis
      projection[1][1] *= -1;

      auto& camera = repository_->cameras.get(0);  // assume this as the main camera for now
      camera.view_matrix = view;
      camera.projection_matrix = projection;

      const auto frame = current_frame_index;

      auto& buffers = repository_->get_buffer<PerFrameDataBuffers>();
      buffers.data.view = camera.view_matrix; // is the camera object actually needed?
      buffers.data.proj = camera.projection_matrix;
      buffers.data.viewproj = camera.projection_matrix * camera.view_matrix;
      buffers.data.inv_viewproj = inverse(camera.projection_matrix * camera.view_matrix);

      void* mapped_data;
      const VmaAllocation allocation = buffers.uniform_buffers[frame].allocation;
      VK_CHECK(vmaMapMemory(gpu_->getAllocator(), allocation, &mapped_data));
      const auto scene_uniform_data = static_cast<PerFrameData*>(mapped_data);
      *scene_uniform_data = buffers.data;
      vmaUnmapMemory(gpu_->getAllocator(), buffers.uniform_buffers[frame].allocation);

      meshlet_push_constants.viewProjection = buffers.data.viewproj;
      meshlet_push_constants.screenWidth = static_cast<float32>(getResolutionWidth());
      meshlet_push_constants.screenHeight = static_cast<float32>(getResolutionHeight());
      meshlet_push_constants.znear = 0.1f;
      meshlet_push_constants.zfar = 1000.0f;
      const glm::vec4 frustumX = normalizePlane(projection_t[3] + projection_t[0]);
      const glm::vec4 frustumY = normalizePlane(projection_t[3] + projection_t[1]);
      meshlet_push_constants.frustum[0] = frustumX.x;
      meshlet_push_constants.frustum[1] = frustumY.y;
      meshlet_push_constants.frustum[2] = frustumX.y;
      meshlet_push_constants.frustum[3] = frustumY.y;
      //TODO
      meshlet_push_constants.pyramidWidth = 0;
      meshlet_push_constants.pyramidHeight = 0;
      meshlet_push_constants.clusterOcclusionEnabled = 0;
    }

    void CameraSystem::cleanup() {
      repository_->cameras.clear();

      const auto& buffers = repository_->get_buffer<PerFrameDataBuffers>();

      for (int i = 0; i < 2; ++i) {
        resource_manager_->destroy_buffer(buffers.uniform_buffers[i]);
      }

      if (buffers.descriptor_layout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(gpu_->getDevice(), buffers.descriptor_layout, nullptr);
      }

      repository_->deregister_buffer<PerFrameDataBuffers>();
    }

}  // namespace gestalt