// Copyright 2023 The OpenXLA Authors
//
// Licensed under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception

#include "openxla/compiler/nvgpu/Dialect/TritonFlow/IR/TritonFlowOps.h"

#include "iree/compiler/Dialect/Util/IR/UtilOps.h"
#include "llvm/ADT/STLExtras.h"
#include "llvm/ADT/TypeSwitch.h"
#include "mlir/Dialect/Func/IR/FuncOps.h"
#include "mlir/IR/BuiltinTypes.h"
#include "mlir/IR/Diagnostics.h"
#include "mlir/IR/FunctionImplementation.h"
#include "mlir/IR/FunctionInterfaces.h"
#include "mlir/IR/OpImplementation.h"
#include "mlir/IR/Operation.h"
#include "mlir/IR/Types.h"
#include "mlir/Support/LogicalResult.h"
#include "openxla/compiler/nvgpu/Dialect/TritonFlow/IR/TritonFlowDialect.h"
#include "triton/Dialect/Triton/IR/Dialect.h"
#include "triton/Dialect/Triton/IR/Types.h"

namespace openxla::compiler::nvgpu::tritonflow {

using namespace mlir;
using namespace mlir::iree_compiler;

// Verifies that |dynamicDims| contains the appropriate number of dims for all
// of the dynamic dimensions in |values|.
static LogicalResult verifyOpDynamicDims(Operation *op, ValueRange values,
                                         ValueRange dynamicDims) {
  unsigned requiredCount = 0;
  for (auto value : values) {
    if (auto shapedType = value.getType().dyn_cast<ShapedType>()) {
      requiredCount += shapedType.getNumDynamicDims();
    }
  }
  if (dynamicDims.size() != requiredCount) {
    return op->emitOpError()
           << "value set has " << requiredCount
           << " dynamic dimensions but only " << dynamicDims.size()
           << " dimension values are attached";
  }
  return success();
}

static LogicalResult verifyArgumentTypes(Operation *op, FunctionType tritonType,
                                         TypeRange args) {
  for (auto pair : llvm::enumerate(llvm::zip(tritonType.getInputs(), args))) {
    auto [tritonArgType, argType] = pair.value();

    // Pointer arguments must be passed as tensors.
    if (auto ptr = tritonArgType.dyn_cast<triton::PointerType>()) {
      auto tensor = argType.dyn_cast<RankedTensorType>();
      if (!tensor)
        return op->emitOpError()
               << "argument #" << pair.index()
               << " must be a tensor matching Triton pointer type at #"
               << pair.index() << " (" << argType << " vs " << ptr << ")";

      if (ptr.getPointeeType() != tensor.getElementType())
        return op->emitOpError()
               << "argument #" << pair.index()
               << " element type must match a Triton pointer type at #"
               << pair.index() << " (" << tensor.getElementType() << " vs "
               << ptr.getPointeeType() << ")";
    }

    // Scalar arguments must be passed as scalar values.
    if (auto integer = tritonArgType.dyn_cast<IntegerType>()) {
      if (integer != argType)
        return op->emitOpError()
               << "argument #" << pair.index() << " must be a scalar of "
               << integer << " type";
    }
  }

  return success();
}

static LogicalResult verifyResultTypes(Operation *op, FunctionType tritonType,
                                       TypeRange rets) {
  // Skip types corresponding to arguments.
  auto numArgs = tritonType.getNumInputs() - rets.size();
  auto tritonRets = tritonType.getInputs().drop_front(numArgs);

  for (auto pair : llvm::enumerate(llvm::zip(tritonRets, rets))) {
    size_t tritonIndex = numArgs + pair.index();
    auto [tritonArgType, argType] = pair.value();

    // Results passed to Triton kernels as destination buffers.
    if (auto ptr = tritonArgType.dyn_cast<triton::PointerType>()) {
      auto tensor = argType.dyn_cast<RankedTensorType>();
      if (!tensor)
        return op->emitOpError()
               << "result #" << pair.index()
               << " must be a tensor matching Triton pointer type at #"
               << tritonIndex << " (" << argType << " vs " << ptr << ")";

      if (ptr.getPointeeType() != tensor.getElementType())
        return op->emitOpError()
               << "result #" << pair.index()
               << " element type must match a Triton pointer type at #"
               << tritonIndex << " (" << tensor.getElementType() << " vs "
               << ptr.getPointeeType() << ")";
    }
  }

  return success();
}

static LogicalResult verifyTritonFunction(Operation *op,
                                          FunctionType tritonFunctionType,
                                          TypeRange args, TypeRange rets) {
  // We don't expect Triton functions returning any results at all.
  if (tritonFunctionType.getNumResults() != 0) {
    return op->emitOpError() << "expected triton function with no results";
  }

  // Triton functions use "destination-passing style" for buffer outputs.
  size_t expectedTritonArgs = args.size() + rets.size();
  if (tritonFunctionType.getNumInputs() != expectedTritonArgs) {
    return op->emitOpError()
           << "expected triton function with " << expectedTritonArgs
           << " arguments, got a function with "
           << tritonFunctionType.getNumInputs() << " arguments";
  }

  // Check that arguments and results match Triton signature.
  if (failed(verifyArgumentTypes(op, tritonFunctionType, args)) ||
      failed(verifyResultTypes(op, tritonFunctionType, rets)))
    return failure();

  return success();
}

//===----------------------------------------------------------------------===//
// triton.executable operation
//===----------------------------------------------------------------------===//

void ExecutableOp::build(OpBuilder &builder, OperationState &state,
                         Twine name) {
  ensureTerminator(*state.addRegion(), builder, state.location);
  state.addAttribute(mlir::SymbolTable::getSymbolAttrName(),
                     builder.getStringAttr(name));
}

LogicalResult ExecutableOp::verify() {
  auto innerModules = getBlock().getOps<ModuleOp>();
  if (llvm::count_if(innerModules, [](ModuleOp) { return true; }) != 1)
    return emitOpError()
           << "expected exactly one inner builtin.module operation";
  return success();
}

//===----------------------------------------------------------------------===//
// triton.executable.export operation
//===----------------------------------------------------------------------===//

void ExecutableExportOp::build(OpBuilder &builder, OperationState &state,
                               StringRef sym_name,
                               FlatSymbolRefAttr function_ref) {
  build(builder, state, /*sym_visibility=*/nullptr,
        builder.getStringAttr(sym_name), function_ref);
}

LogicalResult ExecutableExportOp::verifySymbolUses(
    SymbolTableCollection &symbolTable) {
  auto innerModule = getParentOp<ExecutableOp>().getInnerModule();
  if (!symbolTable.lookupNearestSymbolFrom(innerModule, getFunctionRefAttr()))
    return emitOpError() << "refers to an unknown Triton function: "
                         << getFunctionRefAttr();
  return success();
}

//===----------------------------------------------------------------------===//
// triton.dispatch operation
//===----------------------------------------------------------------------===//

void DispatchOp::build(OpBuilder &builder, OperationState &state,
                       ExecutableExportOp exportOp, ValueRange grid,
                       TypeRange resultTypes, ValueRange resultDims,
                       ValueRange operands, ValueRange operandDims,
                       ArrayAttr tiedOperands,
                       ArrayRef<NamedAttribute> attributes) {
  StringRef executableOpSymName =
      exportOp->getParentOp()
          ->getAttrOfType<StringAttr>(SymbolTable::getSymbolAttrName())
          .getValue();
  state.addAttribute(
      "entry_point",
      SymbolRefAttr::get(builder.getContext(), executableOpSymName,
                         {SymbolRefAttr::get(exportOp)}));

  state.addOperands(grid);
  state.addTypes(resultTypes);
  state.addOperands(operands);
  state.addOperands(operandDims);
  state.addOperands(resultDims);
  state.addAttributes(attributes);
  state.attributes.erase(IREE::Util::TiedOpInterface::getStorageAttrName());
  state.addAttribute(IREE::Util::TiedOpInterface::getStorageAttrName(),
                     tiedOperands);
  state.attributes.erase(getOperandSegmentSizeAttr());
  state.addAttribute(getOperandSegmentSizeAttr(),
                     builder.getDenseI32ArrayAttr({
                         static_cast<int32_t>(grid.size()),
                         static_cast<int32_t>(operands.size()),
                         static_cast<int32_t>(operandDims.size()),
                         static_cast<int32_t>(resultDims.size()),
                     }));
}

LogicalResult DispatchOp::verifySymbolUses(SymbolTableCollection &symbolTable) {
  auto exportOp = symbolTable.lookupNearestSymbolFrom<ExecutableExportOp>(
      getOperation(), getEntryPoint());
  if (!exportOp)
    return emitOpError() << "refers to an unknown Triton entry point: "
                         << getEntryPoint();

  // Find the exported function in the inner module.
  auto innerModule = exportOp.getParentOp<ExecutableOp>().getInnerModule();
  auto tritonFunc = symbolTable.lookupNearestSymbolFrom<triton::FuncOp>(
      innerModule, exportOp.getFunctionRefAttr());

  // Export op itself checks that exported function exists in the inner module,
  // so it only means that it was already lowered to LLVM and we skip verifier.
  if (!tritonFunc) return success();

  // Check that Triton function is compatible with the call arguments.
  if (failed(verifyTritonFunction(getOperation(), tritonFunc.getFunctionType(),
                                  getArguments().getTypes(),
                                  getResultTypes()))) {
    return failure();
  }

  return success();
}

std::pair<unsigned, unsigned> DispatchOp::getTiedOperandsIndexAndLength() {
  return getODSOperandIndexAndLength(1);  // $arguments
}

ValueRange DispatchOp::getOperandDynamicDims(unsigned idx) {
  return IREE::Util::findVariadicDynamicDims(idx - getGrid().size(),
                                             getArguments(), getArgumentDims());
}
ValueRange DispatchOp::getResultDynamicDims(unsigned idx) {
  return IREE::Util::findVariadicDynamicDims(idx, getResults(),
                                             getResultDims());
}

LogicalResult DispatchOp::verify() {
  Operation *op = getOperation();
  if (failed(verifyOpDynamicDims(op, getArguments(), getArgumentDims())) ||
      failed(verifyOpDynamicDims(op, getResults(), getResultDims()))) {
    return failure();
  }
  return success();
}

//===----------------------------------------------------------------------===//
// triton.call operation
//===----------------------------------------------------------------------===//

LogicalResult CallOp::verifySymbolUses(SymbolTableCollection &symbolTable) {
  Operation *op = getOperation();

  auto tritonFunc =
      symbolTable.lookupNearestSymbolFrom<triton::FuncOp>(op, getCalleeAttr());
  if (!tritonFunc)
    return emitOpError() << "refers to an unknown Triton function: "
                         << getCalleeAttr();

  // Check that Triton function is compatible with the call arguments.
  if (failed(verifyTritonFunction(getOperation(), tritonFunc.getFunctionType(),
                                  getArguments().getTypes(),
                                  getResultTypes()))) {
    return failure();
  }

  return success();
}

std::pair<unsigned, unsigned> CallOp::getTiedOperandsIndexAndLength() {
  return getODSOperandIndexAndLength(1);  // $arguments
}

ValueRange CallOp::getOperandDynamicDims(unsigned idx) {
  return IREE::Util::findVariadicDynamicDims(idx - getGrid().size(),
                                             getArguments(), getArgumentDims());
}
ValueRange CallOp::getResultDynamicDims(unsigned idx) {
  return IREE::Util::findVariadicDynamicDims(idx, getResults(),
                                             getResultDims());
}

LogicalResult CallOp::verify() {
  Operation *op = getOperation();
  if (failed(verifyOpDynamicDims(op, getArguments(), getArgumentDims())) ||
      failed(verifyOpDynamicDims(op, getResults(), getResultDims()))) {
    return failure();
  }
  return success();
}

}  // namespace openxla::compiler::nvgpu::tritonflow

#define GET_OP_CLASSES
#include "openxla/compiler/nvgpu/Dialect/TritonFlow/IR/TritonFlowOps.cpp.inc"
