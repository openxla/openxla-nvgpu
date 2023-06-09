################################################################################
# Autogenerated by ../iree/build_tools/bazel_to_cmake/bazel_to_cmake.py from   #
# compiler/src/openxla/compiler/nvgpu/Dialect/CUDNN/IR/BUILD.bazel             #
#                                                                              #
# Use iree_cmake_extra_content from iree/build_defs.oss.bzl to add arbitrary   #
# CMake-only content.                                                          #
#                                                                              #
# To disable autogeneration for this file entirely, delete this header.        #
################################################################################

iree_add_all_subdirs()

iree_cc_library(
  NAME
    IR
  HDRS
    "CUDNNDialect.h"
    "CUDNNOps.h"
    "CUDNNTypes.h"
  TEXTUAL_HDRS
    "CUDNNOps.cpp.inc"
    "CUDNNOps.h.inc"
    "CUDNNDialect.cpp.inc"
    "CUDNNDialect.h.inc"
    "CUDNNTypes.cpp.inc"
    "CUDNNTypes.h.inc"
  SRCS
    "CUDNNDialect.cpp"
    "CUDNNOps.cpp"
    "CUDNNTypes.cpp"
  DEPS
    ::CUDNNAttrsGen
    ::CUDNNEnumsGen
    ::CUDNNOpsGen
    ::CUDNNTypesGen
    LLVMSupport
    MLIRIR
    MLIRSupport
    iree::compiler::Dialect::HAL::IR
    openxla::compiler::nvgpu::defs
  PUBLIC
)

iree_tablegen_library(
  NAME
    CUDNNOpsGen
  TD_FILE
    "CUDNNOps.td"
  OUTS
    --gen-dialect-decls --dialect=cudnn CUDNNDialect.h.inc
    --gen-dialect-defs --dialect=cudnn CUDNNDialect.cpp.inc
    --gen-op-decls CUDNNOps.h.inc
    --gen-op-defs CUDNNOps.cpp.inc
)

iree_tablegen_library(
  NAME
    CUDNNTypesGen
  TD_FILE
    "CUDNNTypes.td"
  OUTS
    --gen-typedef-decls CUDNNTypes.h.inc
    --gen-typedef-defs CUDNNTypes.cpp.inc
)

iree_tablegen_library(
  NAME
    CUDNNEnumsGen
  TD_FILE
    "CUDNNTypes.td"
  OUTS
    --gen-enum-decls CUDNNEnums.h.inc
    --gen-enum-defs CUDNNEnums.cpp.inc
)

iree_tablegen_library(
  NAME
    CUDNNAttrsGen
  TD_FILE
    "CUDNNTypes.td"
  OUTS
    --gen-attrdef-decls CUDNNAttrs.h.inc
    --gen-attrdef-defs CUDNNAttrs.cpp.inc
)

### BAZEL_TO_CMAKE_PRESERVES_ALL_CONTENT_BELOW_THIS_LINE ###
