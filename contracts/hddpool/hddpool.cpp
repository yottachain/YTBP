#include "hddpool.hpp"
#include <eosiolib/action.hpp>
#include <eosiolib/chain.h>
#include <eosiolib/symbol.hpp>
#include <eosiolib/eosio.hpp>
#include <eosiolib/print.hpp>
#include <eosiolib/serialize.hpp>
#include <eosiolib/multi_index.hpp>
#include <eosiolib/transaction.hpp>
#include <eosio.token/eosio.token.hpp>
#include <eosio.system/eosio.system.hpp>
#include <hdddeposit/hdddeposit.hpp>


#include <cmath>
#include <string>
#include <type_traits>
#include <optional>

using namespace eosio;

const uint64_t hours_in_one_day = 24;
const uint64_t minutes_in_one_day = hours_in_one_day * 60;
const uint64_t milliseconds_in_one_day = minutes_in_one_day * 60 * 1000;
const uint64_t milliseconds_in_one_year = milliseconds_in_one_day * 365;

const uint64_t fee_cycle = milliseconds_in_one_day; //计费周期毫秒为单位)

const uint64_t microseconds_in_one_day = minutes_in_one_day * 60 * 1000000;
const uint64_t hddm_latest_stop_time = 1640793600000000ll; //hddm最晚停止时间(2021-12-30 00:00:00)
const uint32_t blocks_per_day        = 2 * 24 * 3600;

//const uint32_t data_slice_size = 16 * 1024; // among 4k-32k,set it as 16k

//以下空间量按照16k一个分片大小为单位
const uint64_t one_gb = 64 * 1024; //1GB
const uint64_t max_user_space = one_gb * 1024 * uint64_t(1024 * 500);      //500P 最大用户存储空间量
const uint64_t max_profit_space = one_gb * 1024 * uint64_t(1024 * 500);    //500P 收益账号最大的生产空间上限
const uint64_t max_pool_space = one_gb * 1024 * uint64_t(1024 * 500);      //500P 矿池配额最大上限
const uint64_t max_miner_space = one_gb * uint64_t(1024 * 100);            //100T 单个矿机最大的物理空间
const uint64_t min_miner_space = 100 * one_gb;                             //100G 单个矿机最小的物理空间    
const int64_t  max_buy_sell_hdd_amount = 2* 1024 * 1024 * 100000000ll;     //2P  单次买卖最大的HDD数量   
const int64_t  min_buy_hdd_amount = 2 * 100000000ll;                       //2   单次购买的最小的HDD数量  

const double  profit_percent = 0.9;  //存储收益划归矿工的部分,剩余部分画给生态节点奖励池

const uint32_t defaul_miner_level = 66438; //矿机默认评级

static constexpr eosio::name active_permission{N(active)};
static constexpr eosio::name token_account{N(eosio.token)};
static constexpr eosio::name hdd_exchg_acc{N(hddpoolexchg)};
static constexpr eosio::name hdd_deposit{N(hdddeposit12)};
static constexpr eosio::name hdd_subsidy_acc{N(hddsubsidyeu)}; //补贴账户
static constexpr eosio::name ecologyfound_acc{N(ecologyfound)}; //生态节点奖励池




static constexpr int64_t max_hdd_amount    = (1LL << 62) - 1;
bool is_hdd_amount_within_range(int64_t amount) { return -max_hdd_amount <= amount && amount <= max_hdd_amount; }

hddpool::hddpool(account_name s)
    : contract(s),
      _globalu(_self, _self),
      _globalm(_self, _self)
{

   if (_globalu.exists())
      _gstateu = _globalu.get();
   else
      _gstateu = hdd_global_state2{};

   if (_globalm.exists())
      _gstatem = _globalm.get();
   else
      _gstatem = hdd_global_state3{};
}

hddpool::~hddpool()
{
   _globalu.set(_gstateu, _self);
   _globalm.set(_gstatem, _self);
}

void hddpool::new_user_hdd(userhdd_index& userhdd, name user, int64_t balance, account_name payer)
{
      userhdd.emplace(payer, [&](auto &row) {
         row.account_name = user;
         row.hdd_storehhdd = balance;
         row.hdd_per_cycle_fee = 0;
         row.hdd_space_store = 0;
         row.last_hddstore_time = current_time();
         row.hdd_minerhdd = 0;
         row.hdd_per_cycle_profit = 0;
         row.hdd_space_profit = 0;
         row.last_hddprofit_time = current_time();
         _gstateu.hdd_total_user += 1;
      });

}

void hddpool::calcprofit(name user)
{
   require_auth( user );
   
   if(user.value == ecologyfound_acc.value) 
      return;

   userhdd_index _userhdd(_self, user.value);
   auto it = _userhdd.find(user.value);
   eosio_assert(it != _userhdd.end(), "user is not a storage provider");

   int64_t eco_inc_balance = 0;
   _userhdd.modify(it, _self, [&](auto &row) {
      uint64_t tmp_t = current_time();
      int64_t tmp_last_balance = it->hdd_minerhdd;
      uint64_t deadline_time = 0;
      int64_t new_balance = calculate_profit_balance(tmp_last_balance, it->hdd_per_cycle_profit, it->last_hddprofit_time, tmp_t, deadline_time);
      eosio_assert(is_hdd_amount_within_range(new_balance), "magnitude of user hdd_minerhdd must be less than 2^62");      
      row.hdd_minerhdd = (int64_t)(profit_percent * (new_balance - tmp_last_balance))  +  tmp_last_balance;
      eco_inc_balance = new_balance - row.hdd_minerhdd;
      row.last_hddprofit_time = tmp_t;
      //print("{\"balance\":", it->hdd_minerhdd, "}");
      //print(new_balance, " -- ", row.hdd_minerhdd, " -- ", eco_inc_balance, "--", (int64_t)(profit_percent * (new_balance - tmp_last_balance)), "--" , tmp_last_balance);

   });

   if(eco_inc_balance > 0 ) {
      userhdd_index _userhdd_eco(_self, ecologyfound_acc.value);
      auto it_eco = _userhdd_eco.find(ecologyfound_acc.value);
      eosio_assert(it_eco != _userhdd_eco.end(), "ecologyfound account is not open");
      _userhdd_eco.modify(it_eco, _self, [&](auto &row) {
         int64_t tmp_last_balance = it_eco->hdd_minerhdd;
         row.hdd_minerhdd = tmp_last_balance + eco_inc_balance;
         eosio_assert(is_hdd_amount_within_range(row.hdd_minerhdd), "magnitude of user hdd_minerhdd must be less than 2^62");      
      });
   }
}

void hddpool::chg_owner_space(userhdd_index& userhdd, name minerowner, uint64_t space_delta, bool is_increase, bool is_calc, uint64_t ct, uint64_t deadline_time)
{
   if(minerowner.value == ecologyfound_acc.value) 
      return;

   auto userhdd_itr = userhdd.find(minerowner.value);
   eosio_assert(userhdd_itr != userhdd.end(), "no owner exists in userhdd table");
   int64_t eco_inc_balance = 0;
   userhdd.modify(userhdd_itr, _self, [&](auto &row) {
      if(is_calc) 
      {
         int64_t tmp_last_balance = userhdd_itr->hdd_minerhdd;
         uint64_t dt = deadline_time;
         int64_t new_balance = calculate_profit_balance(tmp_last_balance, userhdd_itr->hdd_per_cycle_profit, userhdd_itr->last_hddprofit_time, ct, dt);
         eosio_assert(is_hdd_amount_within_range(new_balance), "magnitude of user hdd_minerhdd must be less than 2^62");      
         row.hdd_minerhdd = (int64_t)(profit_percent * (new_balance - tmp_last_balance))  +  tmp_last_balance;
         eco_inc_balance = new_balance - row.hdd_minerhdd;
         row.last_hddprofit_time = ct;
         //print(new_balance, " -- ", row.hdd_minerhdd, " -- ", (int64_t)(profit_percent * (new_balance - tmp_last_balance)), "--", new_balance - row.hdd_minerhdd);

      }
      uint64_t newspace = 0;
      if(is_increase) {
          newspace = row.hdd_space_profit + space_delta;
      }
      else {
         if(row.hdd_space_profit > space_delta) {
            newspace = row.hdd_space_profit - space_delta;
         }
      }
      eosio_assert(newspace <= max_profit_space, "execeed max profie space");
      row.hdd_space_profit = newspace;
      row.hdd_per_cycle_profit = (int64_t)(((double)newspace / (double)one_gb) * ((double)fee_cycle / (double)milliseconds_in_one_year) * 100000000);
   });

   if(eco_inc_balance > 0 ) {
      userhdd_index _userhdd_eco(_self, ecologyfound_acc.value);
      auto it_eco = _userhdd_eco.find(ecologyfound_acc.value);
      eosio_assert(it_eco != _userhdd_eco.end(), "ecologyfound account is not open");
      _userhdd_eco.modify(it_eco, _self, [&](auto &row) {
         int64_t tmp_last_balance = it_eco->hdd_minerhdd;
         row.hdd_minerhdd = tmp_last_balance + eco_inc_balance;
         eosio_assert(is_hdd_amount_within_range(row.hdd_minerhdd), "magnitude of user hdd_minerhdd must be less than 2^62");      
      });
   }


}


void hddpool::getbalance(name user, uint8_t acc_type, name caller)
{
   eosio_assert(is_account(user), "user not a account.");

   //account_name payer = _self;

   if(acc_type == 1) {
      require_auth( user );
      //payer = user.value;
   }
   else if(acc_type == 2) {
      eosio_assert(is_account(caller), "caller not a account.");
      check_bp_account(caller.value, user.value, true);
   } else {
      require_auth( _self );
   }

   userhdd_index _userhdd(_self, user.value);
   auto it = _userhdd.find(user.value);
   if (it == _userhdd.end())
   {
      //new_user_hdd(_userhdd, user, 0, payer);
      print("{\"balance\":", 0, "}");
   }
   else
   {
      _userhdd.modify(it, _self, [&](auto &row) {
         uint64_t tmp_t = current_time();
         int64_t tmp_last_balance = it->hdd_storehhdd;
         int64_t new_balance = calculate_fee_balance(tmp_last_balance, it->hdd_per_cycle_fee, it->last_hddstore_time, tmp_t);
         row.hdd_storehhdd = new_balance;
         eosio_assert(is_hdd_amount_within_range(row.hdd_storehhdd), "magnitude of user hdd_storehhdd must be less than 2^62");      
         row.last_hddstore_time = tmp_t;
         
         print("{\"balance\":", it->hdd_storehhdd, "}");
      });
   }
}

int64_t hddpool::calculate_fee_balance(int64_t oldbalance, int64_t hdd_per_cycle_fee, uint64_t last_hdd_time, uint64_t current_time)
{
   if(current_time <= last_hdd_time)
      return oldbalance;

   uint64_t slot_t = (current_time - last_hdd_time) / 1000ll; //convert to milliseconds
   int64_t new_balance = 0;

   double tick = (double)((double)slot_t / fee_cycle);
   new_balance = oldbalance;
   int64_t delta = (int64_t)(tick * (0 - hdd_per_cycle_fee));
   new_balance += delta;

   return new_balance;
}

int64_t hddpool::calculate_profit_balance(int64_t oldbalance, int64_t hdd_per_cycle_profit, uint64_t last_hdd_time, uint64_t current_time, uint64_t &deadline_time)
{

   if(deadline_time ==0)
      deadline_time = get_newmodel_start_time();

   if(current_time > deadline_time)
      current_time = deadline_time;

   if(current_time <= last_hdd_time)
      return oldbalance;
   
   uint64_t slot_t = (current_time - last_hdd_time) / 1000ll; //convert to milliseconds
   int64_t new_balance = 0;

   double tick = (double)((double)slot_t / fee_cycle);
   new_balance = oldbalance;
   int64_t delta = (int64_t)(tick * hdd_per_cycle_profit);
   new_balance += delta;

   return new_balance;
}


void hddpool::buyhdd(name from, name receiver, int64_t amount, std::string memo)
{
   require_auth(from);

   eosio_assert(is_account(from), "user not a account");
   eosio_assert(is_account(receiver), "receiver not a account");
   eosio_assert(is_account(hdd_exchg_acc), "to not a account");
   eosio_assert(amount >= min_buy_hdd_amount, "amount too low");
   eosio_assert(amount <= max_buy_sell_hdd_amount, "exceed single purchase volume");
   eosio_assert(is_hdd_amount_within_range(amount), "magnitude of amount must be less than 2^62");

   gparams_singleton _gparams(_self, _self);
   hdd_global_param  _gparmas_state;
   if (_gparams.exists()) {
      _gparmas_state = _gparams.get();
   } else {
      _gparmas_state = hdd_global_param{};
   }
   eosio_assert(_gparmas_state.hdd_counter >= amount, "hdd_counter overdrawn");
   _gparmas_state.hdd_counter -= amount;
   _gparams.set(_gparmas_state,_self);

   int64_t delta = 0;
   if(amount%10000 > 0) {
      delta = 1;
   }
   int64_t _yta_amount =(int64_t)( ((double)amount/10000) * ((double)_gparmas_state.hdd_price/(double)_gparmas_state.yta_price) );
   _yta_amount += delta;

   asset quant{_yta_amount, CORE_SYMBOL};
   action(
       permission_level{from, active_permission},
       token_account, N(transfer),
       std::make_tuple(from, hdd_exchg_acc, quant, memo))
       .send();

   userhdd_index _userhdd(_self, receiver.value);
   auto it = _userhdd.find(receiver.value);
   account_name payer = from;
   if (it == _userhdd.end())
   {
      new_user_hdd(_userhdd, receiver, amount, payer);
   }
   else
   {
      _userhdd.modify(it, _self, [&](auto &row) {
         row.hdd_storehhdd += amount;
         eosio_assert(is_hdd_amount_within_range(row.hdd_storehhdd), "magnitude of user hdd_storehhdd must be less than 2^62");      
      });
   }

}

void hddpool::transhdds(name from, name to, int64_t amount, std::string memo)
{
   require_auth(from);

   eosio_assert(is_account(from), "user not a account");
   eosio_assert(is_account(to), "to not a account");
   eosio_assert(amount > 0, "cannot transfer negative hdd amount");
   eosio_assert(is_hdd_amount_within_range(amount), "magnitude of amount must be less than 2^62");
   eosio_assert( memo.size() <= 256, "memo has more than 256 bytes" );

   require_recipient( from );
   require_recipient( to );


   userhdd_index _userhddfrom(_self, from.value);
   auto itfrom = _userhddfrom.find(from.value);
   eosio_assert(itfrom != _userhddfrom.end(), "from not exists in userhdd table");
   eosio_assert(itfrom->hdd_storehhdd >= amount, "hdd overdrawn.");
   _userhddfrom.modify(itfrom, _self, [&](auto &row) {
      row.hdd_storehhdd -= amount;
      eosio_assert(is_hdd_amount_within_range(row.hdd_storehhdd), "magnitude of user hdd_storehhdd must be less than 2^62");      
   });

   userhdd_index _userhddto(_self, to.value);
   auto itto = _userhddto.find(to.value);
   account_name payer = from;
   if (itto == _userhddto.end())
   {
      new_user_hdd(_userhddto, to, amount, payer);
   }
   else
   {
      _userhddto.modify(itto, _self, [&](auto &row) {
         row.hdd_storehhdd += amount;
         eosio_assert(is_hdd_amount_within_range(row.hdd_storehhdd), "magnitude of user hdd_storehhdd must be less than 2^62");      
      });
   }

}

void hddpool::sellhdd(name user, int64_t amount, std::string memo)
{
   require_auth(user);

   eosio_assert( amount > 0, "cannot sell negative hdd amount" );
   eosio_assert(amount <= max_buy_sell_hdd_amount, "exceed single sale volume");
   eosio_assert(is_hdd_amount_within_range(amount), "magnitude of user hdd amount must be less than 2^62");      


   userhdd_index _userhdd(_self, user.value);
   auto it = _userhdd.find(user.value);
   eosio_assert(it != _userhdd.end(), "user not exists in userhdd table.");
   eosio_assert(it->hdd_minerhdd >= amount, "hdd overdrawn.");

   _userhdd.modify(it, _self, [&](auto &row) {
      row.hdd_minerhdd -= amount;
      eosio_assert(is_hdd_amount_within_range(row.hdd_minerhdd), "magnitude of user hdd_minerhdd must be less than 2^62");      
   });

   gparams_singleton _gparams(_self, _self);
   hdd_global_param  _gparmas_state;
   if (_gparams.exists()) {
      _gparmas_state = _gparams.get();
   } else {
      _gparmas_state = hdd_global_param{};
   }

   int64_t _yta_amount =(int64_t)( ( (double)amount/10000) * ((double)_gparmas_state.hdd_price/(double)_gparmas_state.yta_price) * ((double)_gparmas_state.dup_remove_ratio/10000) * ((double)_gparmas_state.dup_remove_dist_ratio/10000) );

   asset quant{_yta_amount, CORE_SYMBOL};
   action(
       permission_level{hdd_exchg_acc, active_permission},
       token_account, N(transfer),
       std::make_tuple(hdd_exchg_acc, user, quant, memo))
       .send();
   
   if(user.value == ecologyfound_acc.value) 
      return;
   
   uint64_t curtime = current_time()/1000000ll; //seconds
   if(curtime>1605243600) //2020-11-13 13:00:00
      return;
   
   if(_gparmas_state.dup_remove_ratio < 40000) {
      int64_t _yta_amount2 =(int64_t)( ( (double)amount/10000) * ((double)_gparmas_state.hdd_price/(double)_gparmas_state.yta_price) * ((double)(40000-_gparmas_state.dup_remove_ratio)/10000) * ((double)_gparmas_state.dup_remove_dist_ratio/10000) );
      asset quant2{_yta_amount2, CORE_SYMBOL};
      action(
         permission_level{hdd_subsidy_acc, active_permission},
         token_account, N(transfer),
         std::make_tuple(hdd_subsidy_acc, user, quant2, std::string("hdd subsidy")))
         .send();
   }

}


void hddpool::sethfee(name user, int64_t fee, name caller)
{
   eosio_assert(is_account(user), "user invalidate");
   eosio_assert(is_account(caller), "caller not an account.");
   eosio_assert( fee >= 0, "must use positive fee value" );


   userhdd_index _userhdd(_self, user.value);
   auto it = _userhdd.find(user.value);
   eosio_assert(it != _userhdd.end(), "user not exists in userhdd table");
   eosio_assert(fee != it->hdd_per_cycle_fee, " the fee is the same \n");

   check_bp_account(caller.value, user.value, true);

   eosio_assert(is_hdd_amount_within_range(fee), "magnitude of fee must be less than 2^62");      


   //每周期费用 <= （占用存储空间/1GB）*（记账周期/ 1年）
   //bool istrue = fee <= (int64_t)(((double)it->hdd_space_store/(double)one_gb) * ((double)fee_cycle/(double)seconds_in_one_year) * 100000000);
   //eosio_assert(istrue , "the fee verification is not right \n");
   _userhdd.modify(it, _self, [&](auto &row) {
      //设置每周期费用之前可能需要将以前的余额做一次计算，然后更改last_hdd_time
      uint64_t tmp_t = current_time();
      int64_t tmp_last_balance = it->hdd_storehhdd;
      int64_t new_balance = calculate_fee_balance(tmp_last_balance, it->hdd_per_cycle_fee, it->last_hddstore_time, tmp_t);
      row.hdd_storehhdd = new_balance;
      eosio_assert(is_hdd_amount_within_range(row.hdd_storehhdd), "magnitude of user hdd_storehhdd must be less than 2^62");            
      row.last_hddstore_time = tmp_t;
      row.hdd_per_cycle_fee = fee;
   });

}

void hddpool::subbalance(name user, int64_t balance, uint8_t acc_type, name caller)
{
   eosio_assert(is_account(user), "user invalidate");

   if(acc_type == 2) {
      eosio_assert(is_account(caller), "caller not a account.");
      check_bp_account(caller.value, user.value, false);

   } else {
      require_auth( _self );
   }

   eosio_assert(is_hdd_amount_within_range(balance), "magnitude of hddbalance must be less than 2^62");  
   eosio_assert( balance >= 0, "must use positive balance value" );


   userhdd_index _userhdd(_self, user.value);
   auto it = _userhdd.find(user.value);
   eosio_assert(it != _userhdd.end(), "user not exists in userhdd table");

   check_bp_account(caller.value, user.value, true);

   _userhdd.modify(it, _self, [&](auto &row) {
      row.hdd_storehhdd -= balance;
      eosio_assert(is_hdd_amount_within_range(row.hdd_storehhdd), "magnitude of user hddbalance must be less than 2^62");
   });   

}

void hddpool::addhspace(name user, uint64_t space, name caller)
{
   eosio_assert(is_account(user), "user invalidate");
   eosio_assert(is_account(caller), "caller not an account.");

   userhdd_index _userhdd(_self, user.value);
   auto it = _userhdd.find(user.value);
   eosio_assert(it != _userhdd.end(), "user not exists in userhdd table");

   check_bp_account(caller.value, user.value, true);

   _userhdd.modify(it, _self, [&](auto &row) {
      row.hdd_space_store += space;
      eosio_assert(row.hdd_space_store <= max_user_space , "overflow max_userspace");
   });

}

void hddpool::subhspace(name user, uint64_t space, name caller)
{
   eosio_assert(is_account(user), "user invalidate");
   eosio_assert(is_account(caller), "caller not an account.");

   userhdd_index _userhdd(_self, user.value);
   auto it = _userhdd.find(user.value);
   eosio_assert(it != _userhdd.end(), "user not exists in userhdd table");

   check_bp_account(caller.value, user.value, true);

   _userhdd.modify(it, _self, [&](auto &row) {
      //eosio_assert(row.hdd_space >= space , "overdraw user hdd_space");
      if(row.hdd_space_store >= space)
         row.hdd_space_store -= space;
      else 
         row.hdd_space_store = 0;
   });

}

void hddpool::fixownspace(name owner, uint64_t space)
{
   eosio_assert(false, "not support now!");
   require_auth( N(hddpooladmin) );

   userhdd_index userhdd(_self, owner.value);
   auto userhdd_itr = userhdd.find(owner.value);
   eosio_assert(userhdd_itr != userhdd.end(), "no owner exists in userhdd table");

   userhdd.modify(userhdd_itr, _self, [&](auto &row) {
      eosio_assert(space <= max_profit_space, "execeed max profie space");
      row.hdd_space_profit = space;
      if(space > 0)
         row.hdd_per_cycle_profit = (int64_t)(((double)space / (double)one_gb) * ((double)fee_cycle / (double)milliseconds_in_one_year) * 100000000);
      else
         row.hdd_per_cycle_profit = 0;
   });
}

void hddpool::addmprofit(name owner, uint64_t minerid, uint64_t space, name caller)
{
   eosio_assert(is_account(owner), "owner invalidate");
   eosio_assert(is_account(caller), "caller not an account.");
   check_bp_account(caller.value, minerid, true);

   eosio_assert((space & 65535) == 0, "invalid space");

   //eosio_assert(owner.value != N(hddpool12345), "owner can not addmprofit now.");  

   maccount_index _maccount(_self, owner.value);
   auto it = _maccount.find(minerid);
   eosio_assert(it != _maccount.end(), "minerid not register");
   if(it->space > 0)
      eosio_assert(it->hdd_per_cycle_profit > 0, "miner is deactive");  

   //check space left -- (is it enough)  -- start ----------
   minerinfo_table _minerinfo(_self, _self);
   auto itminerinfo = _minerinfo.find(minerid);   
   eosio_assert(itminerinfo != _minerinfo.end(), "minerid not exist in minerinfo");

   eosio_assert(itminerinfo->space_left >= space, "exceed max space");
   eosio_assert(itminerinfo->owner == owner, "invalid owner");

   _minerinfo.modify(itminerinfo, _self, [&](auto &row) {
      row.space_left -= space;
   });         
   //check space left -- (is it enough)  -- end   ----------

   uint64_t tmp_t = current_time();
   uint64_t deadline_time = 0;
   _maccount.modify(it, _self, [&](auto &row) {
      int64_t tmp_last_balance = it->hdd_balance;
      int64_t new_balance = calculate_profit_balance(tmp_last_balance, it->hdd_per_cycle_profit, it->last_hdd_time, tmp_t, deadline_time);
      eosio_assert(is_hdd_amount_within_range(new_balance), "magnitude of miner hddbalance must be less than 2^62");
      row.hdd_balance = (int64_t)(profit_percent * (new_balance - tmp_last_balance))  +  tmp_last_balance;
      //print(new_balance, " -- ", row.hdd_balance, " -- ", (int64_t)(profit_percent * (new_balance - tmp_last_balance)), "--", new_balance - row.hdd_balance);
      row.last_hdd_time = tmp_t;
      uint64_t newspace = row.space + space;
      row.space = newspace;
      //每周期收益 += (生产空间/1GB）*（记账周期/ 1年）
      row.hdd_per_cycle_profit = (int64_t)((double)(newspace / (double)one_gb) * ((double)fee_cycle / (double)milliseconds_in_one_year) * 100000000);
   });

   userhdd_index _userhdd(_self, owner.value);
   chg_owner_space(_userhdd, owner, space, true, true, tmp_t, deadline_time);

   //-------------------- sync start ------------------
   miner_table _miner( _self , _self );
   auto itminer = _miner.find(minerid); //如果正式切换到该结构的时候,找不到该矿机id需要报错
   if(itminer != _miner.end()){
      _miner.modify(itminer, _self, [&](auto &row) {
         row.space  += space;
         if(row.status == 2)
            row.status = 0;
      });  
   }
   //-------------------- sync end ------------------

}

void hddpool::submprofit(name owner, uint64_t minerid, uint64_t space, name caller)
{
   eosio_assert(is_account(owner), "owner invalidate");
   eosio_assert(is_account(caller), "caller not an account.");
   check_bp_account(caller.value, minerid, true);

   eosio_assert((space & 65535) == 0, "invalid space");

   maccount_index _maccount(_self, owner.value);
   auto it = _maccount.find(minerid);
   eosio_assert(it != _maccount.end(), "minerid not register");

   bool mactive = true;
   if( it->space > 0 ) {
      if( it->hdd_per_cycle_profit == 0) {
         mactive = false;
      }
   }
   
   //check space left -- (is it enough)  -- start ----------
   minerinfo_table _minerinfo(_self, _self);
   auto itminerinfo = _minerinfo.find(minerid);   
   eosio_assert(itminerinfo != _minerinfo.end(), "minerid not exist in minerinfo");

   eosio_assert(itminerinfo->space_left + space <= itminerinfo->max_space, "exceed max space");
   eosio_assert(itminerinfo->owner == owner, "invalid owner");

   _minerinfo.modify(itminerinfo, _self, [&](auto &row) {
      row.space_left += space;
   });         
   //check space left -- (is it enough)  -- end   ----------

   uint64_t tmp_t = current_time();
   uint64_t deadline_time = 0;

   _maccount.modify(it, _self, [&](auto &row) {
      int64_t tmp_last_balance = it->hdd_balance;
      int64_t new_balance = calculate_profit_balance(tmp_last_balance, it->hdd_per_cycle_profit, it->last_hdd_time, tmp_t, deadline_time);
      eosio_assert(is_hdd_amount_within_range(new_balance), "magnitude of miner hddbalance must be less than 2^62");
      row.hdd_balance = (int64_t)(profit_percent * (new_balance - tmp_last_balance))  +  tmp_last_balance;
      //print(new_balance, " -- ", row.hdd_balance, " -- ", (int64_t)(profit_percent * (new_balance - tmp_last_balance)), "--", new_balance - row.hdd_balance);
      row.last_hdd_time = tmp_t;
      eosio_assert(row.space >= space, "invalid new production space");
      uint64_t newspace = row.space - space;
      row.space = newspace;
      //每周期收益 += (生产空间/1GB）*（记账周期/ 1年）
      if(mactive) {
         row.hdd_per_cycle_profit = (int64_t)((double)(newspace / (double)one_gb) * ((double)fee_cycle / (double)milliseconds_in_one_year) * 100000000);
      }
   });

   if(mactive) {
      userhdd_index _userhdd(_self, owner.value);
      chg_owner_space(_userhdd, owner, space, false, mactive, tmp_t, deadline_time);
   }

   //-------------------- sync start ------------------
   miner_table _miner( _self , _self );
   auto itminer = _miner.find(minerid); //如果正式切换到该结构的时候,找不到该矿机id需要报错
   if(itminer != _miner.end()){
      _miner.modify(itminer, _self, [&](auto &row) {
         row.space  -= space;
      });  
   }
   //-------------------- sync end ------------------


}

void hddpool::calcmbalance(name owner, uint64_t minerid)
{
   eosio_assert(false, "not support now!");
   ((void)owner);
   ((void)minerid);

}

void hddpool::delminer(uint64_t minerid, uint8_t acc_type, name caller)
{
   //eosio_assert(false, "function paused");

   minerinfo_table _minerinfo( _self , _self );
   auto itminerinfo = _minerinfo.find(minerid);

   eosio_assert(itminerinfo != _minerinfo.end(), "minerid not exist in minerinfo table");

   
   if(acc_type == 1) {
      eosio_assert(is_account(caller), "caller not a account.");
      check_bp_account(caller.value, minerid, true);
      eosio_assert(1 == 2, "not support");

   } else if(acc_type == 2) {
      require_auth(itminerinfo->admin);
      eosio_assert(1 == 2, "not support");
   } else {
      require_auth(N(hddpooladml2));
   }

   action(
       permission_level{N(hddpooladmin), active_permission},
       hdd_deposit, N(delminer),
       std::make_tuple(minerid))
       .send(); 

   //从该矿机的收益账号下移除该矿机
   if(itminerinfo->owner.value != 0) {
      maccount_index _maccount(_self, itminerinfo->owner.value);
      auto itmaccount = _maccount.find(minerid);
      if(itmaccount != _maccount.end()) {

         userhdd_index _userhdd(_self, itminerinfo->owner.value);
         if(itmaccount->hdd_per_cycle_profit > 0)
            chg_owner_space(_userhdd, itminerinfo->owner, itmaccount->space, false, false, current_time(), 0);

         _maccount.erase(itmaccount);    
      }
   }

   //归还空间到storepool
   if(itminerinfo->pool_id.value != 0) {
      storepool_index _storepool( _self , _self );
      auto itpool = _storepool.find(itminerinfo->pool_id.value);
      if(itpool != _storepool.end()) {
         _storepool.modify(itpool, _self, [&](auto &row) {
               uint64_t space_left = row.space_left;
               row.space_left = space_left + itminerinfo->max_space;    
         });  
      }
   }

   uint64_t max_space = itminerinfo->max_space;

   //删除该矿机信息
   _minerinfo.erase( itminerinfo );

   chg_total_space(max_space, false);
   //-------------------- sync start ------------------
   miner_table _miner( _self , _self );
   auto itminer = _miner.find(minerid); //如果正式切换到该结构的时候,找不到该矿机id需要报错
   if(itminer != _miner.end()){
      if(itminer->internal_id != 0) {
         del_miner2(itminer->internal_id);
      }
      _miner.erase( itminer );
   }
   //-------------------- sync end ------------------

}

void hddpool::mdeactive(name owner, uint64_t minerid, name caller)
{
   eosio_assert(is_account(owner), "owner invalidate");
   eosio_assert(is_account(caller), "caller not an account.");

   check_bp_account(caller.value, minerid, true);

   //maccount_index _maccount(_self, _self);
   maccount_index _maccount(_self, owner.value);
   auto it = _maccount.find(minerid);
   eosio_assert(it != _maccount.end(), "minerid not register");

   uint64_t space = it->space;
   eosio_assert(it->hdd_per_cycle_profit > 0, "miner has no cycle profit");  
   uint64_t tmp_t = current_time();  
   uint64_t deadline_time = 0;
   _maccount.modify(it, _self, [&](auto &row) {
      int64_t tmp_last_balance = it->hdd_balance;
      int64_t new_balance = calculate_profit_balance(tmp_last_balance, it->hdd_per_cycle_profit, it->last_hdd_time, tmp_t, deadline_time);
      eosio_assert(is_hdd_amount_within_range(new_balance), "magnitude of miner hddbalance must be less than 2^62");
      row.hdd_balance = (int64_t)(profit_percent * (new_balance - tmp_last_balance))  +  tmp_last_balance;
      row.last_hdd_time = tmp_t;
      row.hdd_per_cycle_profit = 0;
   });

   userhdd_index _userhdd(_self, owner.value);
   chg_owner_space(_userhdd, owner, space, false, true, tmp_t, deadline_time);

   //-------------------- sync start ------------------
   miner_table _miner( _self , _self );
   auto itminer = _miner.find(minerid); //如果正式切换到该结构的时候,找不到该矿机id需要报错
   if(itminer != _miner.end()){
      _miner.modify(itminer, _self, [&](auto &row) {
         row.status        = 1;
      });  
   }
   //-------------------- sync end ------------------

}

void hddpool::mactive(name owner, uint64_t minerid, name caller)
{
   eosio_assert(is_account(owner), "owner invalidate");
   eosio_assert(is_account(caller), "caller not an account.");
   check_bp_account(caller.value, minerid, true);

   maccount_index _maccount(_self, owner.value);
   auto it = _maccount.find(minerid);
   eosio_assert(it != _maccount.end(), "minerid not register");
   
   int64_t profit = 0;
   //每周期收益 += (生产空间*数据分片大小/1GB）*（记账周期/ 1年）
   profit = (int64_t)(((double)it->space / (double)one_gb) * ((double)fee_cycle / (double)milliseconds_in_one_year) * 100000000);

   uint64_t tmp_t = current_time();
   uint64_t space = it->space;
   eosio_assert(it->hdd_per_cycle_profit == 0, "miner already has cycle profit");  
   _maccount.modify(it, _self, [&](auto &row) {
      row.hdd_per_cycle_profit = profit;
      row.last_hdd_time = tmp_t;
   });

   userhdd_index _userhdd(_self, owner.value);
   chg_owner_space(_userhdd, owner, space, true, true, tmp_t, 0);

   //-------------------- sync start ------------------
   miner_table _miner( _self , _self );
   auto itminer = _miner.find(minerid); //如果正式切换到该结构的时候,找不到该矿机id需要报错
   if(itminer != _miner.end()){
      _miner.modify(itminer, _self, [&](auto &row) {
         row.status        = 0;
      });  
   }
   //-------------------- sync end ------------------

}

void hddpool::newminer(uint64_t minerid, name adminacc, name dep_acc, asset dep_amount)
{
   //eosio_assert(false, "function paused");

   require_auth(dep_acc);

   eosio_assert(is_account(adminacc), "adminacc invalidate");
   eosio_assert(is_account(dep_acc), "dep_acc invalidate");
   eosio_assert( dep_amount.amount > 0, "must use positive dep_amount" );
   eosio_assert( minerid > 0, "minerid must greater than zero" );


   minerinfo_table _minerinfo( _self , _self );
   auto itminerinfo = _minerinfo.find(minerid);
   eosio_assert(itminerinfo == _minerinfo.end(), "miner already registered \n");  

   account_name payer = _self;
   _minerinfo.emplace(payer, [&](auto &row) {      
      row.minerid    = minerid;
      row.admin      = adminacc;
      row.max_space  = 0;
      row.space_left = 0;
   });    
   
   //-------------------- sync start ------------------
   miner_table _miner( _self , _self );
   auto itminer = _miner.find(minerid);
   eosio_assert(itminer == _miner.end(), "already sync"); 

   _miner.emplace(payer, [&](auto &row) {      
      row.minerid          = minerid;
      row.admin            = adminacc;
      row.max_space        = 0;
      row.space            = 0;
      row.last_modify_time = 0;
      row.internal_id      = 0;
      row.internal_id2     = 0;      
      row.real_space       = 0;
      row.reserved1        = 0;
      row.level            = defaul_miner_level;
      row.status           = 1;
   });             
   //-------------------- sync end ------------------

   action(
       permission_level{dep_acc, active_permission},
       hdd_deposit, N(paydeposit),
       std::make_tuple(dep_acc, minerid, dep_amount))
       .send(); 
}

void hddpool::delstrpool(name poolid)
{
   require_auth(N(hddpooladmin));

   storepool_index _storepool( _self , _self );
   auto itmstorepool = _storepool.find(poolid.value);
   if(itmstorepool != _storepool.end()) {
      eosio_assert(itmstorepool->max_space - itmstorepool->space_left == 0,  "can not delete this storepool.");
      _storepool.erase(itmstorepool);
   }
}

void hddpool::regstrpool(name pool_id, name pool_owner)
{
   require_auth(pool_owner);

   storepool_index _storepool( _self , _self );
   auto itmstorepool = _storepool.find(pool_id.value);
   eosio_assert(itmstorepool == _storepool.end(), "storepool already registered");  

   account_name payer = pool_owner;
   _storepool.emplace(payer, [&](auto &row) {
      row.pool_id    = pool_id;
      row.pool_owner = pool_owner;
      row.max_space  = max_pool_space;
      row.space_left = max_pool_space;
   });

   asset quant{100000, CORE_SYMBOL};
   action(
       permission_level{pool_owner, active_permission},
       token_account, N(transfer),
       std::make_tuple(pool_owner, hdd_exchg_acc, quant, std::string("pay for creation storepool")))
       .send();
}

void hddpool::chgpoolspace(name pool_id, bool is_increase, uint64_t delta_space)
{ 
   require_auth(N(hddpooladml2));

   storepool_index _storepool( _self , _self );
   auto itmstorepool = _storepool.find(pool_id.value);
   eosio_assert(itmstorepool != _storepool.end(), "storepool not exist");  

   uint64_t max_space = 0;
   if(is_increase) {
      max_space = itmstorepool->max_space + delta_space;
   } else {
      if(itmstorepool->max_space > delta_space) {
         max_space = itmstorepool->max_space - delta_space;
      }
   }

   eosio_assert(max_space <= max_pool_space, "execeed max pool space");

   _storepool.modify(itmstorepool, _self, [&](auto &row) {
      uint64_t space_used = row.max_space - row.space_left;
      row.max_space = max_space;
      eosio_assert(space_used <= max_space, "invalid max_space");
      row.space_left = max_space - space_used;    
   });  
}

void hddpool::addm2pool(uint64_t minerid, name pool_id, name minerowner, uint64_t max_space) 
{
   //eosio_assert(false, "function paused");

   eosio_assert(is_account(minerowner), "minerowner invalidate");

   storepool_index _storepool(_self, _self);
   auto itstorepool = _storepool.find(pool_id.value);
   eosio_assert(itstorepool != _storepool.end(), "storepool not registered");

   require_auth(itstorepool->pool_owner);

   minerinfo_table _minerinfo( _self , _self );
   auto itminerinfo = _minerinfo.find(minerid);
   eosio_assert(itminerinfo != _minerinfo.end(), "miner not registered \n");  

   require_auth(itminerinfo->admin);
   
   eosio_assert(itminerinfo->pool_id.value == 0, "miner already join to a pool(@@err:alreadyinpool@@)\n");  
   eosio_assert((max_space & 65535) == 0, "invalid max_space");
   eosio_assert(max_space <= max_miner_space, "miner max_space overflow\n");  
   eosio_assert(max_space >= min_miner_space, "miner max_space underflow\n");  
   eosio_assert((itstorepool->space_left > 0 && itstorepool->space_left >= max_space),"pool space not enough");

   //--- check miner deposit and max_space
   asset deposit = hdddeposit(hdd_deposit).get_miner_deposit(minerid);
   eosio_assert(hdddeposit(hdd_deposit).is_deposit_enough(deposit, max_space),"deposit not enough for miner's max_space -- addm2pool");
   //--- check miner deposit and max_space


   _minerinfo.modify(itminerinfo, _self, [&](auto &row) {
      row.pool_id = pool_id;
      row.owner = minerowner;
      row.max_space = max_space;
      row.space_left = max_space;
   });  

   _storepool.modify(itstorepool, _self, [&](auto &row) {
      row.space_left -= max_space;
   });

   maccount_index _maccount(_self, minerowner.value);
   if (_maccount.begin() == _maccount.end()){
      _gstatem.hdd_macc_user += 1;
   }

   auto itmaccount = _maccount.find(minerid);
   eosio_assert(itmaccount == _maccount.end(), "miner already bind to a owner");

   account_name payer = _self;
   _maccount.emplace(payer, [&](auto &row) {
      row.minerid = minerid;
      row.owner = minerowner;
      row.space = 0;
      row.hdd_per_cycle_profit = 0;
      row.hdd_balance = 0;
      row.last_hdd_time = current_time();
   });

   userhdd_index _userhdd(_self, minerowner.value);
   auto userhdd_itr = _userhdd.find(minerowner.value);
   if (userhdd_itr == _userhdd.end())
   {
      new_user_hdd(_userhdd, minerowner, 0, _self);
   }
   
   chg_total_space(max_space, true);
   //-------------------- sync start ------------------
   //如果正式切换到该结构的时候需要处理重复加入矿池的问题
   miner_table _miner( _self , _self );
   auto itminer = _miner.find(minerid); //如果正式切换到该结构的时候,找不到该矿机id需要报错
   if(itminer != _miner.end()){
      _miner.modify(itminer, _self, [&](auto &row) {
         row.pool_id          = pool_id;
         row.owner            = minerowner;
         row.max_space        = max_space;
         row.space            = 0;
         row.status           = 2;
         row.internal_id      = insert_miner2(minerid);
         row.last_modify_time = current_time();
      });  
   }
   //-------------------- sync end ------------------

}

void hddpool::regminer(uint64_t minerid,name adminacc, name dep_acc,name pool_id, name minerowner, uint64_t max_space, asset dep_amount, bool is_calc)
{
   //eosio_assert(false, "function paused");

   eosio_assert(is_account(adminacc), "adminacc invalidate");
   eosio_assert(is_account(dep_acc), "dep_acc invalidate");
   eosio_assert(is_account(minerowner), "minerowner invalidate");   
   eosio_assert( minerid > 0, "minerid must greater than zero" );

   require_auth(dep_acc);

   storepool_index _storepool(_self, _self);
   auto itstorepool = _storepool.find(pool_id.value);
   eosio_assert(itstorepool != _storepool.end(), "storepool not registered");

   require_auth(itstorepool->pool_owner);


   minerinfo_table _minerinfo( _self , _self );
   auto itminerinfo = _minerinfo.find(minerid);
   eosio_assert(itminerinfo == _minerinfo.end(), "miner already registered \n");  

   eosio_assert((max_space & 65535) == 0, "invalid max_space");
   eosio_assert(max_space <= max_miner_space, "miner max_space overflow\n");  
   eosio_assert(max_space >= min_miner_space, "miner max_space underflow\n");  
   eosio_assert((itstorepool->space_left > 0 && itstorepool->space_left >= max_space),"pool space not enough");

   _storepool.modify(itstorepool, _self, [&](auto &row) {
      row.space_left -= max_space;
   });

   maccount_index _maccount(_self, minerowner.value);
   if (_maccount.begin() == _maccount.end()){
      _gstatem.hdd_macc_user += 1;
   }

   auto itmaccount = _maccount.find(minerid);
   eosio_assert(itmaccount == _maccount.end(), "miner already bind to a owner");

   account_name payer = _self;
   _maccount.emplace(payer, [&](auto &row) {
      row.minerid = minerid;
      row.owner = minerowner;
      row.space = 0;
      row.hdd_per_cycle_profit = 0;
      row.hdd_balance = 0;
      row.last_hdd_time = current_time();
   });

   userhdd_index _userhdd(_self, minerowner.value);
   auto userhdd_itr = _userhdd.find(minerowner.value);
   if (userhdd_itr == _userhdd.end())
   {
      new_user_hdd(_userhdd, minerowner, 0, _self);
   }

   _minerinfo.emplace(payer, [&](auto &row) {      
      row.minerid    = minerid;
      row.owner      = minerowner;
      row.admin      = adminacc;
      row.pool_id    = pool_id;
      row.max_space  = max_space;
      row.space_left = max_space;
   });    

   chg_total_space(max_space, true);
   
   //-------------------- sync start ------------------
   miner_table _miner( _self , _self );
   auto itminer = _miner.find(minerid);
   eosio_assert(itminer == _miner.end(), "already sync"); 

   _miner.emplace(payer, [&](auto &row) {      
      row.minerid          = minerid;
      row.owner            = minerowner;
      row.admin            = adminacc;
      row.pool_id          = pool_id;
      row.max_space        = max_space;
      row.space            = 0;
      row.last_modify_time = current_time();;
      row.internal_id      = insert_miner2(minerid);
      row.internal_id2     = 0;      
      row.real_space       = 0;
      row.reserved1        = 0;
      row.level            = defaul_miner_level;
      row.status           = 2;
   });             
   //-------------------- sync end ------------------
   asset quant = dep_amount;
   if(is_calc) {
      quant = hdddeposit(hdd_deposit).calc_deposit(max_space);
   } else {
         eosio_assert(hdddeposit(hdd_deposit).is_deposit_enough(dep_amount, max_space),"deposit not enough for miner's max_space -- regminer");
   }

   action(
       permission_level{dep_acc, active_permission},
       hdd_deposit, N(paydeposit),
       std::make_tuple(dep_acc, minerid, quant))
       .send(); 

}


void hddpool::mchgstrpool(uint64_t minerid, name new_poolid)
{
   minerinfo_table _minerinfo( _self , _self );
   auto itminerinfo = _minerinfo.find(minerid);
   eosio_assert(itminerinfo != _minerinfo.end(), "miner not registered \n");  

   storepool_index _storepool(_self, _self);
   auto itstorepool = _storepool.find(new_poolid.value);
   eosio_assert(itstorepool != _storepool.end(), "storepool not registered");

   require_auth(itminerinfo->admin);
   require_auth(itstorepool->pool_owner);

   //归还旧矿池空间
   auto itstorepool_old = _storepool.find(itminerinfo->pool_id.value);
   eosio_assert(itstorepool_old != _storepool.end(), "original storepool not registered");
   _storepool.modify(itstorepool_old, _self, [&](auto &row) {
      row.space_left += itminerinfo->max_space;
      if(row.space_left > row.max_space) {
         row.space_left = row.max_space;
      }
   });  

   //加入新矿池，并判断是新矿池配额是否足够容纳新矿机   
   eosio_assert(itstorepool->space_left >= itminerinfo->max_space, "new pool space not enough");
   _storepool.modify(itstorepool, _self, [&](auto &row) {
      row.space_left -= itminerinfo->max_space;
   }); 
   

   //修改minerinfo表中该矿机的矿池id 
   _minerinfo.modify(itminerinfo, _self, [&](auto &row) {
      row.pool_id = new_poolid;
   });  

   //-------------------- sync start ------------------
   miner_table _miner( _self , _self );
   auto itminer = _miner.find(minerid); //如果正式切换到该结构的时候,找不到该矿机id需要报错
   if(itminer != _miner.end()){
      _miner.modify(itminer, _self, [&](auto &rowm) {
         rowm.pool_id = new_poolid;
      });  
   }
   //-------------------- sync end ------------------
   
}

void hddpool::mincdeposit(uint64_t minerid, uint64_t space, asset dep_amount, bool is_calc) {

   miner_table _miner( _self , _self );
   auto itminer = _miner.find(minerid); 
   eosio_assert(itminer != _miner.end(), "miner not registered!");  
   eosio_assert((space & 65535) == 0, "invalid max_space!");
   eosio_assert((space + itminer->max_space) <= max_miner_space, "miner max_space overflow!");  
   eosio_assert((space + itminer->max_space) >= min_miner_space, "miner max_space underflow!"); 

   require_auth(hdddeposit(hdd_deposit).get_miner_depacc(minerid));

   name pool_owner = get_miner_pool_owner(minerid);
   require_auth(pool_owner);

   storepool_index _storepool( _self , _self );
   auto itmstorepool = _storepool.find(itminer->pool_id.value);
   eosio_assert(itmstorepool != _storepool.end(), "storepool not exist");  
   _storepool.modify(itmstorepool, _self, [&](auto &rowpool) {
      eosio_assert(rowpool.space_left >= space, "exceed storepool's max space");  
      rowpool.space_left -= space;
   });  

   _miner.modify(itminer, _self, [&](auto &row) {
      row.max_space += space;
      if(row.real_space > 0) 
         row.real_space += space;
   });  

   chg_total_space(space, true);

   asset need_amount = hdddeposit(hdd_deposit).get_miner_forfeit(minerid);
   need_amount += hdddeposit(hdd_deposit).calc_deposit(space);
   if(!is_calc) {
      eosio_assert(dep_amount.amount >= need_amount.amount, "deposit not enough");  
      need_amount = dep_amount;
   }

   action(
       permission_level{N(hddpooladmin), active_permission},
       hdd_deposit, N(incdeposit),
       std::make_tuple(minerid, need_amount))
       .send(); 

   //-------------------- sync to old table start------------------
   minerinfo_table _minerinfo( _self , _self );
   auto itminerinfo = _minerinfo.find(minerid);
   if(itminerinfo != _minerinfo.end()){
      _minerinfo.modify(itminerinfo, _self, [&](auto &rowm) {
         rowm.max_space  += space;
         rowm.space_left += space;
      });  

   }
   //-------------------- sync to old table end------------------

}

void hddpool::mchgspace(uint64_t minerid, uint64_t max_space)
{
   //eosio_assert(false, "not support now!");

   minerinfo_table _minerinfo( _self , _self );
   auto itminerinfo = _minerinfo.find(minerid);
   eosio_assert(itminerinfo != _minerinfo.end(), "miner not registered \n");  
   eosio_assert((max_space & 65535) == 0, "invalid max_space");
   eosio_assert(max_space <= max_miner_space, "miner max_space overflow\n");  
   eosio_assert(max_space >= min_miner_space, "miner max_space underflow\n");  


   require_auth(itminerinfo->admin);

   name pool_owner = get_miner_pool_owner(minerid);
   require_auth(pool_owner);

   //--- check miner deposit and max_space
   asset deposit = hdddeposit(hdd_deposit).get_miner_deposit(minerid);
   eosio_assert(hdddeposit(hdd_deposit).is_deposit_enough(deposit, max_space),"deposit not enough for miner's max_space -- addm2pool");
   //--- check miner deposit and max_space

   uint64_t space_delta = 0;
   bool is_increase = true;
   _minerinfo.modify(itminerinfo, _self, [&](auto &row) {
      maccount_index _maccount(_self, itminerinfo->owner.value);
      auto itmaccount = _maccount.find(minerid);
      eosio_assert(itmaccount != _maccount.end(), "miner owner invalidate");
      eosio_assert(itmaccount->space <= max_space, "max_space less then miner profit space");

      storepool_index _storepool( _self , _self );
      auto itmstorepool = _storepool.find(row.pool_id.value);
      eosio_assert(itmstorepool != _storepool.end(), "storepool not exist");  
      _storepool.modify(itmstorepool, _self, [&](auto &rowpool) {
         if(max_space  > row.max_space) {
            eosio_assert(rowpool.space_left >= (max_space - row.max_space), "exceed storepool's max space");      
            rowpool.space_left -= (max_space - row.max_space);
            space_delta = (max_space - row.max_space);
            is_increase = true;
         } else {
            rowpool.space_left += (row.max_space - max_space);
            space_delta = (row.max_space - max_space);
            is_increase = false;
         }
      });  

      uint64_t space_used = row.max_space - row.space_left;
      row.max_space = max_space;
      eosio_assert(space_used <= max_space, "invalid max_space");      
      row.space_left = max_space - space_used;
   });

   chg_total_space(space_delta, is_increase);
   //-------------------- sync start ------------------
   miner_table _miner( _self , _self );
   auto itminer = _miner.find(minerid); //如果正式切换到该结构的时候,找不到该矿机id需要报错
   if(itminer != _miner.end()){
      _miner.modify(itminer, _self, [&](auto &rowm) {
         rowm.max_space = max_space;
         if(rowm.real_space > 0) {
            if(is_increase)
               rowm.real_space += space_delta;
            else {
               if(rowm.real_space > space_delta){
                  rowm.real_space -= space_delta;
               } else {
                  rowm.real_space = 1;
               }
            }
         }
      });  
   }
   //-------------------- sync end ------------------

}

void hddpool::mchgadminacc(uint64_t minerid, name new_adminacc)
{
   minerinfo_table _minerinfo( _self , _self );
   auto itminerinfo = _minerinfo.find(minerid);
   eosio_assert(itminerinfo != _minerinfo.end(), "miner not registered \n");  

   require_auth(itminerinfo->admin);

   eosio_assert(is_account(new_adminacc), "new admin is not an account.");

   _minerinfo.modify(itminerinfo, _self, [&](auto &row) {
      row.admin = new_adminacc;
   });

   //-------------------- sync start ------------------
   miner_table _miner( _self , _self );
   auto itminer = _miner.find(minerid); //如果正式切换到该结构的时候,找不到该矿机id需要报错
   if(itminer != _miner.end()){
      _miner.modify(itminer, _self, [&](auto &row) {
         row.admin = new_adminacc;
      });  
   }
   //-------------------- sync end ------------------
   

}

void hddpool::mchgowneracc(uint64_t minerid, name new_owneracc)
{
   minerinfo_table _minerinfo( _self , _self );
   auto itminerinfo = _minerinfo.find(minerid);
   eosio_assert(itminerinfo != _minerinfo.end(), "miner not registered \n");  
   eosio_assert(is_account(new_owneracc), "new owner is not an account.");
   eosio_assert(itminerinfo->owner.value != 0, "no owner for this miner");

   name pool_owner = get_miner_pool_owner(minerid);

   require_auth(itminerinfo->admin);
   require_auth(pool_owner);

   maccount_index _maccount_old(_self, itminerinfo->owner.value);
   auto itmaccount_old = _maccount_old.find(minerid);
   eosio_assert(itmaccount_old != _maccount_old.end(), "minerid not register");
   //@@@封禁矿机不能变更收益账户,没有被采购任何空间的矿机也不能变更
   eosio_assert(itmaccount_old->hdd_per_cycle_profit > 0, "miner has no cycle profit.");  

   uint64_t space = itmaccount_old->space;


   uint64_t tmp_t = current_time();

   //结算旧owner账户当前的收益
   userhdd_index _userhdd_old(_self, itminerinfo->owner.value);
   chg_owner_space(_userhdd_old, itminerinfo->owner, space, false, true, tmp_t, 0);

   //结算新owner账户当前的收益
   userhdd_index _userhdd_new(_self, new_owneracc.value);
   auto userhdd_new_itr = _userhdd_new.find(new_owneracc.value);
   if (userhdd_new_itr == _userhdd_new.end())
   {
      new_user_hdd(_userhdd_new, new_owneracc, 0, _self);
      userhdd_new_itr = _userhdd_new.find(new_owneracc.value);
   }
   chg_owner_space(_userhdd_new, new_owneracc, space, true, true, tmp_t, 0);

   maccount_index _maccount_new(_self, new_owneracc.value);
   auto itmaccount_new = _maccount_new.find(minerid);
   eosio_assert(itmaccount_new == _maccount_new.end(), "new owner already own this miner");

   //将该矿机加入新的收益账户的矿机收益列表中   
   account_name payer = _self;
   _maccount_new.emplace(payer, [&](auto &row) {
      row.minerid = minerid;
      row.owner = new_owneracc;
      row.space = itmaccount_old->space;
      row.hdd_per_cycle_profit = itmaccount_old->hdd_per_cycle_profit;
      row.hdd_balance = itmaccount_old->hdd_balance;
      row.last_hdd_time = itmaccount_old->last_hdd_time;
   });

   //将该矿机从旧的收益账户的矿机收益列表中删除
   _maccount_old.erase(itmaccount_old);

   _minerinfo.modify(itminerinfo, _self, [&](auto &rowminer) {
      //变更矿机表的收益账户名称
      rowminer.owner = new_owneracc;
   });

   //-------------------- sync start ------------------
   miner_table _miner( _self , _self );
   auto itminer = _miner.find(minerid); //如果正式切换到该结构的时候,找不到该矿机id需要报错
   if(itminer != _miner.end()){
      _miner.modify(itminer, _self, [&](auto &rowm) {
         rowm.owner = new_owneracc;
      });  
   }
   //-------------------- sync end ------------------

}

/*
bool hddpool::is_bp_account(uint64_t uservalue)
{
   account_name producers[21];
   uint32_t bytes_populated = get_active_producers(producers, sizeof(account_name) * 21);
   uint32_t count = bytes_populated / sizeof(account_name);
   for (uint32_t i = 0; i < count; i++)
   {
      if (producers[i] == uservalue)
         return true;
   }
   return false;
}
*/

void hddpool::check_bp_account(account_name bpacc, uint64_t id, bool isCheckId) {
   require_auth(bpacc);
   return;
    account_name shadow;
    uint64_t seq_num = eosiosystem::getProducerSeq(bpacc, shadow);
    //print("bpname ----", name{bpacc}, "\n");
    eosio_assert(seq_num > 0 && seq_num < 22, "invalidate bp account");
    if(isCheckId) {
      eosio_assert( (id%21) == (seq_num-1), "can not access this id");
    }
    require_auth(shadow);
    //require_auth(bpacc);
}

void hddpool::calc_deposit_rate() {   
   gusdprice_singleton _gusdprice(_self, _self);
   usd_price  _gusdprice_state;
   if(_gusdprice.exists()) {
      _gusdprice_state = _gusdprice.get();
   } else {
      _gusdprice_state = usd_price{};
   }

   gparams_singleton _gparams(_self, _self);
   hdd_global_param  _gparmas_state;
   if(_gparams.exists()) {
      _gparmas_state = _gparams.get();
   } else {
      _gparmas_state = hdd_global_param{};
   }

   uint64_t yta_u_price = (uint64_t)( (_gparmas_state.yta_price * 10000) / _gusdprice_state.usdprice );
   int64_t rate = 10000;
   if(yta_u_price <= 2000){
      rate = 10000;
   } else {
      double drate = (1.0 / sqrt((double)yta_u_price/10000)) * 0.38;
      rate = (int64_t) (drate * 10000);
   }

   gdeprate_singleton _grate(_self, _self);
   deposit_rate  _grate_state;
   if(_grate.exists()) {
      _grate_state = _grate.get();
   } else {
      _grate_state = deposit_rate{};
   }
   _grate_state.rate = rate;
   _grate.set(_grate_state,_self);

   //int64_t rate2 = rate/100;
   //action(
   //    permission_level{N(hddpooladmin), active_permission},
   //    hdd_deposit, N(setrate),
   //    std::make_tuple(rate2))
   //    .send(); 
}


void hddpool::sethddprice(uint64_t price) {
   require_auth(_self);

   eosio_assert( price > 0, "invalid price" );

   gparams_singleton _gparams(_self, _self);
   hdd_global_param  _gparmas_state;
   if (_gparams.exists()) {
      _gparmas_state = _gparams.get();
   } else {
      _gparmas_state = hdd_global_param{};
   }
   _gparmas_state.hdd_price = price;
   _gparams.set(_gparmas_state,_self);

}

void hddpool::setdrdratio(uint64_t ratio) {
   require_auth(_self);

   eosio_assert( ratio >= 10000, "invalid deduplication ratio" );

   gparams_singleton _gparams(_self, _self);
   hdd_global_param  _gparmas_state;
   if (_gparams.exists()) {
      _gparmas_state = _gparams.get();
   } else {
      _gparmas_state = hdd_global_param{};
   }
   _gparmas_state.dup_remove_dist_ratio = ratio;
   _gparams.set(_gparmas_state,_self);

}

void hddpool::setusdprice(uint64_t price, uint8_t acc_type) {
   eosio_assert( acc_type >= 1, "invalid acc_type" );
   require_auth( N(hddpooladmin) );

   eosio_assert( price > 0, "invalid price" );

   gusdprice_singleton _gusdprice(_self, _self);
   usd_price  _gusdprice_state;
   if(_gusdprice.exists()) {
      _gusdprice_state = _gusdprice.get();
   } else {
      _gusdprice_state = usd_price{};
   }

   _gusdprice_state.usdprice = price;

   _gusdprice.set(_gusdprice_state,_self);

   calc_deposit_rate();      
}


void hddpool::setytaprice(uint64_t price, uint8_t acc_type) {
   if(acc_type == 1) {
      require_auth( N(hddpooladml1) );
   }
   else if(acc_type == 2) {
      require_auth( N(hddpooladmin) );
   } else {
      require_auth( _self );
   }

   eosio_assert( price > 0, "invalid price" );

   gparams_singleton _gparams(_self, _self);
   hdd_global_param  _gparmas_state;
   if (_gparams.exists()) {
      _gparmas_state = _gparams.get();
   } else {
      _gparmas_state = hdd_global_param{};
   }

   paramguard_singleton _paramguard(_self, _self);
   hdd_price_guard  _paramguard_state;
   if (_paramguard.exists()) {
      _paramguard_state = _paramguard.get();
   } else {
      _paramguard_state = hdd_price_guard{};
   }

   if(acc_type == 100) {    

      do{
         uint64_t tmp_t;
         uint64_t delta;
         uint64_t ct = current_time();

         tmp_t = (ct-_paramguard_state.last_ytaprice_guard_time1) / 1000000ll; //seconds
         delta = (uint64_t)(abs((int64_t)price - (int64_t)_paramguard_state.last_yta_guard_price1)) * 100 / _paramguard_state.last_yta_guard_price1;
         
         if(tmp_t < _paramguard_state.yta_price_timespan_1) {
            if(delta > _paramguard_state.yta_price_delta_1) break; 
         }
         tmp_t = (ct-_paramguard_state.last_ytaprice_guard_time2) / 1000000ll; //seconds
         delta = (uint64_t)(abs((int64_t)price - (int64_t)_paramguard_state.last_yta_guard_price2)) * 100 / _paramguard_state.last_yta_guard_price2;
         if(tmp_t < _paramguard_state.yta_price_timespan_2) {
            if(delta > _paramguard_state.yta_price_delta_2) break; 
         }

         _gparmas_state.yta_price = price;

         tmp_t = (ct-_paramguard_state.last_ytaprice_guard_time1) / 1000000ll; //seconds
         if(tmp_t > _paramguard_state.yta_price_timespan_1) {
            _paramguard_state.last_yta_guard_price1 = price;
            _paramguard_state.last_ytaprice_guard_time1 = ct;
         }

         tmp_t = (ct-_paramguard_state.last_ytaprice_guard_time2) / 1000000ll; //seconds
         if(tmp_t > _paramguard_state.yta_price_timespan_2) {
            _paramguard_state.last_yta_guard_price2 = price;
            _paramguard_state.last_ytaprice_guard_time2 = ct;            
         }

      } while(false);

   } else {
      _gparmas_state.yta_price = price;
      _paramguard_state.last_yta_guard_price1 = price;
      _paramguard_state.last_yta_guard_price2 = price;
      _paramguard_state.last_ytaprice_guard_time1 = current_time();
      _paramguard_state.last_ytaprice_guard_time2 = current_time();
   }
   _gparams.set(_gparmas_state,_self);
   _paramguard.set(_paramguard_state,_self);

   calc_deposit_rate();
}

void hddpool::setdrratio(uint64_t ratio, uint8_t acc_type) {
   if(acc_type == 1) {
      require_auth( N(hddpooladml1) );
   }
   else if(acc_type == 2) {
      require_auth( N(hddpooladmin) );
   } else {
      require_auth( _self );
   }

   eosio_assert( ratio > 0 && ratio <= 10000, "invalid deduplication distribute ratio" );

   gparams_singleton _gparams(_self, _self);
   hdd_global_param  _gparmas_state;
   if (_gparams.exists()) {
      _gparmas_state = _gparams.get();
   } else {
      _gparmas_state = hdd_global_param{};
   }

   paramguard_singleton _paramguard(_self, _self);
   hdd_price_guard  _paramguard_state;
   if (_paramguard.exists()) {
      _paramguard_state = _paramguard.get();
   } else {
      _paramguard_state = hdd_price_guard{};
   }

   if(acc_type == 1) {

      do{
         uint64_t tmp_t;
         uint64_t delta;
         uint64_t ct = current_time();


         tmp_t = (ct-_paramguard_state.last_duprmv_ratio_guard_time1) / 1000000ll; //seconds
         delta = (uint64_t)(abs((int64_t)ratio - (int64_t)_paramguard_state.last_dup_remove_guard_ratio1)) * 100 / _paramguard_state.last_dup_remove_guard_ratio1;
         
         if(tmp_t < _paramguard_state.duprmv_ratio_timespan_1) {
            if(delta > _paramguard_state.duprmv_ratio_delta_1) break; 
         }
         tmp_t = (ct-_paramguard_state.last_duprmv_ratio_guard_time2) / 1000000ll; //seconds
         delta = (uint64_t)(abs((int64_t)ratio - (int64_t)_paramguard_state.last_dup_remove_guard_ratio2)) * 100 / _paramguard_state.last_dup_remove_guard_ratio2;
         if(tmp_t < _paramguard_state.duprmv_ratio_timespan_2) {
            if(delta > _paramguard_state.duprmv_ratio_delta_2) break; 
         }

         _gparmas_state.dup_remove_ratio = ratio;

         tmp_t = (ct-_paramguard_state.last_duprmv_ratio_guard_time1) / 1000000ll; //seconds
         if(tmp_t > _paramguard_state.duprmv_ratio_timespan_1) {
            _paramguard_state.last_dup_remove_guard_ratio1 = ratio;
            _paramguard_state.last_duprmv_ratio_guard_time1 = ct;
         }

         tmp_t = (ct-_paramguard_state.last_duprmv_ratio_guard_time2) / 1000000ll; //seconds
         if(tmp_t > _paramguard_state.duprmv_ratio_timespan_2) {
            _paramguard_state.last_dup_remove_guard_ratio2 = ratio;
            _paramguard_state.last_duprmv_ratio_guard_time2 = ct;            
         }

      } while(false);


   } else {
      _gparmas_state.dup_remove_ratio = ratio;
      _paramguard_state.last_dup_remove_guard_ratio1 = ratio;
      _paramguard_state.last_dup_remove_guard_ratio2 = ratio;
      _paramguard_state.last_duprmv_ratio_guard_time1 = current_time();
      _paramguard_state.last_duprmv_ratio_guard_time2 = current_time();
   }
   _gparams.set(_gparmas_state,_self);
   _paramguard.set(_paramguard_state,_self);

}

void hddpool::addhddcnt(int64_t count, uint8_t acc_type) {
   if(acc_type == 1) {
      require_auth( N(hddpooladml1) );
   }
   else if(acc_type == 2) {
      require_auth( N(hddpooladmin) );
   } else {
      require_auth( _self );
   }

   gparams_singleton _gparams(_self, _self);
   hdd_global_param  _gparmas_state;
   if (_gparams.exists()) {
      _gparmas_state = _gparams.get();
   } else {
      _gparmas_state = hdd_global_param{};
   }
   _gparmas_state.hdd_counter += count;
   _gparams.set(_gparmas_state,_self);
   
}

uint32_t hddpool::insert_miner2(uint64_t minerid){
   
   gminer2ex_singleton _gminer2ex(_self, _self);
   miner2ex  _gstate;
   if(_gminer2ex.exists()) {
      _gstate = _gminer2ex.get();
   } else {
      _gstate = miner2ex{};
   }
   _gstate.max_miner_count += 1;
   uint64_t next_id = _gstate.next_id;
   if(next_id == _gstate.max_table_count + 1) {
      _gstate.max_table_count += 1;
      _gstate.next_id += 1;

      miner2_table _miner2( _self , _self );
      account_name payer = _self;
      _miner2.emplace(payer, [&](auto &row) {      
         row.internal_id  = next_id;
         row.minerid      = minerid;
         row.next_id      = 0;
      });       
   } else {
      miner2_table _miner2( _self , _self );
      auto itminer2 = _miner2.find(next_id);
      eosio_assert(itminer2 != _miner2.end(), "can not find next_id");  
      _gstate.next_id = itminer2->next_id;
      _miner2.modify(itminer2, _self, [&](auto &row) {
         row.minerid      = minerid;
         row.next_id      = 0;
      });  
   }

   _gminer2ex.set(_gstate,_self);

   return (uint32_t)next_id;
}

void hddpool::del_miner2(uint64_t internal_id){
   
   if(internal_id == 0)
      return;

   gminer2ex_singleton _gminer2ex(_self, _self);
   miner2ex  _gstate;
   eosio_assert(_gminer2ex.exists(), "miner2 table is empty");     
   _gstate = _gminer2ex.get();

   _gstate.max_miner_count -= 1;

   miner2_table _miner2( _self , _self );
   auto itminer2 = _miner2.find(internal_id);
   eosio_assert(itminer2 != _miner2.end(), "can not find next_id"); 
   uint64_t next_id =  _gstate.next_id;
   _gstate.next_id = internal_id;
   _miner2.modify(itminer2, _self, [&](auto &row) {
      row.minerid      = 0;
      row.next_id      = next_id;
   });  

   _gminer2ex.set(_gstate,_self);

}

void hddpool::oldsync(uint64_t minerid){
   require_auth( N(hddpooladmin) );

   miner_table _miner( _self , _self );
   auto itminer = _miner.find(minerid);
   eosio_assert(itminer == _miner.end(), "already sync"); 

   minerinfo_table _minerinfo( _self , _self );
   auto itminerinfo = _minerinfo.find(minerid);
   eosio_assert(itminerinfo != _minerinfo.end(), "miner not registered");

   _miner.emplace(_self, [&](auto &row) {      
      row.minerid       = minerid;
      row.owner         = itminerinfo->owner;
      row.admin         = itminerinfo->admin;
      row.pool_id       = itminerinfo->pool_id;
      row.max_space     = itminerinfo->max_space;
      if(itminerinfo->max_space >= itminerinfo->space_left){
         row.space         = itminerinfo->max_space - itminerinfo->space_left;
      } else {
         row.space         = 0;         
      }    
      row.last_modify_time = 0;
      row.real_space       = 0;
      row.reserved1        = 0;
      row.level            = defaul_miner_level;
      row.status           = 0;
      row.internal_id      = 0;
      row.internal_id2     = 0;
      if(itminerinfo->max_space > 0) {
         maccount_index _maccount(_self, itminerinfo->owner.value);
         auto itmaccount = _maccount.find(minerid);
         eosio_assert(itmaccount != _maccount.end(), "maccount not found");
         if(itmaccount->space > 0) {
            if(itmaccount->hdd_per_cycle_profit == 0){
               row.status  = 1;
            }
         }
         row.internal_id   = insert_miner2(minerid);
         row.last_modify_time = current_time();
      } else {
         row.status  = 1;
      }
   });      
}

void hddpool::mlevel(uint64_t minerid, uint32_t level, name caller) {

   //eosio_assert(false, "not support now!");
   
   check_bp_account(caller.value, minerid, true);

   miner_table _miner( _self , _self );
   auto itminer = _miner.find(minerid); 
   eosio_assert(itminer != _miner.end(), "minerid not found");
   eosio_assert(itminer->level != level, "same level as before");
   eosio_assert(level <= 89657, "level invalidate");

   _miner.modify(itminer, _self, [&](auto &row) {
      row.level = level;
   });  
   
}

void hddpool::mrspace(uint64_t minerid, uint64_t real_space, name caller) {

   //eosio_assert(false, "not support now!");
   
   check_bp_account(caller.value, minerid, true);

   miner_table _miner( _self , _self );
   auto itminer = _miner.find(minerid); 
   eosio_assert(itminer != _miner.end(), "minerid not found");
   eosio_assert(itminer->max_space > real_space, "real space must less then max_space");
   eosio_assert((real_space & 65535) == 0, "invalid read_space");
   eosio_assert(real_space > 0, "invalid read_space");   
   eosio_assert(real_space != itminer->real_space, "same real space");   

   _miner.modify(itminer, _self, [&](auto &row) {
      row.real_space = real_space;
   });  
   
}

uint64_t hddpool::get_newmodel_start_time() {
   /*
   gnewmparam_singleton _gnewmparam( _self, _self);
   newmparam  _gstate;
   if(_gnewmparam.exists()){
      _gstate = _gnewmparam.get();
      if(_gstate.start_time > 0)
         return _gstate.start_time;
   }*/
   return hddm_latest_stop_time;
}

void hddpool::chg_total_space(uint64_t space_delta, bool is_increase) {
   gcouterstate_singleton _gcounter(_self, _self);
   if(!_gcounter.exists())
      return;

   couterstate _gcounterstate;
   _gcounterstate = _gcounter.get();
   uint64_t delta_gb = space_delta >> 16;
   if(is_increase) {
      _gcounterstate.total_space += delta_gb;
   } else {
      if(_gcounterstate.total_space > delta_gb) {
         _gcounterstate.total_space -= delta_gb;
      } else {
         _gcounterstate.total_space = 0;
      }
   }    
}

void hddpool::startnewm() {
   require_auth( N(hddpooladmin) );

   gcouterstate_singleton _gcounter(_self, _self);
   eosio_assert(_gcounter.exists(),"can not start new model");

   couterstate _gcounterstate;
   _gcounterstate = _gcounter.get();    


   newmparam  _gstate;
   gnewmparam_singleton _gnewmparam( _self, _self);

   if(_gnewmparam.exists())
      _gstate = _gnewmparam.get();
   else 
      _gstate = newmparam{};
   
   eosio_assert(_gstate.is_started == false, "new model already started");

   uint64_t ct = current_time();   

   _gstate.start_time = ct;
   _gstate.span_reward_slot = 12;
   _gstate.new_span_reward_slot = 12;   
   _gstate.reward_type = 0;

   _gstate.reward_day = 1;
   _gstate.reward_month = 1;
   _gstate.next_day_time = ct + microseconds_in_one_day;
   _gstate.total_destory = 0;
   _gstate.cur_destory = 0;
   int64_t reward_issue = (int64_t) (((double)1.0 - std::pow(2,(double)(-1.0)*((double)1/2880))) * 20000000000000);
   _gstate.total_issue = reward_issue;
   _gstate.cur_issue = reward_issue;
   //此处需要增发相应数量的token
   int64_t real_reward = reward_issue / (int64_t)(((double)blocks_per_day / _gstate.span_reward_slot) + 0.5);         
   int64_t real_reward1 = (int64_t)(real_reward * 0.8);
   int64_t real_reward2 = real_reward - real_reward1;
   _gstate.cur_reward1gas = (int64_t)(real_reward1 * 0.1);
   _gstate.cur_reward2gas = (int64_t)(real_reward2 * 0.1);

   _gstate.start_space = _gcounterstate.total_space;
   _gstate.last_inc_space = 0;
   _gstate.task_space = _gstate.start_space + _gstate.last_inc_space;

   _gstate.is_started = true;
   _gnewmparam.set(_gstate,_self);
}

bool hddpool::update_newmodel_params(uint32_t slot, int64_t &reward, int64_t &reward_gas, uint8_t &reward_type) {
   gnewmparam_singleton _gnewmparam( _self, _self);

   if(!_gnewmparam.exists())
      return false;

   newmparam  _gstate;   
   _gstate = _gnewmparam.get();

   if(!_gstate.is_started)
      return false;

   gcouterstate_singleton _gcounter(_self, _self);
   if(!_gcounter.exists())
      return false;

   couterstate _gcounterstate;
   _gcounterstate = _gcounter.get();    

   _gstate.last_reward_slot = slot;

   uint64_t ct = current_time();   

   bool bret = true;

   if(ct >= _gstate.next_day_time) {
      uint32_t reward_day = _gstate.reward_day + 1;
      _gstate.reward_day = reward_day;

      //判断当天容量新增是否满足目标 ??????????
      bool is_destory = true;
      if(_gcounterstate.total_space >= _gstate.task_space)
         is_destory = false;

      int64_t reward_issue = (int64_t) (((double)1.0 - std::pow(2,(double)(-1.0)*((double)reward_day/2880))) * 20000000000000);
      int64_t cur_issue = reward_issue - _gstate.total_issue;
      if(reward_issue > _gstate.total_issue && cur_issue <= 481294288) { // _gstate.cur_issue 判断 _gstate.cur_issue > 0  并且小于第一天的增发量
         // issue _gstate.cur_issue amount token
         _gstate.cur_issue = cur_issue;
         _gstate.total_issue = reward_issue;
         _gstate.cur_destory = 0;
         if(is_destory){
            _gstate.cur_destory = (int64_t)(cur_issue * 0.7);
            // ---- transfer to eosio.null for destory
         }
         int64_t real_rewardtotal = cur_issue - _gstate.cur_destory;
         _gstate.span_reward_slot = _gstate.new_span_reward_slot;
         int64_t real_reward = real_rewardtotal / (int64_t)(((double)blocks_per_day / _gstate.span_reward_slot) + 0.5);         
         int64_t real_reward1 = (int64_t)(real_reward * 0.8);
         int64_t real_reward2 = real_reward - real_reward1;
         _gstate.cur_reward1 = real_reward1;
         _gstate.cur_reward1 = real_reward2;
         _gstate.cur_reward1gas = (int64_t)(real_reward1 * 0.1);
         _gstate.cur_reward2gas = (int64_t)(real_reward2 * 0.1);
         uint64_t last_inc_space = (uint64_t)((double)_gstate.last_inc_space/2 + std::log2(_gstate.last_inc_space) + 0.5);
         _gstate.last_inc_space = last_inc_space;
         _gstate.task_space += last_inc_space;

      } else {
         _gstate.cur_issue = 0;
         _gstate.cur_destory = 0;
         _gstate.cur_reward1 = 0;
         _gstate.cur_reward2 = 0;
         _gstate.cur_reward1gas = 0;
         _gstate.cur_reward2gas = 0;
         bret = false;
      }

      if((_gstate.next_day_time + microseconds_in_one_day) <= ct) 
         _gstate.next_day_time = ct + microseconds_in_one_day;
      else 
         _gstate.next_day_time += microseconds_in_one_day;   
   }

   if(_gstate.reward_type == 0) {
      _gstate.reward_type = 1;
      reward = _gstate.cur_reward1;
      reward_gas = _gstate.cur_reward1gas;
      reward_type = 0;

   } else {
      _gstate.reward_type = 0;
      reward = _gstate.cur_reward2;
      reward_gas = _gstate.cur_reward2gas;
      reward_type = 1;

   }

   _gnewmparam.set(_gstate,_self);
   return true;
}

void hddpool::onbuild(uint32_t slot) {
   require_auth( _self );

   gnewmparam_singleton _gnewmparam( _self, _self);
   if(!_gnewmparam.exists())
      return;

   newmparam  _gstate;   
   _gstate = _gnewmparam.get();

   _gstate.last_build_slot = slot;
   _gstate.span_build_slot = 2;

   _gnewmparam.set(_gstate,_self);

   gminer2ex_singleton _gminer2ex(_self, _self);
   if(!_gminer2ex.exists())
      return;           
   miner2ex  _gminer2ex_state;   
   _gminer2ex_state = _gminer2ex.get();

   gmscore2ex_singleton _gmscore2ex(_self, _self);
   mscore2ex _gmscore2ex_state;
   if(_gmscore2ex.exists())
      _gmscore2ex_state = _gmscore2ex.get();
   else    
      _gmscore2ex_state = mscore2ex{};

   if(_gmscore2ex_state.build_proc_id > _gminer2ex_state.max_table_count) {
      if(_gmscore2ex_state.current_s == 0) {
         _gmscore2ex_state.current_s  = 1;
         _gmscore2ex_state.mscore1_s1_count = _gmscore2ex_state.build_count1;
         _gmscore2ex_state.mscore2_s1_count = _gmscore2ex_state.build_count2;

      } else {
         _gmscore2ex_state.current_s = 0;
         _gmscore2ex_state.mscore1_s0_count = _gmscore2ex_state.build_count1;
         _gmscore2ex_state.mscore2_s0_count = _gmscore2ex_state.build_count2;
      }
      _gmscore2ex_state.build_proc_id = 1;
      _gmscore2ex_state.prev_build_range1 = 0;
      _gmscore2ex_state.prev_build_range2 = 0;
      _gmscore2ex_state.build_count1 = 0;
      _gmscore2ex_state.build_count2 = 0;      
      _gmscore2ex.set(_gmscore2ex_state,_self);
      return;
   }
   
   uint64_t tablescope;
   if(_gmscore2ex_state.current_s == 0) 
      tablescope = N(scopeb);
   else  
      tablescope = N(scopea);

   mscore1_table _mscore1(_self, tablescope);
   mscore2_table _mscore2(_self, tablescope);

   miner2_table _miner2(_self, _self);
   miner_table _miner(_self, _self);

   for(int i = 0; i<2; i++) {
      auto itminer2 = _miner2.find(_gmscore2ex_state.build_proc_id);
       if(itminer2 != _miner2.end()) {
          if(itminer2->minerid != 0) {
             auto itminer = _miner.find(itminer2->minerid);
             if(itminer != _miner.end()) {
                if(itminer->status == 0) { //没有被封禁的矿机才能参与

                     //--------更新容量概率空间表---------
                     uint64_t realspace = itminer->max_space;
                     if(itminer->real_space > 0)
                        realspace = itminer->real_space;
                     uint64_t realspace_gb = realspace >> 16;
                     if(realspace_gb > 0) {
                        double ranged = (double)(realspace_gb * itminer->level) / (double)100;
                        uint64_t range = (uint64_t)ranged;
                        if(range > 0) {
                           _gmscore2ex_state.prev_build_range1 += range;
                           auto itmscore1 = _mscore1.find(_gmscore2ex_state.build_count1 + 1);
                           if(itmscore1 != _mscore1.end()) {
                              _mscore1.modify(itmscore1, _self, [&](auto &row) {
                                 row.minerid  = itminer2->minerid;
                                 row.range    = _gmscore2ex_state.prev_build_range1;
                              });  
                           } else {
                              _mscore1.emplace(_self, [&](auto &row) {      
                                 row.mid      =  _gmscore2ex_state.build_count1 + 1;
                                 row.minerid  =  itminer2->minerid;
                                 row.range    = _gmscore2ex_state.prev_build_range1;
                              });       
                           }
                           _gmscore2ex_state.build_count1 += 1;
                        }
                     }

                     //--------更新存储概率空间表---------
                     uint64_t space = itminer->space;
                     uint64_t space_gb = space >> 16;
                     if(space_gb > 0){
                         _gmscore2ex_state.prev_build_range2 += space_gb;
                        auto itmscore2 = _mscore2.find(_gmscore2ex_state.build_count2 + 1);
                        if(itmscore2 != _mscore2.end()) {
                           _mscore2.modify(itmscore2, _self, [&](auto &row) {
                              row.minerid  = itminer2->minerid;
                              row.range    = _gmscore2ex_state.prev_build_range2;
                           });  
                        } else {
                           _mscore2.emplace(_self, [&](auto &row) {      
                              row.mid      =  _gmscore2ex_state.build_count2 + 1;
                              row.minerid  =  itminer2->minerid;
                              row.range    = _gmscore2ex_state.prev_build_range2;
                           });       
                        }
                        _gmscore2ex_state.build_count2 += 1;
                     }

                } // end of if(itminer->status == 0)
             }
          }
       }

      _gmscore2ex_state.build_proc_id++;
      if(_gmscore2ex_state.build_proc_id > _gminer2ex_state.max_table_count) {
         if(_gmscore2ex_state.current_s == 0) {
            _gmscore2ex_state.current_s  = 1;
            _gmscore2ex_state.mscore1_s1_count = _gmscore2ex_state.build_count1;
            _gmscore2ex_state.mscore2_s1_count = _gmscore2ex_state.build_count2;

         } else {
            _gmscore2ex_state.current_s = 0;
            _gmscore2ex_state.mscore1_s0_count = _gmscore2ex_state.build_count1;
            _gmscore2ex_state.mscore2_s0_count = _gmscore2ex_state.build_count2;
         }
         _gmscore2ex_state.build_proc_id = 1;
         _gmscore2ex_state.prev_build_range1 = 0;
         _gmscore2ex_state.prev_build_range2 = 0;
         _gmscore2ex_state.build_count1 = 0;
         _gmscore2ex_state.build_count2 = 0;      
         _gmscore2ex.set(_gmscore2ex_state,_self);
         return;
      }

   }
   _gmscore2ex.set(_gmscore2ex_state,_self);  
}

void hddpool::onrewardt(uint32_t slot) {
   require_auth( _self );

   //-------------   ------------
   uint8_t reward_type = 0;   
   gnewmparam_singleton _gnewmparam( _self, _self);
   if(!_gnewmparam.exists())
      return;

   newmparam  _gstate;   
   _gstate = _gnewmparam.get();

   _gstate.last_reward_slot = slot;
   reward_type = _gstate.reward_type;
   if(_gstate.reward_type == 0)
      _gstate.reward_type = 1;
   else
      _gstate.reward_type = 0;    
   
   _gnewmparam.set(_gstate,_self);
   //-------------   ------------

   /*
   int64_t reward = 0;
   int64_t reward_gas = 0;
   uint8_t reward_type = 0;
   if(!update_newmodel_params(slot, reward, reward_gas, reward_type))
      return;

   if(reward == 0)
      return;
   */   

   uint64_t random = ((uint32_t)tapos_block_prefix())*((uint16_t)tapos_block_num());
   //print(random);
   //random = 3093635920;

   if(reward_type == 0)
      rewardproc1(random, slot);
   else   
      rewardproc2(random, slot);

   //内联action调用
   //action(
   //   permission_level{_self, N(active)},
   //   _self, N(rewardselt),
   //   std::make_tuple(random1, random2) ).send(); 
   
   //延迟事务调用
   /*
   eosio::transaction out;
   out.actions.emplace_back( permission_level{ _self, N(active) }, _self, N(rewardselt), std::make_tuple(random1, random2) );
   out.delay_sec = 1;
   out.send( (uint128_t(_self) << 64) | slot, _self, false );
   */

}

//容量激励
void hddpool::rewardproc1(uint64_t random, uint32_t slot) {

   gmscore2ex_singleton _gmscore2ex(_self, _self);
   if(!_gmscore2ex.exists())
      return;

   mscore2ex _gmscore2ex_state;
   _gmscore2ex_state = _gmscore2ex.get();
  
   uint64_t tablescope;
   uint64_t max_count = 0;
   if(_gmscore2ex_state.current_s == 0) {
      tablescope = N(scopea);
      max_count = _gmscore2ex_state.mscore1_s0_count;
   } else {
      tablescope = N(scopeb);
      max_count = _gmscore2ex_state.mscore1_s1_count;
   }  
      
   mscore1_table _mscore1(_self, tablescope);

   uint64_t max_range = 0;
   auto itmax = _mscore1.find(max_count);
   if(itmax == _mscore1.end())
      return;
   max_range = itmax->range;

   uint64_t sel_range = random % max_range;
   uint64_t low = 1;
   uint64_t high = max_count;
   uint64_t sel_minerid = 0;
   uint64_t times = 0;
   while(low <= high) {
      uint64_t mid = (low+high)/2;
      times++;
      auto it = _mscore1.find(mid);
      if(it== _mscore1.end()) {
         return;
      }
      if(it->range == sel_range) {
         sel_minerid = it->minerid;
         break;
      } else if( sel_range > it->range ) {
         low = mid + 1;
      } else {
         high = mid -1;
      }
      if(times >= 20)
         return;   
   }

   //print(low , "---" , high, "---", sel_minerid, "---", sel_range);

   if(sel_minerid == 0) {
      auto it = _mscore1.find(high);
      if(it == _mscore1.end())
         return;
      if(it->range >= sel_range)
         sel_minerid = it->minerid;
      else {
         auto it2 = _mscore1.find(low);
         if(it2 == _mscore1.end())
            return;
         sel_minerid = it2->minerid;
      }   
   }

   if(sel_minerid != 0) 
   {
      std::string memo;
      memo = std::to_string(sel_minerid) + ":1";
      eosio::transaction out;
      out.actions.emplace_back( permission_level{ _self, N(active) }, _self, N(rewardlogt), std::make_tuple(memo) );
      out.delay_sec = 1;
      out.send( (uint128_t(_self) << 64) | slot, _self, false );      

   }
}

//存储激励
void hddpool::rewardproc2(uint64_t random, uint32_t slot) {
   gmscore2ex_singleton _gmscore2ex(_self, _self);
   if(!_gmscore2ex.exists())
      return;

   mscore2ex _gmscore2ex_state;
   _gmscore2ex_state = _gmscore2ex.get();
  
   uint64_t tablescope;
   uint64_t max_count = 0;
   if(_gmscore2ex_state.current_s == 0) {
      tablescope = N(scopea);
      max_count = _gmscore2ex_state.mscore2_s0_count;
   } else {
      tablescope = N(scopeb);
      max_count = _gmscore2ex_state.mscore2_s1_count;
   }
      
   mscore2_table _mscore2(_self, tablescope);

   uint64_t max_range = 0;
   auto itmax = _mscore2.find(max_count);
   if(itmax == _mscore2.end())
      return;
   max_range = itmax->range;

   uint64_t sel_range = random % max_range;
   uint64_t low = 1;
   uint64_t high = max_count;
   uint64_t sel_minerid = 0;
   uint64_t times = 0;
   while(low <= high) {
      uint64_t mid = (low+high)/2;
      times++;
      auto it = _mscore2.find(mid);
      if(it== _mscore2.end()) {
         return;
      }
      if(it->range == sel_range) {
         sel_minerid = it->minerid;
         break;
      } else if( sel_range > it->range ) {
         low = mid + 1;
      } else {
         high = mid -1;
      }
      if(times >= 20) {
         return;
      }
            
   }

   //print(low , "---" , high, "---", sel_minerid, "---", sel_range);

   if(sel_minerid == 0) {
      auto it = _mscore2.find(high);
      if(it == _mscore2.end())
         return;
      if(it->range >= sel_range)
         sel_minerid = it->minerid;
      else {
         auto it2 = _mscore2.find(low);
         if(it2 == _mscore2.end())
            return;
         sel_minerid = it2->minerid;
      }   
   }
   
   if(sel_minerid != 0) 
   {
      std::string memo;
      memo = std::to_string(sel_minerid) + ":2";
      eosio::transaction out;
      out.actions.emplace_back( permission_level{ _self, N(active) }, _self, N(rewardlogt), std::make_tuple(memo) );
      out.delay_sec = 1;
      out.send( (uint128_t(_self) << 64) | slot, _self, false );      

   }


}


void hddpool::rewardlogt(std::string memo) {
   require_auth( _self );
}


EOSIO_ABI(hddpool, (getbalance)(buyhdd)(transhdds)(sellhdd)(sethfee)(subbalance)(addhspace)(subhspace)(addmprofit)(delminer)
                  (calcmbalance)(delstrpool)(regstrpool)(chgpoolspace)(newminer)(addm2pool)(submprofit)(regminer)(mlevel)(mrspace)(startnewm)
                  (mchgspace)(mincdeposit)(mchgstrpool)(mchgadminacc)(mchgowneracc)(calcprofit)(fixownspace)(oldsync)(onbuild)(onrewardt)(rewardlogt)
                  (mdeactive)(mactive)(sethddprice)(setusdprice)(setytaprice)(setdrratio)(setdrdratio)(addhddcnt))
