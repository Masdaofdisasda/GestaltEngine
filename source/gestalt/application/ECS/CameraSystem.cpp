#include "CameraSystem.hpp"

#include <EngineConfiguration.hpp>

#include "RenderConfig.hpp"
#include "VulkanCheck.hpp"

#include "FrameProvider.hpp"
#include "VulkanTypes.hpp"
#include "Cameras/AnimationCamera.hpp"
#include "Cameras/FirstPersonCamera.hpp"
#include "Cameras/FreeFlyCamera.hpp"
#include "Cameras/OrbitCamera.hpp"
#include "Interface/IDescriptorLayoutBuilder.hpp"
#include "Interface/IGpu.hpp"
#include "Interface/IResourceManager.hpp"

namespace gestalt::application {

    void CameraSystem::prepare() {

    const auto& per_frame_data_buffers = repository_->per_frame_data_buffers;

    descriptor_layout_builder_->clear();
    const auto descriptor_layout
        = descriptor_layout_builder_
              ->add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                            VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_MESH_BIT_EXT
                                | VK_SHADER_STAGE_TASK_BIT_EXT | VK_SHADER_STAGE_COMPUTE_BIT
                                | VK_SHADER_STAGE_FRAGMENT_BIT)
              .build(gpu_->getDevice(), VK_DESCRIPTOR_SET_LAYOUT_CREATE_DESCRIPTOR_BUFFER_BIT_EXT);

    for (int i = 0; i < getFramesInFlight(); ++i) {
      per_frame_data_buffers->uniform_buffers[i] = resource_manager_->create_buffer(
          sizeof(PerFrameData),
          VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
          VMA_MEMORY_USAGE_CPU_TO_GPU, "perFrameBuffer");
      per_frame_data_buffers->descriptor_buffers[i]
          = resource_manager_->create_descriptor_buffer(descriptor_layout, 1, 0);

      VkBufferDeviceAddressInfo deviceAdressInfo{
          .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
          .buffer = per_frame_data_buffers->uniform_buffers[i]->buffer};
      const VkDeviceAddress per_frame_data_buffer_address
          = vkGetBufferDeviceAddress(gpu_->getDevice(), &deviceAdressInfo);

      per_frame_data_buffers->descriptor_buffers[i]
          ->write_buffer(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, per_frame_data_buffer_address,
                         sizeof(PerFrameData))
          .update();
    }

    vkDestroyDescriptorSetLayout(gpu_->getDevice(), descriptor_layout, nullptr);
  }

    inline glm::vec4 NormalizePlane(glm::vec4 p) { return p / length(glm::vec3(p)); }

  
    glm::mat4 PerspectiveProjection(float fovY, float aspectWbyH, float zNear) {
      const float32 f = 1.0f / tanf(fovY / 2.0f);
      return glm::mat4{f / aspectWbyH, 0.0f, 0.0f, 0.0f, 0.0f, f,    0.0f,  0.0f,
                       0.0f,           0.0f, 0.0f, 1.0f, 0.0f, 0.0f, zNear, 0.0f};
    }

    void CameraSystem::update(const float delta_time, const UserInput& movement, float aspect) {
      aspect_ratio_ = aspect;

      auto& camera_component = repository_->camera_components[active_camera_];
      auto& transform_component = repository_->transform_components[active_camera_];
      std::visit(
          [&]<typename CameraDataType>(CameraDataType& camera_data) {
            using T = std::decay_t<CameraDataType>;
            if constexpr (std::is_same_v<T, FreeFlyCameraData>) {
              FreeFlyCamera::update(delta_time, movement, camera_data);
              transform_component.rotation = camera_data.orientation;
              transform_component.position = camera_data.position;
            } else if constexpr (std::is_same_v<T, OrbitCameraData>) {
              OrbitCamera::update(delta_time, movement, camera_data);
              transform_component.rotation = camera_data.orientation;
              transform_component.position = camera_data.position;
            } else if constexpr (std::is_same_v<T, FirstPersonCameraData>) {
              camera_data.set_position(transform_component.position);
              FirstPersonCamera::update(delta_time, movement, camera_data);
              transform_component.rotation = camera_data.orientation;
            } else if constexpr (std::is_same_v<T, AnimationCameraData>) {
              camera_data.position = transform_component.position;
              camera_data.orientation = transform_component.rotation;
              AnimationCamera::update(delta_time, movement, camera_data);
            }
          },
          camera_component.camera_data);

      const auto frame = frame_->get_current_frame_index();

      const auto& buffers = repository_->per_frame_data_buffers;

      const glm::mat4 view_matrix = std::visit(
          []<typename CameraDataType>(const CameraDataType& camera_data) -> glm::mat4 {
            using T = std::decay_t<CameraDataType>;
            if constexpr (std::is_same_v<T, FreeFlyCameraData>) {
              return camera_data.get_view_matrix();
            } else if constexpr (std::is_same_v<T, OrbitCameraData>) {
              return camera_data.get_view_matrix();
            } else if constexpr (std::is_same_v<T, FirstPersonCameraData>) {
              return camera_data.get_view_matrix();
            } else if constexpr (std::is_same_v<T, AnimationCameraData>) {
              return camera_data.get_view_matrix();
            } else {
              return glm::mat4(1.0f);  // Default return value if no match
            }
          },
          camera_component.camera_data);

      float32 near = 0.1f;
      float32 far = 1000.f;
      glm::mat4 projection = std::visit(
          [&]<typename ProjectionDataType>(const ProjectionDataType& projection_data) -> glm::mat4 {
            using T = std::decay_t<ProjectionDataType>;
            if constexpr (std::is_same_v<T, PerspectiveProjectionData>) {
              near = projection_data.near;
              far = projection_data.far;
              return glm::perspective(projection_data.fov, aspect_ratio_,
                                           projection_data.near, projection_data.far);
            } else if constexpr (std::is_same_v<T, OrthographicProjectionData>) {
              near = projection_data.near;
              far = projection_data.far;
              return glm::orthoRH_ZO(projection_data.left, projection_data.right,
                                     projection_data.bottom,
                                projection_data.top, projection_data.near, projection_data.far);
            } else {
              return glm::mat4(1.0f);
            }
          },
          camera_component.projection_data);
      projection[1][1] *= -1;

      glm::mat4 projection_t = transpose(projection);
      camera_component.view_matrix = view_matrix;
      camera_component.projection_matrix = projection;
      buffers->data[frame].view = view_matrix;
      buffers->data[frame].proj = projection;
      buffers->data[frame].inv_view = inverse(view_matrix);
      buffers->data[frame].inv_viewProj = inverse(projection * view_matrix);
      buffers->data[frame].P00 = projection_t[0][0];
      buffers->data[frame].P11 = projection_t[1][1];

      if (!buffers->freezeCullCamera) {
        buffers->data[frame].cullView = view_matrix;
        buffers->data[frame].cullProj = projection;

        buffers->data[frame].znear = near;
        buffers->data[frame].zfar = far;

        buffers->data[frame].frustum[0] = NormalizePlane(projection_t[3] + projection_t[0]);
        buffers->data[frame].frustum[1] = NormalizePlane(projection_t[3] - projection_t[0]);
        buffers->data[frame].frustum[2] = NormalizePlane(projection_t[3] + projection_t[1]);
        buffers->data[frame].frustum[3] = NormalizePlane(projection_t[3] - projection_t[1]);
        buffers->data[frame].frustum[4] = NormalizePlane(projection_t[3] + projection_t[2]);
        buffers->data[frame].frustum[5] = NormalizePlane(projection_t[3] - projection_t[2]);
      }

      if (false) {
        const glm::mat4 view_matrix = repository_->light_view_projections.get(0).view;
        glm::mat4 projection = repository_->light_view_projections.get(0).proj;

        glm::mat4 projection_t = transpose(projection);
        camera_component.view_matrix = view_matrix;
        camera_component.projection_matrix = projection;
        buffers->data[frame].view = view_matrix;
        buffers->data[frame].proj = projection;
        buffers->data[frame].inv_view = inverse(view_matrix);
        buffers->data[frame].inv_viewProj = inverse(projection * view_matrix);
        buffers->data[frame].P00 = projection_t[0][0];
        buffers->data[frame].P11 = projection_t[1][1];

        buffers->data[frame].cullView = view_matrix;
        buffers->data[frame].cullProj = projection;

        buffers->data[frame].znear = near;
        buffers->data[frame].zfar = far;

        buffers->data[frame].frustum[0] = NormalizePlane(projection_t[3] + projection_t[0]);
        buffers->data[frame].frustum[1] = NormalizePlane(projection_t[3] - projection_t[0]);
        buffers->data[frame].frustum[2] = NormalizePlane(projection_t[3] + projection_t[1]);
        buffers->data[frame].frustum[3] = NormalizePlane(projection_t[3] - projection_t[1]);
        buffers->data[frame].frustum[4] = NormalizePlane(projection_t[3] + projection_t[2]);
        buffers->data[frame].frustum[5] = NormalizePlane(projection_t[3] - projection_t[2]);
      }

      void* mapped_data;
      const VmaAllocation allocation = buffers->uniform_buffers[frame]->allocation;
      VK_CHECK(vmaMapMemory(gpu_->getAllocator(), allocation, &mapped_data));
      const auto scene_uniform_data = static_cast<PerFrameData*>(mapped_data);
      *scene_uniform_data = buffers->data[frame];
      vmaUnmapMemory(gpu_->getAllocator(), buffers->uniform_buffers[frame]->allocation);

      // TODO
      meshlet_push_constants.pyramidWidth = 0;
      meshlet_push_constants.pyramidHeight = 0;
      meshlet_push_constants.clusterOcclusionEnabled = 0;
    }

    void CameraSystem::cleanup() {
      repository_->camera_components.clear();

      const auto& buffers = repository_->per_frame_data_buffers;

      for (int i = 0; i < 2; ++i) {
        resource_manager_->destroy_descriptor_buffer(buffers->descriptor_buffers[i]);
        resource_manager_->destroy_buffer(buffers->uniform_buffers[i]);
      }

    }

}  // namespace gestalt