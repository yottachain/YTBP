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
      
      void map(account_name user, asset  quant, asset gas, string bscaddr);

      void channellog(uint8_t type, asset quant, account_name user);

   private:
        //记录某个账户的存储押金池信息
        struct cbalance {
            uint8_t     type;     //token分类  
            asset       balance;  //token余额
            uint64_t    primary_key()const { return type; }
        };        
        typedef multi_index<N(cbalances), cbalance> cbalances; 

};


