#include "CameraSystem.hpp"

#include "RenderConfig.hpp"
#include "VulkanCheck.hpp"

#include "FrameProvider.hpp"
#include "PerFrameData.hpp"
#include "Repository.hpp"
#include "VulkanCore.hpp"
#include "Interface/IGpu.hpp"
#include "Interface/IResourceAllocator.hpp"
#include <Events/Events.hpp>

#include "Events/EventBus.hpp"

namespace gestalt::application {

  CameraSystem::CameraSystem(IGpu& gpu, IResourceAllocator& resource_allocator,
                             Repository& repository, FrameProvider& frame, EventBus& event_bus)
      : gpu_(gpu),
        resource_allocator_(resource_allocator),
        repository_(repository),
        frame_(frame),
        event_bus_(event_bus)
  {
    event_bus_.subscribe<UpdateFirstPersonCameraEvent>(
        [this](const UpdateFirstPersonCameraEvent& event) {
          const auto first_person_camera
              = repository_.first_person_camera_components.find_mutable(event.entity);
          if (first_person_camera != nullptr) {
            first_person_camera->set_mouse_speed(event.mouse_speed);
          }
        });

    event_bus_.subscribe<UpdateFreeFlyCameraEvent>([this](const UpdateFreeFlyCameraEvent& event) {
      const auto free_fly_camera
          = repository_.free_fly_camera_components.find_mutable(event.entity);
      if (free_fly_camera != nullptr) {
        free_fly_camera->set_mouse_speed(event.mouse_speed);
        free_fly_camera->set_acceleration(event.acceleration);
        free_fly_camera->set_damping(event.damping);
        free_fly_camera->set_max_speed(event.max_speed);
        free_fly_camera->set_fast_coef(event.fast_coef);
        free_fly_camera->set_slow_coef(event.slow_coef);
      }
    });

    event_bus_.subscribe<UpdateOrbitCameraEvent>([this](const UpdateOrbitCameraEvent& event) {
      const auto orbit_camera = repository_.orbit_camera_components.find_mutable(event.entity);
      if (orbit_camera != nullptr) {
        orbit_camera->set_distance(event.distance);
        orbit_camera->set_yaw(event.yaw);
        orbit_camera->set_pitch(event.pitch);
        orbit_camera->set_orbit_speed(event.orbit_speed);
        orbit_camera->set_zoom_speed(event.zoom_speed);
        orbit_camera->set_pan_speed(event.pan_speed);
      }
    });

    event_bus_.subscribe<UpdatePerspectiveProjectionEvent>(
        [this](const UpdatePerspectiveProjectionEvent& event) {
          const auto perspective_projection
              = repository_.perspective_projection_components.find_mutable(event.entity);
          if (perspective_projection != nullptr) {
            perspective_projection->set_fov(event.fov);
            perspective_projection->set_near(event.near);
            perspective_projection->set_far(event.far);
          }
        });
    event_bus.subscribe<UpdateOrthographicProjectionEvent>(
        [this](const UpdateOrthographicProjectionEvent& event) {
          const auto orthographic_projection
              = repository_.orthographic_projection_components.find_mutable(event.entity);
          if (orthographic_projection != nullptr) {
            orthographic_projection->set_left(event.left);
            orthographic_projection->set_right(event.right);
            orthographic_projection->set_bottom(event.bottom);
            orthographic_projection->set_top(event.top);
            orthographic_projection->set_near(event.near);
            orthographic_projection->set_far(event.far);
          }
        });

    const auto& per_frame_data_buffers = repository_.per_frame_data_buffers;

    per_frame_data_buffers->camera_buffer = resource_allocator_.create_buffer(BufferTemplate(
        "Camera Uniform Buffer", sizeof(PerFrameData),
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT, VMA_MEMORY_USAGE_AUTO,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));
  }


  inline glm::vec4 NormalizePlane(glm::vec4 p) { return p / length(glm::vec3(p)); }

  
    glm::mat4 PerspectiveProjection(float fovY, float aspectWbyH, float zNear) {
      const float32 f = 1.0f / tanf(fovY / 2.0f);
      return glm::mat4{f / aspectWbyH, 0.0f, 0.0f, 0.0f, 0.0f, f,    0.0f,  0.0f,
                       0.0f,           0.0f, 0.0f, 1.0f, 0.0f, 0.0f, zNear, 0.0f};
    }

    void CameraSystem::update(const float delta_time, const UserInput& movement, float aspect) {
      aspect_ratio_ = aspect;

    // TODO set this based on camera create event
      if (!repository_.animation_camera_components.find(active_camera_)
          && !repository_.first_person_camera_components.find(active_camera_)
          && !repository_.free_fly_camera_components.find(active_camera_)
          && !repository_.orbit_camera_components.find(active_camera_)) {
        if (const auto animation_cameras = repository_.animation_camera_components.snapshot();
            !animation_cameras.empty()) {
          active_camera_ = animation_cameras.front().first;
        } else if (const auto first_person_cameras
                   = repository_.first_person_camera_components.snapshot();
                   !first_person_cameras.empty()) {
          active_camera_ = first_person_cameras.front().first;
        } else if (const auto free_fly_cameras = repository_.free_fly_camera_components.snapshot();
                   !free_fly_cameras.empty()) {
          active_camera_ = free_fly_cameras.front().first;
        } else if (const auto orbit_cameras = repository_.orbit_camera_components.snapshot();
                   !orbit_cameras.empty()) {
          active_camera_ = orbit_cameras.front().first;
        }
      }

      auto transform_component = repository_.transform_components.find(active_camera_);
      auto view_matrix = glm::mat4(1.0f);
      if (const auto free_fly_camera_component
          = repository_.free_fly_camera_components.find_mutable(active_camera_);
          free_fly_camera_component != nullptr) {
        free_fly_camera_component->update(delta_time, movement);
        view_matrix = free_fly_camera_component->view_matrix();
        event_bus_.emit<MoveEntityEvent>(
            MoveEntityEvent{active_camera_, free_fly_camera_component->position(),
            free_fly_camera_component->orientation(), transform_component->scale_uniform()});
      } else if (const auto orbit_camera_component
                 = repository_.orbit_camera_components.find_mutable(active_camera_);
                 orbit_camera_component != nullptr) {
        orbit_camera_component->update(delta_time, movement);
        view_matrix = orbit_camera_component->view_matrix();
        event_bus_.emit<MoveEntityEvent>(MoveEntityEvent{
            active_camera_, orbit_camera_component->position(),
            orbit_camera_component->orientation(),
            transform_component->scale_uniform()});
      } else if (const auto first_person_camera_component
                 = repository_.first_person_camera_components.find_mutable(active_camera_);
                 first_person_camera_component != nullptr) {
        first_person_camera_component->set_position(transform_component->position());
        first_person_camera_component->update(movement);
        view_matrix = first_person_camera_component->view_matrix();
        event_bus_.emit<MoveEntityEvent>(
            MoveEntityEvent{active_camera_, first_person_camera_component->position(),
            first_person_camera_component->orientation(), transform_component->scale_uniform()});
      } else if (const auto animation_camera_camera_component
                 = repository_.animation_camera_components.find_mutable(active_camera_);
                 animation_camera_camera_component != nullptr) {
        animation_camera_camera_component->set_position(transform_component->position());
        animation_camera_camera_component->set_orientation(transform_component->rotation());
        animation_camera_camera_component->update();
        view_matrix = animation_camera_camera_component->view_matrix();
      }

      const auto frame = frame_.get_current_frame_index();
      const auto& buffers = repository_.per_frame_data_buffers;

      float32 near = 0.1f;
      float32 far = 1000.f;

    glm::mat4 projection{1.0f};

    if (const auto camera_component
        = repository_.perspective_projection_components.find_mutable(active_camera_);
        camera_component != nullptr) {
      camera_component->set_aspect_ratio(aspect_ratio_);
      projection = camera_component->projection_matrix();
    } else if (const auto camera_component
               = repository_.orthographic_projection_components.find_mutable(active_camera_);
               camera_component != nullptr) {
      projection = camera_component->projection_matrix();
    }

      projection[1][1] *= -1;

      glm::mat4 projection_t = transpose(projection);
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
      
    const auto staging = resource_allocator_.create_buffer(std::move(BufferTemplate(
          "Camera Staging",
          sizeof(PerFrameData),
          VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT,
          VMA_MEMORY_USAGE_AUTO, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)));

      void* mapped_data;
      VK_CHECK(vmaMapMemory(gpu_.getAllocator(), staging->get_allocation(), &mapped_data));
      memcpy(mapped_data, &buffers->data[frame], sizeof(PerFrameData));
      vmaUnmapMemory(gpu_.getAllocator(), staging->get_allocation());

    gpu_.immediateSubmit([&](VkCommandBuffer cmd) {
        VkBufferCopy copy_region;
        copy_region.size = sizeof(PerFrameData);
        copy_region.srcOffset = 0;
        copy_region.dstOffset = 0;
        vkCmdCopyBuffer(cmd, staging->get_buffer_handle(),
                        buffers->camera_buffer->get_buffer_handle(), 1, &copy_region);
      });
      resource_allocator_.destroy_buffer(staging);

      // TODO
      meshlet_push_constants.pyramidWidth = 0;
      meshlet_push_constants.pyramidHeight = 0;
      meshlet_push_constants.clusterOcclusionEnabled = 0;
    }

  
  CameraSystem::~CameraSystem() {
      const auto& buffers = repository_.per_frame_data_buffers;
      resource_allocator_.destroy_buffer(buffers->camera_buffer);

    }

}  // namespace gestalt