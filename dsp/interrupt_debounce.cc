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

#include "toolbelt/dsp/interrupt_debounce.h"

namespace toolbelt::dsp {

// InterruptBasedDebounce ----------------------------------------------------

void InterruptBasedDebounce::OnInterrupt(pw::digital_io::State new_state) {
  interrupt_state_ = new_state;
  interrupt_fired_ = true;
  waker_.Wake();
}

// LeadingEdgeDebounce -------------------------------------------------------

LeadingEdgeDebounce::LeadingEdgeDebounce(
    pw::async2::TimeProvider<pw::chrono::SystemClock>& time,
    pw::chrono::SystemClock::duration threshold)
    : time_(time), threshold_(threshold) {}

pw::async2::Poll<> LeadingEdgeDebounce::DoPend(pw::async2::Context& cx) {
  switch (phase_) {
    case Phase::kWaiting: {
      PW_ASYNC_STORE_WAKER(cx, waker_, "LeadingEdgeDebounce waiting");
      if (!interrupt_fired_.exchange(false)) {
        return pw::async2::Pending();
      }
      if (interrupt_state_ != pw::digital_io::State::kActive) {
        return pw::async2::Pending();
      }
      waker_.Clear();
      NotifyChange(State::kActive);
      phase_ = Phase::kDebouncing;
      timer_ = time_.WaitFor(threshold_);
      cx.ReEnqueue();
      return pw::async2::Pending();
    }

    case Phase::kDebouncing: {
      if (timer_.Pend(cx).IsPending())
        return pw::async2::Pending();
      // Drain any interrupts that arrived during the debounce window.
      interrupt_fired_ = false;
      phase_ = Phase::kWaitingForReset;
      cx.ReEnqueue();
      return pw::async2::Pending();
    }

    case Phase::kWaitingForReset: {
      PW_ASYNC_STORE_WAKER(cx, waker_, "LeadingEdgeDebounce active");
      if (!interrupt_fired_.exchange(false)) {
        return pw::async2::Pending();
      }
      if (interrupt_state_ != pw::digital_io::State::kInactive) {
        return pw::async2::Pending();
      }
      waker_.Clear();
      NotifyChange(State::kInactive);
      phase_ = Phase::kWaiting;
      cx.ReEnqueue();
      return pw::async2::Pending();
    }
  }

  return pw::async2::Pending();  // unreachable
}

// PulseWidthDebounce --------------------------------------------------------

PulseWidthDebounce::PulseWidthDebounce(
    pw::async2::TimeProvider<pw::chrono::SystemClock>& time,
    pw::chrono::SystemClock::duration threshold)
    : time_(time), threshold_(threshold) {}

pw::async2::Poll<> PulseWidthDebounce::DoPend(pw::async2::Context& cx) {
  switch (phase_) {
    case Phase::kIdle: {
      PW_ASYNC_STORE_WAKER(cx, waker_, "PulseWidthDebounce idle");
      if (!interrupt_fired_.exchange(false)) {
        return pw::async2::Pending();
      }
      if (interrupt_state_ != pw::digital_io::State::kActive) {
        return pw::async2::Pending();
      }
      waker_.Clear();
      timer_ = time_.WaitFor(threshold_);
      phase_ = Phase::kMeasuring;
      cx.ReEnqueue();
      return pw::async2::Pending();
    }

    case Phase::kMeasuring: {
      // Check timer first: expiry confirms a long press.
      if (!timer_.Pend(cx).IsPending()) {
        waker_.Clear();
        NotifyChange(State::kActive);
        // If button was released concurrently with timer expiry, also notify.
        if (interrupt_fired_.exchange(false) &&
            interrupt_state_ == pw::digital_io::State::kInactive) {
          NotifyChange(State::kInactive);
          phase_ = Phase::kIdle;
        } else {
          phase_ = Phase::kWaitingForRelease;
        }
        cx.ReEnqueue();
        return pw::async2::Pending();
      }

      // Timer still pending. Check for early release (before threshold).
      if (interrupt_fired_.exchange(false) &&
          interrupt_state_ == pw::digital_io::State::kInactive) {
        waker_.Clear();
        timer_ = {};
        phase_ = Phase::kIdle;
        cx.ReEnqueue();
        return pw::async2::Pending();
      }

      // Still waiting; store interrupt waker for early-release detection.
      PW_ASYNC_STORE_WAKER(cx, waker_, "PulseWidthDebounce measuring");
      return pw::async2::Pending();
    }

    case Phase::kWaitingForRelease: {
      PW_ASYNC_STORE_WAKER(
          cx, waker_, "PulseWidthDebounce waiting for release");
      if (!interrupt_fired_.exchange(false)) {
        return pw::async2::Pending();
      }
      if (interrupt_state_ != pw::digital_io::State::kInactive) {
        return pw::async2::Pending();
      }
      waker_.Clear();
      NotifyChange(State::kInactive);
      phase_ = Phase::kIdle;
      cx.ReEnqueue();
      return pw::async2::Pending();
    }
  }

  return pw::async2::Pending();  // unreachable
}

}  // namespace toolbelt::dsp
