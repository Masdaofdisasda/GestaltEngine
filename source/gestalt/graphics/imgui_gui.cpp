#include "imgui_gui.h"

#include <imgui.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_vulkan.h>

#include <glm/gtc/quaternion.hpp>

#include "vk_initializers.h"
#include "vk_pipelines.h"

void imgui_gui::init(
    vk_gpu& gpu, sdl_window& window, vk_swapchain& swapchain, gui_actions& actions) {
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
  init_info.ColorAttachmentFormat = swapchain_.swapchain_image_format;

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
      = vkinit::rendering_info(swapchain_.swapchain_extent, &colorAttachment, nullptr);

  vkCmdBeginRendering(cmd, &renderInfo);

  ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);

  vkCmdEndRendering(cmd);
}

void imgui_gui::update(const SDL_Event& e) {
  ImGui_ImplSDL2_ProcessEvent(&e);
}

void imgui_gui::new_frame() {
  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplSDL2_NewFrame(window_.handle);
  ImGui::NewFrame();

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

  if (ImGui::Begin("Stats")) {
    const auto& stats = actions_.get_stats();

    ImGui::Text("frametime %f ms", stats.frametime);
    ImGui::Text("draw time %f ms", stats.mesh_draw_time);
    ImGui::Text("update time %f ms", stats.scene_update_time);
    ImGui::Text("triangles %i", stats.triangle_count);
    ImGui::Text("draws %i", stats.drawcall_count);
    ImGui::End();
  }

  if (ImGui::Begin("Light")) {
    auto& scene_data = actions_.get_scene_data();

    ImGui::SliderFloat("Light X", &scene_data.sunlightDirection.x, -10.f, 10.f);
    ImGui::SliderFloat("Light Y", &scene_data.sunlightDirection.y, -10.f, 10.f);
    ImGui::SliderFloat("Light Z", &scene_data.sunlightDirection.z, -10.f, 10.f);
    ImGui::SliderFloat("Light intensity", &scene_data.sunlightDirection.w, 0.f, 100.f);
    ImGui::End();
  }

  ImGui::Render();
}
