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

#include <vector>

#include "pw_async2/dispatcher_for_test.h"
#include "pw_async2/simulated_time_provider.h"
#include "pw_unit_test/framework.h"

namespace toolbelt::dsp {
namespace {

// Records all state changes emitted by a Debounce via NextState().
class StateCapture : public pw::async2::Task {
 public:
  explicit StateCapture(Debounce& d) : debounce_(d) {}

  std::vector<Debounce::State> captured;

 private:
  pw::async2::Poll<> DoPend(pw::async2::Context& cx) override {
    while (true) {
      auto poll = debounce_.NextState().Pend(cx);
      if (poll.IsPending())
        return pw::async2::Pending();
      captured.push_back(*poll);
    }
  }

  Debounce& debounce_;
};

// ---------------------------------------------------------------------------
// LeadingEdgeDebounce
// ---------------------------------------------------------------------------

TEST(LeadingEdgeDebounce, NotifiesActiveImmediatelyOnRisingEdge) {
  pw::async2::SimulatedTimeProvider<pw::chrono::SystemClock> time;
  pw::async2::DispatcherForTest dispatcher;

  LeadingEdgeDebounce debouncer(time, std::chrono::milliseconds(50));
  StateCapture capture(debouncer);

  dispatcher.Post(capture);
  dispatcher.Post(debouncer);
  dispatcher.RunUntilStalled();
  EXPECT_TRUE(capture.captured.empty());

  // Rising edge → notifies kActive immediately, before the timer expires.
  debouncer.OnInterrupt(pw::digital_io::State::kActive);
  dispatcher.RunUntilStalled();
  ASSERT_EQ(capture.captured.size(), 1u);
  EXPECT_EQ(capture.captured[0], Debounce::State::kActive);

  // Advance past threshold — no additional notification.
  time.AdvanceTime(std::chrono::milliseconds(51));
  dispatcher.RunUntilStalled();
  ASSERT_EQ(capture.captured.size(), 1u);

  // Falling edge → notifies kInactive.
  debouncer.OnInterrupt(pw::digital_io::State::kInactive);
  dispatcher.RunUntilStalled();
  ASSERT_EQ(capture.captured.size(), 2u);
  EXPECT_EQ(capture.captured[1], Debounce::State::kInactive);

  capture.Deregister();
  debouncer.Deregister();
}

TEST(LeadingEdgeDebounce, IgnoresBouncesDuringDebounceWindow) {
  pw::async2::SimulatedTimeProvider<pw::chrono::SystemClock> time;
  pw::async2::DispatcherForTest dispatcher;

  LeadingEdgeDebounce debouncer(time, std::chrono::milliseconds(50));
  StateCapture capture(debouncer);

  dispatcher.Post(capture);
  dispatcher.Post(debouncer);
  dispatcher.RunUntilStalled();

  // Rising edge → kActive notified immediately.
  debouncer.OnInterrupt(pw::digital_io::State::kActive);
  dispatcher.RunUntilStalled();
  ASSERT_EQ(capture.captured.size(), 1u);
  EXPECT_EQ(capture.captured[0], Debounce::State::kActive);

  // Noise during the debounce window — these must not produce extra events.
  debouncer.OnInterrupt(pw::digital_io::State::kInactive);
  dispatcher.RunUntilStalled();
  debouncer.OnInterrupt(pw::digital_io::State::kActive);
  dispatcher.RunUntilStalled();
  ASSERT_EQ(capture.captured.size(), 1u);

  // Timer expires; still only the original kActive.
  time.AdvanceTime(std::chrono::milliseconds(51));
  dispatcher.RunUntilStalled();
  ASSERT_EQ(capture.captured.size(), 1u);

  // Clean falling edge after debounce → kInactive.
  debouncer.OnInterrupt(pw::digital_io::State::kInactive);
  dispatcher.RunUntilStalled();
  ASSERT_EQ(capture.captured.size(), 2u);
  EXPECT_EQ(capture.captured[1], Debounce::State::kInactive);

  capture.Deregister();
  debouncer.Deregister();
}

// ---------------------------------------------------------------------------
// PulseWidthDebounce
// ---------------------------------------------------------------------------

TEST(PulseWidthDebounce, FiresForLongPulse) {
  pw::async2::SimulatedTimeProvider<pw::chrono::SystemClock> time;
  pw::async2::DispatcherForTest dispatcher;

  PulseWidthDebounce debouncer(time, std::chrono::milliseconds(100));
  StateCapture capture(debouncer);

  dispatcher.Post(capture);
  dispatcher.Post(debouncer);
  dispatcher.RunUntilStalled();

  debouncer.OnInterrupt(pw::digital_io::State::kActive);
  dispatcher.RunUntilStalled();

  // Advance past threshold, then falling edge.
  time.AdvanceTime(std::chrono::milliseconds(101));
  debouncer.OnInterrupt(pw::digital_io::State::kInactive);
  dispatcher.RunUntilStalled();

  ASSERT_EQ(capture.captured.size(), 2u);
  EXPECT_EQ(capture.captured[0], Debounce::State::kActive);
  EXPECT_EQ(capture.captured[1], Debounce::State::kInactive);

  capture.Deregister();
  debouncer.Deregister();
}

TEST(PulseWidthDebounce, IgnoresShortPulse) {
  pw::async2::SimulatedTimeProvider<pw::chrono::SystemClock> time;
  pw::async2::DispatcherForTest dispatcher;

  PulseWidthDebounce debouncer(time, std::chrono::milliseconds(100));
  StateCapture capture(debouncer);

  dispatcher.Post(capture);
  dispatcher.Post(debouncer);
  dispatcher.RunUntilStalled();

  debouncer.OnInterrupt(pw::digital_io::State::kActive);
  dispatcher.RunUntilStalled();

  // Falling edge before threshold.
  time.AdvanceTime(std::chrono::milliseconds(50));
  debouncer.OnInterrupt(pw::digital_io::State::kInactive);
  dispatcher.RunUntilStalled();

  EXPECT_TRUE(capture.captured.empty());

  capture.Deregister();
  debouncer.Deregister();
}

// ---------------------------------------------------------------------------
// Shared interrupt source: LeadingEdgeDebounce + PulseWidthDebounce together
// ---------------------------------------------------------------------------

TEST(SharedInterrupt, LeadingEdgeAndPulseWidthShareOneSource) {
  pw::async2::SimulatedTimeProvider<pw::chrono::SystemClock> time;
  pw::async2::DispatcherForTest dispatcher;

  // Tap threshold: 50 ms leading-edge debounce window.
  // Long-press threshold: 200 ms pulse width.
  LeadingEdgeDebounce tap(time, std::chrono::milliseconds(50));
  PulseWidthDebounce long_press(time, std::chrono::milliseconds(200));

  StateCapture tap_capture(tap);
  StateCapture lp_capture(long_press);

  dispatcher.Post(tap_capture);
  dispatcher.Post(lp_capture);
  dispatcher.Post(tap);
  dispatcher.Post(long_press);
  dispatcher.RunUntilStalled();

  // Simulate a single interrupt source fanning out via the base-class pointer.
  InterruptBasedDebounce* debouncers[] = {&tap, &long_press};
  auto fire = [&](pw::digital_io::State s) {
    for (auto* d : debouncers)
      d->OnInterrupt(s);
  };

  // --- Long press ---
  // Rising edge: tap fires kActive immediately; long_press starts measuring.
  fire(pw::digital_io::State::kActive);
  dispatcher.RunUntilStalled();
  ASSERT_EQ(tap_capture.captured.size(), 1u);
  EXPECT_EQ(tap_capture.captured[0], Debounce::State::kActive);
  EXPECT_TRUE(lp_capture.captured.empty());

  // Advance past both thresholds; let tap's debounce window expire first so
  // kWaitingForReset is entered before the falling edge arrives.
  time.AdvanceTime(std::chrono::milliseconds(201));
  dispatcher.RunUntilStalled();
  fire(pw::digital_io::State::kInactive);
  dispatcher.RunUntilStalled();

  // tap sees kInactive (after its debounce window).
  ASSERT_EQ(tap_capture.captured.size(), 2u);
  EXPECT_EQ(tap_capture.captured[1], Debounce::State::kInactive);

  // long_press fires Active + Inactive because pulse exceeded 200 ms.
  ASSERT_EQ(lp_capture.captured.size(), 2u);
  EXPECT_EQ(lp_capture.captured[0], Debounce::State::kActive);
  EXPECT_EQ(lp_capture.captured[1], Debounce::State::kInactive);

  tap_capture.Deregister();
  lp_capture.Deregister();
  tap.Deregister();
  long_press.Deregister();
}

}  // namespace
}  // namespace toolbelt::dsp
