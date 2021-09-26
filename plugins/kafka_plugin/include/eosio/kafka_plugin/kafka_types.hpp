#pragma once

#include <eosio/chain/block_header_state.hpp>

namespace eosio {
    using std::string;
    using eosio::chain::block_header_state;
    using chain::signed_block_header;
    using chain::block_id_type;

    struct kafka_block_accept_st {
        uint32_t current_block_num;
        block_id_type current_block_id;
        signed_block_header current_block_header;
        uint32_t irreversible_block_num;
        block_id_type irreversible_block_id;
    };
}

FC_REFLECT(eosio::kafka_block_accept_st, (current_block_num)(current_block_id)(current_block_header)(irreversible_block_num)(irreversible_block_id))

