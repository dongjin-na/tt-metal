import torch
import torch.nn as nn
import tt_lib
from typing import Optional, Tuple, Union
from loguru import logger

from tests.python_api_testing.models.whisper.whisper_common import (
    torch2tt_tensor,
    tt2torch_tensor,
    linear,
)

from tt_lib.fallback_ops import fallback_ops


class TtWhisperAttention(nn.Module):
    def __init__(
        self,
        config,
        state_dict,
        base_address,
        device,
        embed_dim: int,
        num_heads: int,
        dropout: float = 0.0,
        is_decoder: bool = False,
        bias: bool = True,
    ):
        super().__init__()

        self.device = device
        self.state_dict = state_dict

        self.embed_dim = embed_dim
        self.num_heads = num_heads
        self.dropout = dropout
        self.head_dim = embed_dim // num_heads
        self.config = config
        self.base_address = base_address
        self.scaling = self.head_dim**-0.5
        self.is_decoder = is_decoder
        self.out_mem_config_l1 = tt_lib.tensor.MemoryConfig(True, tt_lib.tensor.BufferType.L1)

        if (self.head_dim * num_heads) != self.embed_dim:
            raise ValueError(
                f"embed_dim must be divisible by num_heads (got `embed_dim`: {self.embed_dim}"
                f" and `num_heads`: {num_heads})."
            )

        self.k_proj_weight = torch2tt_tensor(
            state_dict[f"{base_address}.k_proj.weight"], self.device
        )
        self.k_proj_bias = None
        self.v_proj_weight = torch2tt_tensor(
            state_dict[f"{base_address}.v_proj.weight"], self.device
        )
        self.v_proj_bias = torch2tt_tensor(
            state_dict[f"{base_address}.v_proj.bias"], self.device
        )
        self.q_proj_weight = torch2tt_tensor(
            state_dict[f"{base_address}.q_proj.weight"], self.device
        )
        self.q_proj_bias = torch2tt_tensor(
            state_dict[f"{base_address}.q_proj.bias"], self.device
        )
        self.out_proj_weight = torch2tt_tensor(
            state_dict[f"{base_address}.out_proj.weight"], self.device
        )
        self.out_proj_bias = torch2tt_tensor(
            state_dict[f"{base_address}.out_proj.bias"], self.device
        )

        self.cached_q_proj_shape = None
        self.q_proj_mul_const = None

    # Copied from transformers.models.bart.modeling_bart.BartAttention._shape with BART->whisper
    def _shape(self, tt_tensor: tt_lib.tensor.Tensor, seq_len: int, bsz: int):
        tt_tensor = fallback_ops.reshape(
            tt_tensor, bsz, seq_len, self.num_heads, self.head_dim
        )
        tt_tensor = tt_lib.tensor.transpose_hc(tt_tensor)
        return tt_tensor

    def forward(
        self,
        hidden_states: tt_lib.tensor.Tensor,
        key_value_states: Optional[tt_lib.tensor.Tensor] = None,
        past_key_value: Optional[Tuple[tt_lib.tensor.Tensor]] = None,
        attention_mask: Optional[tt_lib.tensor.Tensor] = None,
        layer_head_mask: Optional[torch.Tensor] = None,
        output_attentions: bool = False,
    ) -> Tuple[
        tt_lib.tensor.Tensor,
        Optional[tt_lib.tensor.Tensor],
        Optional[Tuple[tt_lib.tensor.Tensor]],
    ]:
        # if key_value_states are provided this layer is used as a cross-attention layer for the decoder
        is_cross_attention = key_value_states is not None

        (
            _,
            bsz,
            tgt_len,
            _,
        ) = hidden_states.shape()

        q_proj_output = linear(hidden_states, self.q_proj_weight, self.q_proj_bias)
        q_proj_shape = q_proj_output.shape()

        if q_proj_shape == self.cached_q_proj_shape:
            q_proj_mul_const = self.q_proj_mul_const
        else:
            self.q_proj_mul_const = fallback_ops.full(q_proj_shape, self.scaling)
            self.cached_q_proj_shape = q_proj_shape
            q_proj_mul_const = self.q_proj_mul_const

        query_states = tt_lib.tensor.mul(q_proj_output, q_proj_mul_const, output_mem_config=self.out_mem_config_l1)

        if (
            is_cross_attention
            and past_key_value is not None
            and past_key_value[0].shape[2] == key_value_states.shape()[1]
        ):
            # reuse k,v, cross_attentions
            key_states = past_key_value[0]
            value_states = past_key_value[1]

        elif is_cross_attention:
            # cross_attentions
            key_states = self._shape(
                linear(key_value_states, self.k_proj_weight, self.k_proj_bias), -1, bsz
            )
            value_states = self._shape(
                linear(key_value_states, self.v_proj_weight, self.v_proj_bias), -1, bsz
            )

        elif past_key_value is not None:
            # reuse k, v, self_attention
            key_states = self._shape(
                linear(hidden_states, self.k_proj_weight, self.k_proj_bias), -1, bsz
            )
            value_states = self._shape(
                linear(hidden_states, self.v_proj_weight, self.v_proj_bias), -1, bsz
            )

            key_states = fallback_ops.concat([past_key_value[0], key_states], dim=-2)
            value_states = fallback_ops.concat(
                [past_key_value[1], value_states], dim=-2
            )

        else:
            # self_attention
            key_states = self._shape(
                linear(hidden_states, self.k_proj_weight, self.k_proj_bias), -1, bsz
            )
            value_states = self._shape(
                linear(hidden_states, self.v_proj_weight, self.v_proj_bias), -1, bsz
            )

        if self.is_decoder:
            # if cross_attention save Tuple(torch.Tensor, torch.Tensor) of all cross attention key/value_states.
            # Further calls to cross_attention layer can then reuse all cross-attention
            # key/value_states (first "if" case)
            # if uni-directional self-attention (decoder) save Tuple(torch.Tensor, torch.Tensor) of
            # all previous decoder key/value_states. Further calls to uni-directional self-attention
            # can concat previous decoder key/value_states to current projected key/value_states (third "elif" case)
            # if encoder bi-directional self-attention `past_key_value` is always `None`
            past_key_value = (key_states, value_states)

        proj_shape = [1, bsz * self.num_heads, -1, self.head_dim]
        query_states = self._shape(query_states, tgt_len, bsz)  # 4d

        # Apply reshaping
        query_states = fallback_ops.reshape(query_states, *proj_shape)
        key_states = fallback_ops.reshape(key_states, *proj_shape)
        value_states = fallback_ops.reshape(value_states, *proj_shape)

        key_states_transposed = tt_lib.tensor.transpose(key_states)
        # logger.info(f"key_states_transposed: {key_states_transposed.shape()}")
        src_len = key_states.shape()[-2]
        attn_weights = tt_lib.tensor.bmm(query_states, key_states_transposed)
        # logger.info(f"attn_weights {attn_weights.shape()}")

        if attn_weights.shape() != [1, bsz * self.num_heads, tgt_len, src_len]:
            raise ValueError(
                f"Attention weights should be of size {(1, bsz * self.num_heads, tgt_len, src_len)}, but is"
                f" {attn_weights.shape()}"
            )

        if attention_mask is not None:
            if attention_mask.shape() != [bsz, 1, tgt_len, src_len]:
                raise ValueError(
                    f"Attention mask should be of size {[bsz, 1, tgt_len, src_len]}, but is {attention_mask.shape()}"
                )
            # TTM implementation. Doesn't work for now
            torch_attn_weights = tt2torch_tensor(attn_weights)
            torch_attention_mask = tt2torch_tensor(attention_mask)

            torch_attn_weights = (
                torch_attn_weights.view(bsz, self.num_heads, tgt_len, src_len)
                + torch_attention_mask
            )
            torch_attn_weights = torch_attn_weights.view(
                bsz * self.num_heads, tgt_len, src_len
            )
            attn_weights = torch2tt_tensor(torch_attn_weights, self.device)

        attn_weights = fallback_ops.softmax(attn_weights, dim=-1)

        if layer_head_mask is not None:
            if layer_head_mask.shape() != [1, 1, 1, self.num_heads]:
                raise ValueError(
                    f"Head mask for a single layer should be of size {(self.num_heads,)}, but is"
                    f" {layer_head_mask.shape()}"
                )

            layer_head_mask_reshaped = fallback_ops.reshape(
                layer_head_mask, 1, -1, 1, 1
            )
            attn_weights = fallback_ops.reshape(
                attn_weights, bsz, self.num_heads, tgt_len, src_len
            )
            attn_weights = tt_lib.tensor.bcast(
                attn_weights,
                layer_head_mask_reshaped,
                tt_lib.tensor.BcastOpMath.MUL,
                tt_lib.tensor.BcastOpDim.HW,
                self.out_mem_config_l1
            )
            attn_weights = fallback_ops.reshape(
                attn_weights, 1, bsz * self.num_heads, tgt_len, src_len
            )

        if output_attentions:
            # this operation is a bit awkward, but it's required to
            # make sure that attn_weights keeps its gradient.
            # In order to do so, attn_weights have to be reshaped
            # twice and have to be reused in the following
            # attn_weights_copy = tt2torch_tensor(attn_weights)
            # attn_weights_copy = torch2tt_tensor(attn_weights_copy, self.device)
            attn_weights_reshaped = fallback_ops.reshape(
                attn_weights, bsz, self.num_heads, tgt_len, src_len
            )
            attn_weights = fallback_ops.reshape(
                attn_weights_reshaped, 1, bsz * self.num_heads, tgt_len, src_len
            )

        else:
            attn_weights_reshaped = None

        """
        TODO: Dropout
        """
        # attn_probs = nn.functional.dropout(attn_weights, p=self.dropout, training=self.training)

        attn_probs = attn_weights

        attn_output = tt_lib.tensor.bmm(attn_probs, value_states)
        value_states.deallocate()

        if attn_output.shape() != [1, bsz * self.num_heads, tgt_len, self.head_dim]:
            raise ValueError(
                f"`attn_output` should be of size {(bsz * self.num_heads, tgt_len, self.head_dim)}, but is"
                f" {attn_output.shape()}"
            )
        attn_output = tt_lib.tensor.reshape(
            attn_output, bsz, self.num_heads, tgt_len, self.head_dim
        )
        attn_output = tt_lib.tensor.transpose_hc(attn_output)

        attn_output = fallback_ops.reshape(attn_output, 1, bsz, tgt_len, self.embed_dim)

        attn_output = linear(attn_output, self.out_proj_weight, self.out_proj_bias)

        return attn_output, attn_weights_reshaped, past_key_value
