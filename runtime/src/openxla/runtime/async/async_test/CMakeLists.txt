# Copyright 2023 The IREE Authors
#
# Licensed under the Apache License v2.0 with LLVM Exceptions.
# See https://llvm.org/LICENSE.txt for license information.
# SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

iree_add_all_subdirs()

iree_bytecode_module(
    NAME
      module_test_module
    SRC
      "module_test.mlir"
    C_IDENTIFIER
      "openxla_async_module_test_module"
    FLAGS
      "--compile-mode=vm"
    TESTONLY
    PUBLIC
)

iree_cc_library(
  NAME
    async_test
  INCLUDES
    "${CMAKE_CURRENT_BINARY_DIR}/../../../../build_tools/third_party/"
  HDRS
    "module.h"
  SRCS
    "module.cpp"
  DEPS
    openxla::runtime::async::defs
    openxla::runtime::async::async_runtime
    tsl::async_value
    iree::vm
    iree::base::internal::threading
  PUBLIC
)
