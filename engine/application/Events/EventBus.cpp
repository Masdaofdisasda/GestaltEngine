#include "EventBus.hpp"

gestalt::application::EventBus::EventBus() {
  producer_queue_ = &queue_a_;
  consumer_queue_ = &queue_b_;
}

void gestalt::application::EventBus::poll() {
  std::swap(producer_queue_, consumer_queue_);

  producer_queue_->clear();

  for (auto& event_ptr : *consumer_queue_) {
    dispatch_event(*event_ptr);
  }
  consumer_queue_->clear();
}

void gestalt::application::EventBus::dispatch_event(const IEventHolder& evt_holder) {
  const auto type_id = std::type_index(evt_holder.type());
  const auto it = subscribers_.find(type_id);
  if (it != subscribers_.end()) {
    const void* event_data_ptr = evt_holder.data();
    for (auto& func : it->second) {
      func(event_data_ptr);
    }
  }
}