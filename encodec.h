#pragma once

#include <cmath>
#include <fstream>
#include <iostream>
#include <map>
#include <thread>
#include <string> 
#include <vector>

#include "ggml.h"

#define ENCODEC_FILE_MAGIC   'ggml'
#define ENCODEC_FILE_VERSION 1

static const size_t MB = 1024*1024;

struct encodec_hparams {
    int32_t in_channels          = 1;
    int32_t hidden_dim           = 128;
    int32_t n_filters            = 32;
    int32_t ratios[4]            = {8, 5, 4, 2};
    int32_t kernel_size          = 7;
    int32_t residual_kernel_size = 3;
    int32_t compress             = 2;
    int32_t n_lstm_layers        = 2;
    int32_t stride               = 1;

    // number of codebooks is determined by the bandwidth selected.
    // Supported bandwidths are 1.5kbps (n_q = 2), 3 kbps (n_q = 4), 6 kbps (n_q = 8) and 12 kbps (n_q =16) and 24kbps (n_q=32).
    int32_t n_q                  = 32;
    int32_t n_bins               = 1024;
    int32_t sr                   = 24000;
};

// res + downsample block at some ratio
struct encodec_encoder_block {
    // conv1
    struct ggml_tensor * conv_1_w;
    struct ggml_tensor * conv_1_b;

    // conv2
    struct ggml_tensor * conv_2_w;
    struct ggml_tensor * conv_2_b;

    // shortcut
    struct ggml_tensor * conv_sc_w;
    struct ggml_tensor * conv_sc_b;

    // downsampling layers
    struct ggml_tensor * ds_conv_w;
    struct ggml_tensor * ds_conv_b;
};

struct encodec_lstm {
    struct ggml_tensor * l0_ih_w;
    struct ggml_tensor * l0_hh_w;

    struct ggml_tensor * l0_ih_b;
    struct ggml_tensor * l0_hh_b;

    struct ggml_tensor * l1_ih_w;
    struct ggml_tensor * l1_hh_w;

    struct ggml_tensor * l1_ih_b;
    struct ggml_tensor * l1_hh_b;
};

struct encodec_encoder {
    struct ggml_tensor * init_conv_w;
    struct ggml_tensor * init_conv_b;

    encodec_lstm lstm;

    struct ggml_tensor * final_conv_w;
    struct ggml_tensor * final_conv_b;

    std::vector<encodec_encoder_block> blocks;
};

struct encodec_quant_block {
    struct ggml_tensor * embed;
};

struct encodec_quantizer {
    std::vector<encodec_quant_block> blocks;
};

struct encodec_decoder_block {
    //upsampling layers
    struct ggml_tensor * us_conv_w;
    struct ggml_tensor * us_conv_b;

    // conv1
    struct ggml_tensor * conv_1_w;
    struct ggml_tensor * conv_1_b;

    // conv2
    struct ggml_tensor * conv_2_w;
    struct ggml_tensor * conv_2_b;

    // shortcut
    struct ggml_tensor * conv_sc_w;
    struct ggml_tensor * conv_sc_b;
};

struct encodec_decoder {
    struct ggml_tensor * init_conv_w;
    struct ggml_tensor * init_conv_b;

    encodec_lstm lstm;

    struct ggml_tensor * final_conv_w;
    struct ggml_tensor * final_conv_b;

    std::vector<encodec_decoder_block> blocks;
};

struct encodec_model {
    encodec_hparams hparams;

    encodec_encoder   encoder;
    encodec_quantizer quantizer;
    encodec_decoder   decoder;

    // context
    struct ggml_context * ctx;
    int n_loaded;

    std::map<std::string, struct ggml_tensor *> tensors;
};

struct encodec_context {
    std::unique_ptr<encodec_model> model;

    struct ggml_context * ctx_audio;
    struct ggml_tensor  * reconstructed_audio;

    // buffer for `ggml_graph_plan.work_data`
    std::vector<uint8_t> work_buffer;

    // buffers to evaluate the model
    std::vector<uint8_t> buf_alloc;
    std::vector<uint8_t> buf_compute;

    struct ggml_allocr * allocr = {};

    // statistics
    int64_t t_load_us    = 0;
    int64_t t_compute_ms = 0;
};

std::shared_ptr<encodec_context> encodec_load_model(const std::string & model_path);

bool encodec_reconstruct_audio(
                   encodec_context & ectx,
                std::vector<float> & raw_audio,
                               int   n_threads);

void encodec_free(encodec_context & ectx);