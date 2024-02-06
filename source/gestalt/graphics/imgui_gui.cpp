#include "imgui_gui.h"

#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_vulkan.h>

#include <glm/gtc/quaternion.hpp>

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
  init_info.ColorAttachmentFormat = swapchain_->swapchain_image_format;

  init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

  ImGui_ImplVulkan_Init(&init_info, VK_NULL_HANDLE);

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

void imgui_gui::menu_bar() {
  if (ImGui::BeginMainMenuBar()) {
    if (ImGui::BeginMenu("File")) {
      if (ImGui::MenuItem("Exit")) {
        actions_.exit();
      }
      // Add more menu items here if needed
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
          // Code to add an Orbit Camera
        }
        if (ImGui::MenuItem("Directional Light")) {
          // Code to add an Orbit Camera
        }
        if (ImGui::MenuItem("Spot Light")) {
          // Code to add an Orbit Camera
        }
        ImGui::EndMenu();
      }
      // Add more menu items here if needed
      ImGui::EndMenu();
    }
    // Add other menus like "Edit", "View", etc. here

    ImGui::EndMainMenuBar();
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
  if (ImGui::Begin("Light")) {
    auto& scene_data = actions_.get_scene_data();

    ImGui::SliderFloat("Light X", &scene_data.sunlightDirection.x, -10.f, 10.f);
    ImGui::SliderFloat("Light Y", &scene_data.sunlightDirection.y, -10.f, 10.f);
    ImGui::SliderFloat("Light Z", &scene_data.sunlightDirection.z, -10.f, 10.f);
    ImGui::SliderFloat("Light intensity", &scene_data.sunlightDirection.w, 0.f, 100.f);
  }
  ImGui::End();
}

void imgui_gui::scene_graph() {
  float windowWidth = 300.0f;                               // Width of the ImGui window
  float windowHeight = window_.extent.height;                        // Full height of the application window
  ImVec2 windowPos = ImVec2(window_.extent.width - windowWidth, 0);  // Position to the right
  ImVec2 windowSize = ImVec2(windowWidth, windowHeight);    // Size of the ImGui window

  ImGui::SetNextWindowPos(windowPos);
  ImGui::SetNextWindowSize(windowSize);
  if (ImGui::Begin("Scene Graph", nullptr)) {
    show_scene_hierarchy_window();
  }
  ImGui::End();
}

void imgui_gui::new_frame() {
  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplSDL2_NewFrame(window_.handle);
  ImGui::NewFrame();

  menu_bar();

  stats();

  lights();

  scene_graph();

  render_settings();

  ImGui::Render();
}

void imgui_gui::render_settings() {
  if (ImGui::Begin("Rendering Settings")) {
    render_config& config = actions_.get_render_config();

    if (ImGui::CollapsingHeader("General Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
      ImGui::Checkbox("Always Opaque", &config.always_opaque);
    }

    if (ImGui::CollapsingHeader("SSAO Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
      ImGui::Checkbox("Enable SSAO", &config.enable_ssao);
      ImGui::SliderInt("SSAO Quality", &config.ssao_quality, 1, 4);
      ImGui::Checkbox("Show SSAO", &config.ssao.show_ssao_only);
      ImGui::SliderFloat("Scale", &config.ssao.scale, 0.0f, 2.0f,
                         "%.3f");  
      ImGui::SliderFloat("Bias", &config.ssao.bias, 0.0f, 1.0f,
                         "%.3f");  
      ImGui::SliderFloat("Radius", &config.ssao.radius, 0.0f, .5f, 
                         "%.2f");
      ImGui::SliderFloat("Attenuation Scale", &config.ssao.attScale, 0.0f, 2.0f,
                         "%.2f");  
      ImGui::SliderFloat("Distance Scale", &config.ssao.distScale, 0.0f, 10.0f,
                         "%.2f");
    }

    if (ImGui::CollapsingHeader("HDR Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
      ImGui::Checkbox("Enable HDR", &config.enable_hdr);
      ImGui::SliderInt("Bloom Quality", &config.bloom_quality, 1, 4);
      ImGui::Checkbox("Show Bright Pass", &config.hdr.show_bright_pass);
      ImGui::SliderFloat("Exposure", &config.hdr.exposure, 0.1f, 2.0f,
                         "%.3f");  
      ImGui::SliderFloat("Max White", &config.hdr.maxWhite, 0.1f, 2.5f,
                         "%.3f");  
      ImGui::SliderFloat("Bloom Strength", &config.hdr.bloomStrength, 0.0f, 2.f, 
                         "%.2f");
      ImGui::SliderFloat("Adaption Speed", &config.hdr.adaptationSpeed, 0.1f, 2.0f, "%.2f");
      ImGui::ColorPicker4("Lift", &config.hdr.lift[0], ImGuiColorEditFlags_Float);
      ImGui::ColorPicker4("Gamma", &config.hdr.gamma[0], ImGuiColorEditFlags_Float);
      ImGui::ColorPicker4("Gain", &config.hdr.gain[0], ImGuiColorEditFlags_Float);
    }

    if (ImGui::CollapsingHeader("Shadow Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
      ImGui::Checkbox("Enable Shadows", &config.enable_shadows);
    }

    // TODO: Add more sections for other settings as necessary
  }

  ImGui::End();
}


void imgui_gui::display_scene_hierarchy(const entity_component& node) {
  if (node.is_valid()) {

    ImGuiTreeNodeFlags node_flags = ImGuiTreeNodeFlags_OpenOnArrow;

    // If the node has no children, make it a leaf node
    if (node.children.empty()) {
      node_flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
    }

    // Check if the node is the selected node
    if (node.entity == selected_node_.entity) {
      node_flags |= ImGuiTreeNodeFlags_Selected;
    }

    bool node_open = ImGui::TreeNodeEx(node.name.c_str(), node_flags);

    // Check for item selection
    if (ImGui::IsItemClicked()) {
        selected_node_ = node;
    }

    // Recursively display children if the node is open
    if (node_open && !node.children.empty()) {
      for (const auto child_entity : node.children) {
        display_scene_hierarchy(actions_.get_scene_object(child_entity));
      }
      ImGui::TreePop();
    }
  }
}

void imgui_gui::show_scene_hierarchy_window() {
  ImVec2 windowAvail = ImGui::GetContentRegionAvail();
  ImVec2 childSize = ImVec2(0, windowAvail.y * 0.4f);

  // Child window for the scrollable scene hierarchy
  if (ImGui::BeginChild("HierarchyTree", childSize, true)) {
    const entity_component& root = actions_.get_scene_root();
    for (const auto child_entity : root.children) {
      display_scene_hierarchy(actions_.get_scene_object(child_entity));
    }
  }
  ImGui::EndChild();

  ImGui::Separator();

  childSize = ImVec2(0, windowAvail.y - childSize.y);  // Remaining space for the selected node
  if (ImGui::BeginChild("SelectedNodeDetails", childSize, false)) {
    if (selected_node_.is_valid()) {
      ImGui::Text(selected_node_.name.c_str());

      if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_None)) {
        auto& transform = actions_.get_transform_component(selected_node_.transform);
        ImGui::DragFloat3("Position", &transform.position.x, 0.1f);
        glm::vec3 euler = eulerAngles(transform.rotation);
        if (ImGui::DragFloat3("Rotation", &euler.x, 0.1f)) {
          transform.rotation = glm::quat(euler);
        }
        ImGui::DragFloat3("Scale", &transform.scale.x, 0.1f);
        transform.is_dirty = true;
      }
      if (selected_node_.has_mesh()) {
        if (ImGui::CollapsingHeader("Materials", ImGuiTreeNodeFlags_None)) {
          auto& mesh = actions_.get_mesh_component(selected_node_.mesh);
          for (auto surface_index : mesh.surfaces) {
            auto& surface = actions_.get_surface(surface_index);
            auto& material = actions_.get_material(surface.material);
            if (ImGui::TreeNode(material.name.c_str())) {
              auto& config = material.config;

              if (ImGui::CollapsingHeader("Albedo", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::DragFloat4("Factor", &config.constants.albedo_factor.x, 0.01f, 0.0f, 1.0f, "%.2f");
                ImGui::Checkbox("Use Texture", &config.use_albedo_tex);
                if (config.use_albedo_tex) {
                  ImGui::Text("URI: %s", config.albedo_uri.c_str());
                }
              }

              if (ImGui::CollapsingHeader("Metallic-Roughness", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::DragFloat2("Factor", &config.constants.metal_rough_factor.x, 0.01f, 0.0f, 1.0f,
                                  "%.2f");
                ImGui::Checkbox("Use Texture", &config.use_metal_rough_tex);
                if (config.use_metal_rough_tex) {
                  ImGui::Text("URI: %s", config.metal_rough_uri.c_str());
                }
              }

              if (ImGui::CollapsingHeader("Normal Map", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::DragFloat("Scale", &config.normal_scale, 0.01f, 0.0f, 10.0f, "%.2f");
                ImGui::Checkbox("Use Texture", &config.use_normal_tex);
                if (config.use_normal_tex) {
                  ImGui::Text("URI: %s", config.normal_uri.c_str());
                }
              }

              if (ImGui::CollapsingHeader("Emissive", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::DragFloat3("Factor", &config.emissive_factor.x, 0.01f, 0.0f, 10.0f, "%.2f");
                ImGui::Checkbox("Use Texture", &config.use_emissive_tex);
                if (config.use_emissive_tex) {
                  ImGui::Text("URI: %s", config.emissive_uri.c_str());
                }
              }

              if (ImGui::CollapsingHeader("Occlusion", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::DragFloat("Strength", &config.occlusion_strength, 0.01f, 0.0f, 10.0f,
                                 "%.2f");
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
      }
    }
  }
  ImGui::EndChild();
}
