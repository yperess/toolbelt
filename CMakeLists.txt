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

cmake_minimum_required(VERSION 3.20)

if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
  project(toolbelt)

  include($ENV{PW_ROOT}/pw_build/pigweed.cmake)

  # Backends for host builds.
  set(pw_assert.check_BACKEND pw_assert.print_and_abort_check_backend CACHE STRING "" FORCE)
  set(pw_assert.assert_BACKEND pw_assert.print_and_abort_assert_backend CACHE STRING "" FORCE)
  set(pw_log_BACKEND pw_log_basic CACHE STRING "" FORCE)
  set(pw_sys_io_BACKEND pw_sys_io_stdio CACHE STRING "" FORCE)
  set(pw_chrono.system_clock_BACKEND pw_chrono_stl.system_clock CACHE STRING "" FORCE)
  set(pw_sync.binary_semaphore_BACKEND pw_sync_stl.binary_semaphore_backend CACHE STRING "" FORCE)
  set(pw_sync.interrupt_spin_lock_BACKEND pw_sync_stl.interrupt_spin_lock CACHE STRING "" FORCE)
  set(pw_sync.mutex_BACKEND pw_sync_stl.mutex_backend CACHE STRING "" FORCE)
  set(pw_sync.thread_notification_BACKEND pw_sync.binary_semaphore_thread_notification_backend CACHE STRING "" FORCE)
  set(pw_sync.timed_thread_notification_BACKEND pw_sync.binary_semaphore_timed_thread_notification_backend CACHE STRING "" FORCE)
  set(pw_thread.id_BACKEND pw_thread_stl.id CACHE STRING "" FORCE)
  set(pw_thread.sleep_BACKEND pw_thread_stl.sleep CACHE STRING "" FORCE)
  set(pw_thread.thread_BACKEND pw_thread_stl.thread CACHE STRING "" FORCE)
  set(pw_thread.thread_creation_BACKEND pw_thread_stl.thread_creation CACHE STRING "" FORCE)

  add_subdirectory(third_party/pigweed)
endif()

add_subdirectory(dsp)
