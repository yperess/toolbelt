// Copyright 2026 Yuval Peress
//
// Licensed under the Apache License, Version 2.0 (the "License"); you may not
// use this file except in compliance with the License. You may obtain a copy of
// the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
// License for the specific language governing permissions and limitations under
// the License.

#pragma once

#include <cstdint>

#include "pw_async2/task.h"
#include "pw_async2/waker.h"

namespace toolbelt::dsp {

/// Abstract base for async debounce implementations.
///
/// A ``Debounce`` is a ``pw::async2::Task``. Post it to a dispatcher to start
/// processing. Subclasses implement ``DoPend()`` and call ``NotifyChange()``
/// when the debounced signal transitions.
///
/// State transitions are consumed via ``NextState()``, which returns a
/// ``pw::async2::Future<Debounce::State>`` that resolves to the next stable
/// ``kActive`` or ``kInactive`` value. Each call to ``NextState()`` produces
/// a one-shot future for exactly one transition; call it again in a loop to
/// observe subsequent transitions.
class Debounce : public pw::async2::Task {
 public:
  enum class State : bool { kActive = true, kInactive = false };

  /// One-shot future returned by NextState(). Satisfies
  /// pw::async2::Future<State>.
  class StateChange {
   public:
    using value_type = State;

    constexpr StateChange() = default;
    StateChange(StateChange&&) = default;
    StateChange& operator=(StateChange&&) = default;

    [[nodiscard]] bool is_pendable() const {
      return debounce_ != nullptr && !complete_;
    }
    [[nodiscard]] bool is_complete() const { return complete_; }

    pw::async2::Poll<State> Pend(pw::async2::Context& cx);

   private:
    friend class Debounce;
    explicit StateChange(Debounce& d) : debounce_(&d) {}

    Debounce* debounce_ = nullptr;
    bool complete_ = false;
  };

  /// Returns a one-shot future for the next debounced state transition.
  ///
  /// The returned future resolves exactly once. Call ``NextState()`` again
  /// inside a loop to observe subsequent transitions.
  StateChange NextState() { return StateChange(*this); }

  /// Returns the most recently notified state; defaults to kInactive.
  [[nodiscard]] State state() const;

 protected:
  /// Records a state transition and wakes any pending NextState() future.
  ///
  /// Call from ``DoPend()`` when debounce logic confirms a transition.
  /// Queues up to two notifications per ``DoPend()`` invocation, which
  /// supports ``PulseWidthDebounce`` emitting ``kActive`` then ``kInactive``
  /// for a single press in the same poll cycle.
  void NotifyChange(State new_state);

 private:
  pw::async2::Waker change_waker_;
  State state_ = State::kInactive;
  State notify_queue_[2] = {};
  uint8_t notify_head_ = 0;
  uint8_t notify_count_ = 0;
};

}  // namespace toolbelt::dsp
