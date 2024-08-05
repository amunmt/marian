#include "tensors/tensor_operators.h"
#include "tensors/allocator.h"
#include <numeric>

// CPU implementation of proper Marian top-k operator for TopkNodeOp
// This file contains a lot of code-duplicaton with src/translator/nth_element.cpp
// the goal is to replace the beam-search specific topk search with this code.
// Currently this is only used in the unit tests, but we will move forward and
// make the beam-search more graph and operator-based.

namespace marian {
namespace cpu {

void TopK(Tensor outVal, Tensor outInd, Ptr<Allocator> /*allocator*/, const Tensor in, int k, int axis, bool descending) {

  ABORT_IF(axis != in->shape().size() - 1, "Currently only works for last axis");
  ABORT_IF(in->type() != Type::float32, "Input should have type {}", Type::float32);
  ABORT_IF(outInd->type() != Type::uint32, "Output should be have type {}", Type::uint32);

  int cols = in->shape()[axis];
  int rows = in->shape().elements() / cols;

  ABORT_IF(k > cols, "Cannot select more than {} elements for axis {}", cols, axis);

  std::vector<IndexType> idxs(cols);
  std::iota(idxs.begin(), idxs.end(), 0);

  const float* inDataPtr = in->data<float>();
  IndexType* outIndPtr   = outInd->data<IndexType>();
  float* outValPtr       = outVal->data<float>();
  for(int i = 0; i < rows; ++i) {
    std::partial_sort(
      // sorts the top N (beam size) idxs by score to the front
      idxs.begin(),
      idxs.begin() + k,
      idxs.end(),
      [&](int a, int b) {
        return descending ? inDataPtr[a] > inDataPtr[b] : inDataPtr[a] < inDataPtr[b];
      }
    );

    for(int j = 0; j < k; j++) {
      outIndPtr[j] = idxs[j];
      outValPtr[j] = inDataPtr[idxs[j]];
    }

    outIndPtr += k;
    outValPtr += k;
    inDataPtr += cols;
  }
}

template <typename T>
void SortTyped(Tensor outVal, Tensor outInd, Ptr<Allocator> /*allocator*/, const Tensor in, int axis, bool descending) {
  int cols = in->shape()[axis];
  int rows = in->shape().elements() / cols;

  std::vector<IndexType> idxs(cols);
  std::iota(idxs.begin(), idxs.end(), 0);

  const T* inDataPtr = in->data<T>();
  IndexType* outIndPtr   = outInd->data<IndexType>();
  T* outValPtr       = outVal->data<T>();
  for(int i = 0; i < rows; ++i) {
    std::sort(
      idxs.begin(),
      idxs.end(),
      [&](int a, int b) {
        return descending ? inDataPtr[a] > inDataPtr[b] : inDataPtr[a] < inDataPtr[b];
      }
    );

    for(int j = 0; j < cols; j++) {
      outIndPtr[j] = idxs[j];
      outValPtr[j] = inDataPtr[idxs[j]];
    }

    outIndPtr += cols;
    outValPtr += cols;
    inDataPtr += cols;
  }
}

// CPU implementation of Marian sort operator for SortNodeOp
void Sort(Tensor outVal, Tensor outInd, Ptr<Allocator> /*allocator*/, const Tensor in, int axis, bool descending) {
  ABORT_IF(axis != in->shape().size() - 1, "Currently only works for last axis");
  ABORT_IF(outInd->type() != Type::uint32, "Output indices should be have type {}", Type::uint32);

  if(in->type() == Type::float32)
    SortTyped<float>(outVal, outInd, nullptr, in, axis, descending);
  else if(in->type() == Type::uint32)
    SortTyped<uint32_t>(outVal, outInd, nullptr, in, axis, descending);
  else
    ABORT("Unsupported type {}", in->type());
}

}
}
