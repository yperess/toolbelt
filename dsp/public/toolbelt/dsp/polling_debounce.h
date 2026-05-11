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
#include <optional>

#include "pw_async2/time_provider.h"
#include "pw_chrono/system_clock.h"
#include "pw_function/function.h"
#include "toolbelt/dsp/debounce.h"

namespace toolbelt::dsp {

/// Debounces a signal by polling at a fixed interval.
///
/// Samples the signal every ``poll_interval`` and requires ``stable_count``
/// consecutive identical readings before emitting a state change via
/// ``NextState()``. This makes it suitable for signals without interrupt
/// support, or for ADC-style inputs converted to a binary state.
///
/// @param sample      Callable returning the current ``Debounce::State``.
/// @param time        Time provider used to schedule polling ticks.
/// @param poll_interval  How often to call ``sample``.
/// @param stable_count   Number of consecutive identical samples required
///                       before a transition is reported.
///
/// Example setup:
///
///   pw::dsp::PollingDebounce btn(
///       []() { return ReadPin(); },
///       time_provider,
///       /*poll_interval=*/std::chrono::milliseconds(10),
///       /*stable_count=*/5);
///   dispatcher.Post(btn);
class PollingDebounce : public Debounce {
public:
  PollingDebounce(pw::Function<State()> sample,
                  pw::async2::TimeProvider<pw::chrono::SystemClock> &time,
                  pw::chrono::SystemClock::duration poll_interval,
                  uint32_t stable_count);

private:
  pw::async2::Poll<> DoPend(pw::async2::Context &cx) override;

  std::optional<State> ProcessSample(State sample);

  pw::Function<State()> sample_;
  pw::async2::TimeProvider<pw::chrono::SystemClock> &time_;
  pw::chrono::SystemClock::duration poll_interval_;
  uint32_t stable_count_;
  uint32_t count_ = 0;
  State last_sample_ = State::kInactive;
  pw::async2::TimeFuture<pw::chrono::SystemClock> timer_;
};

} // namespace toolbelt::dsp
