#include "frame_graph.h"

#include <queue>
#include <set>
#include <unordered_set>

#include "geometry_pass.h"
#include "hdr_pass.h"
#include "lighting_pass.h"
#include "shadow_pass.h"
#include "skybox_pass.h"
#include "ssao_pass.h"
#include "vk_images.h"
#include "vk_initializers.h"

void frame_graph::init(const vk_gpu& gpu, const sdl_window& window,
                       const std::shared_ptr<resource_manager>& resource_manager,
                       const std::shared_ptr<imgui_gui>& imgui_gui) {
  gpu_ = gpu;
  window_ = window;
  resource_manager_ = resource_manager;
  imgui_ = imgui_gui;

  swapchain_->init(gpu_, {window_.extent.width, window_.extent.height, 1});
  commands_->init(gpu_, frames_);
  sync_->init(gpu_, frames_);

  render_passes_.push_back(std::make_unique<directional_depth_pass>()); //TODO investigate sorting
  render_passes_.push_back(std::make_unique<deferred_pass>());
  render_passes_.push_back(std::make_unique<lighting_pass>());
  render_passes_.push_back(std::make_unique<skybox_pass>());
  render_passes_.push_back(std::make_unique<infinite_grid_pass>());
  //render_passes_.push_back(std::make_unique<geometry_pass>());
  //render_passes_.push_back(std::make_unique<transparent_pass>());
  render_passes_.push_back(std::make_unique<ssao_filter_pass>());
  render_passes_.push_back(std::make_unique<ssao_blur_pass>());
  render_passes_.push_back(std::make_unique<ssao_final_pass>());
  render_passes_.push_back(std::make_unique<bright_pass>());
  render_passes_.push_back(std::make_unique<bloom_blur_pass>());
  render_passes_.push_back(std::make_unique<streaks_pass>());
  render_passes_.push_back(std::make_unique<luminance_pass>());
  render_passes_.push_back(std::make_unique<luminance_downscale_pass>());
  render_passes_.push_back(std::make_unique<light_adaptation_pass>());
  render_passes_.push_back(std::make_unique<tonemap_pass>());
  render_passes_.push_back(std::make_unique<debug_aabb_pass>());

  for (int i = 0; i < FRAME_OVERLAP; i++) {
    std::vector<DescriptorAllocatorGrowable::PoolSizeRatio> frame_sizes = {
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4},
    };

    frames_[i].descriptor_pool = DescriptorAllocatorGrowable{};
    frames_[i].descriptor_pool.init(gpu_.device, 1000, frame_sizes);
  }

  create_resources();
  // the shader pipelines need acccess to their resources to be created
  resource_manager_->direct_original_mapping = direct_original_mapping_;

  for (auto& pass : render_passes_) {
    pass->init(gpu_, resource_manager_);
  }

  resource_manager_->direct_original_mapping.clear();
}

std::string frame_graph::get_final_transformation(const std::string& original) {
  std::string current = original;
  while (resource_transformations_.find(current) != resource_transformations_.end()) {
    current = resource_transformations_[current];
  }
  return current;
}

void frame_graph::calculate_resource_transform() {
  resource_transformations_.clear();
  std::unordered_map<std::string, std::string> resource_predecessors;

  for (auto& pass : render_passes_) {
    auto t = pass->get_dependencies();
    for (auto& r : t.write_resources) {
      resource_transformations_[r.second->getId()] = r.first;
      resource_predecessors[r.first] = r.second->getId();
    }
  }

  for (const auto& pair : resource_predecessors) {
    std::string originalId = pair.second;  // Start with the immediate predecessor
    std::string currentId = pair.first;

    // Trace back to the original source
    while (resource_predecessors.find(originalId) != resource_predecessors.end()) {
      originalId = resource_predecessors[originalId];
    }

    // Now, originalId is the original source for currentId
    direct_original_mapping_[currentId] = originalId;
  }

  std::unordered_set<std::string> values;
  for (const auto& pair : resource_transformations_) {
    values.insert(pair.second);
  }

  std::unordered_set<std::string> originalKeys;
  for (const auto& pair : resource_transformations_) {
    if (values.find(pair.first) == values.end()) {
      // If the key is not in the set of values, it's an original key
      originalKeys.insert(pair.first);
    }
  }

  resource_pairs_.clear();
  for (const auto& key : originalKeys) {
    resource_pairs_.emplace_back(key, get_final_transformation(key));
    direct_original_mapping_[key] = key;
  }
}

void frame_graph::create_resources() {

  for (auto [original, final] : resource_pairs_) {
    resource_manager_->update_resource_id(final, original);
  }

  std::unordered_map<std::string, std::shared_ptr<shader_resource>> resource_map;

  for (auto& pass : render_passes_) {
    auto t = pass->get_dependencies();
    for (auto& r : t.read_resources) {
      resource_map[r->getId()] = r;
    }
    for (auto& r : t.write_resources) {
      resource_map[r.second->getId()] = r.second;
    }
  }

  calculate_resource_transform();

  for (const auto& original_id : resource_pairs_ | std::views::keys) {
    auto id = original_id;
    while (resource_transformations_.contains(id)) {
      id = resource_transformations_[id];
      resource_map.erase(id);
    }
  }

  for (auto& r : resource_map | std::views::values) {
    if (auto image_resource = std::dynamic_pointer_cast<color_image_resource>(r)) {
      if (resource_manager_->get_resource<AllocatedImage>(image_resource->getId())) {
        continue;
      }

      auto image = std::make_shared<AllocatedImage>(image_type::color);
      image->imageExtent
          = {image_resource->get_extent().width, image_resource->get_extent().height, 1};
      if (image->imageExtent.height == 0 && image->imageExtent.width == 0) {
        image->imageExtent.width
            = static_cast<uint32_t>(window_.extent.width * image_resource->get_scale());
        image->imageExtent.height
            = static_cast<uint32_t>(window_.extent.height * image_resource->get_scale());
      }
      resource_manager_->create_color_frame_buffer(
          {image->getExtent2D().width, image->getExtent2D().height, 1}, *image.get());
      resource_manager_->add_resource(image_resource->getId(), image);
    } else if (auto image_resource = std::dynamic_pointer_cast<depth_image_resource>(r)) {
      if (resource_manager_->get_resource<AllocatedImage>(image_resource->getId())) {
        continue;
      }

      auto image = std::make_shared<AllocatedImage>(image_type::depth);
      image->imageExtent
          = {image_resource->get_extent().width, image_resource->get_extent().height, 1};
      if (image->imageExtent.height == 0 && image->imageExtent.width == 0) {
        image->imageExtent.width
            = static_cast<uint32_t>(window_.extent.width * image_resource->get_scale());
        image->imageExtent.height
            = static_cast<uint32_t>(window_.extent.height * image_resource->get_scale());
      }
      resource_manager_->create_depth_frame_buffer(
          {image->getExtent2D().width, image->getExtent2D().height, 1}, *image.get());
      resource_manager_->add_resource(image_resource->getId(), image);
    } else if (auto buffer = std::dynamic_pointer_cast<buffer_resource>(r)) {
      // TODO: create buffer
    }
  }
}

void frame_graph::build_graph() {

  graph_.clear();

  // Initialize in_degree for all passes to 0.
  for (size_t i = 0; i < render_passes_.size(); ++i) {
    in_degree_[i] = 0;
  }

  populate_graph();

  topological_sort();
}

void frame_graph::populate_graph() {
  // Map each resource to the last pass that writes to it.
  std::unordered_map<std::string, size_t> last_writer;

  for (size_t i = 0; i < render_passes_.size(); ++i) {
    auto& pass = render_passes_[i];

    // Update graph and in_degree based on write dependencies.
    for (const auto& resource : pass->get_dependencies().write_resources) {
      // If there's a previous writer, add a dependency from that writer to this pass.
      if (last_writer.find(resource.second->getId()) != last_writer.end()) {
        size_t writer_pass_index = last_writer[resource.second->getId()];
        graph_[writer_pass_index].push_back(i);
        in_degree_[i]++;
      }
      // Update the last writer of the resource to this pass.
      last_writer[resource.second->getId()] = i;
    }

    // (Optional) Handle read dependencies similarly if needed.
  }
}


void frame_graph::topological_sort() {

  sorted_passes_.clear();

  std::queue<size_t> q;

  // Find all passes with no incoming edge and add them to the queue.
  for (const auto& in_deg : in_degree_) {
    if (in_deg.second == 0) {
      q.push(in_deg.first);
    }
  }

  while (!q.empty()) {
    size_t current_pass = q.front();
    q.pop();
    sorted_passes_.push_back(current_pass);

    // For each pass dependent on the current pass.
    for (auto& dep_pass : graph_[current_pass]) {
      // Decrease the in-degree of the dependent pass.
      if (--in_degree_[dep_pass] == 0) {
        // If no more dependencies, add it to the queue.
        q.push(dep_pass);
      }
    }
  }

  // Check for cycles in the graph (if the graph is not a DAG).
  if (sorted_passes_.size() != render_passes_.size()) {

    // For simplicity, we'll just clear the sorted_passes indicating an error.
    sorted_passes_.clear();
  }
}

void frame_graph::execute(size_t id, VkCommandBuffer cmd) {
  for (const auto& reads = render_passes_[id]->get_dependencies().read_resources;
       const auto& read : reads) {
    if (std::dynamic_pointer_cast<color_image_resource>(read)
        || std::dynamic_pointer_cast<depth_image_resource>(read)) {
      std::shared_ptr<AllocatedImage> resource
          = resource_manager_->get_resource<AllocatedImage>(read->getId());
      vkutil::transition_read(cmd, *resource);
    }
  }

  for (const auto& writes = render_passes_[id]->get_dependencies().write_resources;
       const auto& write : writes) {
    if (std::dynamic_pointer_cast<color_image_resource>(write.second)
        || std::dynamic_pointer_cast<depth_image_resource>(write.second)) {
      std::shared_ptr<AllocatedImage> resource
          = resource_manager_->get_resource<AllocatedImage>(write.second->getId());
      vkutil::transition_write(cmd, *resource);
    }
  }

  render_passes_[id]->execute(cmd);

  for (const auto& writes = render_passes_[id]->get_dependencies().write_resources;
       const auto& write : writes) {
    if (std::dynamic_pointer_cast<color_image_resource>(write.second)
        || std::dynamic_pointer_cast<depth_image_resource>(write.second)) {
      resource_manager_->update_resource_id(write.second->getId(), write.first);
    }
  }
}


bool frame_graph::acquire_next_image() {
  VK_CHECK(vkWaitForFences(gpu_.device, 1, &get_current_frame().render_fence, true, 1000000000));

  get_current_frame().descriptor_pool.clear_pools(gpu_.device);
  resource_manager_->descriptor_pool = &get_current_frame().descriptor_pool;

  VkResult e = vkAcquireNextImageKHR(gpu_.device, swapchain_->swapchain, 1000000000,
                                     get_current_frame().swapchain_semaphore, nullptr,
                                     &swapchain_image_index_);
  if (e == VK_ERROR_OUT_OF_DATE_KHR) {
    resize_requested_ = true;
    return true;
  }

  VK_CHECK(vkResetFences(gpu_.device, 1, &get_current_frame().render_fence));
  VK_CHECK(vkResetCommandBuffer(get_current_frame().main_command_buffer, 0));

  return false;
}

VkCommandBuffer frame_graph::start_draw() {
  VkCommandBuffer cmd = get_current_frame().main_command_buffer;
  VkCommandBufferBeginInfo cmdBeginInfo
      = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

  VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

  return cmd;
}

void frame_graph::execute_passes() {
  create_resources();

  build_graph();

  if (resize_requested_) {
    swapchain_->resize_swapchain(window_);
    resize_requested_ = false;
  }

  if (acquire_next_image()) {
    return;
  }

  VkCommandBuffer cmd = start_draw();

  for (const size_t index : sorted_passes_) {
    execute(index, cmd);
  }

  resource_manager_->main_draw_context_.opaque_surfaces.clear();
  resource_manager_->main_draw_context_.transparent_surfaces.clear();

  const auto color_image = resource_manager_->get_resource<AllocatedImage>("scene_debug_aabb");

  vkutil::transition_image(cmd, color_image->image, color_image->currentLayout,
                           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
  color_image->currentLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  vkutil::transition_image(cmd, swapchain_->swapchain_images[swapchain_image_index_],
                           VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

  vkutil::copy_image_to_image(cmd, color_image->image,
                              swapchain_->swapchain_images[swapchain_image_index_],
                              color_image->getExtent2D(), get_window().extent);
  vkutil::transition_image(cmd, swapchain_->swapchain_images[swapchain_image_index_],
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                           VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

  imgui_->draw(cmd, swapchain_->swapchain_image_views[swapchain_image_index_]);

  vkutil::transition_image(cmd, swapchain_->swapchain_images[swapchain_image_index_],
                           VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                           VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

  present(cmd);
}

void frame_graph::cleanup() {
  for (auto& pass : render_passes_) {
    pass->cleanup();
  }
}

void frame_graph::present(VkCommandBuffer cmd) {
  VK_CHECK(vkEndCommandBuffer(cmd));

  VkCommandBufferSubmitInfo cmdinfo = vkinit::command_buffer_submit_info(cmd);
  VkSemaphoreSubmitInfo waitInfo = vkinit::semaphore_submit_info(
      VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, get_current_frame().swapchain_semaphore);
  VkSemaphoreSubmitInfo signalInfo = vkinit::semaphore_submit_info(
      VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, get_current_frame().render_semaphore);

  VkSubmitInfo2 submit = vkinit::submit_info(&cmdinfo, &signalInfo, &waitInfo);

  VK_CHECK(vkQueueSubmit2(gpu_.graphics_queue, 1, &submit, get_current_frame().render_fence));
  VkPresentInfoKHR presentInfo = vkinit::present_info();
  presentInfo.pSwapchains = &swapchain_->swapchain;
  presentInfo.swapchainCount = 1;
  presentInfo.pWaitSemaphores = &get_current_frame().render_semaphore;
  presentInfo.waitSemaphoreCount = 1;
  presentInfo.pImageIndices = &swapchain_image_index_;

  VkResult presentResult = vkQueuePresentKHR(gpu_.graphics_queue, &presentInfo);
  if (presentResult == VK_ERROR_OUT_OF_DATE_KHR) {
    resize_requested_ = true;
    return;
  }

  frame_number_++;
}
