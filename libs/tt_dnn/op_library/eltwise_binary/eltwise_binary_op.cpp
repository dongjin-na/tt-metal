#include "tt_dnn/op_library/eltwise_binary/eltwise_binary_op.hpp"
#include "tt_metal/host_api.hpp"
#include "constants.hpp"
#include "tt_dnn/op_library/auto_pad.hpp"

using namespace tt::constants;

namespace eltwise_binary_op_utils {
using namespace tt::tt_metal;

void add_defines(ComputeKernel * eltwise_binary_kernel, BinaryOpType::Enum op_type){
    string op_name, op_code;
    switch (op_type) {
        case BinaryOpType::ADD: op_name = "add_tiles"; op_code = "0"; break;
        case BinaryOpType::SUB: op_name = "sub_tiles"; op_code = "1"; break;
        case BinaryOpType::MUL: op_name = "mul_tiles"; op_code = "2"; break;
        default: TT_ASSERT(false && "Undefined op type");
    }
    eltwise_binary_kernel->add_define("ELTWISE_OP", op_name.c_str());
    eltwise_binary_kernel->add_define("ELTWISE_OP_CODE", op_code.c_str());
}

BinaryOpParallelizationStrategy::Enum get_parallelization_strategy(const Tensor &input_tensor_a, const Tensor &input_tensor_b){
    uint32_t num_tiles = input_tensor_a.volume() / TILE_HW;
    if(num_tiles > 1){
        return BinaryOpParallelizationStrategy::MULTI_CORE;
    }
    else{
        return BinaryOpParallelizationStrategy::SINGLE_CORE;
    }
}

}  // eltwise_binary_op_utils

namespace tt {

namespace tt_metal {


void EltwiseBinary::validate(const std::vector<std::reference_wrapper<const Tensor>> &input_tensors) const {
    const auto& input_tensor_a = input_tensors.at(0).get();
    const auto& input_tensor_b = input_tensors.at(1).get();
    TT_ASSERT(input_tensor_a.shape() == input_tensor_b.shape(), "Input shapes must be the same!");
}

std::vector<Shape> EltwiseBinary::compute_output_shapes(const std::vector<std::reference_wrapper<const Tensor>> &input_tensors) const {
    const auto& input_tensor = input_tensors.at(0).get();
    return {input_tensor.shape()};
}

std::vector<Tensor> EltwiseBinary::create_output_tensors(const std::vector<std::reference_wrapper<const Tensor>> &input_tensors) const {
    return detail::generic_create_output_tensors(*this, input_tensors);
}


Program EltwiseBinary::create_program(const std::vector<std::reference_wrapper<const Tensor>>& input_tensors, std::vector<Tensor> &output_tensors) const {
    const auto& input_tensor_a = input_tensors.at(0).get();
    const auto& input_tensor_b = input_tensors.at(1).get();
    auto& output_tensor = output_tensors.at(0);

    switch (eltwise_binary_op_utils::get_parallelization_strategy(input_tensor_a, input_tensor_b)){
        case BinaryOpParallelizationStrategy::MULTI_CORE:
            return eltwise_binary_multi_core(input_tensor_a, input_tensor_b, output_tensor, this->op_type);
            break;
        case BinaryOpParallelizationStrategy::SINGLE_CORE:
        default:
            return eltwise_binary_single_core(input_tensor_a, input_tensor_b, output_tensor, this->op_type);
    }

}

}  // namespace tt_metal

}  // namespace tt
