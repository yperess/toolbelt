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

#include "toolbelt/dsp/polling_debounce.h"

#include <algorithm>
#include <utility>

namespace toolbelt::dsp {

PollingDebounce::PollingDebounce(
    pw::Function<State()> sample,
    pw::async2::TimeProvider<pw::chrono::SystemClock>& time,
    pw::chrono::SystemClock::duration poll_interval,
    uint32_t stable_count)
    : sample_(std::move(sample)),
      time_(time),
      poll_interval_(poll_interval),
      stable_count_(stable_count) {}

std::optional<Debounce::State> PollingDebounce::ProcessSample(State sample) {
  if (sample == last_sample_) {
    count_ = std::min(count_ + 1, stable_count_);
  } else {
    count_ = 1;
  }
  last_sample_ = sample;
  if (count_ < stable_count_) {
    return std::nullopt;
  }
  return last_sample_;
}

pw::async2::Poll<> PollingDebounce::DoPend(pw::async2::Context& cx) {
  if (timer_.is_pendable()) {
    if (timer_.Pend(cx).IsPending()) {
      return pw::async2::Pending();
    }
    auto new_state = ProcessSample(sample_());
    if (new_state && new_state != state()) {
      NotifyChange(*new_state);
    }
  }
  timer_ = time_.WaitUntil(time_.now() + poll_interval_);
  if (timer_.Pend(cx).IsPending()) {
    return pw::async2::Pending();
  }
  cx.ReEnqueue();
  return pw::async2::Pending();
}

}  // namespace toolbelt::dsp
