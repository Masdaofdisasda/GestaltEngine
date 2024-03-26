#include "imgui_gui.h"

#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_vulkan.h>

#include <ImGuizmo.h>
#include <ImGuiFileDialog.h>

#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/random.hpp>

#include "vk_initializers.h"

void imgui_gui::init(vk_gpu& gpu, sdl_window& window,
                     const std::shared_ptr<vk_swapchain>& swapchain, gui_actions& actions) {
  gpu_ = gpu;
  window_ = window;
  swapchain_ = swapchain;
  actions_ = actions;
  deletion_service_.init(gpu_.device, gpu_.allocator);

  // 1: create descriptor pool for IMGUI
  //  the size of the pool is very oversize, but it's copied from imgui demo
  //  itself.
  VkDescriptorPoolSize pool_sizes[] = {{VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
                                       {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
                                       {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
                                       {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
                                       {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
                                       {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
                                       {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
                                       {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
                                       {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
                                       {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
                                       {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}};

  VkDescriptorPoolCreateInfo pool_info = {};
  pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
  pool_info.maxSets = 1000;
  pool_info.poolSizeCount = (uint32_t)std::size(pool_sizes);
  pool_info.pPoolSizes = pool_sizes;

  VkDescriptorPool imguiPool;
  VK_CHECK(vkCreateDescriptorPool(gpu_.device, &pool_info, nullptr, &imguiPool));

  // 2: initialize imgui library

  // this initializes the core structures of imgui
  ImGui::CreateContext();

  // this initializes imgui for SDL
  ImGui_ImplSDL2_InitForVulkan(window_.handle);

  VkPipelineRenderingCreateInfoKHR pipelineRenderingCreateInfo = {
    .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
    .colorAttachmentCount = 1,
    .pColorAttachmentFormats = &swapchain_->swapchain_image_format,
  };

  // this initializes imgui for Vulkan
  ImGui_ImplVulkan_InitInfo init_info = {};
  init_info.Instance = gpu_.instance;
  init_info.PhysicalDevice = gpu_.chosen_gpu;
  init_info.Device = gpu_.device;
  init_info.Queue = gpu_.graphics_queue;
  init_info.DescriptorPool = imguiPool;
  init_info.MinImageCount = 3;
  init_info.ImageCount = 3;
  init_info.UseDynamicRendering = true;
  init_info.PipelineRenderingCreateInfo = pipelineRenderingCreateInfo;

  init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

  ImGui_ImplVulkan_Init(&init_info);

  // execute a gpu command to upload imgui font textures
  gpu_.immediate_submit([&](VkCommandBuffer cmd) { ImGui_ImplVulkan_CreateFontsTexture(); });

  // clear font textures from cpu data
  ImGui_ImplVulkan_DestroyFontsTexture();

  // add the destroy the imgui created structures
  // NOTE: i think ImGui_ImplVulkan_Shutdown() destroy the imguiPool
  deletion_service_.push(imguiPool);
  deletion_service_.push_function([this]() { ImGui::DestroyContext(); });
  deletion_service_.push_function([this]() { ImGui_ImplVulkan_Shutdown(); });
}

void imgui_gui::cleanup() {
  deletion_service_.flush();
}

void imgui_gui::draw(VkCommandBuffer cmd, VkImageView target_image_view) {
  VkRenderingAttachmentInfo colorAttachment
      = vkinit::attachment_info(target_image_view, nullptr, VK_IMAGE_LAYOUT_GENERAL);
  VkRenderingInfo renderInfo
      = vkinit::rendering_info(swapchain_->swapchain_extent, &colorAttachment, nullptr);

  vkCmdBeginRendering(cmd, &renderInfo);

  ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

  vkCmdEndRendering(cmd);
}

void imgui_gui::update(const SDL_Event& e) {
  ImGui_ImplSDL2_ProcessEvent(&e);
}

void imgui_gui::guizmo() {
  ImGuizmo::BeginFrame();

  auto [viewCam, proj] = actions_.get_database().get_camera(0);
  proj[1][1] *= -1;  // Flip the Y-axis for opengl like system
  glm::mat4 identity = glm::mat4(1.0f);

  // Convert glm matrices to arrays for ImGuizmo
  float* view = glm::value_ptr(viewCam);
  float* projection = glm::value_ptr(proj);
  float model[16];

  // Setup for using Gizmo without a separate window
  ImGuiIO& io = ImGui::GetIO();

  ImVec2 viewManipulatorPosition
      = ImVec2(10, io.DisplaySize.y - 128 - 10);  // Assuming 128x128 view manipulator size, 10
                                                  // pixels padding from the bottom and left edges.

  ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);

  // Draw the view manipulator
  float camDistance = 10.0f;  // Adjust this value as needed for your scene
  ImGuizmo::ViewManipulate(view, camDistance, viewManipulatorPosition, ImVec2(128, 128),
                           0xB4101010);

  const auto transform_component
      = actions_.get_database().get_transform_component(selected_entity_);
  if (transform_component.has_value()) {
    auto& transform = transform_component.value().get();

    glm::vec3 t = transform.position;
    glm::quat r = transform.rotation;
    glm::vec3 s = glm::vec3(transform.scale);

    float* translation = &t.x;
    float* rotation = &r.x;
    float* scale = &s.x;

    ImGuizmo::RecomposeMatrixFromComponents(translation, rotation, scale, model);

    if (Manipulate(view, projection, ImGuizmo::TRANSLATE, ImGuizmo::LOCAL, model)) {

      // Convert the model matrix back to components
      ImGuizmo::DecomposeMatrixToComponents(model, translation, rotation, scale);

      transform.position
          = glm::vec3(translation[0], translation[1], translation[2]);
      transform.is_dirty = true;
    }
  }
}

void imgui_gui::check_file_dialog() {
  if (ImGuiFileDialog::Instance()->Display("ChooseGLTF")) {

    // Check if user confirmed the selection
    if (ImGuiFileDialog::Instance()->IsOk()) {
      std::string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
      std::string filePath = ImGuiFileDialog::Instance()->GetCurrentPath();

      fmt::print("Importing File: {}\n", filePathName);
      actions_.load_gltf(filePathName);
    }

    // Close the dialog after retrieval
    ImGuiFileDialog::Instance()->Close();
  }
}


void imgui_gui::menu_bar() {
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
      if (ImGui::BeginMenu("Add Camera")) {
        if (ImGui::MenuItem("Free Fly Camera")) {
          actions_.add_camera();
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
      ImGui::MenuItem("Light View", nullptr, &show_lights_);
      ImGui::MenuItem("Stats", nullptr, &show_stats_);
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


    ImGui::EndMainMenuBar();
  }

  // Depending on the action, display appropriate controls
  if (current_action_ == action::add_point_light) {
    if (ImGui::Begin("Add Point Light", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
      static glm::vec3 color = glm::vec3(1.0f);  // Default to white
      static float intensity = 1.0f;
      static glm::vec3 position = glm::vec3(0.0f);

      ImGui::ColorEdit3("Color", &color.x);
      ImGui::SliderFloat("Intensity", &intensity, 0.0f, 100.0f, "%.3f",
                         ImGuiSliderFlags_Logarithmic);
      ImGui::InputFloat3("Position", &position.x);

      if (ImGui::Button("Add Light")) {
        actions_.get_component_factory().create_point_light(color, intensity, position);

        current_action_ = action::none;
      }
      ImGui::SameLine();
      if (ImGui::Button("Cancel")) {
        current_action_ = action::none;
      }

      ImGui::Separator();

      auto& root = actions_.get_database().get_node_component(root_entity).value().get();

      static glm::vec3 min_bounds = root.bounds.min;
      static glm::vec3 max_bounds = root.bounds.max;
      static glm::vec2 intensity_range = glm::vec2(1.f);
      static int light_count = 10;

      ImGui::InputFloat3("Min Bounds", &min_bounds.x);
      ImGui::InputFloat3("Max Bounds", &max_bounds.x);
      ImGui::SliderFloat2("Intensity Range", &intensity_range.x, 0.0f, 100.0f, "%.3f",
                         ImGuiSliderFlags_Logarithmic);
      ImGui::InputInt("Light Count", &light_count);

      if (ImGui::Button("Generate")) {
        // Calculate grid spacing based on the number of lights
        int grid_count = std::ceil(std::pow(light_count, 1.0f / 3.0f));
        glm::vec3 grid_spacing = (max_bounds - min_bounds) / static_cast<float>(grid_count);

        // Iterate over grid points and place lights
        for (int i = 0; i < grid_count; ++i) {
          for (int j = 0; j < grid_count; ++j) {
            for (int k = 0; k < grid_count; ++k) {
              // Calculate position within the grid
              glm::vec3 position
                  = min_bounds
                    + glm::vec3(i * grid_spacing.x, j * grid_spacing.y, k * grid_spacing.z);
              // Randomize position slightly within grid cell
              position += grid_spacing;

              glm::vec3 color = linearRand(glm::vec3(0.0f), glm::vec3(1.0f));  // Random color
              float intensity = glm::linearRand(
                  intensity_range.x, intensity_range.y);  // Random intensity in range [0.5, 10.0]

              actions_.get_component_factory().create_point_light(color, intensity, position);
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
      static float intensity = 1.0f;
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

void imgui_gui::stats() {
  if (ImGui::Begin("Stats")) {
    const auto& stats = actions_.get_stats();

    ImGui::Text("frametime %f ms", stats.frametime);
    ImGui::Text("draw time %f ms", stats.mesh_draw_time);
    ImGui::Text("update time %f ms", stats.scene_update_time);
    ImGui::Text("triangles %i", stats.triangle_count);
    ImGui::Text("draws %i", stats.drawcall_count);
  }
  ImGui::End();
}

void imgui_gui::lights() {
  if (ImGui::Begin("Lights")) {

      const char* lightOptions[]
        = {"Directional Light", "Point Light", "Spot Light"};
    static int currentOption = 0; // default to directional light
      ImGui::Combo("Select Type", &currentOption, lightOptions, IM_ARRAYSIZE(lightOptions));

    std::vector<std::pair<entity, light_component>> lights;
    if (currentOption == 0) {
       lights = actions_.get_database().
          get_lights(light_type::directional);
    } else if (currentOption == 1) {
           lights = actions_.get_database().
          get_lights(light_type::point);
    } else if (currentOption == 2) {
           lights = actions_.get_database().
          get_lights(light_type::spot);
    }

    int selectedLightIndex = 0;  // Default to the first light
    if (!lights.empty()) {

      ImGui::SliderInt("Select Light", &selectedLightIndex, 0, lights.size() -1);

      auto& [entity, light_component] = lights[selectedLightIndex];
      const auto transform_component
          = actions_.get_database().get_transform_component(entity).value();

      show_light_component(light_component, transform_component.get());
    }

  }
  ImGui::End();
}

void imgui_gui::scene_graph() {
  float windowWidth = 300.0f;                               // Width of the ImGui window
  uint32_t menuBarHeight = 18;
  float windowHeight
      = window_.extent.height - menuBarHeight;  // Full height of the application window
  ImVec2 windowPos
      = ImVec2(window_.extent.width - windowWidth, menuBarHeight);  // Position to the right
  ImVec2 windowSize = ImVec2(windowWidth, windowHeight);    // Size of the ImGui window

  ImGui::SetNextWindowPos(windowPos);
  ImGui::SetNextWindowSize(windowSize);
  if (ImGui::Begin("Scene Graph", nullptr)) {
    show_scene_hierarchy_window();
  }
  ImGui::End();
}

void imgui_gui::new_frame() {
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

  if (show_stats_) {
    stats();
  }

  if (show_lights_) {
    lights();
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

  check_file_dialog();

  ImGui::Render();
}

void imgui_gui::light_adaptation_settings() {
  if (ImGui::Begin("Light Adaptation")) {
    render_config& config = actions_.get_render_config();

    // Use ImGuiSliderFlags_Logarithmic to create a logarithmic slider.
    // The slider's value directly represents the actual value being modified.
    ImGui::SliderFloat("Light Adaption Speed", &config.light_adaptation.adaptation_speed_dark2light,
                       0.0001f, 100.0f, "Light: %.4f", ImGuiSliderFlags_Logarithmic);

    ImGui::SliderFloat("Dark Adaption Speed", &config.light_adaptation.adaptation_speed_light2dark,
                       0.0001f, 100.0f, "Dark: %.4f", ImGuiSliderFlags_Logarithmic);

    ImGui::SliderFloat("Min Luminance", &config.light_adaptation.min_luminance, 0.0001f, 1.0f,
                       "Min: %.4f", ImGuiSliderFlags_Logarithmic);

    ImGui::SliderFloat("Max Luminance", &config.light_adaptation.max_luminance, 1.0f, 100.0f,
                       "Max: %.4f", ImGuiSliderFlags_Logarithmic);
  }
  ImGui::End();
}


void imgui_gui::sky_settings() {
  if (ImGui::Begin("Sky & Atmosphere")) {
    auto& [rayleighScattering, unused1, ozoneAbsorption, unused2, mieScattering, unused3]
        = actions_.get_render_config().skybox;

    // Rayleigh Scattering Coefficients
    ImGui::Text("Rayleigh Scattering");
    ImGui::SliderFloat("Rayleigh Red", &rayleighScattering.x, 0.0f, 1e-5f, "%.8f",
                       ImGuiSliderFlags_Logarithmic);
    ImGui::SliderFloat("Rayleigh Green", &rayleighScattering.y, 0.0f, 1e-5f, "%.8f",
                       ImGuiSliderFlags_Logarithmic);
    ImGui::SliderFloat("Rayleigh Blue", &rayleighScattering.z, 0.0f, 1e-5f, "%.8f",
                       ImGuiSliderFlags_Logarithmic);

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

void imgui_gui::grid_settings() {
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

void imgui_gui::shading_settings() {
  if (ImGui::Begin("Shading")) {
    auto& config = actions_.get_render_config();

    ImGui::Checkbox("Always Opaque", &config.always_opaque);

    const char* shadow_mode_options[] = {"On", "Off"};
    int current_shadow_mode = config.lighting.shadow_mode;
    if (ImGui::Combo("Shadow Mode", &current_shadow_mode, shadow_mode_options,
                     IM_ARRAYSIZE(shadow_mode_options))) {
      config.lighting.shadow_mode = current_shadow_mode;
    }

    ImGui::SliderFloat("Shadow Bias", &config.shadow.shadow_bias, 1.0f, 2.0f, "%.4f");
    ImGui::SliderFloat("Shadow Slope Bias", &config.shadow.shadow_slope_bias, 1.0f, 2.0f, "%.4f");

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

    if (ImGui::CollapsingHeader("SSAO Settings")) {
      ImGui::Checkbox("Enable SSAO", &config.enable_ssao);
      ImGui::SliderInt("SSAO Quality", &config.ssao_quality, 1, 4);
      ImGui::Checkbox("Show SSAO", &config.ssao.show_ssao_only);
      ImGui::SliderFloat("Scale", &config.ssao.scale, 0.0f, 10.0f, "%.3f");
      ImGui::SliderFloat("Bias", &config.ssao.bias, 0.0f, 1.0f, "%.3f");
      ImGui::SliderFloat("Radius", &config.ssao.radius, 0.0f, .5f, "%.2f");
      ImGui::SliderFloat("Attenuation Scale", &config.ssao.attScale, 0.0f, 2.0f, "%.2f");
      ImGui::SliderFloat("Distance Scale", &config.ssao.distScale, 0.0f, 10.0f, "%.2f");
    }
  }
  ImGui::End();
}

void imgui_gui::tone_map_settings() {
  render_config& config = actions_.get_render_config();

  if (ImGui::CollapsingHeader("HDR Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::Checkbox("Enable HDR", &config.enable_hdr);
    ImGui::SliderInt("Bloom Quality", &config.bloom_quality, 1, 4);
    const char* toneMappingOptions[]
        = {"Reinhard Extended", "Uncharted 2", "ACES approximation", "ACES fitted"};
    int currentOption = config.hdr.toneMappingOption;
    if (ImGui::Combo("Tone Mapping Option", &currentOption, toneMappingOptions,
                     IM_ARRAYSIZE(toneMappingOptions))) {
      config.hdr.toneMappingOption = currentOption;
    }

    ImGui::Checkbox("Show Bright Pass", &config.hdr.show_bright_pass);
    ImGui::SliderFloat("Exposure", &config.hdr.exposure, 0.1f, 2.0f, "%.3f");
    ImGui::SliderFloat("Max White", &config.hdr.maxWhite, 0.1f, 2.5f, "%.3f");
    ImGui::SliderFloat("Bloom Strength", &config.hdr.bloomStrength, 0.0f, 2.f, "%.2f");
    ImGui::ColorPicker4("Lift", &config.hdr.lift[0], ImGuiColorEditFlags_Float);
    ImGui::ColorPicker4("Gamma", &config.hdr.gamma[0], ImGuiColorEditFlags_Float);
    ImGui::ColorPicker4("Gain", &config.hdr.gain[0], ImGuiColorEditFlags_Float);
  }

  if (ImGui::CollapsingHeader("Streak Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::SliderFloat("Intensity", &config.streaks.intensity, 0.0f, 2.0f);
    ImGui::SliderFloat("Attenuation", &config.streaks.attenuation, 0.0f, 2.0f);
    ImGui::SliderInt("Streak Samples", &config.streaks.streak_samples, 1, 10);
    ImGui::SliderInt("Number of Streaks", &config.streaks.num_streaks, 1, 10);
  }
}


void imgui_gui::display_scene_hierarchy(const entity entity) {
  const auto& node_optional = actions_.get_database().get_node_component(entity);
  if (node_optional.has_value()) {
    node_component& node = node_optional.value().get();

        ImGuiTreeNodeFlags node_flags
        = ImGuiTreeNodeFlags_OpenOnArrow;

    // If the node has no children, make it a leaf node
    if (node.children.empty()) {
      node_flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }

    // Check if the node is the selected node
    if (entity == selected_entity_) {
      node_flags |= ImGuiTreeNodeFlags_Selected;
    }

    ImGui::PushID(entity);

    // Start a horizontal group to align the tree node and the checkbox
    ImGui::BeginGroup();

    bool node_open = ImGui::TreeNodeEx(node.name.c_str(), node_flags);

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
    ImGui::Checkbox("##visible", &node.visible);  // ## to hide label but keep unique ID

    ImGui::EndGroup();

    // Pop ID to avoid conflicts with other elements
    ImGui::PopID();

    // Check for item selection
    if (ImGui::IsItemClicked()) {
      selected_entity_ = entity;
    }

    // Recursively display children if the node is open
    if (node_open && !node.children.empty()) {
      for (const auto child_entity : node.children) {
        display_scene_hierarchy(child_entity);
      }
      ImGui::TreePop();
    }
  }
}

void imgui_gui::show_transform_component(node_component& node, transform_component& transform) {
  ImGui::Text("Local Transform:");

  // Local position control
  if (ImGui::DragFloat3("Local Position", &transform.position.x, 0.1f)) {
    transform.is_dirty = true;
  }

  // Local rotation controls
  glm::vec3 euler = degrees(eulerAngles(transform.rotation));
  if (ImGui::DragFloat("Local X Rotation", &euler.x, 1.0f)) {
    transform.rotation = glm::quat(radians(euler));
    transform.is_dirty = true;
  }
  if (ImGui::DragFloat("Local Y Rotation", &euler.y, 1.0f)) {
    transform.rotation = glm::quat(radians(euler));
    transform.is_dirty = true;
  }
  if (ImGui::DragFloat("Local Z Rotation", &euler.z, 1.0f)) {
    transform.rotation = glm::quat(radians(euler));
    transform.is_dirty = true;
  }

  // Local scale control
  float local_scale = transform.scale;
  if (ImGui::DragFloat("Local Scale", &local_scale, 0.1f)) {
    transform.scale = local_scale;
    transform.is_dirty = true;
  }

  ImGui::Separator();
  ImGui::Text("World Transform:");

  glm::mat4 localTransform = glm::translate(glm::mat4(1.0f), transform.position)
                             * glm::toMat4(transform.rotation)
                             * glm::scale(glm::mat4(1.0f), glm::vec3(transform.scale));
  glm::mat4 parentWorldTransform = glm::translate(glm::mat4(1.0f), transform.parent_position)
                             * glm::toMat4(transform.parent_rotation)
                             * glm::scale(glm::mat4(1.0f), glm::vec3(transform.parent_scale));
  glm::mat4 inverseParentWorldTransform = glm::inverse(parentWorldTransform);
  glm::mat4 worldTransform = parentWorldTransform * localTransform;

  // World position control (taking parent transform into account)
  glm::vec3 world_position = glm::vec3(worldTransform[3]);
  if (ImGui::DragFloat3("World Position", &world_position.x, 0.1f)) {
    transform.position = glm::vec3(inverseParentWorldTransform * glm::vec4(world_position, 1.0f));
    transform.is_dirty = true;
  }

  // World rotation controls (taking parent transform into account)
  glm::vec3 world_euler = degrees(eulerAngles(quat_cast(worldTransform)));
  if (ImGui::DragFloat("World X Rotation", &world_euler.x, 1.0f)) {
    transform.rotation = normalize(quat_cast(
        inverseParentWorldTransform
        * toMat4(glm::quat(radians(world_euler)))));

    transform.is_dirty = true;
  }
  if (ImGui::DragFloat("World Y Rotation", &world_euler.y, 1.0f)) {
    transform.rotation = normalize(
        quat_cast(inverseParentWorldTransform * toMat4(glm::quat(radians(world_euler)))));
    transform.is_dirty = true;
  }
  if (ImGui::DragFloat("World Z Rotation", &world_euler.z, 1.0f)) {
    transform.rotation = normalize(
        quat_cast(inverseParentWorldTransform * toMat4(glm::quat(radians(world_euler)))));
    transform.is_dirty = true;
  }

  // World scale control (taking parent transform into account)
  float world_scale = glm::length(glm::vec3(worldTransform[0]));
  if (ImGui::DragFloat("World Scale", &world_scale, 0.1f)) {
    transform.scale = world_scale / transform.parent_scale;
    transform.is_dirty = true;
  }

  ImGui::DragFloat3("AABB max", &node.bounds.max.x);
  ImGui::DragFloat3("AABB min", &node.bounds.min.x);
}

void imgui_gui::show_mesh_component(mesh_component& mesh_component) {
  auto& mesh = actions_.get_database().get_mesh(mesh_component.mesh);

  for (auto surface_index : mesh.surfaces) {
    auto& surface = actions_.get_database().get_surface(surface_index);
    auto& material = actions_.get_database().get_material(surface.material);
    if (ImGui::TreeNode(material.name.c_str())) {
      auto& config = material.config;

      // TODO update materials based on GUI changes
      if (ImGui::CollapsingHeader("Albedo", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragFloat4("Albedo Color", &config.constants.albedo_factor.x, 0.01f, 0.0f, 1.0f,
                          "%.2f");
        ImGui::Checkbox("Use Texture", &config.use_albedo_tex);
        if (config.use_albedo_tex) {
          ImGui::Text("URI: %s", config.albedo_uri.c_str());
        }
      }

      if (ImGui::CollapsingHeader("Metallic-Roughness", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragFloat2("Metal-Rough Factor", &config.constants.metal_rough_factor.x, 0.01f, 0.0f,
                          1.0f, "%.2f");
        ImGui::Checkbox("Use Texture", &config.use_metal_rough_tex);
        if (config.use_metal_rough_tex) {
          ImGui::Text("URI: %s", config.metal_rough_uri.c_str());
        }
      }
      /*
      if (ImGui::CollapsingHeader("Normal Map", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragFloat("Scale", &config.constants.normal_scale, 0.01f, 0.0f, 10.0f, "%.2f");
        ImGui::Checkbox("Use Texture", &config.use_normal_tex);
        if (config.use_normal_tex) {
          ImGui::Text("URI: %s", config.normal_uri.c_str());
        }
      }*/

      if (ImGui::CollapsingHeader("Emissive", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragFloat3("Emmissive Factor", &config.constants.emissiveFactor.x, 0.01f, 0.0f,
                          10.0f, "%.2f");
        ImGui::Checkbox("Use Texture", &config.use_emissive_tex);
        if (config.use_emissive_tex) {
          ImGui::Text("URI: %s", config.emissive_uri.c_str());
        }
      }

      if (ImGui::CollapsingHeader("Occlusion", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::DragFloat("Occlusion Strength", &config.constants.occlusionStrength, 0.01f, 0.0f,
                         10.0f, "%.2f");
        ImGui::Checkbox("Use Texture", &config.use_occlusion_tex);
        if (config.use_occlusion_tex) {
          ImGui::Text("URI: %s", config.occlusion_uri.c_str());
        }
      }

      ImGui::Checkbox("Double Sided", &config.double_sided);
      ImGui::Checkbox("Transparent", &config.transparent);
      ImGui::DragFloat("Alpha Cutoff", &config.constants.alpha_cutoff, 0.01f, 0.0f, 1.0f, "%.2f");

      ImGui::TreePop();
    }
  }
}

void imgui_gui::show_light_component(light_component& light, transform_component& transform) {
  ImGui::ColorPicker3("Color", &light.color.x, ImGuiColorEditFlags_Float);
  ImGui::SliderFloat("Intensity", &light.intensity, 0.0f, 100.0f, "%.3f",
                     ImGuiSliderFlags_Logarithmic);
  if (light.type == light_type::point) {
    if (ImGui::DragFloat3("Position", &transform.position.x, 0.1f)) {
      light.is_dirty = true;
    }
  }
  if (light.type == light_type::directional) {
    glm::vec3 euler_angles
        = degrees(glm::eulerAngles(transform.rotation));  // Convert quaternion to Euler angles

    float azimuth = euler_angles.y;    // Azimuth angle (yaw)
    float elevation = -euler_angles.x;  // Elevation angle (pitch)

    if (ImGui::SliderFloat("Azimuth", &azimuth, -89.f, 89.0f)) {
      light.is_dirty = true;
    }

    if (ImGui::SliderFloat("Elevation", &elevation, 1.f, 179.f)) {
      light.is_dirty = true;
    }

    // Update rotation quaternion based on user input
    transform.rotation = glm::quat(radians(glm::vec3(-elevation, azimuth, 0.0f)));
  }
}

void imgui_gui::show_node_component() {
  if (selected_entity_ != invalid_entity) {
    auto& selected_node = actions_.get_database().get_node_component(selected_entity_)->get();
    ImGui::Text(selected_node.name.c_str());

    const auto transform_optional
        = actions_.get_database().get_transform_component(selected_entity_);
    if (transform_optional.has_value()) {
      if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
        show_transform_component(selected_node, transform_optional.value().get());
      }
    }

    const auto& mesh_optional = actions_.get_database().get_mesh_component(selected_entity_);
    if (mesh_optional.has_value()) {
      if (ImGui::CollapsingHeader("Materials", ImGuiTreeNodeFlags_DefaultOpen)) {
        show_mesh_component(mesh_optional.value().get());
      }
    }

    const auto& light_optional = actions_.get_database().get_light_component(selected_entity_);
    if (light_optional.has_value() && transform_optional.has_value()) {
      if (ImGui::CollapsingHeader("Light", ImGuiTreeNodeFlags_DefaultOpen)) {
        show_light_component(light_optional.value().get(), transform_optional.value().get());
      }
    }
  }
}

void imgui_gui::show_scene_hierarchy_window() {
  ImVec2 windowAvail = ImGui::GetContentRegionAvail();
  ImVec2 childSize = ImVec2(0, windowAvail.y * 0.4f);

  // Child window for the scrollable scene hierarchy
  if (ImGui::BeginChild("HierarchyTree", childSize, true)) {
    constexpr entity root_entity = 0; //TODO get root entity
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
