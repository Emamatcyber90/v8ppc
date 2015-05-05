// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_COMMON_OPERATOR_H_
#define V8_COMPILER_COMMON_OPERATOR_H_

#include "src/compiler/machine-type.h"
#include "src/unique.h"

namespace v8 {
namespace internal {

// Forward declarations.
class ExternalReference;


namespace compiler {

// Forward declarations.
class CallDescriptor;
struct CommonOperatorGlobalCache;
class Operator;


// Prediction hint for branches.
enum class BranchHint : uint8_t { kNone, kTrue, kFalse };

inline size_t hash_value(BranchHint hint) { return static_cast<size_t>(hint); }

std::ostream& operator<<(std::ostream&, BranchHint);

BranchHint BranchHintOf(const Operator* const);


class SelectParameters final {
 public:
  explicit SelectParameters(MachineType type,
                            BranchHint hint = BranchHint::kNone)
      : type_(type), hint_(hint) {}

  MachineType type() const { return type_; }
  BranchHint hint() const { return hint_; }

 private:
  const MachineType type_;
  const BranchHint hint_;
};

bool operator==(SelectParameters const&, SelectParameters const&);
bool operator!=(SelectParameters const&, SelectParameters const&);

size_t hash_value(SelectParameters const& p);

std::ostream& operator<<(std::ostream&, SelectParameters const& p);

SelectParameters const& SelectParametersOf(const Operator* const);


// Flag that describes how to combine the current environment with
// the output of a node to obtain a framestate for lazy bailout.
class OutputFrameStateCombine {
 public:
  enum Kind {
    kPushOutput,  // Push the output on the expression stack.
    kPokeAt       // Poke at the given environment location,
                  // counting from the top of the stack.
  };

  static OutputFrameStateCombine Ignore() {
    return OutputFrameStateCombine(kPushOutput, 0);
  }
  static OutputFrameStateCombine Push(size_t count = 1) {
    return OutputFrameStateCombine(kPushOutput, count);
  }
  static OutputFrameStateCombine PokeAt(size_t index) {
    return OutputFrameStateCombine(kPokeAt, index);
  }

  Kind kind() const { return kind_; }
  size_t GetPushCount() const {
    DCHECK_EQ(kPushOutput, kind());
    return parameter_;
  }
  size_t GetOffsetToPokeAt() const {
    DCHECK_EQ(kPokeAt, kind());
    return parameter_;
  }

  bool IsOutputIgnored() const {
    return kind_ == kPushOutput && parameter_ == 0;
  }

  size_t ConsumedOutputCount() const {
    return kind_ == kPushOutput ? GetPushCount() : 1;
  }

  bool operator==(OutputFrameStateCombine const& other) const {
    return kind_ == other.kind_ && parameter_ == other.parameter_;
  }
  bool operator!=(OutputFrameStateCombine const& other) const {
    return !(*this == other);
  }

  friend size_t hash_value(OutputFrameStateCombine const&);
  friend std::ostream& operator<<(std::ostream&,
                                  OutputFrameStateCombine const&);

 private:
  OutputFrameStateCombine(Kind kind, size_t parameter)
      : kind_(kind), parameter_(parameter) {}

  Kind const kind_;
  size_t const parameter_;
};


// The type of stack frame that a FrameState node represents.
enum FrameStateType {
  JS_FRAME,          // Represents an unoptimized JavaScriptFrame.
  ARGUMENTS_ADAPTOR  // Represents an ArgumentsAdaptorFrame.
};


class FrameStateCallInfo final {
 public:
  FrameStateCallInfo(
      FrameStateType type, BailoutId bailout_id,
      OutputFrameStateCombine state_combine,
      MaybeHandle<JSFunction> jsfunction = MaybeHandle<JSFunction>())
      : type_(type),
        bailout_id_(bailout_id),
        frame_state_combine_(state_combine),
        jsfunction_(jsfunction) {}

  FrameStateType type() const { return type_; }
  BailoutId bailout_id() const { return bailout_id_; }
  OutputFrameStateCombine state_combine() const { return frame_state_combine_; }
  MaybeHandle<JSFunction> jsfunction() const { return jsfunction_; }

 private:
  FrameStateType type_;
  BailoutId bailout_id_;
  OutputFrameStateCombine frame_state_combine_;
  MaybeHandle<JSFunction> jsfunction_;
};

bool operator==(FrameStateCallInfo const&, FrameStateCallInfo const&);
bool operator!=(FrameStateCallInfo const&, FrameStateCallInfo const&);

size_t hash_value(FrameStateCallInfo const&);

std::ostream& operator<<(std::ostream&, FrameStateCallInfo const&);


size_t ProjectionIndexOf(const Operator* const);


// The {IrOpcode::kParameter} opcode represents an incoming parameter to the
// function. This class bundles the index and a debug name for such operators.
class ParameterInfo final {
 public:
  ParameterInfo(int index, const char* debug_name)
      : index_(index), debug_name_(debug_name) {}

  int index() const { return index_; }
  const char* debug_name() const { return debug_name_; }

 private:
  int index_;
  const char* debug_name_;
};

std::ostream& operator<<(std::ostream&, ParameterInfo const&);

int ParameterIndexOf(const Operator* const);
const ParameterInfo& ParameterInfoOf(const Operator* const);


// Interface for building common operators that can be used at any level of IR,
// including JavaScript, mid-level, and low-level.
class CommonOperatorBuilder final : public ZoneObject {
 public:
  explicit CommonOperatorBuilder(Zone* zone);

  // Special operator used only in Branches to mark them as always taken, but
  // still unfoldable. This is required to properly connect non terminating
  // loops to end (in both the sea of nodes and the CFG).
  const Operator* Always();

  const Operator* Dead();
  const Operator* End();
  const Operator* Branch(BranchHint = BranchHint::kNone);
  const Operator* IfTrue();
  const Operator* IfFalse();
  const Operator* IfSuccess();
  const Operator* IfException();
  const Operator* Switch(size_t control_output_count);
  const Operator* IfValue(int32_t value);
  const Operator* IfDefault();
  const Operator* Throw();
  const Operator* Deoptimize();
  const Operator* Return();

  const Operator* Start(int num_formal_parameters);
  const Operator* Loop(int control_input_count);
  const Operator* Merge(int control_input_count);
  const Operator* Parameter(int index, const char* debug_name = nullptr);

  const Operator* OsrNormalEntry();
  const Operator* OsrLoopEntry();
  const Operator* OsrValue(int index);

  const Operator* Int32Constant(int32_t);
  const Operator* Int64Constant(int64_t);
  const Operator* Float32Constant(volatile float);
  const Operator* Float64Constant(volatile double);
  const Operator* ExternalConstant(const ExternalReference&);
  const Operator* NumberConstant(volatile double);
  const Operator* HeapConstant(const Unique<HeapObject>&);

  const Operator* Select(MachineType, BranchHint = BranchHint::kNone);
  const Operator* Phi(MachineType type, int value_input_count);
  const Operator* EffectPhi(int effect_input_count);
  const Operator* EffectSet(int arguments);
  const Operator* ValueEffect(int arguments);
  const Operator* Finish(int arguments);
  const Operator* StateValues(int arguments);
  const Operator* TypedStateValues(const ZoneVector<MachineType>* types);
  const Operator* FrameState(
      FrameStateType type, BailoutId bailout_id,
      OutputFrameStateCombine state_combine,
      MaybeHandle<JSFunction> jsfunction = MaybeHandle<JSFunction>());
  const Operator* Call(const CallDescriptor* descriptor);
  const Operator* TailCall(const CallDescriptor* descriptor);
  const Operator* Projection(size_t index);

  // Constructs a new merge or phi operator with the same opcode as {op}, but
  // with {size} inputs.
  const Operator* ResizeMergeOrPhi(const Operator* op, int size);

 private:
  Zone* zone() const { return zone_; }

  const CommonOperatorGlobalCache& cache_;
  Zone* const zone_;

  DISALLOW_COPY_AND_ASSIGN(CommonOperatorBuilder);
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_COMMON_OPERATOR_H_
