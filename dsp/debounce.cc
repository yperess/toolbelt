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

#include "toolbelt/dsp/debounce.h"

namespace toolbelt::dsp {

pw::async2::Poll<Debounce::State> Debounce::StateChange::Pend(
    pw::async2::Context& cx) {
  if (debounce_->notify_count_ > 0) {
    State result = debounce_->notify_queue_[debounce_->notify_head_];
    debounce_->notify_head_ =
        static_cast<uint8_t>((debounce_->notify_head_ + 1u) % 2u);
    --debounce_->notify_count_;
    complete_ = true;
    return pw::async2::Ready(result);
  }
  PW_ASYNC_STORE_WAKER(
      cx, debounce_->change_waker_, "waiting for debounce state change");
  return pw::async2::Pending();
}

Debounce::State Debounce::state() const { return state_; }

void Debounce::NotifyChange(State new_state) {
  state_ = new_state;
  if (notify_count_ < 2) {
    notify_queue_[(notify_head_ + notify_count_) % 2] = new_state;
    ++notify_count_;
  }
  if (notify_count_ == 1) {
    change_waker_.Wake();
  }
}

}  // namespace pw::dsp
