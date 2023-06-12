#pragma once

#include "tensor/tensor.hpp"
#include "tt_dnn/op_library/operation.hpp"

namespace tt {

namespace tt_metal {

// TODO: Accept parallelization

struct Untilize : Operation {

    Untilize() {
    }

    Untilize(const Untilize&) = delete;
    Untilize& operator=(const Untilize&) = delete;
    ~Untilize() {}

    void validate(const std::vector<std::reference_wrapper<const Tensor>> &input_tensors) const override;
    std::vector<tt::tt_metal::Shape> compute_output_shapes(const std::vector<std::reference_wrapper<const Tensor>> &input_tensors) const override;
    std::vector<Tensor> create_output_tensors(const std::vector<std::reference_wrapper<const Tensor>> &input_tensors) const override;
    Program create_program(const std::vector<std::reference_wrapper<const Tensor>>& input_tensors, std::vector<Tensor> &output_tensors) const override;
};

struct UntilizeWithUnpadding : Operation {
    const std::array<uint32_t, 4> output_tensor_start;
    const std::array<uint32_t, 4> output_tensor_end;
    const std::array<uint32_t, 4> output_tensor_shape;

    UntilizeWithUnpadding(const std::array<uint32_t, 4> &output_tensor_start, const std::array<uint32_t, 4> &output_tensor_end)
        : output_tensor_start(output_tensor_start), output_tensor_end(output_tensor_end),
        output_tensor_shape{
        output_tensor_end[0] - output_tensor_start[0] + 1,
        output_tensor_end[1] - output_tensor_start[1] + 1,
        output_tensor_end[2] - output_tensor_start[2] + 1,
        output_tensor_end[3] - output_tensor_start[3] + 1,
        } {
    }

    UntilizeWithUnpadding(const UntilizeWithUnpadding&) = delete;
    UntilizeWithUnpadding& operator=(const UntilizeWithUnpadding&) = delete;
    ~UntilizeWithUnpadding() {}

    void validate(const std::vector<std::reference_wrapper<const Tensor>> &input_tensors) const override;
    std::vector<tt::tt_metal::Shape> compute_output_shapes(const std::vector<std::reference_wrapper<const Tensor>> &input_tensors) const override;
    std::vector<Tensor> create_output_tensors(const std::vector<std::reference_wrapper<const Tensor>> &input_tensors) const override;
    Program create_program(const std::vector<std::reference_wrapper<const Tensor>>& input_tensors, std::vector<Tensor> &output_tensors) const override;
};


Tensor untilize (const Tensor &a);
Tensor untilize_with_unpadding(const Tensor &a, const std::array<uint32_t, 4> &output_tensor_start, const std::array<uint32_t, 4> &output_tensor_end);

}  // namespace tt_metal

}  // namespace tt
