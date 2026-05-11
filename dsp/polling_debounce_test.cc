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
      if (poll.IsPending()) {
        return pw::async2::Pending();
      }
      captured.push_back(*poll);
    }
  }

  Debounce& debounce_;
};

TEST(PollingDebounce, NotifiesOnStableTransitionToActive) {
  pw::async2::SimulatedTimeProvider<pw::chrono::SystemClock> time;
  pw::async2::DispatcherForTest dispatcher;

  Debounce::State current_pin = Debounce::State::kInactive;

  PollingDebounce debouncer([&]() { return current_pin; },
                            time,
                            std::chrono::milliseconds(10),
                            /*stable_count=*/3);

  StateCapture capture(debouncer);
  dispatcher.Post(capture);
  dispatcher.Post(debouncer);
  dispatcher.RunUntilStalled();

  // First poll: inactive — no change.
  time.AdvanceTime(std::chrono::milliseconds(10));
  dispatcher.RunUntilStalled();
  EXPECT_TRUE(capture.captured.empty());

  // Drive active; needs 3 stable samples.
  current_pin = Debounce::State::kActive;
  time.AdvanceTime(std::chrono::milliseconds(10));
  dispatcher.RunUntilStalled();
  time.AdvanceTime(std::chrono::milliseconds(10));
  dispatcher.RunUntilStalled();
  EXPECT_TRUE(capture.captured.empty());  // Only 2 stable samples so far.

  time.AdvanceTime(std::chrono::milliseconds(10));
  dispatcher.RunUntilStalled();
  ASSERT_EQ(capture.captured.size(), 1u);
  EXPECT_EQ(capture.captured[0], Debounce::State::kActive);

  capture.Deregister();
  debouncer.Deregister();
}

TEST(PollingDebounce, IgnoresNoiseThatNeverStabilizes) {
  pw::async2::SimulatedTimeProvider<pw::chrono::SystemClock> time;
  pw::async2::DispatcherForTest dispatcher;

  Debounce::State current_pin = Debounce::State::kInactive;

  PollingDebounce debouncer([&]() { return current_pin; },
                            time,
                            std::chrono::milliseconds(10),
                            /*stable_count=*/3);

  StateCapture capture(debouncer);
  dispatcher.Post(capture);
  dispatcher.Post(debouncer);
  dispatcher.RunUntilStalled();

  // Flicker active → inactive before reaching stable_count.
  current_pin = Debounce::State::kActive;
  time.AdvanceTime(std::chrono::milliseconds(10));
  dispatcher.RunUntilStalled();
  time.AdvanceTime(std::chrono::milliseconds(10));
  dispatcher.RunUntilStalled();
  current_pin = Debounce::State::kInactive;
  time.AdvanceTime(std::chrono::milliseconds(10));
  dispatcher.RunUntilStalled();

  EXPECT_TRUE(capture.captured.empty());

  capture.Deregister();
  debouncer.Deregister();
}

}  // namespace
}  // namespace toolbelt::dsp
