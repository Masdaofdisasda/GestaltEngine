#include <EngineConfig.hpp>

#include "RenderConfig.hpp"
#include "SceneSystem.hpp"
#include "descriptor.hpp"

namespace gestalt::application {

    void CameraSystem::prepare() {
      free_fly_camera_ = std::make_unique<FreeFlyCamera>();
      free_fly_camera_->init(glm::vec3(7, 1.8, -7), glm::vec3(0, 0, 0), glm::vec3(0, 1, 0));
      active_camera_ = std::make_unique<Camera>();
      active_camera_->init(*free_fly_camera_);

      const auto& per_frame_data_buffers = repository_->per_frame_data_buffers;

      descriptor_layout_builder_->clear();
      const auto descriptor_layout
          = descriptor_layout_builder_->add_binding(
                0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                              VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_MESH_BIT_EXT | VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_COMPUTE_BIT
                                  | VK_SHADER_STAGE_FRAGMENT_BIT)
                .build(gpu_->getDevice(),
                       VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);

      for (int i = 0; i < getFramesInFlight(); ++i) {
        per_frame_data_buffers->uniform_buffers[i] = resource_manager_->create_buffer(
            sizeof(PerFrameData),
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
            VMA_MEMORY_USAGE_CPU_TO_GPU, "perFrameBuffer");
        per_frame_data_buffers->descriptor_buffers[i] = resource_manager_->create_descriptor_buffer(
            descriptor_layout, 1,
            0);

        VkBufferDeviceAddressInfo deviceAdressInfo{
            .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
            .buffer = per_frame_data_buffers->uniform_buffers[i]->buffer};
        VkDeviceAddress per_frame_data_buffer_address
            = vkGetBufferDeviceAddress(gpu_->getDevice(), &deviceAdressInfo);

        per_frame_data_buffers->descriptor_buffers[i]->
                               write_buffer(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                                             per_frame_data_buffer_address, sizeof(PerFrameData))
                               .update();
      }

      vkDestroyDescriptorSetLayout(gpu_->getDevice(), descriptor_layout, nullptr);

    }

    void CameraSystem::update_cameras(const float delta_time, const Movement& movement, float aspect) {
      aspect_ = aspect;
              active_camera_->update(delta_time, movement);
    }

    glm::vec4 normalizePlane(glm::vec4 p) { return p / length(glm::vec3(p)); }

  
    glm::mat4 perspectiveProjection(float fovY, float aspectWbyH, float zNear) {
      float f = 1.0f / tanf(fovY / 2.0f);
      return glm::mat4(f / aspectWbyH, 0.0f, 0.0f, 0.0f, 0.0f, f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f,
                  0.0f, 0.0f, zNear, 0.0f);
    }

    void CameraSystem::update() {
      const glm::mat4 view = active_camera_->get_view_matrix();

      // camera projection
      //glm::mat4 projection = perspectiveProjection(glm::radians(fov_), aspect_, near_plane_);
      glm::mat4 projection = glm::perspective(glm::radians(fov_), aspect_, near_plane_, far_plane_);
      glm::mat4 projection_t = transpose(projection);

      // invert the Y direction on projection matrix so that we are more similar
      // to opengl and gltf axis
      projection[1][1] *= -1;

      auto& camera = repository_->cameras.get(0);  // assume this as the main camera for now
      camera.view_matrix = view;
      camera.projection_matrix = projection;

      const auto frame = frame_->get_current_frame_index();

      auto& buffers = repository_->per_frame_data_buffers;
      buffers->data.view = camera.view_matrix;  // is the camera object actually needed?
      buffers->data.proj = camera.projection_matrix;
      buffers->data.inv_view = inverse(camera.view_matrix);
      buffers->data.inv_viewProj = inverse(camera.projection_matrix * camera.view_matrix);
      buffers->data.P00 = projection_t[0][0];
      buffers->data.P11 = projection_t[1][1];

      if (!buffers->freezeCullCamera) {

        buffers->data.cullView = camera.view_matrix;
        buffers->data.cullProj = camera.projection_matrix;

        buffers->data.znear = near_plane_;
        buffers->data.zfar = far_plane_;

        buffers->data.frustum[0] = normalizePlane(projection_t[3] + projection_t[0]);
        buffers->data.frustum[1] = normalizePlane(projection_t[3] - projection_t[0]);
        buffers->data.frustum[2] = normalizePlane(projection_t[3] + projection_t[1]);
        buffers->data.frustum[3] = normalizePlane(projection_t[3] - projection_t[1]);
        buffers->data.frustum[4] = normalizePlane(projection_t[3] + projection_t[2]);
        buffers->data.frustum[5] = normalizePlane(projection_t[3] - projection_t[2]);
      }

      void* mapped_data;
      const VmaAllocation allocation = buffers->uniform_buffers[frame]->allocation;
      VK_CHECK(vmaMapMemory(gpu_->getAllocator(), allocation, &mapped_data));
      const auto scene_uniform_data = static_cast<PerFrameData*>(mapped_data);
      *scene_uniform_data = buffers->data;
      vmaUnmapMemory(gpu_->getAllocator(), buffers->uniform_buffers[frame]->allocation);

      //TODO
      meshlet_push_constants.pyramidWidth = 0;
      meshlet_push_constants.pyramidHeight = 0;
      meshlet_push_constants.clusterOcclusionEnabled = 0;
    }

    void CameraSystem::cleanup() {
      repository_->cameras.clear();

      const auto& buffers = repository_->per_frame_data_buffers;

      for (int i = 0; i < 2; ++i) {
        resource_manager_->destroy_descriptor_buffer(buffers->descriptor_buffers[i]);
        resource_manager_->destroy_buffer(buffers->uniform_buffers[i]);
      }

    }

}  // namespace gestalt