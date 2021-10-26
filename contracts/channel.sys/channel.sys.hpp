/**
 *  @file
 *  @copyright defined in eos/LICENSE
 */
#pragma once

#include <eosiolib/asset.hpp>
#include <eosiolib/eosio.hpp>

#include <string>

using std::string;
using eosio::name;
using eosio::asset;
using eosio::multi_index;
using eosio::indexed_by;
using eosio::const_mem_fun;


class mchannel : public eosio::contract {
   public:
      mchannel( account_name self ):contract(self){}

      void transfercore( account_name from,
                     account_name to,
                     asset        quantity,
                     string       memo );      


   private:
};


