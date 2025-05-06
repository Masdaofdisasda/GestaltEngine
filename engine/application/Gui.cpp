#include "Gui.hpp"

#include "VulkanCore.hpp"
#include "VulkanCheck.hpp"

#include <fmt/core.h>

#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_vulkan.h>

#include <ImGuizmo.h>
#include <ImGuiFileDialog.h>

#include <algorithm>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/random.hpp>

#include "vk_initializers.hpp"
#include "Components/DirectionalLightComponent.hpp"
#include "Components/SpotLightComponent.hpp"
#include "Components/PointLightComponent.hpp"
#include "ECS/ComponentFactory.hpp"
#include "Events/EventBus.hpp"
#include "Events/Events.hpp"
#include "Interface/IGpu.hpp"
#include "Mesh/MeshSurface.hpp"
#include <glm/gtx/quaternion.hpp>

namespace gestalt::application {

  constexpr int kMaxPoolElements = 128;

    Gui::Gui(IGpu& gpu, Window& window, VkFormat swapchain_format, Repository& repository,
           EventBus& event_bus,
             GuiCapabilities actions)
        : gpu_(gpu), window_(window), repository_(repository), event_bus_(event_bus), actions_(actions)
      {

      // 1: create descriptor pool for IMGUI
      VkDescriptorPoolSize pool_sizes[] = {{VK_DESCRIPTOR_TYPE_SAMPLER, kMaxPoolElements},
             {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, kMaxPoolElements},
             {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, kMaxPoolElements},
             {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, kMaxPoolElements},
             {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, kMaxPoolElements},
             {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, kMaxPoolElements},
             {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, kMaxPoolElements},
             {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, kMaxPoolElements},
             {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, kMaxPoolElements},
             {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, kMaxPoolElements},
             {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, kMaxPoolElements}};

      VkDescriptorPoolCreateInfo pool_info = {};
      pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
      pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
      pool_info.maxSets = 100;
      pool_info.poolSizeCount = static_cast<uint32>(std::size(pool_sizes));
      pool_info.pPoolSizes = pool_sizes;

      VK_CHECK(vkCreateDescriptorPool(gpu_.getDevice(), &pool_info, nullptr, &imguiPool_));
      gpu_.set_debug_name("ImGui Descriptor Pool", VK_OBJECT_TYPE_DESCRIPTOR_POOL,
                           reinterpret_cast<uint64_t>(imguiPool_));

      // 2: initialize imgui library

      // this initializes the core structures of imgui
      ImGui::CreateContext();

      // this initializes imgui for SDL
      ImGui_ImplSDL2_InitForVulkan(window_.get_handle());

      ImGui_ImplVulkan_LoadFunctions(
          [](const char* function_name, void* user_data) -> PFN_vkVoidFunction {
            return vkGetInstanceProcAddr((VkInstance)user_data, function_name);
          },
          gpu_.getInstance());

      VkPipelineRenderingCreateInfoKHR pipelineRenderingCreateInfo = {
          .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
          .colorAttachmentCount = 1,
          .pColorAttachmentFormats = &swapchain_format,
      };

      // this initializes imgui for Vulkan
      ImGui_ImplVulkan_InitInfo init_info = {};
      init_info.Instance = gpu_.getInstance();
      init_info.PhysicalDevice = gpu_.getPhysicalDevice();
      init_info.Device = gpu_.getDevice();
      init_info.Queue = gpu_.getGraphicsQueue();
      init_info.DescriptorPool = imguiPool_;
      init_info.MinImageCount = 3;
      init_info.ImageCount = 3;
      init_info.UseDynamicRendering = true;
      init_info.PipelineRenderingCreateInfo = pipelineRenderingCreateInfo;

      init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

      ImGui_ImplVulkan_Init(&init_info);

      gpu_.immediateSubmit( // wait until the font texture is uploaded
          [&](VkCommandBuffer cmd) {
            ImGui_ImplVulkan_CreateFontsTexture();
          });
    }

    Gui::~Gui() {
      // Destroy ImGui context
      ImGui_ImplVulkan_Shutdown();
      ImGui_ImplSDL2_Shutdown();
      ImGui::DestroyContext();

      // Destroy ImGui descriptor pool
      if (imguiPool_ != VK_NULL_HANDLE) {
        vkDestroyDescriptorPool(gpu_.getDevice(), imguiPool_, nullptr);
        imguiPool_ = VK_NULL_HANDLE;
      }
    }

    void Gui::draw(VkCommandBuffer cmd, const VkImageView swapchain_view, const VkExtent2D swapchain_extent) {
      VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(
          swapchain_view, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
      VkRenderingInfo renderInfo
          = vkinit::rendering_info(swapchain_extent, &colorAttachment, nullptr);

      vkCmdBeginRendering(cmd, &renderInfo);

      ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

      vkCmdEndRendering(cmd);
    }

    void Gui::update(const SDL_Event& e) { ImGui_ImplSDL2_ProcessEvent(&e); }

    void Gui::guizmo() {
      ImGuizmo::BeginFrame();

      const Entity camera_entity = actions_.get_active_camera();
      glm::mat4 viewCam{1.f};
      glm::mat4 proj{1.f};
      if (const auto cam = repository_.animation_camera_components.find(camera_entity); cam != nullptr) {
        viewCam = cam->view_matrix();
      } else if (const auto cam = repository_.free_fly_camera_components.find(camera_entity);
                 cam != nullptr) {
        viewCam = cam->view_matrix();
      } else if (const auto cam = repository_.first_person_camera_components.find(camera_entity);
                 cam != nullptr) {
        viewCam = cam->view_matrix();
      } else if (const auto cam = repository_.orbit_camera_components.find(camera_entity);
                 cam != nullptr) {
        viewCam = cam->view_matrix();
      }
      if (const auto cam = repository_.perspective_projection_components.find(camera_entity);
          cam != nullptr) {
        proj = cam->projection_matrix();
      } else if (const auto cam
                 = repository_.orthographic_projection_components.find(camera_entity);
                 cam != nullptr) {
        proj = cam->projection_matrix();
      }

      proj[1][1] *= -1;  // Flip the Y-axis for opengl like system

      // Convert glm matrices to arrays for ImGuizmo
      float* view = glm::value_ptr(viewCam);
      float* projection = glm::value_ptr(proj);
      float model[16];

      // Setup for using Gizmo without a separate window
      ImGuiIO& io = ImGui::GetIO();

      ImVec2 viewManipulatorPosition = ImVec2(
          10, io.DisplaySize.y - 128 - 10);  // Assuming 128x128 view manipulator size, 10
                                             // pixels padding from the bottom and left edges.

      ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);

      // Draw the view manipulator
      float camDistance = 10.0f;  // Adjust this value as needed for your scene
      ImGuizmo::ViewManipulate(view, camDistance, viewManipulatorPosition, ImVec2(128, 128),
                               0xB4101010);

      const auto transform_component = repository_.transform_components.find(selected_entity_);
      if (transform_component != nullptr) {
        auto& transform = transform_component;

        glm::mat4 localTransform = glm::translate(glm::mat4(1.0f), transform->position())
                                   * glm::toMat4(transform->rotation())
                                   * glm::scale(glm::mat4(1.0f), transform->scale());
        glm::mat4 parentWorldTransform
            = glm::translate(glm::mat4(1.0f), transform->parent_position)
              * glm::toMat4(transform->parent_rotation)
              * glm::scale(glm::mat4(1.0f), glm::vec3(transform->parent_scale));
        glm::mat4 inverseParentWorldTransform = glm::inverse(parentWorldTransform);
        glm::mat4 worldTransform = parentWorldTransform * localTransform;

        glm::vec3 t = glm::vec3(worldTransform[3]);
        glm::quat r = transform->rotation();
        glm::vec3 s = transform->scale();

        float* translation = &t.x;
        float* rotation = &r.x;
        float* scale = &s.x;

        ImGuizmo::RecomposeMatrixFromComponents(translation, rotation, scale, model);

        if (guizmo_operation_ == 0) {
          if (Manipulate(view, projection, ImGuizmo::TRANSLATE, ImGuizmo::LOCAL, model)) {
            ImGuizmo::DecomposeMatrixToComponents(model, translation, rotation, scale);

            const auto new_pos
                = glm::vec3(inverseParentWorldTransform
                            * glm::vec4(translation[0], translation[1], translation[2], 1.0f));
            event_bus_.emit<TranslateEntityEvent>(
                TranslateEntityEvent{selected_entity_, new_pos});

            transform->is_dirty = true;
          }
        } else if (guizmo_operation_ == 1) {
          if (Manipulate(view, projection, ImGuizmo::TRANSLATE, ImGuizmo::LOCAL, model)) {
            ImGuizmo::DecomposeMatrixToComponents(model, translation, rotation, scale);

            // TODO transform.rotation = glm::quat(rotation[3], rotation[0], rotation[1],
            // rotation[2]);
            transform->is_dirty = true;
          }
        } else {
          if (Manipulate(view, projection, ImGuizmo::SCALE, ImGuizmo::LOCAL, model)) {
            ImGuizmo::DecomposeMatrixToComponents(model, translation, rotation, scale);

            //transform->scale = std::max({scale[0], scale[1], scale[2]});
            transform->is_dirty = true;
          }
        }
      }
    }

    void Gui::check_file_dialog() {
      if (ImGuiFileDialog::Instance()->Display("ChooseGLTF")) {
        // Check if user confirmed the selection
        if (ImGuiFileDialog::Instance()->IsOk()) {
          std::filesystem::path filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
          std::filesystem::path filePath = ImGuiFileDialog::Instance()->GetCurrentPath();

          fmt::print("Importing File: {}\n",
                     filePathName.string());  // Convert path to string for printing
          actions_.load_gltf(filePathName);   // Pass filePathName directly as a filesystem path
        }

        // Close the dialog after retrieval
        ImGuiFileDialog::Instance()->Close();
      }
    }

    void Gui::show_help() {
      ImGui::Begin("Help - Controls", &show_help_, ImGuiWindowFlags_AlwaysAutoResize);

      ImGui::Text("Overall Control Scheme");
      ImGui::Separator();

      ImGui::Text("Global Controls:");
      ImGui::BulletText("F1 - Lock mouse to window");
      ImGui::BulletText("F2 - Unlock mouse");
      ImGui::BulletText("F3 - Toggle Free Camera/Animation Camera");

      ImGui::Spacing();

      ImGui::Text("Free Camera Controls:");
      ImGui::BulletText("W/A/S/D - Move Camera");
      ImGui::BulletText("LSHIFT - Move Down");
      ImGui::BulletText("SPACE - Move Up");
      ImGui::BulletText("LCTRL - Increase Speed");
      ImGui::BulletText("LALT - Decrease Speed");
      ImGui::BulletText("Right Mouse Button + Drag - Rotate View");

      ImGui::End();
    }

    void Gui::menu_bar() {
      if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
          if (ImGui::BeginMenu("Import")) {
            if (ImGui::MenuItem("GLTF")) {
              IGFD::FileDialogConfig config;
              config.path = "../../assets";
              ImGuiFileDialog::Instance()->OpenDialog("ChooseGLTF", "Choose File", ".glb,.gltf",
                                                      config);
            }
            ImGui::EndMenu();
          }

          ImGui::Separator();

          if (ImGui::MenuItem("Exit")) {
            actions_.exit();
          }
          ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Edit")) {
          ImGui::MenuItem("Light Editor", nullptr, &show_lights_);
          ImGui::MenuItem("Camera Editor", nullptr, &show_cameras_);
          if (ImGui::BeginMenu("Guizmo Mode")) {
            if (ImGui::MenuItem("Translate")) {
              guizmo_operation_ = 0;
            }
            if (ImGui::MenuItem("Rotate")) {
              guizmo_operation_ = 1;
            }
            if (ImGui::MenuItem("Scale")) {
              guizmo_operation_ = 2;
            }
            ImGui::EndMenu();
          }

          if (ImGui::BeginMenu("Add Camera")) {
            if (ImGui::MenuItem("Free Fly Camera")) {
              // actions_.add_camera();
            }
            if (ImGui::MenuItem("Orbit Camera")) {
              // Code to add an Orbit Camera
            }
            ImGui::EndMenu();
          }

          if (ImGui::BeginMenu("Add Light Source")) {
            if (ImGui::MenuItem("Point Light")) {
              current_action_ = action::add_point_light;
            }
            if (ImGui::MenuItem("Directional Light")) {
              current_action_ = action::add_directional_light;
            }
            if (ImGui::MenuItem("Spot Light")) {
              // Code to add an Orbit Camera
            }
            ImGui::EndMenu();
          }
          ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Window")) {
          ImGui::MenuItem("Scene Graph", nullptr, &show_scene_hierarchy_);
          ImGui::MenuItem("Guizmo", nullptr, &show_guizmo_);

          if (ImGui::BeginMenu("Settings")) {
            ImGui::MenuItem("Shading", nullptr, &show_shading_settings);
            ImGui::MenuItem("HDR & Tonemapping", nullptr, &show_tone_map_settings);
            ImGui::MenuItem("Light Adaptation", nullptr, &show_light_adapt_settings_);
            ImGui::MenuItem("Sky & Atmosphere", nullptr, &show_sky_settings);
            ImGui::MenuItem("Grid", nullptr, &show_grid_settings);

            ImGui::EndMenu();
          }

          ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Help")) {
          if (ImGui::MenuItem("Show Controls")) {
            show_help_ = true;  // Open the Help window
          }
          ImGui::EndMenu();
        }

        ImGui::EndMainMenuBar();
      }

      // Depending on the action, display appropriate controls
      if (current_action_ == action::add_point_light) {
        if (ImGui::Begin("Add Point Light", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
          static glm::vec3 color = glm::vec3(1.0f);  // Default to white
          static float intensity = 1.0f;
          static float range = 5.0f;
          static glm::vec3 position = glm::vec3(0.0f);

          ImGui::ColorEdit3("Color", &color.x);
          ImGui::SliderFloat("Intensity", &intensity, 0.0f, 100.0f, "%.3f",
                             ImGuiSliderFlags_Logarithmic);
          ImGui::SliderFloat("Range", &range, 0.0f, 100.0f, "%.3f", ImGuiSliderFlags_Logarithmic);
          ImGui::InputFloat3("Position", &position.x);

          if (ImGui::Button("Add Light")) {
            actions_.get_component_factory().create_point_light(color, intensity, position, range);

            current_action_ = action::none;
          }
          ImGui::SameLine();
          if (ImGui::Button("Cancel")) {
            current_action_ = action::none;
          }

          ImGui::Separator();

          const auto& root = repository_.scene_graph.find(root_entity);

          static glm::vec3 min_bounds = root->bounds.min;
          static glm::vec3 max_bounds = root->bounds.max;
          static glm::vec2 intensity_range = glm::vec2(1.f);
          static float radius = 5.0f;
          static int32 light_count = 10;

          ImGui::InputFloat3("Min Bounds", &min_bounds.x);
          ImGui::InputFloat3("Max Bounds", &max_bounds.x);
          ImGui::SliderFloat2("Intensity Range", &intensity_range.x, 0.0f, 100.0f, "%.3f",
                              ImGuiSliderFlags_Logarithmic);
          ImGui::SliderFloat("Radius", &radius, 0.0f, 100.0f, "%.3f",
                             ImGuiSliderFlags_Logarithmic);
          ImGui::InputInt("Light Count", &light_count);

          if (ImGui::Button("Generate")) {
            // Calculate grid spacing based on the number of lights
            int grid_count = static_cast<int>(std::ceil(std::pow(light_count, 1.0f / 3.0f)));
            glm::vec3 grid_spacing = (max_bounds - min_bounds) / static_cast<float>(grid_count);

            // Iterate over grid points and place lights
            for (int i = 0; i < grid_count; ++i) {
              for (int j = 0; j < grid_count; ++j) {
                for (int k = 0; k < grid_count; ++k) {
                  // Calculate position within the grid
                  glm::vec3 position
                      = min_bounds
                        + glm::vec3(i * grid_spacing.x, j * grid_spacing.y, k * grid_spacing.z);
                  position += grid_spacing;

                  glm::vec3 color = linearRand(glm::vec3(0.0f), glm::vec3(1.0f));
                  float intensity = glm::linearRand(intensity_range.x, intensity_range.y);

                  actions_.get_component_factory().create_point_light(color, intensity, position,
                                                                      radius);
                }
              }
            }
            current_action_ = action::none;
          }

          ImGui::End();
        }
      }

      if (current_action_ == action::add_directional_light) {
        if (ImGui::Begin("Add Directional Light", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
          static glm::vec3 color = glm::vec3(1.0f);  // Default to white
          static float32 intensity = 1.0f;
          static glm::vec3 direction = glm::vec3(0.0f, -1.0f, 0.0f);  // Default pointing downwards

          ImGui::ColorEdit3("Color", &color.x);
          ImGui::SliderFloat("Intensity", &intensity, 0.0f, 100.0f, "%.3f",
                             ImGuiSliderFlags_Logarithmic);
          ImGui::InputFloat3("Direction", &direction.x);

          if (ImGui::Button("Add Light")) {
            // auto newLight = light_component::DirectionalLight(color, intensity, direction);

            // TODO
            // const size_t entity = actions_.get_database().add_light(???);
            // selected_node_ = &actions_.get_database().get_node(entity);

            current_action_ = action::none;
          }

          ImGui::End();
        }
      }
    }

    void Gui::lights() {
      if (ImGui::Begin("Lights")) {
        if (ImGui::CollapsingHeader("Directional Lights", ImGuiTreeNodeFlags_DefaultOpen)) {
          auto lights = repository_.directional_light_components.snapshot();
          int selectedLightIndex = 0;  // Default to the first light
          int32 maxLights = static_cast<int32>(lights.size()) - 1;
          if (!lights.empty()) {
            ImGui::SliderInt("Select Light", &selectedLightIndex, 0, maxLights);
            auto& [entity, light_component] = lights[selectedLightIndex];
            selected_entity_ = entity;
            const auto transform_component = repository_.transform_components.find(entity);
            show_directional_light_component(&light_component, transform_component);
          }
        }

        if (ImGui::CollapsingHeader("Point Lights", ImGuiTreeNodeFlags_DefaultOpen)) {
          auto lights = repository_.point_light_components.snapshot();
          int selectedLightIndex = 0;  // Default to the first light
          int32 maxLights = static_cast<int32>(lights.size()) - 1;
          if (!lights.empty()) {
            ImGui::SliderInt("Select Light", &selectedLightIndex, 0, maxLights);
            auto& [entity, light_component] = lights[selectedLightIndex];
            selected_entity_ = entity;
            const auto transform_component = repository_.transform_components.find(entity);
            show_point_light_component(&light_component, transform_component);
          }
        }

        if (ImGui::CollapsingHeader("Spot Lights", ImGuiTreeNodeFlags_DefaultOpen)) {
          auto lights = repository_.spot_light_components.snapshot();
          int selectedLightIndex = 0;  // Default to the first light
          int32 maxLights = static_cast<int32>(lights.size()) - 1;
          if (!lights.empty()) {
            ImGui::SliderInt("Select Light", &selectedLightIndex, 0, maxLights);
            auto& [entity, light_component] = lights[selectedLightIndex];
            selected_entity_ = entity;
            const auto transform_component = repository_.transform_components.find(entity);
            show_spotlight_component(&light_component, transform_component);
          }
        }
        ImGui::End();
      }
    }

    void Gui::cameras() {
      if (ImGui::Begin("Cameras")) {

        if (ImGui::CollapsingHeader("Animation Cameras", ImGuiTreeNodeFlags_DefaultOpen)) {
          auto cameras = repository_.animation_camera_components.snapshot();

          static int selectedCameraIndex = 0;
          uint32 maxCameras = static_cast<uint32>(cameras.size()) - 1;
          if (!cameras.empty()) {
            ImGui::SliderInt("Select Camera", &selectedCameraIndex, 0, maxCameras);

            auto& [entity, camera_component] = cameras[selectedCameraIndex];

            show_camera_component(&camera_component);
            ImGui::Separator();
            if (ImGui::Button("Set as Active Camera")) {
              actions_.set_active_camera(entity);
            }
          }
        }

        if (ImGui::CollapsingHeader("First Person Cameras", ImGuiTreeNodeFlags_DefaultOpen)) {
          auto cameras = repository_.first_person_camera_components.snapshot();

          static int selectedCameraIndex = 0;
          uint32 maxCameras = static_cast<uint32>(cameras.size()) - 1;
          if (!cameras.empty()) {
            ImGui::SliderInt("Select Camera", &selectedCameraIndex, 0, maxCameras);

            auto& [entity, camera_component] = cameras[selectedCameraIndex];

            show_camera_component(&camera_component);
            ImGui::Separator();
            if (ImGui::Button("Set as Active Camera")) {
              actions_.set_active_camera(entity);
            }
          }
        }

        if (ImGui::CollapsingHeader("Free Fly Cameras", ImGuiTreeNodeFlags_DefaultOpen)) {
          auto cameras = repository_.free_fly_camera_components.snapshot();

          static int selectedCameraIndex = 0;
          uint32 maxCameras = static_cast<uint32>(cameras.size()) - 1;
          if (!cameras.empty()) {
            ImGui::SliderInt("Select Camera", &selectedCameraIndex, 0, maxCameras);

            auto& [entity, camera_component] = cameras[selectedCameraIndex];

            show_camera_component(&camera_component);
            ImGui::Separator();
            if (ImGui::Button("Set as Active Camera")) {
              actions_.set_active_camera(entity);
            }
          }
        }

        if (ImGui::CollapsingHeader("Orbit Cameras", ImGuiTreeNodeFlags_DefaultOpen)) {
          auto cameras = repository_.orbit_camera_components.snapshot();

          static int selectedCameraIndex = 0;
          uint32 maxCameras = static_cast<uint32>(cameras.size()) - 1;
          if (!cameras.empty()) {
            ImGui::SliderInt("Select Camera", &selectedCameraIndex, 0, maxCameras);

            auto& [entity, camera_component] = cameras[selectedCameraIndex];

            show_camera_component(&camera_component);
            ImGui::Separator();
            if (ImGui::Button("Set as Active Camera")) {
              actions_.set_active_camera(entity);
            }
          }
        }

      }
      ImGui::End();
    }

    void Gui::scene_graph() {
      float32 windowWidth = 300.0f;  // Width of the ImGui window
      float32 menuBarHeight = 18.f;
      float32 windowHeight
          = static_cast<float32>(window_.get_height()) - menuBarHeight;  // Full height of the application window
      ImVec2 windowPos = ImVec2(static_cast<float32> (window_.get_width()) - windowWidth,
                                menuBarHeight);               // Position to the right
      ImVec2 windowSize = ImVec2(windowWidth, windowHeight);            // Size of the ImGui window

      ImGui::SetNextWindowPos(windowPos);
      ImGui::SetNextWindowSize(windowSize);
      if (ImGui::Begin("Scene Graph", nullptr)) {
        show_scene_hierarchy_window();
      }
      ImGui::End();
    }

    void Gui::new_frame() {
      ImGuizmo::SetImGuiContext(ImGui::GetCurrentContext());

      ImGui_ImplVulkan_NewFrame();
      ImGui_ImplSDL2_NewFrame();
      ImGui::NewFrame();

      if (show_guizmo_) {
        guizmo();
      }

      menu_bar();

      if (show_scene_hierarchy_) {
        scene_graph();
      }

      if (show_lights_) {
        lights();
      }

      if (show_cameras_) {
        cameras();
      }

      if (show_light_adapt_settings_) {
        light_adaptation_settings();
      }

      if (show_sky_settings) {
        sky_settings();
      }

      if (show_grid_settings) {
        grid_settings();
      }

      if (show_shading_settings) {
        shading_settings();
      }

      if (show_tone_map_settings) {
        tone_map_settings();
      }

      if (show_help_) {
        show_help();
      }

      check_file_dialog();

      ImGui::Render();
    }

    void Gui::light_adaptation_settings() {
      if (ImGui::Begin("Exposure & Light Adaptation")) {
        RenderConfig& config = actions_.get_render_config();
        auto& lum_params = config.luminance_params;
        auto& cam_params = config.hdr;

        ImGui::Text("Histogram-based Luminance");
        if (ImGui::SliderFloat("min_log_lum", &lum_params.min_log_lum, -20.0f, 20.0f, "%.2f")) {
          lum_params.log_lum_range = lum_params.max_log_lum - lum_params.min_log_lum;
          lum_params.inv_log_lum_range = 1.0f / lum_params.log_lum_range;
          lum_params.min_log_lum = std::min(lum_params.min_log_lum, lum_params.max_log_lum);
          if (lum_params.min_log_lum == 0.f) {
            lum_params.min_log_lum = -0.01f;
          }
        }
        if (ImGui::SliderFloat("max_log_lum", &lum_params.max_log_lum, -20.0f, 20.0f, "%.2f")) {
          lum_params.log_lum_range = lum_params.max_log_lum - lum_params.min_log_lum;
          lum_params.inv_log_lum_range = 1.0f / lum_params.log_lum_range;
          lum_params.max_log_lum = std::max(lum_params.max_log_lum, lum_params.min_log_lum);
          if (lum_params.max_log_lum == 0.f) {
            lum_params.max_log_lum = 0.01f;
          }
        }
        // Show derived logLumRange, read-only
        ImGui::Text("log_lum_range: %.2f", lum_params.log_lum_range);
        ImGui::Text("inv_log_lum_range: %.3f", lum_params.inv_log_lum_range);

        // time_coeff to control adaptation speed
        ImGui::SliderFloat("time_coeff", &lum_params.time_coeff, 0.01f, 1.0f, "%.2f");
        ImGui::Separator();

        // "Camera-like" controls
        ImGui::Text("Camera-based Exposure");
        ImGui::SliderFloat("ISO", &cam_params.iso, 25.0f, 6400.0f, "%.0f");
        ImGui::SliderFloat("Shutter (s)", &cam_params.shutter, 1.0f / 4000.0f, 1.0f / 1.0f, "%.4f",
                           ImGuiSliderFlags_Logarithmic);
        ImGui::SliderFloat("Aperture (f-stop)", &cam_params.aperture, 1.0f, 22.0f, "%.1f");
        ImGui::SliderFloat("EV Compensation", &cam_params.exposureCompensation, -4.0f, 4.0f,
                           "%.2f");

      }
      ImGui::End();
    }

    void Gui::sky_settings() {
      if (ImGui::Begin("Sky & Atmosphere")) {
        auto& [rayleighScattering, showSkybox, ozoneAbsorption, unused2, mieScattering, unused3]
            = actions_.get_render_config().skybox;

        // Rayleigh Scattering Coefficients
        ImGui::Text("Rayleigh Scattering");
        ImGui::SliderFloat("Rayleigh Red", &rayleighScattering.x, 0.0f, 1e-5f, "%.8f",
                           ImGuiSliderFlags_Logarithmic);
        ImGui::SliderFloat("Rayleigh Green", &rayleighScattering.y, 0.0f, 1e-5f, "%.8f",
                           ImGuiSliderFlags_Logarithmic);
        ImGui::SliderFloat("Rayleigh Blue", &rayleighScattering.z, 0.0f, 1e-5f, "%.8f",
                           ImGuiSliderFlags_Logarithmic);

        const char* sky_box_options[] = {"Atmosphere", "Environment Map"};
        int current_shadow_mode = showSkybox;
        if (ImGui::Combo("Shadow Mode", &current_shadow_mode, sky_box_options,
                         IM_ARRAYSIZE(sky_box_options))) {
          showSkybox = current_shadow_mode;
        }


        // Ozone Absorption Coefficients
        ImGui::Text("Ozone Absorption");
        ImGui::SliderFloat("Ozone Red", &ozoneAbsorption.x, 0.0f, 0.001f, "%.8f",
                           ImGuiSliderFlags_Logarithmic);
        ImGui::SliderFloat("Ozone Green", &ozoneAbsorption.y, 0.0f, 0.002f, "%.8f",
                           ImGuiSliderFlags_Logarithmic);
        ImGui::SliderFloat("Ozone Blue", &ozoneAbsorption.z, 0.0f, 0.0001f, "%.8f",
                           ImGuiSliderFlags_Logarithmic);

        // Mie Scattering Coefficients
        ImGui::Text("Mie Scattering");
        ImGui::SliderFloat("Mie Red", &mieScattering.x, 0.0f, 5e-5f, "%.8f",
                           ImGuiSliderFlags_Logarithmic);
        ImGui::SliderFloat("Mie Green", &mieScattering.y, 0.0f, 5e-5f, "%.8f",
                           ImGuiSliderFlags_Logarithmic);
        ImGui::SliderFloat("Mie Blue", &mieScattering.z, 0.0f, 5e-5f, "%.8f",
                           ImGuiSliderFlags_Logarithmic);
      }
      ImGui::End();
    }

    void Gui::grid_settings() {
      if (ImGui::Begin("Sky & Atmosphere")) {
        auto& grid_params = actions_.get_render_config().grid;

        ImGui::SliderFloat("Major Line Width", &grid_params.majorLineWidth, 0.01f, 0.1f);
        ImGui::SliderFloat("Minor Line Width", &grid_params.minorLineWidth, 0.01f, 0.1f);
        ImGui::SliderFloat("Axis Line Width", &grid_params.axisLineWidth, 0.01f, 0.1f);
        ImGui::SliderFloat("Major Grid Division", &grid_params.majorGridDivision, 1.0f, 10.0f);
        ImGui::SliderFloat("Axis Dash Scale", &grid_params.axisDashScale, 1.0f, 2.0f);
      }
      ImGui::End();
    }

    void Gui::shading_settings() {
      if (ImGui::Begin("Shading")) {
        auto& config = actions_.get_render_config();

        ImGui::Checkbox("Always Opaque", &config.always_opaque);

        ImGui::Checkbox("View AABBs", &config.debug_aabb_bvh);

        ImGui::Checkbox("View Mesh Bounds", &config.debug_bounds_mesh);

        static bool freeze_frustum_cull = false;
        if( ImGui::Checkbox("Freeze Frustum Culling", &freeze_frustum_cull)) {
          repository_.per_frame_data_buffers->freezeCullCamera = freeze_frustum_cull;
        }

        const char* shadow_mode_options[] = {"On", "Off"};
        int current_shadow_mode = config.lighting.shadow_mode;
        if (ImGui::Combo("Shadow Mode", &current_shadow_mode, shadow_mode_options,
                         IM_ARRAYSIZE(shadow_mode_options))) {
          config.lighting.shadow_mode = current_shadow_mode;
        }

        const char* shading_mode_options[]
            = {"All", "Only Directional Light", "Only Point Light", "Only Spot Light", "None"};
        int current_shading_mode = config.lighting.shading_mode;
        if (ImGui::Combo("Shading Mode", &current_shading_mode, shading_mode_options,
                         IM_ARRAYSIZE(shading_mode_options))) {
          config.lighting.shading_mode = current_shading_mode;
        }

        const char* ibl_mode_options[] = {"Full", "Only Diffuse", "Only Specular", "Off"};
        int current_ibl_mode = config.lighting.ibl_mode;
        if (ImGui::Combo("IBL Mode", &current_ibl_mode, ibl_mode_options,
                         IM_ARRAYSIZE(ibl_mode_options))) {
          config.lighting.ibl_mode = current_ibl_mode;
        }

        const char* debug_mode_options[] = {
            "None",  "Albedo", "Normals",        "Emissive", "Occlusion/Rough/Metal", "Occlusion",
            "Rough", "Metal",  "World Position", "Depth",    "Light Space Position",
        };
        int current_debug_mode = config.lighting.debug_mode;
        if (ImGui::Combo("Debug Mode", &current_debug_mode, debug_mode_options,
                         IM_ARRAYSIZE(debug_mode_options))) {
          config.lighting.debug_mode = current_debug_mode;
        }

         if (ImGui::CollapsingHeader("Volumetric Lighting Settings")) {
          auto& params = config.volumetric_lighting;
          // Halton XY (usually small jitter values, range [-1.0, 1.0])
          ImGui::SliderFloat2("Halton XY", &params.halton_xy.x, -1.0f, 1.0f, "%.2f");

          // Temporal Reprojection Jitter Scale (sensible values between 0 and 1)
          ImGui::SliderFloat("Jitter Scale", &params.temporal_reprojection_jitter_scale, 0.0f, 1.0f,
                             "%.2f");

          // Density Modifier (controls overall fog density, typical range [0.0001, 5.0])
          ImGui::SliderFloat("Density Modifier", &params.density_modifier, 0.0001f, 100.0f, "%.4f",
                             ImGuiSliderFlags_Logarithmic);

          // Noise Scale (scales noise for fog, typical range [0.1, 10.0])
          ImGui::SliderFloat("Noise Scale", &params.noise_scale, 0.0001f, 10.0f, "%.4f",
                             ImGuiSliderFlags_Logarithmic);

          ImGui::Combo("Noise Type", (int*)&params.noise_type,
                       "Blue Noise\0Interleaved Noise\0");

          // Volumetric Noise Position Multiplier (range [0.1, 10.0] for controlling noise
          // tiling/position)
          ImGui::SliderFloat("Noise Position Multiplier",
                             &params.volumetric_noise_position_multiplier, 0.1f, 10.0f, "%.2f");

          // Volumetric Noise Speed Multiplier (range [0.0, 10.0] for noise animation speed)
          ImGui::SliderFloat("Noise Speed Multiplier", &params.volumetric_noise_speed_multiplier,
                             0.0001f, 10.0f, "%.4f", ImGuiSliderFlags_Logarithmic);

          // Height Fog Density (controls fog density along the Y-axis, range [0.0, 1.0])
          ImGui::SliderFloat("Height Fog Density", &params.height_fog_density, 0.0001f, 100.0f, "%.4f",
                             ImGuiSliderFlags_Logarithmic);

          // Height Fog Falloff (range [0.1, 10.0] for how quickly fog density falls off with
          // height)
          ImGui::SliderFloat("Height Fog Falloff", &params.height_fog_falloff, 0.0001f, 2.0f, "%.2f",
                             ImGuiSliderFlags_Logarithmic);

          // Box Position (world-space position of the fog box, wide range)
          ImGui::SliderFloat3("Box Position", &params.box_position.x, -2000.0f, 2000.0f, "%.2f");

          // Box Fog Density (density of the fog within the box, range [0.0, 1.0])
          ImGui::SliderFloat("Box Fog Density", &params.box_fog_density, 0.0001f, 100.0f, "%.4f",
                             ImGuiSliderFlags_Logarithmic);

          // Box Size (size of the fog box in world-space units, typical range [1.0, 100.0])
          ImGui::SliderFloat3("Box Size", &params.box_size.x, 1.0f, 1000.0f, "%.2f");

           ImGui::SliderInt("Enable Spatial Filtering", &params.enable_spatial_filter, 0, 1);

          // Scattering Factor (how much light scatters in the fog, range [0.0, 1.0])
           ImGui::SliderFloat("Scattering Factor", &params.scattering_factor, 0.0001f, 1.0f, "%.4f",
                              ImGuiSliderFlags_Logarithmic);

           if (params.phase_type == 3) {
            // Droplet Size (controls the size of the droplets, range [5.0, 50.0])
             ImGui::SliderFloat("Droplet Size", &params.phase_anisotropy, 0.01f, 50.0f, "%.3f",
                                ImGuiSliderFlags_Logarithmic);
           } else {
             // Phase Anisotropy (controls forward/backward scattering, range [-1.0, 1.0])
             ImGui::SliderFloat("Phase Anisotropy", &params.phase_anisotropy, -1.0f, 1.0f, "%.3f");
           }

          ImGui::Combo("Phase Type", (int*)&params.phase_type, "Henyey-Greenstein\0Cornette-Shanks\0Shlick\0Henyey-Greenstein + Draine\0");
        }

        if (ImGui::CollapsingHeader("SSAO Settings")) {
          ImGui::Checkbox("Enable SSAO", &config.enable_ssao);
          ImGui::SliderInt("SSAO Quality", &config.ssao_quality, 1, 4);
          ImGui::Checkbox("Show SSAO", &config.ssao.show_ssao_only);

          ImGui::SliderFloat("Blur Radius", &config.ssao_radius, 0.0f, 10.0f, "%.3f");
          ImGui::SliderFloat("Blur Depth Threshold", &config.depthThreshold, 0.0f, 10.0f, "%.3f");

          ImGui::SliderFloat("Scale", &config.ssao.scale, 0.0f, 10.0f, "%.3f");
          ImGui::SliderFloat("Bias", &config.ssao.bias, 0.0f, 1.0f, "%.3f");
          ImGui::SliderFloat("Radius", &config.ssao.radius, 0.0f, .5f, "%.2f");
          ImGui::SliderFloat("Attenuation Scale", &config.ssao.attScale, 0.0f, 2.0f, "%.2f");
          ImGui::SliderFloat("Distance Scale", &config.ssao.distScale, 0.0f, 10.0f, "%.2f");
        }
      }
      ImGui::End();
    }

    void Gui::tone_map_settings() {
      RenderConfig& config = actions_.get_render_config();

      if (ImGui::CollapsingHeader("HDR Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Checkbox("Enable HDR", &config.enable_hdr);
        ImGui::SliderInt("Bloom Quality", &config.bloom_quality, 1, 3);
        const char* toneMappingOptions[]
            = {"Reinhard Extended", "Uncharted 2", "ACES approximation", "ACES fitted", "AGX"};
        int currentOption = config.hdr.toneMappingOption;
        if (ImGui::Combo("Tone Mapping Option", &currentOption, toneMappingOptions,
                         IM_ARRAYSIZE(toneMappingOptions))) {
          config.hdr.toneMappingOption = currentOption;
        }

        bool showBrightPass = config.hdr.show_bright_pass;
        if (ImGui::Checkbox("Show Bright Pass", &showBrightPass)) {
          config.hdr.show_bright_pass = static_cast<int>(showBrightPass);
        }
        ImGui::SliderFloat("Exposure", &config.hdr.exposure, 0.1f, 2.0f, "%.3f");
        ImGui::SliderFloat("Max White", &config.hdr.maxWhite, 0.1f, 10.f, "%.3f");
        ImGui::SliderFloat("Bloom Strength", &config.hdr.bloomStrength, 0.0f, 1.f, "%.4f",
                           ImGuiSliderFlags_Logarithmic);

        ImGui::SliderFloat("Vignette Radius", &config.hdr.vignette_radius, 0.0f, 2.0f, "%.3f");
        ImGui::SliderFloat("Vignette Softness", &config.hdr.vignette_softness, 0.0f, 1.0f, "%.3f");

        ImGui::ColorPicker4("Lift", &config.hdr.lift[0], ImGuiColorEditFlags_Float);
        ImGui::ColorPicker4("Gamma", &config.hdr.gamma[0], ImGuiColorEditFlags_Float);
        ImGui::ColorPicker4("Gain", &config.hdr.gain[0], ImGuiColorEditFlags_Float);

        ImGui::SliderFloat("Saturation", &config.hdr.saturation, 0.0f, 2.0f, "%.3f");
        ImGui::SliderFloat("Contrast", &config.hdr.contrast, 0.0f, 2.0f, "%.3f");
      }
    }

    void Gui::display_scene_hierarchy(const Entity entity) {
      const auto& node = repository_.scene_graph.find_mutable(entity);
      if (node != nullptr) {

        ImGuiTreeNodeFlags node_flags = ImGuiTreeNodeFlags_OpenOnArrow;

        // If the node has no children, make it a leaf node
        if (node->children.empty()) {
          node_flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
        }

        // Check if the node is the selected node
        if (entity == selected_entity_) {
          node_flags |= ImGuiTreeNodeFlags_Selected;
        }

        ImGui::PushID(entity);

        // Start a horizontal group to align the tree node and the checkbox
        ImGui::BeginGroup();

        bool node_open = ImGui::TreeNodeEx(node->name.c_str(), node_flags);

        // If the tree node is open, we need a way to click the node itself, not just the arrow
        if (ImGui::IsItemClicked()) {
          selected_entity_ = entity;
        }

        // Calculate position for the checkbox to align it to the right
        float windowWidth = ImGui::GetWindowWidth();
        float checkboxWidth = ImGui::GetFrameHeight();  // Approximate width of a checkbox
        float offset = windowWidth - checkboxWidth
                       - ImGui::GetStyle().ItemSpacing.x * 3.f;  // Adjust this as needed

        // Move cursor and render checkbox
        ImGui::SameLine(offset);
        ImGui::Checkbox("##visible", &node->visible);  // ## to hide label but keep unique ID

        ImGui::EndGroup();

        // Pop ID to avoid conflicts with other elements
        ImGui::PopID();

        // Check for item selection
        if (ImGui::IsItemClicked()) {
          selected_entity_ = entity;
        }

        // Recursively display children if the node is open
        if (node_open && !node->children.empty()) {
          for (const auto child_entity : node->children) {
            display_scene_hierarchy(child_entity);
          }
          ImGui::TreePop();
        }
      }
    }

    void Gui::show_transform_component(const NodeComponent* node, const TransformComponent* transform) {
      ImGui::Text("Local Transform:");

      // Local position control
      auto position = transform->position();
      if (ImGui::DragFloat3("Local Position", &position.x, 0.1f)) {
        event_bus_.emit<TranslateEntityEvent>(TranslateEntityEvent{selected_entity_, position});
      }

      // Local rotation controls
      glm::vec3 euler = glm::degrees(eulerAngles(transform->rotation()));
      if (ImGui::DragFloat("Local X Rotation", &euler.x, 1.0f)) {
        event_bus_.emit<RotateEntityEvent>(
            RotateEntityEvent{selected_entity_, glm::quat(radians(euler))});
      }
      if (ImGui::DragFloat("Local Y Rotation", &euler.y, 1.0f)) {
        event_bus_.emit<RotateEntityEvent>(
            RotateEntityEvent{selected_entity_, glm::quat(radians(euler))});
      }
      if (ImGui::DragFloat("Local Z Rotation", &euler.z, 1.0f)) {
        event_bus_.emit<RotateEntityEvent>(
            RotateEntityEvent{selected_entity_, glm::quat(radians(euler))});
      }

      // Local scale control
      float local_scale = transform->scale_uniform();
      if (ImGui::DragFloat("Local Scale", &local_scale, 0.005f)) {
        event_bus_.emit<ScaleEntityEvent>(ScaleEntityEvent{selected_entity_, local_scale});
      }

      ImGui::Separator();
      ImGui::Text("World Transform:");

      glm::mat4 localTransform = translate(glm::mat4(1.0f), transform->position())
                                 * toMat4(transform->rotation())
                                 * scale(glm::mat4(1.0f), transform->scale());
      glm::mat4 parentWorldTransform
          = translate(glm::mat4(1.0f), transform->parent_position)
            * toMat4(transform->parent_rotation)
            * scale(glm::mat4(1.0f), glm::vec3(transform->parent_scale));
      glm::mat4 inverseParentWorldTransform = inverse(parentWorldTransform);
      glm::mat4 worldTransform = parentWorldTransform * localTransform;

      // World position control (taking parent transform into account)
      glm::vec3 world_position = glm::vec3(worldTransform[3]);
      if (ImGui::DragFloat3("World Position", &world_position.x, 0.1f)) {
        auto new_pos
            = glm::vec3(inverseParentWorldTransform * glm::vec4(world_position, 1.0f));
        event_bus_.emit<TranslateEntityEvent>(TranslateEntityEvent{selected_entity_, new_pos});
      }

      // World rotation controls (taking parent transform into account)
      glm::vec3 world_euler = degrees(eulerAngles(quat_cast(worldTransform)));
      if (ImGui::DragFloat("World X Rotation", &world_euler.x, 1.0f)) {
        auto new_rot = normalize(
            quat_cast(inverseParentWorldTransform * toMat4(glm::quat(radians(world_euler)))));
        event_bus_.emit<RotateEntityEvent>(RotateEntityEvent{selected_entity_, new_rot});
      }
      if (ImGui::DragFloat("World Y Rotation", &world_euler.y, 1.0f)) {
        auto new_rot = normalize(
            quat_cast(inverseParentWorldTransform * toMat4(glm::quat(radians(world_euler)))));
        event_bus_.emit<RotateEntityEvent>(RotateEntityEvent{selected_entity_, new_rot});
      }
      if (ImGui::DragFloat("World Z Rotation", &world_euler.z, 1.0f)) {
        auto new_rot = normalize(
            quat_cast(inverseParentWorldTransform * toMat4(glm::quat(radians(world_euler)))));
        event_bus_.emit<RotateEntityEvent>(RotateEntityEvent{selected_entity_, new_rot});
      }

      // World scale control (taking parent transform into account)
      float world_scale = length(glm::vec3(worldTransform[0]));
      if (ImGui::DragFloat("World Scale", &world_scale, 0.005f)) {
        auto new_scale = world_scale / transform->parent_scale;
        event_bus_.emit<ScaleEntityEvent>(ScaleEntityEvent{selected_entity_, new_scale});
      }
      ImGui::Text("AABB max: (%.3f, %.3f, %.3f)", node->bounds.max.x, node->bounds.max.y,
                  node->bounds.max.z);
      ImGui::Text("AABB min: (%.3f, %.3f, %.3f)", node->bounds.min.x, node->bounds.min.y,
                  node->bounds.min.z);

    }

    void Gui::show_mesh_component(const MeshComponent* mesh_component) {
      auto& mesh = repository_.meshes.get(mesh_component->mesh);

      for (auto& surface : mesh.surfaces) {
        auto& material = repository_.materials.get(surface.material);
        if (ImGui::TreeNode(material.name.c_str())) {
          auto& config = material.config;

          if (ImGui::CollapsingHeader("Albedo", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ImGui::ColorPicker4("Albedo Color", &config.constants.albedo_color.x)) {
              material.is_dirty = true;
            }
            bool use_texture = (config.constants.flags & kAlbedoTextureFlag) != 0;

            if (ImGui::Checkbox("Use Albedo Texture", &use_texture)) {
              if (use_texture) {
                config.constants.flags |= kAlbedoTextureFlag;
                material.is_dirty = true;
              } else {
                config.constants.flags &= ~kAlbedoTextureFlag;
                material.is_dirty = true;
              }
            }
          }

          if (ImGui::CollapsingHeader("Metallic-Roughness", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ImGui::SliderFloat("Metalness", &config.constants.metal_rough_factor.y, 0.0f, 1.0f,
                                   "%.2f")) {
              material.is_dirty = true;
            }
            if (ImGui::SliderFloat("Roughness", &config.constants.metal_rough_factor.x, 0.0f, 1.0f,
                                   "%.2f")) {
              material.is_dirty = true;
            }
            bool use_texture = (config.constants.flags & kMetalRoughTextureFlag) != 0;

            if (ImGui::Checkbox("Use Metal-Roughness Texture", &use_texture)) {
              if (use_texture) {
                config.constants.flags |= kMetalRoughTextureFlag;
                material.is_dirty = true;
              } else {
                config.constants.flags &= ~kMetalRoughTextureFlag;
                material.is_dirty = true;
              }
            }
          }

          if (ImGui::CollapsingHeader("Normal", ImGuiTreeNodeFlags_DefaultOpen)) {
            // normal factor?
            bool use_texture = (config.constants.flags & kNormalTextureFlag) != 0;

            if (ImGui::Checkbox("Use Normal Texture", &use_texture)) {
              if (use_texture) {
                config.constants.flags |= kNormalTextureFlag;
                material.is_dirty = true;
              } else {
                config.constants.flags &= ~kNormalTextureFlag;
                material.is_dirty = true;
              }
            }
          }

          if (ImGui::CollapsingHeader("Emissive", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ImGui::ColorPicker3("Emissive Color", &config.constants.emissiveColor.x)) {
              material.is_dirty = true;
            }
            if (ImGui::SliderFloat("Emissive Strength", &config.constants.emissiveStrength, 0.0f,
                                   10.0f, "%.2f")) {
              material.is_dirty = true;
            }
            bool use_texture = (config.constants.flags & kEmissiveTextureFlag) != 0;

            if (ImGui::Checkbox("Use Emissive Texture", &use_texture)) {
              if (use_texture) {
                config.constants.flags |= kEmissiveTextureFlag;
                material.is_dirty = true;
              } else {
                config.constants.flags &= ~kEmissiveTextureFlag;
                material.is_dirty = true;
              }
            }
          }

          if (ImGui::CollapsingHeader("Occlusion", ImGuiTreeNodeFlags_DefaultOpen)) {
            if (ImGui::DragFloat("Occlusion Strength", &config.constants.occlusionStrength, 0.01f,
                                 0.0f, 10.0f, "%.2f")) {
              material.is_dirty = true;
            }
            bool use_texture = (config.constants.flags & kOcclusionTextureFlag) != 0;

            if (ImGui::Checkbox("Use Occlusion Texture", &use_texture)) {
              if (use_texture) {
                config.constants.flags |= kOcclusionTextureFlag;
                material.is_dirty = true;
              } else {
                config.constants.flags &= ~kOcclusionTextureFlag;
                material.is_dirty = true;
              }
            }
          }

          if (ImGui::Checkbox("Double Sided", &config.double_sided)) {
            material.is_dirty = true;
          }
          if (ImGui::Checkbox("Transparent", &config.transparent)) {
            material.is_dirty = true;
          }
          if (ImGui::DragFloat("Alpha Cutoff", &config.constants.alpha_cutoff, 0.01f, 0.0f, 1.0f,
                               "%.2f")) {
            material.is_dirty = true;
          }

          ImGui::TreePop();
        }
      }
    }

    void Gui::show_directional_light_component(const DirectionalLightComponent* light,
                                               const TransformComponent* transform) {
      auto color = light->color();
      if (ImGui::ColorPicker3("Color", &color.x, ImGuiColorEditFlags_Float)) {
        event_bus_.emit<UpdateLightEvent>(
            UpdateLightEvent{selected_entity_, color, light->intensity()});
      }
      auto intensity = light->intensity();
      if (ImGui::SliderFloat("Intensity", &intensity, 0.0f, 150000.0f, "%.3f",
                             ImGuiSliderFlags_Logarithmic)) {
        event_bus_.emit<UpdateLightEvent>(
            UpdateLightEvent{selected_entity_, light->color(), intensity});
      }
      glm::vec3 euler_angles
          = degrees(eulerAngles(transform->rotation()));  // Convert quaternion to Euler angles

      float azimuth = euler_angles.y;     // Azimuth angle (yaw)
      float elevation = -euler_angles.x;  // Elevation angle (pitch)

      if (ImGui::SliderFloat("Azimuth", &azimuth, -89.f, 89.0f)) {
        light->is_dirty = true;
      }

      if (ImGui::SliderFloat("Elevation", &elevation, 1.f, 179.f)) {
        light->is_dirty = true;
      }

      // Update rotation quaternion based on user input
      auto new_rot = glm::quat(glm::radians(glm::vec3(-elevation, azimuth, 0.0f)));
      event_bus_.emit<RotateEntityEvent>({selected_entity_, new_rot});
    }

    void Gui::show_point_light_component(const PointLightComponent* light,
                                         const TransformComponent* transform) {
      auto color = light->color();
      if (ImGui::ColorPicker3("Color", &color.x, ImGuiColorEditFlags_Float)) {
        event_bus_.emit<UpdateLightEvent>(
            UpdateLightEvent{selected_entity_, color, light->intensity()});
      }
      auto intensity = light->intensity();
      if (ImGui::SliderFloat("Intensity", &intensity, 0.0f, 150000.0f, "%.3f",
                             ImGuiSliderFlags_Logarithmic)) {
        event_bus_.emit<UpdateLightEvent>(
            UpdateLightEvent{selected_entity_, light->color(), intensity});
      }

      auto range = light->range();
      if (ImGui::SliderFloat("Radius", &range, 0.0f, 100.0f, "%.3f",
                             ImGuiSliderFlags_Logarithmic)) {
        event_bus_.emit<UpdatePointLightEvent>(UpdatePointLightEvent{selected_entity_, range});
      }

      auto position = transform->position();
      if (ImGui::DragFloat3("Position", &position.x, 0.1f)) {
        event_bus_.emit<TranslateEntityEvent>({selected_entity_, position});
      }
    }

    void Gui::show_spotlight_component(const SpotLightComponent* light,
                                       const TransformComponent* transform) {
      auto color = light->color();
      if (ImGui::ColorPicker3("Color", &color.x, ImGuiColorEditFlags_Float)) {
        event_bus_.emit<UpdateLightEvent>(
            UpdateLightEvent{selected_entity_, color, light->intensity()});
      }
      auto intensity = light->intensity();
      if (ImGui::SliderFloat("Intensity", &intensity, 0.0f, 150000.0f, "%.3f",
                             ImGuiSliderFlags_Logarithmic)) {
        event_bus_.emit<UpdateLightEvent>(
            UpdateLightEvent{selected_entity_, light->color(), intensity});
      }

      auto range = light->range();
      if (ImGui::SliderFloat("Radius", &range, 0.0f, 100.0f, "%.3f",
                             ImGuiSliderFlags_Logarithmic)) {
        event_bus_.emit<UpdateSpotLightEvent>(UpdateSpotLightEvent{
            selected_entity_, range, light->inner_cone_cos(), light->outer_cone_cos()});
      }
      float innerDeg = glm::degrees(acosf(light->inner_cone_cos()));
      float outerDeg = glm::degrees(acosf(light->outer_cone_cos()));

      // Draw sliders in degrees
      ImGui::Text("Spotlight Angles (deg)");
      if (ImGui::SliderFloat("Inner Angle", &innerDeg, 0.0f, 90, "%.1f")) {
        light->is_dirty = true;
      }
      if (ImGui::SliderFloat("Outer Angle", &outerDeg, 0.0f, 90, "%.1f")) {
        light->is_dirty = true;
      }

      // Enforce that the outer angle is >= inner angle
      if (outerDeg < innerDeg) {
        outerDeg = innerDeg;
        light->is_dirty = true;
      }

      // Convert angles (in degrees) -> cosines (in radians)
      float innerRadians = glm::radians(innerDeg);
      float outerRadians = glm::radians(outerDeg);
      event_bus_.emit<UpdateSpotLightEvent>(
          UpdateSpotLightEvent{selected_entity_, range, cosf(innerRadians), cosf(outerRadians)});
    }

    void Gui::show_camera_component(const AnimationCameraComponent* camera) {
      ImGui::Text("Animation Camera:");
      ImGui::Text("Current Position: (%.2f, %.2f, %.2f)", camera->position().x,
                  camera->position().y, camera->position().z);
      ImGui::Text("Current Orientation (Quat): (%.2f, %.2f, %.2f, %.2f)", camera->orientation().x,
                  camera->orientation().y, camera->orientation().z, camera->orientation().w);
    }

    void Gui::show_camera_component(const FreeFlyCameraComponent* camera) {
      ImGui::Text("FreeFly Camera:");
      auto mouse_speed = camera->mouse_speed();
      if (ImGui::SliderFloat("Mouse Speed", &mouse_speed, 0.1f, 10.0f, "%.2f")) {
        event_bus_.emit<UpdateFreeFlyCameraEvent>(UpdateFreeFlyCameraEvent{
            selected_entity_, mouse_speed, camera->acceleration(), camera->damping(),
            camera->max_speed(), camera->fast_coef(), camera->slow_coef()});
      }
      auto acceleration = camera->acceleration();
      if (ImGui::SliderFloat("Acceleration", &acceleration, 0.1f, 100.0f, "%.2f")) {
        event_bus_.emit<UpdateFreeFlyCameraEvent>(UpdateFreeFlyCameraEvent{
            selected_entity_, camera->mouse_speed(), acceleration, camera->damping(),
            camera->max_speed(), camera->fast_coef(), camera->slow_coef()});
      }
      auto damping = camera->damping();
      if (ImGui::SliderFloat("Damping", &damping, 0.1f, 1, "%.2f")) {
        event_bus_.emit<UpdateFreeFlyCameraEvent>(UpdateFreeFlyCameraEvent{
            selected_entity_, camera->mouse_speed(), camera->acceleration(), damping,
            camera->max_speed(), camera->fast_coef(), camera->slow_coef()});
      }
      auto max_speed = camera->max_speed();
      if (ImGui::SliderFloat("Max Speed", &max_speed, 0.01f, 10.0f, "%.2f")) {
        event_bus_.emit<UpdateFreeFlyCameraEvent>(UpdateFreeFlyCameraEvent{
            selected_entity_, camera->mouse_speed(), camera->acceleration(), camera->damping(),
            max_speed, camera->fast_coef(), camera->slow_coef()});
      }
      auto fast_coef = camera->fast_coef();
      if (ImGui::SliderFloat("Fast Coefficient", &fast_coef, 0.1f, 10.0f, "%.2f")) {
        event_bus_.emit<UpdateFreeFlyCameraEvent>(UpdateFreeFlyCameraEvent{
            selected_entity_, camera->mouse_speed(), camera->acceleration(), camera->damping(),
            camera->max_speed(), fast_coef, camera->slow_coef()});
      }
      auto slow_coef = camera->slow_coef();
      if (ImGui::SliderFloat("Slow Coefficient", &slow_coef, 0.0001f, 1.0f, "%.4f")) {
        event_bus_.emit<UpdateFreeFlyCameraEvent>(UpdateFreeFlyCameraEvent{
            selected_entity_, camera->mouse_speed(), camera->acceleration(), camera->damping(),
            camera->max_speed(), camera->fast_coef(), slow_coef});
      }
    }

    void Gui::show_camera_component(const FirstPersonCameraComponent* camera) {
      ImGui::Text("FirstPerson Camera:");
      auto mouse_speed = camera->mouse_speed();
      if (ImGui::SliderFloat("Mouse Speed", &mouse_speed, 0.1f, 10.0f, "%.2f")) {
        event_bus_.emit<UpdateFirstPersonCameraEvent>(
            UpdateFirstPersonCameraEvent{selected_entity_, mouse_speed});
      }
    }

    void Gui::show_camera_component(const OrbitCameraComponent* camera) {
      ImGui::Text("Orbit Camera:");
      auto distance = camera->distance();
      if (ImGui::SliderFloat("Distance", &distance, camera->min_distance(), camera->max_distance(),
                             "%.2f")) {
        event_bus_.emit<UpdateOrbitCameraEvent>(UpdateOrbitCameraEvent{
            selected_entity_, distance, camera->yaw(), camera->pitch(), camera->orbit_speed(),
            camera->zoom_speed(), camera->pan_speed()});
      }
      auto yaw = camera->yaw();
      if (ImGui::SliderFloat("Yaw", &yaw, -180.0f, 180.0f, "%.2f")) {
        event_bus_.emit<UpdateOrbitCameraEvent>(UpdateOrbitCameraEvent{
            selected_entity_, camera->distance(), yaw, camera->pitch(), camera->orbit_speed(),
            camera->zoom_speed(), camera->pan_speed()});
      }
      auto pitch = camera->pitch();
      if (ImGui::SliderFloat("Pitch", &pitch, -90.0f, 90.0f, "%.2f")) {
        event_bus_.emit<UpdateOrbitCameraEvent>(UpdateOrbitCameraEvent{
            selected_entity_, camera->distance(), camera->yaw(), pitch, camera->orbit_speed(),
            camera->zoom_speed(), camera->pan_speed()});
      }
      auto orbit_speed = camera->orbit_speed();
      if (ImGui::SliderFloat("Orbit Speed", &orbit_speed, 0.1f, 10.0f, "%.2f")) {
        event_bus_.emit<UpdateOrbitCameraEvent>(UpdateOrbitCameraEvent{
            selected_entity_, camera->distance(), camera->yaw(), camera->pitch(), orbit_speed,
            camera->zoom_speed(), camera->pan_speed()});
      }
      auto zoom_speed = camera->zoom_speed();
      if (ImGui::SliderFloat("Zoom Speed", &zoom_speed, 0.01f, 1.0f, "%.2f")) {
        event_bus_.emit<UpdateOrbitCameraEvent>(UpdateOrbitCameraEvent{
            selected_entity_, camera->distance(), camera->yaw(), camera->pitch(),
            camera->orbit_speed(), zoom_speed, camera->pan_speed()});
      }
      auto pan_speed = camera->pan_speed();
      if (ImGui::SliderFloat("Pan Speed", &pan_speed, 0.1f, 100.0f, "%.2f")) {
        event_bus_.emit<UpdateOrbitCameraEvent>(UpdateOrbitCameraEvent{
            selected_entity_, camera->distance(), camera->yaw(), camera->pitch(),
            camera->orbit_speed(), camera->zoom_speed(), pan_speed});
      }
      ImGui::Text("Target: (%.2f, %.2f, %.2f)", camera->target().x, camera->target().y,
                  camera->target().z);  // Display Target
    }

    void Gui::show_projection_component(const PerspectiveProjectionComponent* projection) {
      ImGui::Text("Perspective Projection:");
      float fovInDegrees = projection->fov() * 180.0f / glm::pi<float>();
      if (ImGui::SliderFloat("Field of View (Degrees)", &fovInDegrees, 30.0f, 120.0f, "%.1f")) {
        // Convert the FOV back to radians when the user changes the slider value
        const float32 fov = fovInDegrees * (glm::pi<float>() / 180.0f);
        event_bus_.emit<UpdatePerspectiveProjectionEvent>(UpdatePerspectiveProjectionEvent{
            selected_entity_, fov, projection->near(), projection->far()});
      }
      auto near = projection->near();
      if (ImGui::SliderFloat("Near Clip", &near, 0.01f, 10.0f, "%.2f")) {
        event_bus_.emit<UpdatePerspectiveProjectionEvent>(UpdatePerspectiveProjectionEvent{
            selected_entity_, projection->fov(), near, projection->far()});
      }
      auto far = projection->far();
      if (ImGui::SliderFloat("Far Clip", &far, 10.0f, 10000.0f, "%.2f",
                             ImGuiSliderFlags_Logarithmic)) {
        event_bus_.emit<UpdatePerspectiveProjectionEvent>(UpdatePerspectiveProjectionEvent{
            selected_entity_, projection->fov(), projection->near(), far});
      }
    }

    void Gui::show_projection_component(const OrthographicProjectionComponent* projection) {
              ImGui::Text("Orthographic Projection:");
      auto left = projection->left();
      if (ImGui::SliderFloat("Left", &left, -100.0f, 100.0f, "%.2f")) {
        event_bus_.emit<UpdateOrthographicProjectionEvent>(UpdateOrthographicProjectionEvent{
            selected_entity_, left, projection->right(), projection->bottom(), projection->top(),
            projection->near(), projection->far()});
      }
      auto right = projection->right();
      if (ImGui::SliderFloat("Right", &right, -100.0f, 100.0f, "%.2f")) {
        event_bus_.emit<UpdateOrthographicProjectionEvent>(UpdateOrthographicProjectionEvent{
            selected_entity_, projection->left(), right, projection->bottom(), projection->top(),
            projection->near(), projection->far()});
      }
      auto bottom = projection->bottom();
      if (ImGui::SliderFloat("Bottom", &bottom, -100.0f, 100.0f, "%.2f")) {
        event_bus_.emit<UpdateOrthographicProjectionEvent>(UpdateOrthographicProjectionEvent{
            selected_entity_, projection->left(), projection->right(), bottom,
            projection->top(), projection->near(), projection->far()});
      }
      auto top = projection->top();
      if (ImGui::SliderFloat("Top", &top, -100.0f, 100.0f, "%.2f")) {
        event_bus_.emit<UpdateOrthographicProjectionEvent>(UpdateOrthographicProjectionEvent{
            selected_entity_, projection->left(), projection->right(), projection->bottom(),
            top, projection->near(), projection->far()});
      }
      auto near = projection->near();
      if (ImGui::SliderFloat("Near Clip", &near, 0.01f, 10.0f, "%.2f")) {
        event_bus_.emit<UpdateOrthographicProjectionEvent>(UpdateOrthographicProjectionEvent{
            selected_entity_, projection->left(), projection->right(), projection->bottom(),
            projection->top(), near, projection->far()});
      }
      auto far = projection->far();
      if (ImGui::SliderFloat("Far Clip", &far, 10.0f, 10000.0f, "%.2f",
                             ImGuiSliderFlags_Logarithmic)) {
        event_bus_.emit<UpdateOrthographicProjectionEvent>(UpdateOrthographicProjectionEvent{
            selected_entity_, projection->left(), projection->right(), projection->bottom(),
            projection->top(), projection->near(), far});
      }
    }

    void Gui::show_node_component() {
      if (selected_entity_ != invalid_entity) {
        auto selected_node = repository_.scene_graph.find(selected_entity_);
        ImGui::Text(selected_node->name.c_str());

        const auto transform = repository_.transform_components.find(selected_entity_);
        if (transform != nullptr) {
          if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
            show_transform_component(selected_node, transform);
          }
        }

        const auto mesh = repository_.mesh_components.find(selected_entity_);
        if (mesh != nullptr) {
          if (ImGui::CollapsingHeader("Materials", ImGuiTreeNodeFlags_DefaultOpen)) {
            show_mesh_component(mesh);
          }
        }

        if (const auto dir_light = repository_.directional_light_components.find(selected_entity_);
            dir_light != nullptr && transform != nullptr) {
          if (ImGui::CollapsingHeader("Directional Light", ImGuiTreeNodeFlags_DefaultOpen)) {
            show_directional_light_component(dir_light, transform);
          }
        }

        if (const auto point_light = repository_.point_light_components.find(selected_entity_);
            point_light != nullptr && transform != nullptr) {
          if (ImGui::CollapsingHeader("Point Light", ImGuiTreeNodeFlags_DefaultOpen)) {
            show_point_light_component(point_light, transform);
          }
        }

        if (const auto spot_light = repository_.spot_light_components.find(selected_entity_);
            spot_light != nullptr && transform != nullptr) {
          if (ImGui::CollapsingHeader("Spot Light", ImGuiTreeNodeFlags_DefaultOpen)) {
            show_spotlight_component(spot_light, transform);
          }
        }

        if (const auto camera = repository_.animation_camera_components.find(selected_entity_); camera != nullptr) {
          if (ImGui::CollapsingHeader("Animation Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
            show_camera_component(camera);
          }
        } else if (const auto camera = repository_.first_person_camera_components.find(selected_entity_); camera != nullptr) {
          if (ImGui::CollapsingHeader("First Person Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
            show_camera_component(camera);
          }
        } else if (const auto camera = repository_.free_fly_camera_components.find(selected_entity_); camera != nullptr) {
          if (ImGui::CollapsingHeader("Free Fly Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
            show_camera_component(camera);
          }
        } else if (const auto camera = repository_.orbit_camera_components.find(selected_entity_); camera != nullptr) {
          if (ImGui::CollapsingHeader("Orbit Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
            show_camera_component(camera);
          }
        }

        if (const auto projection
            = repository_.perspective_projection_components.find(selected_entity_);
            projection != nullptr) {
          if (ImGui::CollapsingHeader("Perspective Projection", ImGuiTreeNodeFlags_DefaultOpen)) {
            show_projection_component(projection);
          }
        } else if (const auto projection
                   = repository_.orthographic_projection_components.find(selected_entity_);
                   projection != nullptr) {
          if (ImGui::CollapsingHeader("Orthographic Projection", ImGuiTreeNodeFlags_DefaultOpen)) {
            show_projection_component(projection);
          }
        }
      }
    }

    void Gui::show_scene_hierarchy_window() {
      ImVec2 windowAvail = ImGui::GetContentRegionAvail();
      ImVec2 childSize = ImVec2(0, windowAvail.y * 0.4f);

      // Child window for the scrollable scene hierarchy
      if (ImGui::BeginChild("HierarchyTree", childSize, true)) {
        constexpr Entity root_entity = 0;  // TODO get root entity
        if (selected_entity_ == invalid_entity) {
          selected_entity_ = root_entity;
        }
        display_scene_hierarchy(root_entity);
      }
      ImGui::EndChild();

      ImGui::Separator();

      childSize = ImVec2(0, windowAvail.y - childSize.y);  // Remaining space for the selected node
      if (ImGui::BeginChild("SelectedNodeDetails", childSize, false)) {
        if (selected_entity_ != invalid_entity) {
          show_node_component();
        }
      }
      ImGui::EndChild();
    }
}  // namespace gestalt