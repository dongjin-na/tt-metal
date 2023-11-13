/*
 * SPDX-FileCopyrightText: © 2023 Tenstorrent Inc.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "tensor/tensor.hpp"
#include "tt_dnn/op_library/run_operation.hpp"

namespace tt {

namespace tt_metal {

// TODO: Accept parallelization
enum class OptimizedConvOpParallelizationStrategy {
    MULTI_CORE = 0, MULTI_CORE_REUSE = 1, MULTI_CORE_REUSE_MCAST = 2, SINGLE_CORE = 3
};

struct OptimizedConvParallelizationConfig {
    CoreCoord grid_size; // (x,y)
    uint32_t per_core_out_matrix_height_ntiles;
    uint32_t per_core_weight_matrix_width_ntiles;
    // std::size_t in0_block_w;
    // std::size_t out_subblock_h;
    // std::size_t out_subblock_w;
    // std::size_t per_core_M;
    // std::size_t per_core_N;

    static constexpr auto attribute_names =
        std::make_tuple("grid_size", "per_core_out_matrix_height_ntiles", "per_core_weight_matrix_width_ntiles");
    const auto attribute_values() const {
        return std::make_tuple(
            std::cref(this->grid_size),
            std::cref(this->per_core_out_matrix_height_ntiles),
            std::cref(this->per_core_weight_matrix_width_ntiles));
    }
};

struct OptimizedConvBlockConfig {
    uint32_t act_block_h_ntiles;
    uint32_t act_block_w_ntiles;
    uint32_t act_c_num_blocks;
    uint32_t weight_block_w_ntiles;
    uint32_t out_block_h_ntiles;
    uint32_t out_subblock_h_ntiles;
    uint32_t out_subblock_w_ntiles;

    static constexpr auto attribute_names = std::make_tuple(
        "act_block_h_ntiles",
        "act_block_w_ntiles",
        "act_c_num_blocks",
        "weight_block_w_ntiles",
        "out_block_h_ntiles",
        "out_subblock_h_ntiles",
        "out_subblock_w_ntiles");
    const auto attribute_values() const {
        return std::make_tuple(
            std::cref(this->act_block_h_ntiles),
            std::cref(this->act_block_w_ntiles),
            std::cref(this->act_c_num_blocks),
            std::cref(this->weight_block_w_ntiles),
            std::cref(this->out_block_h_ntiles),
            std::cref(this->out_subblock_h_ntiles),
            std::cref(this->out_subblock_w_ntiles));
    }
};

struct OptimizedConv {
    OptimizedConvParallelizationConfig parallelization_config;
    OptimizedConvBlockConfig block_config;

    const std::vector<int> conv_params;
    const uint32_t output_channels;
    bool untilize_out, has_bias, fuse_relu;
    MathFidelity math_fidelity;
    uint32_t extra_padding_for_32B_alignment;
    const MemoryConfig output_mem_config;
    const DataType output_dtype;
    Shape input_tensor_shape; // For sharded input, input tensor shape is nonsense
    OptimizedConv(const std::vector<int>&c_params,
        uint32_t output_channels, bool untile_out,
        bool has_bias, bool fuse_relu,
        MathFidelity mfidelity, const OptimizedConvParallelizationConfig& p_config,
        const OptimizedConvBlockConfig& b_config,
        uint32_t e_padding_for_32B_alignment,
        MemoryConfig output_mem_config, DataType output_dtype, Shape input_tensor_shape) :
            output_channels(output_channels),
            conv_params(c_params),
            untilize_out(untile_out),
            has_bias(has_bias),
            fuse_relu(fuse_relu),
            math_fidelity(mfidelity),
            parallelization_config(p_config),
            block_config(b_config),
            extra_padding_for_32B_alignment(e_padding_for_32B_alignment),
            output_mem_config(output_mem_config), output_dtype(output_dtype), input_tensor_shape(input_tensor_shape) {}

    void validate(const std::vector<Tensor>& input_tensors, const std::vector<std::optional<const Tensor>>& optional_input_tensors) const;
    std::vector<Shape> compute_output_shapes(const std::vector<Tensor>& input_tensors) const;
    std::vector<Tensor> create_output_tensors(const std::vector<Tensor>& input_tensors) const;
    operation::ProgramWithCallbacks create_program(const std::vector<Tensor>& input_tensors, const std::vector<std::optional<const Tensor>>& optional_input_tensors, std::vector<Tensor> &output_tensors) const;

    static constexpr auto attribute_names = std::make_tuple(
        "conv_params",
        "output_channels",
        "untilize_out",
        "has_bias",
        "fuse_relu",
        "math_fidelity",
        "extra_padding_for_32B_alignment",
        "output_mem_config",
        "output_dtype");
    const auto attribute_values() const {
        return std::make_tuple(
            std::cref(this->conv_params),
            std::cref(this->output_channels),
            std::cref(this->untilize_out),
            std::cref(this->has_bias),
            std::cref(this->fuse_relu),
            std::cref(this->math_fidelity),
            std::cref(this->extra_padding_for_32B_alignment),
            std::cref(this->output_mem_config),
            std::cref(this->output_dtype));
    }
};

Tensor optimized_conv(const Tensor& a, const Tensor &b, std::optional<const Tensor> bias,
    const vector<int> conv_params, uint32_t output_channels,
    bool untilize_out, bool has_bias, bool fuse_relu, MathFidelity math_fidelity,
    const OptimizedConvParallelizationConfig& parallelization_config,
    const OptimizedConvBlockConfig& block_config, uint32_t extra_padding_for_32B_alignment=0,
    std::optional<MemoryConfig> output_mem_config = std::nullopt,
    std::optional<DataType> output_dtype=std::nullopt,
    std::optional<std::array<std::uint32_t, 4>> input_tensor_shape = std::nullopt
);

}  // namespace tt_metal

}  // namespace tt
