# Copyright 2026 Yuval Peress
#
# Licensed under the Apache License, Version 2.0 (the "License"); you may not
# use this file except in compliance with the License. You may obtain a copy of
# the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the
# License for the specific language governing permissions and limitations under
# the License.

include($ENV{PW_ROOT}/pw_build/pigweed.cmake)

pw_add_library(dsp.debounce STATIC
  HEADERS
    public/toolbelt/dsp/debounce.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    pw_async2
  SOURCES
    debounce.cc
)

pw_add_library(dsp.polling_debounce STATIC
  HEADERS
    public/toolbelt/dsp/polling_debounce.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    dsp.debounce
    pw_async2
    pw_async2.time_provider
    pw_chrono.system_clock
    pw_function
  SOURCES
    polling_debounce.cc
)

pw_add_library(dsp.interrupt_debounce STATIC
  HEADERS
    public/toolbelt/dsp/interrupt_debounce.h
  PUBLIC_INCLUDES
    public
  PUBLIC_DEPS
    dsp.debounce
    pw_async2
    pw_async2.time_provider
    pw_chrono.system_clock
    pw_digital_io
  SOURCES
    interrupt_debounce.cc
)

pw_add_test(dsp.polling_debounce_test
  SOURCES
    polling_debounce_test.cc
  PRIVATE_DEPS
    dsp.polling_debounce
    pw_async2.testing
    pw_async2.simulated_time_provider
  GROUPS
    modules
    toolbelt
)

pw_add_test(dsp.interrupt_debounce_test
  SOURCES
    interrupt_debounce_test.cc
  PRIVATE_DEPS
    dsp.interrupt_debounce
    pw_async2.testing
    pw_async2.simulated_time_provider
    pw_digital_io
  GROUPS
    modules
    toolbelt
)
