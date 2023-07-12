# Copyright 2023 The OpenXLA Authors
#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

cmake_minimum_required(VERSION 3.21...3.24)

project(OPENXLA_NVGPU)

include(CMakeDependentOption)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(OPENXLA_NVGPU_ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# TODO: Fix this once the project is slotted into place.
if(NOT IREE_ROOT_DIR)
  set(IREE_ROOT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../iree")
endif()

set(IREE_CXX_STANDARD ${CMAKE_CXX_STANDARD})

# Enable testing before any subdirectories are added.
enable_testing()

#-------------------------------------------------------------------------------
# Project settings (for standalone builds)
#-------------------------------------------------------------------------------

option(OPENXLA_NVGPU_BUILD_COMPILER "Build NVPU compiler components" ON)

# Runtime components.
option(OPENXLA_NVGPU_BUILD_RUNTIME "Builds runtime components (can be individually enabled)" ON)
cmake_dependent_option(OPENXLA_NVGPU_BUILD_RUNTIME_CUDNN "Build NVPU CUDNN runtime components" ON ${OPENXLA_NVGPU_BUILD_RUNTIME} OFF)
cmake_dependent_option(OPENXLA_NVGPU_BUILD_RUNTIME_TRITON "Build NVGPU Triton runtime components" ON ${OPENXLA_NVGPU_BUILD_RUNTIME} OFF)
cmake_dependent_option(OPENXLA_ASYNC_BUILD_RUNTIME "Build async runtime components" ON ${OPENXLA_NVGPU_BUILD_RUNTIME} OFF)

option(OPENXLA_NVGPU_INCLUDE_DEVICE_TESTS "Include tests that depend on the device in test targets" ON)

#-------------------------------------------------------------------------------
# Customize IREE compiler defaults.
#-------------------------------------------------------------------------------

cmake_dependent_option(IREE_BUILD_COMPILER "Builds the IREE compiler" ON ${OPENXLA_NVGPU_BUILD_COMPILER} OFF)
option(IREE_TARGET_BACKEND_DEFAULTS "Disables target backend" OFF)
if(OPENXLA_NVGPU_BUILD_COMPILER)
  option(IREE_BUILD_COMPILER "Enable IREE compiler build" ON)
  option(IREE_BUILD_SAMPLES "Disable IREE samples build" OFF)
  option(IREE_TARGET_BACKEND_CUDA "Enables CUDA target backend" ON)

  # Since the standalone build is primarily targeted for development mode,
  # enabled shared library builds, which enable better plugin flows
  # (but are not how we typically package for deployment).
  option(IREE_COMPILER_BUILD_SHARED_LIBS "Enables shared libraries in the compiler by default" ON)
endif()

#-------------------------------------------------------------------------------
# Customize IREE runtime defaults.
#-------------------------------------------------------------------------------

cmake_dependent_option(IREE_BUILD_RUNTIME "Builds IREE runtime components" ON ${OPENXLA_NVGPU_BUILD_RUNTIME} OFF)
option(IREE_HAL_DRIVER_DEFAULTS "Disable all HAL drivers by default" OFF)
if(OPENXLA_NVGPU_BUILD_RUNTIME)
  option(IREE_HAL_DRIVER_CUDA "Enables the 'cuda' runtime HAL driver" ON)
endif()

#-------------------------------------------------------------------------------
# Customize IREE LIT test runner.
#-------------------------------------------------------------------------------

# TODO(ezhulenev): Python3 executable and LLVM LIT are required for running lit
# tests under `runtime` folder. Figure out how to initialize them as a part of
# `iree_setup_toolchain` invoked below.

find_package(Python3 COMPONENTS Interpreter QUIET)

set(IREE_TEST_TMPDIR_ROOT "${CMAKE_CURRENT_BINARY_DIR}/test_tmpdir")
set(LLVM_EXTERNAL_LIT "${IREE_ROOT_DIR}/third_party/llvm-project/llvm/utils/lit/lit.py")

#-------------------------------------------------------------------------------
# Add OpenXLA Nvgpu compiler plugin to IREE compiler tools.
#-------------------------------------------------------------------------------

set(IREE_COMPILER_PLUGIN_PATHS "${CMAKE_CURRENT_SOURCE_DIR}" CACHE STRING "OpenXLA Nvgpu plugins")

#-------------------------------------------------------------------------------
# Add OpenXLA Nvgpu runtime modules too IREE runtime tools.
#-------------------------------------------------------------------------------

if(OPENXLA_NVGPU_BUILD_RUNTIME_CUDNN)
  set(IREE_EXTERNAL_TOOLING_MODULES "cudnn" CACHE STRING "OpenXLA Nvgpu cuDNN module")

  set(IREE_EXTERNAL_TOOLING_MODULE_CUDNN_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/runtime/src/openxla/runtime/nvgpu" CACHE STRING "OpenXLA Nvgpu cuDNN module source dir")
  set(IREE_EXTERNAL_TOOLING_MODULE_CUDNN_BINARY_DIR "${CMAKE_CURRENT_BINARY_DIR}/runtime/src/openxla/runtime/nvgpu" CACHE STRING "OpenXLA Nvgpu cuDNN module binary dir")
  set(IREE_EXTERNAL_TOOLING_MODULE_CUDNN_TARGET "openxla_runtime_nvgpu_cudnn_cudnn_module" CACHE STRING "OpenXLA Nvgpu cuDNN module target")
  set(IREE_EXTERNAL_TOOLING_MODULE_CUDNN_NAME "cudnn" CACHE STRING "OpenXLA Nvgpu cuDNN module name")
  set(IREE_EXTERNAL_TOOLING_MODULE_CUDNN_REGISTER_TYPES "openxla_nvgpu_cudnn_module_register_types" CACHE STRING "OpenXLA Nvgpu cuDNN module type registration")
  set(IREE_EXTERNAL_TOOLING_MODULE_CUDNN_CREATE "openxla_nvgpu_cudnn_module_create" CACHE STRING "OpenXLA Nvgpu cuDNN module creator")
endif()

#-------------------------------------------------------------------------------
# Set up IREE toolchain with OpenXLA Nvgpu modules.
# We set the IREE dependency up as EXCLUDE_FROM_ALL and then add an explicit
# target to include the minimal IREE dependencies needed to build and run
# tests. This can save 20%+ on default builds that just need to run tests.
#-------------------------------------------------------------------------------

add_subdirectory("${IREE_ROOT_DIR}" "iree_core" EXCLUDE_FROM_ALL)

# Handle various global definitions that need to be set at the global
# toolchain level.
iree_setup_toolchain()

add_custom_target(openxla-nvgpu-iree-deps
  ALL
  COMMENT
    "IREE dependencies needed for a standalone NVGPU build (allows IREE to be EXCLUDE_FROM_ALL)"
)

if(OPENXLA_NVGPU_BUILD_COMPILER)
  add_dependencies(openxla-nvgpu-iree-deps
    iree-compile
    iree-opt
    FileCheck
  )

  if (IREE_BUILD_PYTHON_BINDINGS)
    add_dependencies(openxla-nvgpu-iree-deps
      IREECompilerPythonModules
    )
  endif()
endif()

if(OPENXLA_NVGPU_BUILD_RUNTIME)
  add_dependencies(openxla-nvgpu-iree-deps
    iree-benchmark-module
    iree-benchmark-trace
    iree-check-module
    iree-run-module
    iree-run-trace
  )

  if (IREE_BUILD_PYTHON_BINDINGS)
    add_dependencies(openxla-nvgpu-iree-deps
      iree_runtime_bindings_python_runtime
    )
  endif()
endif()

#-------------------------------------------------------------------------------
# OpenXLA NVGPU Runtime.
#
# Integration of libraries with IREE runtime via custom VM modules.
#-------------------------------------------------------------------------------

if(OPENXLA_NVGPU_BUILD_RUNTIME)
  add_subdirectory(runtime)
endif()

#-------------------------------------------------------------------------------
# NVIDIA dependencies
#-------------------------------------------------------------------------------

if(OPENXLA_NVGPU_BUILD_RUNTIME_CUDNN)
  add_subdirectory(build_tools/third_party/cudnn-frontend EXCLUDE_FROM_ALL)
endif()

#-------------------------------------------------------------------------------
# Triton dependencies
#-------------------------------------------------------------------------------

add_subdirectory(build_tools/third_party/triton EXCLUDE_FROM_ALL)

#-------------------------------------------------------------------------------
# Google absl/tsl dependencies
#-------------------------------------------------------------------------------

if(OPENXLA_ASYNC_BUILD_RUNTIME)
  add_subdirectory(build_tools/third_party/tsl EXCLUDE_FROM_ALL)
  add_subdirectory(third_party/abseil-cpp EXCLUDE_FROM_ALL)
endif()


#-------------------------------------------------------------------------------
# Convenience test target.
#-------------------------------------------------------------------------------

add_custom_target(openxla-nvgpu-test-deps)
add_dependencies(openxla-nvgpu-test-deps openxla-nvgpu-iree-deps)

set(OPENXLA_NVGPU_CTEST_ARGS "-L;^openxla/.+/(nvgpu|async)")
if(NOT OPENXLA_NVGPU_INCLUDE_DEVICE_TESTS)
  list(APPEND OPENXLA_NVGPU_CTEST_ARGS "-LE" "driver=cuda")
endif()
add_custom_target(openxla-nvgpu-run-tests
  COMMENT
    "Run OpenXLA NVGPU unit tests"
    WORKING_DIRECTORY
    "${CMAKE_CURRENT_BINARY_DIR}"
  USES_TERMINAL
  VERBATIM
  COMMAND_EXPAND_LISTS
  COMMAND
    "${CMAKE_COMMAND}" -E echo
    "The 'openxla-nvgpu-run-tests' target is a helper for running ctest. For advanced"
    "options, build dependencies and invoke ctest independently as in:"
  COMMAND
    "${CMAKE_COMMAND}" -E echo
    "  (cd ${CMAKE_CURRENT_BINARY_DIR} && cmake --build . --target openxla-nvgpu-test-deps && ctest ${OPENXLA_NVGPU_CTEST_ARGS} --output-on-failure)"
  COMMAND
    "${CMAKE_COMMAND}" -E echo
    "Run tests in parallel by setting a variable like CTEST_PARALLEL_LEVEL=25."
  COMMAND
    "${CMAKE_CTEST_COMMAND}" ${OPENXLA_NVGPU_CTEST_ARGS} --output-on-failure
)
add_dependencies(openxla-nvgpu-run-tests openxla-nvgpu-test-deps)
