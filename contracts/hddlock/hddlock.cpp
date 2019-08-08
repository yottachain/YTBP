#include "hddlock.hpp"
#include <eosiolib/action.hpp>
#include <eosiolib/chain.h>
#include <eosiolib/symbol.hpp>
#include <eosiolib/eosio.hpp>
#include <eosiolib/print.hpp>
#include <eosiolib/serialize.hpp>
#include <eosiolib/multi_index.hpp>
//#include <eosio.token/eosio.token.hpp>
//#include <eosio.system/eosio.system.hpp>

using namespace eosio;

void hddlock::init() {

}

void hddlock::addrule(uint64_t id, std::vector<uint64_t>& times, std::vector<uint8_t>& percentage, std::string& desc) {

    eosio_assert(times.size() >= 2, "invalidate size of times array");
    eosio_assert(times.size() == percentage.size(), "times and percentage in different size.");

}


EOSIO_ABI( hddlock, (init)(addrule))
