#pragma once

#include <functional>
#include <memory>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <vector>

namespace gestalt::application {

  class EventBus {
  public:
    EventBus();

    ~EventBus() = default;

    EventBus(const EventBus&) = delete;
    EventBus& operator=(const EventBus&) = delete;

    EventBus(EventBus&&) = delete;
    EventBus& operator=(EventBus&&) = delete;

    /**
     * Subscribe to a specific event type T.
     *
     * @param callback A function or lambda that takes (const T&) as a parameter.
     */
    template <typename T> void subscribe(std::function<void(const T&)> callback) {
      const auto type_id = std::type_index(typeid(T));
      auto& vec = subscribers_[type_id];

      std::function<void(const void*)> wrapper
          = [cb = std::move(callback)](const void* evPtr) { cb(*static_cast<const T*>(evPtr)); };

      vec.push_back(std::move(wrapper));
    }

    /**
     * Emit an event of type T. Thread-safe in the sense that
     * it only modifies the producer queue, not the consumer queue.
     */
    template <typename T> void emit(const T& event_data) {
      auto ptr = std::make_unique<EventHolder<T>>(event_data);
      producer_queue_->push_back(std::move(ptr));
    }

    /**
     * Poll/Dispatch events to subscribers.
     * Swap the producer and consumer queues so we can safely
     * process all events that were emitted during the last frame/update.
     */
    void poll();

  private:
    /**
     * Base class to allow storing any event type polymorphically.
     */
    struct IEventHolder {
      virtual ~IEventHolder() = default;
      [[nodiscard]] virtual const std::type_info& type() const = 0;
      [[nodiscard]] virtual const void* data() const = 0;
    };

    /**
     * Templated derived class that holds an event of type T.
     */
    template <typename T> struct EventHolder : public IEventHolder {
      T event_data;

      explicit EventHolder(const T& data) : event_data(data) {}

      [[nodiscard]] const std::type_info& type() const override { return typeid(T); }

      [[nodiscard]] const void* data() const override { return &event_data; }
    };

    /**
     * Dispatch a single event pointer to all subscribers
     * who listen for this event type.
     */
    void dispatch_event(const IEventHolder& evt_holder);

    std::unordered_map<std::type_index, std::vector<std::function<void(const void*)>>> subscribers_;

    std::vector<std::unique_ptr<IEventHolder>> queue_a_;
    std::vector<std::unique_ptr<IEventHolder>> queue_b_;
    std::vector<std::unique_ptr<IEventHolder>>* producer_queue_;
    std::vector<std::unique_ptr<IEventHolder>>* consumer_queue_;
  };

}  // namespace gestalt::application