#pragma once
#include <Arduino.h>
struct Channel { bool done=true; };
using rmt_channel_handle_t=Channel*;
using rmt_encoder_handle_t=void*;
struct rmt_symbol_word_t { unsigned level0:1; unsigned duration0:15; unsigned level1:1; unsigned duration1:15; };
struct rmt_tx_channel_config_t { gpio_num_t gpio_num;int clk_src;int resolution_hz;int mem_block_symbols;int trans_queue_depth;int intr_priority; };
struct rmt_transmit_config_t { struct { int eot_level; } flags; };
constexpr int RMT_CLK_SRC_DEFAULT=0;
inline Channel channels[6];inline unsigned allocations=0,transmissions=0;
inline const rmt_symbol_word_t* inFlight=nullptr;
inline int rmt_new_tx_channel(const rmt_tx_channel_config_t*,rmt_channel_handle_t* c){*c=&channels[allocations++];return ESP_OK;}
inline int rmt_enable(rmt_channel_handle_t){return ESP_OK;}
inline int rmt_del_channel(rmt_channel_handle_t){return ESP_OK;}
inline int rmt_tx_wait_all_done(rmt_channel_handle_t c,int timeout){return c->done?ESP_OK:ESP_ERR_TIMEOUT;}
inline int rmt_transmit(rmt_channel_handle_t c,rmt_encoder_handle_t,const void* data,size_t,const rmt_transmit_config_t*){c->done=false;inFlight=(const rmt_symbol_word_t*)data;transmissions++;return ESP_OK;}
