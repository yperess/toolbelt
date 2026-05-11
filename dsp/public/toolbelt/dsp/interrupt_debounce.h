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

#include <atomic>

#include "pw_async2/time_provider.h"
#include "pw_async2/waker.h"
#include "pw_chrono/system_clock.h"
#include "pw_digital_io/digital_io.h"
#include "toolbelt/dsp/debounce.h"

namespace toolbelt::dsp {

/// Base class for interrupt-driven debouncers.
///
/// Exposes ``OnInterrupt()`` as a common entry point so that a single GPIO
/// interrupt source can be fanned out to multiple debouncers without the
/// wiring code needing to know the concrete debouncer type:
///
///   io.SetInterruptHandler(
///       pw::digital_io::InterruptTrigger::kBothEdges,
///       [&](pw::digital_io::State s) {
///         tap.OnInterrupt(s);
///         long_press.OnInterrupt(s);
///       });
///   io.EnableInterruptHandler();
///
/// Subclasses implement ``DoPend()`` and read the protected interrupt state
/// fields (``waker_``, ``interrupt_fired_``, ``interrupt_state_``).
class InterruptBasedDebounce : public Debounce {
public:
  void OnInterrupt(pw::digital_io::State new_state);

protected:
  pw::async2::Waker waker_;
  std::atomic<bool> interrupt_fired_{false};
  pw::digital_io::State interrupt_state_{pw::digital_io::State::kInactive};
};

/// Interrupt-driven debouncer with leading-edge notification.
///
/// Fires ``kActive`` immediately on the first rising edge (no wait), then
/// opens a debounce window of ``threshold`` duration during which all
/// further interrupts are ignored. After the window, waits for the falling
/// edge and fires ``kInactive``.
///
/// This gives minimum latency for tap/press detection: the ``kActive``
/// notification arrives before any contact bounce can occur.
///
/// @param time       Time provider used to run the debounce-window timer.
/// @param threshold  Duration of the post-rise noise-rejection window.
class LeadingEdgeDebounce : public InterruptBasedDebounce {
public:
  LeadingEdgeDebounce(pw::async2::TimeProvider<pw::chrono::SystemClock> &time,
                      pw::chrono::SystemClock::duration threshold);

private:
  pw::async2::Poll<> DoPend(pw::async2::Context &cx) override;

  enum class Phase { kWaiting, kDebouncing, kWaitingForReset };

  pw::async2::TimeProvider<pw::chrono::SystemClock> &time_;
  pw::chrono::SystemClock::duration threshold_;
  pw::async2::TimeFuture<pw::chrono::SystemClock> timer_;
  Phase phase_ = Phase::kWaiting;
};

/// Interrupt-driven debouncer for minimum-hold-time detection.
///
/// Starts a timer on the rising edge. If the signal is still active when the
/// timer expires, fires ``kActive``; releases that fire ``kInactive``.
/// Presses shorter than ``threshold`` produce no notification and are treated
/// as noise.
///
/// This is appropriate for long-press detection where only deliberate holds
/// should register (e.g. a 2 s factory-reset gesture).
///
/// @param time       Time provider used to run the hold timer.
/// @param threshold  Minimum hold duration required to emit ``kActive``.
class PulseWidthDebounce : public InterruptBasedDebounce {
public:
  PulseWidthDebounce(pw::async2::TimeProvider<pw::chrono::SystemClock> &time,
                     pw::chrono::SystemClock::duration threshold);

private:
  pw::async2::Poll<> DoPend(pw::async2::Context &cx) override;

  enum class Phase { kIdle, kMeasuring, kWaitingForRelease };

  pw::async2::TimeProvider<pw::chrono::SystemClock> &time_;
  pw::chrono::SystemClock::duration threshold_;
  Phase phase_ = Phase::kIdle;
  pw::async2::TimeFuture<pw::chrono::SystemClock> timer_;
};

} // namespace toolbelt::dsp
