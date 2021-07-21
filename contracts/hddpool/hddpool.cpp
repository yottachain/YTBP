#include "hddpool.hpp"
#include <eosiolib/action.hpp>
#include <eosiolib/chain.h>
#include <eosiolib/symbol.hpp>
#include <eosiolib/eosio.hpp>
#include <eosiolib/print.hpp>
#include <eosiolib/serialize.hpp>
#include <eosiolib/multi_index.hpp>
#include <eosio.token/eosio.token.hpp>
#include <eosio.system/eosio.system.hpp>
#include <hdddeposit/hdddeposit.hpp>
#include <hddlock/hddlock.hpp>


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

static constexpr eosio::name active_permission{N(active)};
static constexpr eosio::name token_account{N(eosio.token)};
static constexpr eosio::name hdd_exchg_acc{N(hddpoolexchg)};
static constexpr eosio::name hdd_deposit{N(hdddeposit12)};
static constexpr eosio::name hdd_lock{N(hddlock12345)};
static constexpr eosio::name hdd_subsidy_acc{N(hddsubsidyeu)}; //补贴账户
static constexpr eosio::name ecologyfound_acc{N(ecologyfound)}; //生态节点奖励池


const uint64_t old_max_minerid = 9000000;                      //旧模型下最大的矿机id  
const uint64_t old_last_profit_time = 1628524800000000;        //旧模型收益最终结算时间  


static constexpr int64_t max_hdd_amount    = (1LL << 62) - 1;
bool is_hdd_amount_within_range(int64_t amount) { return -max_hdd_amount <= amount && amount <= max_hdd_amount; }

//新增存储用户
void hddpool::new_users_hdd(usershdd_index& usershdd, name user, int64_t balance, account_name payer)
{
   bool is_inc_user = false;
   usershdd.emplace(payer, [&](auto &row) {      
      row.account_name = user;
      userhdd1_index _userhdd1(_self, user.value);
      auto it = _userhdd1.find(user.value);
      if (it != _userhdd1.end()){
         row.hdd_storehhdd = balance + it->hdd_storehhdd;
         row.hdd_per_cycle_fee = it->hdd_per_cycle_fee;
         row.hdd_space_store = it->hdd_space_store;
         row.last_hddstore_time = it->last_hddstore_time;
      } else {
         row.hdd_storehhdd = balance;
         row.hdd_per_cycle_fee = 0;
         row.hdd_space_store = 0;
         row.last_hddstore_time = current_time();
         is_inc_user = true;
      }
   });
   
   if(is_inc_user) {
      ghddtotal_singleton _gtotal(_self, _self);
      hdd_total_info  _gtotal_state;
      if (_gtotal.exists()) {
         _gtotal_state = _gtotal.get();
      } else {
         _gtotal_state = hdd_total_info{};
      }
      _gtotal_state.hdd_total_user += 1;
      _gtotal.set(_gtotal_state,_self);
   }
}

void hddpool::new_userm_hdd(usermhdd_index& usermhdd, name user, account_name payer)
{
   int64_t old_hddm = 0;
   old_hddm = cacl_old_hddm(user);
   bool is_inc_user = false;
   if(old_hddm < 0) {
      is_inc_user = true;
      old_hddm = 0;
   }

   usermhdd.emplace(payer, [&](auto &row) {
      row.account_name = user;
      row.hdd_minerhdd = old_hddm;
      row.hdd_per_cycle_profit = 0;
      row.hdd_space_profit = 0;
      row.max_space = 0;
      row.internal_id = 0;
      row.last_hddprofit_time = current_time();
   });

   if(is_inc_user) {
      ghddtotal_singleton _gtotal(_self, _self);
      hdd_total_info  _gtotal_state;
      if (_gtotal.exists()) {
         _gtotal_state = _gtotal.get();
      } else {
         _gtotal_state = hdd_total_info{};
      }
      _gtotal_state.hdd_macc_user += 1;
      _gtotal.set(_gtotal_state,_self);
   }
}

int64_t hddpool::cacl_old_hddm(name user) 
{
   int64_t old_hddm = 0;

   if(user.value == ecologyfound_acc.value) 
      return 0;

   userhdd1_index _userhdd1(_self, user.value);
   auto itold = _userhdd1.find(user.value);
   if(itold != _userhdd1.end()) {

      if(itold->last_hddprofit_time >= old_last_profit_time)
         return itold->hdd_minerhdd;
      
      int64_t eco_inc_balance = 0;
      _userhdd1.modify(itold, _self, [&](auto &row) {
         uint64_t tmp_t = old_last_profit_time;
         int64_t tmp_last_balance = itold->hdd_minerhdd;
         int64_t new_balance = calculate_balance(tmp_last_balance, 0, itold->hdd_per_cycle_profit, itold->last_hddprofit_time, tmp_t);
         eosio_assert(is_hdd_amount_within_range(new_balance), "magnitude of user hdd_minerhdd must be less than 2^62");      
         row.hdd_minerhdd = (int64_t)(profit_percent * (new_balance - tmp_last_balance))  +  tmp_last_balance;
         eco_inc_balance = new_balance - row.hdd_minerhdd;
         row.last_hddprofit_time = tmp_t;
         old_hddm = row.hdd_minerhdd;
         //print("{\"balance\":", itold->hdd_minerhdd, "}");
         //print(new_balance, " -- ", row.hdd_minerhdd, " -- ", eco_inc_balance, "--", (int64_t)(profit_percent * (new_balance - tmp_last_balance)), "--" , tmp_last_balance);
      });

      if(eco_inc_balance > 0 ) {
         usermhdd_index _usermhdd_eco(_self, ecologyfound_acc.value);
         auto it_eco = _usermhdd_eco.find(ecologyfound_acc.value);
         if(it_eco == _usermhdd_eco.end()) {
            _usermhdd_eco.emplace(_self, [&](auto &row) {
               row.account_name = ecologyfound_acc;
               row.hdd_minerhdd = eco_inc_balance;
               row.hdd_per_cycle_profit = 0;
               row.hdd_space_profit = 0;
               row.last_hddprofit_time = current_time();
               eosio_assert(is_hdd_amount_within_range(row.hdd_minerhdd), "magnitude of user hdd_minerhdd must be less than 2^62");      
            });
         } else {
            _usermhdd_eco.modify(it_eco, _self, [&](auto &row) {
               int64_t tmp_last_balance = it_eco->hdd_minerhdd;
               row.hdd_minerhdd = tmp_last_balance + eco_inc_balance;
               eosio_assert(is_hdd_amount_within_range(row.hdd_minerhdd), "magnitude of user hdd_minerhdd must be less than 2^62");      
            });
         }
      }

   } else {
      old_hddm = -1;
   }

   return old_hddm;
}

int64_t hddpool::calculate_balance(int64_t oldbalance, int64_t hdd_per_cycle_fee, int64_t hdd_per_cycle_profit, uint64_t last_hdd_time, uint64_t current_time)
{

   uint64_t slot_t = (current_time - last_hdd_time) / 1000ll; //convert to milliseconds
   int64_t new_balance = 0;

   double tick = (double)((double)slot_t / fee_cycle);
   new_balance = oldbalance;
   int64_t delta = (int64_t)(tick * (hdd_per_cycle_profit - hdd_per_cycle_fee));
   new_balance += delta;

   return new_balance;
}

void hddpool::getbalance(name user, uint8_t acc_type, name caller)
{
   eosio_assert(is_account(user), "user not a account.");


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

   usershdd_index _usershdd(_self, user.value);
   auto it = _usershdd.find(user.value);
   if (it == _usershdd.end())
   {
      userhdd1_index _userhdd1(_self, user.value);
      auto itold = _userhdd1.find(user.value);
      account_name payer = _self;
      if(itold != _userhdd1.end()) {
         new_users_hdd(_usershdd, user, 0, payer);
         it = _usershdd.find(user.value);
      }
   }
   
   if(it != _usershdd.end()) {
      _usershdd.modify(it, _self, [&](auto &row) {
         uint64_t tmp_t = current_time();
         int64_t tmp_last_balance = it->hdd_storehhdd;
         int64_t new_balance = calculate_balance(tmp_last_balance, it->hdd_per_cycle_fee, 0, it->last_hddstore_time, tmp_t);
         row.hdd_storehhdd = new_balance;
         eosio_assert(is_hdd_amount_within_range(row.hdd_storehhdd), "magnitude of user hdd_storehhdd must be less than 2^62");      
         row.last_hddstore_time = tmp_t;
         
         print("{\"balance\":", it->hdd_storehhdd, "}");
      });
   } else {
      print("{\"balance\":", 0, "}");
   }
}

void hddpool::calcprofit(name user)
{
   require_auth( user );
   
   if(user.value == ecologyfound_acc.value) 
      return;

   usermhdd_index _usermhdd(_self, user.value);
   auto it = _usermhdd.find(user.value);
   if(it == _usermhdd.end()) {
      userhdd1_index _userhdd1(_self, user.value);
      auto itold = _userhdd1.find(user.value);
      eosio_assert(itold != _userhdd1.end(), "user is not a storage provider");
      account_name payer = _self;
      new_userm_hdd(_usermhdd, user, payer);      
      return;
   }
   //eosio_assert(it != _usermhdd.end(), "user is not a storage provider");

   int64_t eco_inc_balance = 0;
   _usermhdd.modify(it, _self, [&](auto &row) {
      uint64_t tmp_t = current_time();
      int64_t tmp_last_balance = it->hdd_minerhdd;
      int64_t new_balance = calculate_balance(tmp_last_balance, 0, it->hdd_per_cycle_profit, it->last_hddprofit_time, tmp_t);
      eosio_assert(is_hdd_amount_within_range(new_balance), "magnitude of user hdd_minerhdd must be less than 2^62");      
      row.hdd_minerhdd = (int64_t)(profit_percent * (new_balance - tmp_last_balance))  +  tmp_last_balance;
      eco_inc_balance = new_balance - row.hdd_minerhdd;
      row.last_hddprofit_time = tmp_t;
      //print("{\"balance\":", it->hdd_minerhdd, "}");
      //print(new_balance, " -- ", row.hdd_minerhdd, " -- ", eco_inc_balance, "--", (int64_t)(profit_percent * (new_balance - tmp_last_balance)), "--" , tmp_last_balance);

   });

   if(eco_inc_balance > 0 ) {
      usermhdd_index _usermhdd_eco(_self, ecologyfound_acc.value);
      auto it_eco = _usermhdd_eco.find(ecologyfound_acc.value);
      if(it_eco == _usermhdd_eco.end()) {
         _usermhdd_eco.emplace(_self, [&](auto &row) {
            row.account_name = ecologyfound_acc;
            row.hdd_minerhdd = eco_inc_balance;
            row.hdd_per_cycle_profit = 0;
            row.hdd_space_profit = 0;
            row.last_hddprofit_time = current_time();
         });
      } else {
         _usermhdd_eco.modify(it_eco, _self, [&](auto &row) {
            int64_t tmp_last_balance = it_eco->hdd_minerhdd;
            row.hdd_minerhdd = tmp_last_balance + eco_inc_balance;
            eosio_assert(is_hdd_amount_within_range(row.hdd_minerhdd), "magnitude of user hdd_minerhdd must be less than 2^62");      
         });
      }
   }
}

void hddpool::chg_owner_space(usermhdd_index& usermhdd, name minerowner, uint64_t space_delta, bool is_increase, bool is_calc, uint64_t ct)
{
   if(minerowner.value == ecologyfound_acc.value) 
      return;

   auto usermhdd_itr = usermhdd.find(minerowner.value);
   eosio_assert(usermhdd_itr != usermhdd.end(), "no owner exists in usermhdd table");
   int64_t eco_inc_balance = 0;
   usermhdd.modify(usermhdd_itr, _self, [&](auto &row) {
      if(is_calc) 
      {
         int64_t tmp_last_balance = usermhdd_itr->hdd_minerhdd;
         int64_t new_balance = calculate_balance(tmp_last_balance, 0, usermhdd_itr->hdd_per_cycle_profit, usermhdd_itr->last_hddprofit_time, ct);
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
      usermhdd_index _usermhdd_eco(_self, ecologyfound_acc.value);
      auto it_eco = _usermhdd_eco.find(ecologyfound_acc.value);
      eosio_assert(it_eco != _usermhdd_eco.end(), "ecologyfound account is not open");
      _usermhdd_eco.modify(it_eco, _self, [&](auto &row) {
         int64_t tmp_last_balance = it_eco->hdd_minerhdd;
         row.hdd_minerhdd = tmp_last_balance + eco_inc_balance;
         eosio_assert(is_hdd_amount_within_range(row.hdd_minerhdd), "magnitude of user hdd_minerhdd must be less than 2^62");      
      });
   }

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

   usershdd_index _usershdd(_self, receiver.value);
   auto it = _usershdd.find(receiver.value);
   account_name payer = from;
   if (it == _usershdd.end())
   {
      new_users_hdd(_usershdd, receiver, amount, payer);
   }
   else
   {
      _usershdd.modify(it, _self, [&](auto &row) {
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


   usershdd_index _usershddfrom(_self, from.value);
   auto itfrom = _usershddfrom.find(from.value);
   eosio_assert(itfrom != _usershddfrom.end(), "from not exists in usershdd table");
   eosio_assert(itfrom->hdd_storehhdd >= amount, "hdd overdrawn.");
   _usershddfrom.modify(itfrom, _self, [&](auto &row) {
      row.hdd_storehhdd -= amount;
      eosio_assert(is_hdd_amount_within_range(row.hdd_storehhdd), "magnitude of user hdd_storehhdd must be less than 2^62");      
   });

   usershdd_index _usershddto(_self, to.value);
   auto itto = _usershddto.find(to.value);
   account_name payer = from;
   if (itto == _usershddto.end())
   {
      new_users_hdd(_usershddto, to, amount, payer);
   }
   else
   {
      _usershddto.modify(itto, _self, [&](auto &row) {
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


   usermhdd_index _usermhdd(_self, user.value);
   auto it = _usermhdd.find(user.value);
   eosio_assert(it != _usermhdd.end(), "user not exists in usermhdd table.");
   eosio_assert(it->hdd_minerhdd >= amount, "hdd overdrawn.");

   _usermhdd.modify(it, _self, [&](auto &row) {
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


   usershdd_index _usershdd(_self, user.value);
   auto it = _usershdd.find(user.value);
   if (it == _usershdd.end())
   {
      userhdd1_index _userhdd1(_self, user.value);
      auto itold = _userhdd1.find(user.value);
      eosio_assert(itold != _userhdd1.end(), "user not exists in usershdd table");
      account_name payer = _self;
      new_users_hdd(_usershdd, user, 0, payer);
      it = _usershdd.find(user.value);
   }
   eosio_assert(it != _usershdd.end(), "user not exists in usershdd table");

   eosio_assert(fee != it->hdd_per_cycle_fee, " the fee is the same \n");

   check_bp_account(caller.value, user.value, true);

   eosio_assert(is_hdd_amount_within_range(fee), "magnitude of fee must be less than 2^62");      


   //每周期费用 <= （占用存储空间/1GB）*（记账周期/ 1年）
   //bool istrue = fee <= (int64_t)(((double)it->hdd_space_store/(double)one_gb) * ((double)fee_cycle/(double)seconds_in_one_year) * 100000000);
   //eosio_assert(istrue , "the fee verification is not right \n");
   _usershdd.modify(it, _self, [&](auto &row) {
      //设置每周期费用之前可能需要将以前的余额做一次计算，然后更改last_hdd_time
      uint64_t tmp_t = current_time();
      int64_t tmp_last_balance = it->hdd_storehhdd;
      int64_t new_balance = calculate_balance(tmp_last_balance, it->hdd_per_cycle_fee, 0, it->last_hddstore_time, tmp_t);
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


   usershdd_index _usershdd(_self, user.value);
   auto it = _usershdd.find(user.value);
   if (it == _usershdd.end())
   {
      userhdd1_index _userhdd1(_self, user.value);
      auto itold = _userhdd1.find(user.value);
      eosio_assert(itold != _userhdd1.end(), "user not exists in usershdd table");
      account_name payer = _self;
      new_users_hdd(_usershdd, user, 0, payer);
      it = _usershdd.find(user.value);
   }
   eosio_assert(it != _usershdd.end(), "user not exists in usershdd table");

   check_bp_account(caller.value, user.value, true);

   _usershdd.modify(it, _self, [&](auto &row) {
      row.hdd_storehhdd -= balance;
      eosio_assert(is_hdd_amount_within_range(row.hdd_storehhdd), "magnitude of user hddbalance must be less than 2^62");
   });   

}

void hddpool::addhspace(name user, uint64_t space, name caller)
{
   eosio_assert(is_account(user), "user invalidate");
   eosio_assert(is_account(caller), "caller not an account.");

   usershdd_index _usershdd(_self, user.value);
   auto it = _usershdd.find(user.value);
   if (it == _usershdd.end())
   {
      userhdd1_index _userhdd1(_self, user.value);
      auto itold = _userhdd1.find(user.value);
      eosio_assert(itold != _userhdd1.end(), "user not exists in usershdd table");
      account_name payer = _self;
      new_users_hdd(_usershdd, user, 0, payer);
      it = _usershdd.find(user.value);
   }
   eosio_assert(it != _usershdd.end(), "user not exists in usershdd table");

   check_bp_account(caller.value, user.value, true);

   _usershdd.modify(it, _self, [&](auto &row) {
      row.hdd_space_store += space;
      eosio_assert(row.hdd_space_store <= max_user_space , "overflow max_userspace");
   });

}

void hddpool::subhspace(name user, uint64_t space, name caller)
{
   eosio_assert(is_account(user), "user invalidate");
   eosio_assert(is_account(caller), "caller not an account.");

   usershdd_index _usershdd(_self, user.value);
   auto it = _usershdd.find(user.value);
   if (it == _usershdd.end())
   {
      userhdd1_index _userhdd1(_self, user.value);
      auto itold = _userhdd1.find(user.value);
      eosio_assert(itold != _userhdd1.end(), "user not exists in usershdd table");
      account_name payer = _self;
      new_users_hdd(_usershdd, user, 0, payer);
      it = _usershdd.find(user.value);
   }
   eosio_assert(it != _usershdd.end(), "user not exists in usershdd table");

   check_bp_account(caller.value, user.value, true);

   _usershdd.modify(it, _self, [&](auto &row) {
      //eosio_assert(row.hdd_space >= space , "overdraw user hdd_space");
      if(row.hdd_space_store >= space)
         row.hdd_space_store -= space;
      else 
         row.hdd_space_store = 0;
   });

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
      row.max_space  = 0;
      row.space_left = 0;
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

void hddpool::addmprofitnew(uint64_t minerid, uint64_t space)
{
   minerinfo2_table _minerinfo2(_self, _self);
   auto itminerinfo2 = _minerinfo2.find(minerid);   
   eosio_assert(itminerinfo2 != _minerinfo2.end(), "minerid not exist in minerinfo2");

   eosio_assert(is_account(itminerinfo2->owner), "owner invalidate");

   maccount2_index _maccount2(_self, itminerinfo2->owner.value);
   auto it2 = _maccount2.find(minerid);
   eosio_assert(it2 != _maccount2.end(), "minerid not register");

   eosio_assert(itminerinfo2->max_space >= itminerinfo2->space + space, "exceed max space");

   eosio_assert(itminerinfo2->status == 0, "invalid miner status."); //非正常状态矿机不能增加生产空间

   _minerinfo2.modify(itminerinfo2, _self, [&](auto &row) {
      row.space += space;
   });    

   uint64_t tmp_t = current_time();
   _maccount2.modify(it2, _self, [&](auto &row) {
      int64_t tmp_last_balance = it2->hdd_balance;
      int64_t new_balance = calculate_balance(tmp_last_balance, 0, it2->hdd_per_cycle_profit, it2->last_hdd_time, tmp_t);
      eosio_assert(is_hdd_amount_within_range(new_balance), "magnitude of miner hddbalance must be less than 2^62");
      row.hdd_balance = (int64_t)(profit_percent * (new_balance - tmp_last_balance))  +  tmp_last_balance;
      //print(new_balance, " -- ", row.hdd_balance, " -- ", (int64_t)(profit_percent * (new_balance - tmp_last_balance)), "--", new_balance - row.hdd_balance);
      row.last_hdd_time = tmp_t;
      uint64_t newspace = row.space + space;
      row.space = newspace;
      //每周期收益 += (生产空间/1GB）*（记账周期/ 1年）
      row.hdd_per_cycle_profit = (int64_t)((double)(newspace / (double)one_gb) * ((double)fee_cycle / (double)milliseconds_in_one_year) * 100000000);
   });

   usermhdd_index _usermhdd(_self, itminerinfo2->owner.value);
   chg_owner_space(_usermhdd, itminerinfo2->owner, space, true, true, tmp_t);

}

void hddpool::addmprofitold(uint64_t minerid, uint64_t space)
{
   minerinfo1_table _minerinfo1(_self, _self);
   auto itminerinfo1 = _minerinfo1.find(minerid);   
   eosio_assert(itminerinfo1 != _minerinfo1.end(), "minerid not exist in minerinfo1");   

   eosio_assert(is_account(itminerinfo1->owner), "owner invalidate");

   maccount1_index _maccount1(_self, itminerinfo1->owner.value);
   auto it1 = _maccount1.find(minerid);
   eosio_assert(it1 != _maccount1.end(), "minerid not register");
   if(it1->space > 0)
      eosio_assert(it1->hdd_per_cycle_profit > 0, "miner is deactive");  

   eosio_assert(itminerinfo1->space_left >= space, "exceed max space");

   _minerinfo1.modify(itminerinfo1, _self, [&](auto &row) {
      row.space_left -= space;
   });      

   uint64_t tmp_t = current_time();
   _maccount1.modify(it1, _self, [&](auto &row) {
      row.last_hdd_time = tmp_t;
      //row.hdd_balance = 0;
      uint64_t newspace = row.space + space;
      row.space = newspace;
      //每周期收益 += (生产空间/1GB）*（记账周期/ 1年）
      row.hdd_per_cycle_profit = (int64_t)((double)(newspace / (double)one_gb) * ((double)fee_cycle / (double)milliseconds_in_one_year) * 100000000);
   });

}

void hddpool::addmprofit(uint64_t minerid, uint64_t space, name caller)
{
   eosio_assert(is_account(caller), "caller not an account.");
   check_bp_account(caller.value, minerid, true);

   uint64_t remainder = space % one_gb;
   eosio_assert(remainder == 0, "invalid space.");
   //eosio_assert(owner.value != N(hddpool12345), "owner can not addmprofit now.");  

   chg_total_space(space, true, true);

   minerinfo2_table _minerinfo2(_self, _self);
   auto itminerinfo2 = _minerinfo2.find(minerid);   
   if(itminerinfo2 != _minerinfo2.end())
      addmprofitnew(minerid, space);
   else
      addmprofitold(minerid, space);
}

void hddpool::mdeactivenew(uint64_t minerid)
{
   minerinfo2_table _minerinfo2(_self, _self);
   auto itminerinfo2 = _minerinfo2.find(minerid);   
   eosio_assert(itminerinfo2 != _minerinfo2.end(), "minerid not exist in minerinfo2");

   eosio_assert(is_account(itminerinfo2->owner), "owner invalidate");   

   maccount2_index _maccount2(_self, itminerinfo2->owner.value);
   auto it2 = _maccount2.find(minerid);
   eosio_assert(it2 != _maccount2.end(), "minerid not register");


   eosio_assert(itminerinfo2->status == 0, "invalid miner status."); //已经封禁的矿机不能再次封禁
   _minerinfo2.modify(itminerinfo2, _self, [&](auto &row) {
      row.status = 1;
   });
   

   uint64_t space = it2->space;
   //eosio_assert(it->hdd_per_cycle_profit > 0, "miner has no cycle profit");  
   uint64_t tmp_t = current_time();   
   _maccount2.modify(it2, _self, [&](auto &row) {
      int64_t tmp_last_balance = it2->hdd_balance;
      int64_t new_balance = calculate_balance(tmp_last_balance, 0, it2->hdd_per_cycle_profit, it2->last_hdd_time, tmp_t);
      eosio_assert(is_hdd_amount_within_range(new_balance), "magnitude of miner hddbalance must be less than 2^62");
      row.hdd_balance = (int64_t)(profit_percent * (new_balance - tmp_last_balance))  +  tmp_last_balance;
      row.last_hdd_time = tmp_t;
      row.hdd_per_cycle_profit = 0;
   });
   
   usermhdd_index _usermhdd(_self, itminerinfo2->owner.value);
   chg_owner_space(_usermhdd, itminerinfo2->owner, space, false, true, tmp_t);

}

void hddpool::mdeactiveold(uint64_t minerid)
{
   minerinfo1_table _minerinfo1(_self, _self);
   auto itminerinfo1 = _minerinfo1.find(minerid);   
   eosio_assert(itminerinfo1 != _minerinfo1.end(), "minerid not exist in minerinfo1");

   eosio_assert(is_account(itminerinfo1->owner), "owner invalidate");   

   maccount1_index _maccount1(_self, itminerinfo1->owner.value);
   auto it1 = _maccount1.find(minerid);
   eosio_assert(it1 != _maccount1.end(), "minerid not register");

   eosio_assert(it1->hdd_per_cycle_profit > 0, "miner has no cycle profit");  
   
   uint64_t tmp_t = current_time();   
   _maccount1.modify(it1, _self, [&](auto &row) {
      row.last_hdd_time = tmp_t;
      row.hdd_per_cycle_profit = 0;
   });
   
}

void hddpool::mdeactive(uint64_t minerid, name caller)
{   
   eosio_assert(is_account(caller), "caller not an account.");

   check_bp_account(caller.value, minerid, true);

   minerinfo2_table _minerinfo2(_self, _self);
   auto itminerinfo2 = _minerinfo2.find(minerid);   
   if(itminerinfo2 != _minerinfo2.end())
      mdeactivenew(minerid);
   else
      mdeactiveold(minerid);

}

void hddpool::mactivenew(uint64_t minerid)
{
   minerinfo2_table _minerinfo2(_self, _self);
   auto itminerinfo2 = _minerinfo2.find(minerid);   
   eosio_assert(itminerinfo2 != _minerinfo2.end(), "minerid not exist in minerinfo2");

   maccount2_index _maccount2(_self, itminerinfo2->owner.value);
   auto it2 = _maccount2.find(minerid);
   eosio_assert(it2 != _maccount2.end(), "minerid not register");

   eosio_assert(itminerinfo2->status != 0, "invalid miner status."); //已经封禁的矿机不能再次封禁
   _minerinfo2.modify(itminerinfo2, _self, [&](auto &row) {
      row.status = 0;
   });

   int64_t profit = 0;
   //每周期收益 += (生产空间*数据分片大小/1GB）*（记账周期/ 1年）
   profit = (int64_t)(((double)it2->space / (double)one_gb) * ((double)fee_cycle / (double)milliseconds_in_one_year) * 100000000);

   uint64_t tmp_t = current_time();
   uint64_t space = it2->space;
   //eosio_assert(it->hdd_per_cycle_profit == 0, "miner already has cycle profit");  
   _maccount2.modify(it2, _self, [&](auto &row) {
      row.hdd_per_cycle_profit = profit;
      row.last_hdd_time = tmp_t;
   });

   usermhdd_index _usermhdd(_self, itminerinfo2->owner.value);
   chg_owner_space(_usermhdd, itminerinfo2->owner, space, true, true, tmp_t);

}

void hddpool::mactiveold(uint64_t minerid)
{
   minerinfo1_table _minerinfo1(_self, _self);
   auto itminerinfo1 = _minerinfo1.find(minerid);   
   eosio_assert(itminerinfo1 != _minerinfo1.end(), "minerid not exist in minerinfo2");

   maccount1_index _maccount1(_self, itminerinfo1->owner.value);
   auto it1 = _maccount1.find(minerid);
   eosio_assert(it1 != _maccount1.end(), "minerid not register");

   eosio_assert(it1->hdd_per_cycle_profit == 0, "miner already has cycle profit");  

   int64_t profit = 0;
   //每周期收益 += (生产空间*数据分片大小/1GB）*（记账周期/ 1年）
   profit = (int64_t)(((double)it1->space / (double)one_gb) * ((double)fee_cycle / (double)milliseconds_in_one_year) * 100000000);

   uint64_t tmp_t = current_time();
   _maccount1.modify(it1, _self, [&](auto &row) {
      row.hdd_per_cycle_profit = profit;
      row.last_hdd_time = tmp_t;
   });

}

void hddpool::mactive(uint64_t minerid, name caller)
{   
   //eosio_assert(is_account(owner), "owner invalidate");
   eosio_assert(is_account(caller), "caller not an account.");
   check_bp_account(caller.value, minerid, true);

   minerinfo2_table _minerinfo2(_self, _self);
   auto itminerinfo2 = _minerinfo2.find(minerid);   
   if(itminerinfo2 != _minerinfo2.end())
      mactivenew(minerid);
   else
      mactiveold(minerid);      
}

void hddpool::check_deposit_enough( asset deposit, uint64_t max_space )
{
   int64_t rate;
   gdeprate_singleton _grate(_self, _self);
   if (_grate.exists()) {
      rate = _grate.get().rate;
   } else {
      rate = 10000;
   }

   double drate = ((double)rate)/100;
   int64_t am = (int64_t)((((double)max_space)/one_gb) * drate * 10000);

   eosio_assert(deposit.amount >= am ,"deposit not enough for miner's max_space.");   
}

void hddpool::check_token_enough( asset deposit, name user )
{
   bool is_frozen = hddlock(hdd_lock).is_frozen(user.value);  
   eosio_assert( !is_frozen, "frozen user can not create deposit pool" );

   auto balance   = eosio::token(N(eosio.token)).get_balance( user.value , deposit.symbol.name() );
   auto frozen_asset = hdddeposit(hdd_deposit).get_deposit(user.value);
   auto lock_asset = hddlock(hdd_lock).get_lock_asset(user.value);
   eosio_assert( frozen_asset.symbol == lock_asset.symbol, "symbol precision mismatch" );
   frozen_asset.amount += lock_asset.amount;
   frozen_asset.amount += hddpool::get_dep_lock(user.value);
   eosio_assert( frozen_asset.symbol == deposit.symbol, "symbol precision mismatch" );
   eosio_assert( balance.amount - frozen_asset.amount >= deposit.amount, "free token not enough" );

}

void hddpool::add_deposit( asset deposit, name user )
{
    depositinfo_index _deposit_info(_self , _self);
    auto it = _deposit_info.find(user);
    if(it != _deposit_info.end()) {
      eosio_assert( it->deposit.symbol == deposit.symbol, "symbol precision mismatch" );
      _deposit_info.modify(it, _self, [&](auto &row) {
      row.deposit.amount += deposit.amount;
      });  
    } else {
      _deposit_info.emplace(_self, [&](auto &row) {      
         row.user = user;
         row.deposit = deposit;
      });          
    }
}

//重新抵押接入旧矿机
void hddpool::redeposit(uint64_t minerid, name dep_acc, asset dep_amount) {

   //矿机id分隔
   eosio_assert(minerid <= old_max_minerid, "invalid minerid");

   eosio_assert(is_account(dep_acc), "dep_acc invalidate");
   require_auth(dep_acc);
   eosio_assert(dep_amount.symbol == CORE_SYMBOL, "must use core asset for miner deposit.");
   eosio_assert( dep_amount.amount > 0, "must use positive dep_amount" );

   minerinfo1_table _minerinfo1( _self , _self );
   auto itminerinfo1 = _minerinfo1.find(minerid);
   eosio_assert(itminerinfo1 != _minerinfo1.end(), "minerid not exist.");  
   eosio_assert(itminerinfo1->max_space > 0, "minerid not ready.");  

   require_auth(itminerinfo1->admin);

   minerinfo2_table _minerinfo2( _self , _self );
   auto itminerinfo2 = _minerinfo2.find(minerid);
   eosio_assert(itminerinfo2 == _minerinfo2.end(), "minerid alreay redeposit.");  

   //确定押金数量是否足够
   check_deposit_enough(dep_amount, itminerinfo1->max_space );

   //检查抵押账户下是否有足够的活token
   check_token_enough(dep_amount, dep_acc);

   //删除旧的抵押记录
   action(
       permission_level{N(hddpooladmin), active_permission},
       hdd_deposit, N(rmdeposit),
       std::make_tuple(minerid))
       .send(); 

   //生成新的抵押记录
   add_deposit(dep_amount, dep_acc);

   uint64_t space = itminerinfo1->max_space - itminerinfo1->space_left;

   account_name payer = _self;
   _minerinfo2.emplace(payer, [&](auto &row) {      
      row.minerid    = minerid;
      row.owner      = itminerinfo1->owner;
      row.admin      = itminerinfo1->admin;
      row.depacc     = dep_acc;
      row.pool_id    = itminerinfo1->pool_id;
      row.deposit    = dep_amount;
      row.dep_total  = dep_amount;
      row.max_space  = itminerinfo1->max_space;
      row.space      = space;
      row.internal_id  = 0;
      row.miner_type = 0;
      row.status     = 0; 
   });      

   //删除该矿机旧模型下的收益信息
   if(itminerinfo1->owner.value != 0) {
      maccount1_index _maccount1(_self, itminerinfo1->owner.value);
      auto itmaccount1 = _maccount1.find(minerid);
      if(itmaccount1 != _maccount1.end()) {
         _maccount1.erase(itmaccount1);    
      }
   }

   ///新增该矿机在新模型下的收益信息
   maccount2_index _maccount2(_self, itminerinfo1->owner.value);
   auto itmaccount2 = _maccount2.find(minerid);
   eosio_assert(itmaccount2 == _maccount2.end(), "miner already bind to a owner");
   int64_t profit = 0;
   //每周期收益 += (生产空间*数据分片大小/1GB）*（记账周期/ 1年）
   profit = (int64_t)(((double)space / (double)one_gb) * ((double)fee_cycle / (double)milliseconds_in_one_year) * 100000000);
   _maccount2.emplace(payer, [&](auto &row) {
      row.minerid = minerid;
      row.owner = itminerinfo1->owner;
      row.space = space;
      row.hdd_per_cycle_profit = profit;
      row.hdd_balance = 0;
      row.last_hdd_time = current_time();
   });

   usermhdd_index _usermhdd(_self, itminerinfo1->owner.value);
   auto usermhdd_itr = _usermhdd.find(itminerinfo1->owner.value);
   if (usermhdd_itr == _usermhdd.end())
   {
      new_userm_hdd(_usermhdd, itminerinfo1->owner, _self);
   }

   uint64_t tmp_t = current_time();
   chg_owner_space(_usermhdd, itminerinfo2->owner, space, true, true, tmp_t);

}


void hddpool::regminer(uint64_t minerid, name adminacc, name dep_acc, asset dep_amount, name pool_id, name minerowner, uint64_t max_space)
{
   //矿机id分隔
   eosio_assert(minerid > old_max_minerid, "invalid minerid");
   
   eosio_assert(is_account(adminacc), "adminacc invalidate");
   eosio_assert(is_account(dep_acc), "dep_acc invalidate");
   eosio_assert(is_account(minerowner), "minerowner invalidate");

   uint64_t remainder = max_space % one_gb;
   eosio_assert(remainder == 0, "invalid max_space.");   

   require_auth(dep_acc);

   storepool_index _storepool(_self, _self);
   auto itstorepool = _storepool.find(pool_id.value);
   eosio_assert(itstorepool != _storepool.end(), "storepool not registered");
   require_auth(itstorepool->pool_owner);

   eosio_assert( dep_amount.amount > 0, "must use positive dep_amount" );

   minerinfo2_table _minerinfo2( _self , _self );
   auto itminerinfo2 = _minerinfo2.find(minerid);
   eosio_assert(itminerinfo2 == _minerinfo2.end(), "miner already registered \n");  

   account_name payer = _self;
   _minerinfo2.emplace(payer, [&](auto &row) {      
      row.minerid    = minerid;
      row.owner      = minerowner;
      row.admin      = adminacc;
      row.depacc     = dep_acc;
      row.pool_id    = pool_id;
      row.deposit    = dep_amount;
      row.dep_total  = dep_amount;
      row.max_space  = max_space;
      row.space      = 0;
      row.internal_id  = 0;
      row.miner_type = 0;
      row.status     = 0; 
   });   

   //确定押金数量是否足够
   check_deposit_enough(dep_amount, max_space );

   //检查抵押账户下是否有足够的活token
   check_token_enough(dep_amount, dep_acc);

   //生成新的抵押记录
   add_deposit(dep_amount, dep_acc);

   eosio_assert(max_space <= max_miner_space, "miner max_space overflow\n");  
   eosio_assert(max_space >= min_miner_space, "miner max_space underflow\n");  
   eosio_assert((itstorepool->space_left > 0 && itstorepool->space_left >= max_space),"pool space not enough");

   _storepool.modify(itstorepool, _self, [&](auto &row) {
      row.space_left -= max_space;
   });

   maccount2_index _maccount2(_self, minerowner.value);

   auto itmaccount2 = _maccount2.find(minerid);
   eosio_assert(itmaccount2 == _maccount2.end(), "miner already bind to a owner");

   _maccount2.emplace(payer, [&](auto &row) {
      row.minerid = minerid;
      row.owner = minerowner;
      row.space = 0;
      row.hdd_per_cycle_profit = 0;
      row.hdd_balance = 0;
      row.last_hdd_time = current_time();
   });

   usermhdd_index _usermhdd(_self, minerowner.value);
   auto usermhdd_itr = _usermhdd.find(minerowner.value);
   if (usermhdd_itr == _usermhdd.end())
   {
      new_userm_hdd(_usermhdd, minerowner, _self);
   }

   chg_total_space(max_space, true, false);

}



void hddpool::mforfeit(uint64_t minerid, asset quant, name caller)
{
   eosio_assert(is_account(caller), "caller not an account.");
   check_bp_account(caller.value, minerid, true);

   minerinfo2_table _minerinfo2(_self, _self);
   auto itminerinfo2 = _minerinfo2.find(minerid);   
   if(itminerinfo2 != _minerinfo2.end()) {
      eosio_assert(quant.symbol == CORE_SYMBOL, "must use core asset for hdd deposit.");
      eosio_assert(quant.amount > 0, "must use positive quant" );
      asset quatreal = quant;
      if(itminerinfo2->deposit.amount < quatreal.amount)
         quatreal.amount = itminerinfo2->deposit.amount;
     
      depositinfo_index _deposit_info(_self , _self);    
      const auto& acc = _deposit_info.get( itminerinfo2->depacc.value, "no deposit record for this user");
      eosio_assert( acc.deposit.amount >= quatreal.amount, "overdrawn deposit" );

      _minerinfo2.modify( itminerinfo2, 0, [&]( auto& row ) {
        row.deposit.amount -= quatreal.amount;
      });

      _deposit_info.modify( acc, 0, [&]( auto& row ) {
        row.deposit.amount -= quatreal.amount;
      });  

      action(
         permission_level{itminerinfo2->depacc, active_permission},
         token_account, N(transfer),
         std::make_tuple(itminerinfo2->depacc, N(yottaforfeit), quatreal, std::string("draw forfeit")))
         .send();
   }
   else {
      action(
         permission_level{N(hddpooladmin), active_permission},
         hdd_deposit, N(mforfeitold),
         std::make_tuple(minerid,quant))
         .send(); 
   }
            
}


void hddpool::delminer(uint64_t minerid, uint8_t acc_type, name caller)
{
   
   minerinfo2_table _minerinfo2( _self , _self );
   auto itminerinfo2 = _minerinfo2.find(minerid);

   eosio_assert(itminerinfo2 != _minerinfo2.end(), "minerid not exist in minerinfo2 table");

   
   if(acc_type == 1) {
      eosio_assert(is_account(caller), "caller not a account.");
      check_bp_account(caller.value, minerid, true);
      eosio_assert(1 == 2, "not support");

   } else if(acc_type == 2) {
      require_auth(itminerinfo2->admin);
      eosio_assert(false, "not support");
   } else {
      require_auth(N(hddpooladml2));
   }

   //释放矿机抵押金
   depositinfo_index _deposit_info(_self , _self);
   auto itdeposit = _deposit_info.find(itminerinfo2->depacc);
   eosio_assert(itdeposit != _deposit_info.end(), "invalid deposit account");
   eosio_assert(itdeposit->deposit.amount >= itminerinfo2->deposit.amount, "overdrawn deposit");
   _deposit_info.modify( itdeposit, 0, [&]( auto& row ) {
      row.deposit -= itminerinfo2->deposit;
   });   

   //从该矿机的收益账号下移除该矿机
   if(itminerinfo2->owner.value != 0) {
      maccount2_index _maccount2(_self, itminerinfo2->owner.value);
      auto itmaccount2 = _maccount2.find(minerid);
      if(itmaccount2 != _maccount2.end()) {

         usermhdd_index _usermhdd(_self, itminerinfo2->owner.value);
         if(itminerinfo2->status == 0)
            chg_owner_space(_usermhdd, itminerinfo2->owner, itmaccount2->space, false, false, current_time());

         _maccount2.erase(itmaccount2);    
      }
   }

   //归还空间到storepool
   if(itminerinfo2->pool_id.value != 0) {
      storepool_index _storepool( _self , _self );
      auto itpool = _storepool.find(itminerinfo2->pool_id.value);
      if(itpool != _storepool.end()) {
         _storepool.modify(itpool, _self, [&](auto &row) {
               uint64_t space_left = row.space_left;
               row.space_left = space_left + itminerinfo2->max_space;    
         });  
      }
   }

   //删除该矿机信息
   _minerinfo2.erase( itminerinfo2 );

   //修改全网总空间
   chg_total_space(itminerinfo2->max_space, false, false);
   chg_total_space(itminerinfo2->space, false, true);

}


void hddpool::mchgstrpool(uint64_t minerid, name new_poolid)
{
   
   minerinfo2_table _minerinfo2( _self , _self );
   auto itminerinfo2 = _minerinfo2.find(minerid);
   eosio_assert(itminerinfo2 != _minerinfo2.end(), "miner not registered.");  

   storepool_index _storepool(_self, _self);
   auto itstorepool = _storepool.find(new_poolid.value);
   eosio_assert(itstorepool != _storepool.end(), "storepool not registered");

   require_auth(itminerinfo2->admin);
   require_auth(itstorepool->pool_owner);

   //归还旧矿池空间
   auto itstorepool_old = _storepool.find(itminerinfo2->pool_id.value);
   eosio_assert(itstorepool_old != _storepool.end(), "original storepool not registered");
   _storepool.modify(itstorepool_old, _self, [&](auto &row) {
      row.space_left += itminerinfo2->max_space;
      if(row.space_left > row.max_space) {
         row.space_left = row.max_space;
      }
   });  

   //加入新矿池，并判断是新矿池配额是否足够容纳新矿机   
   eosio_assert(itstorepool->space_left >= itminerinfo2->max_space, "new pool space not enough");
   _storepool.modify(itstorepool, _self, [&](auto &row) {
      row.space_left -= itminerinfo2->max_space;
   }); 
   

   //修改minerinfo表中该矿机的矿池id 
   _minerinfo2.modify(itminerinfo2, _self, [&](auto &row) {
      row.pool_id = new_poolid;
   });  
   
}

void hddpool::mchgspace(uint64_t minerid, uint64_t max_space)
{
   minerinfo2_table _minerinfo2( _self , _self );
   auto itminerinfo2 = _minerinfo2.find(minerid);
   eosio_assert(itminerinfo2 != _minerinfo2.end(), "miner not registered \n");  
   eosio_assert(max_space <= max_miner_space, "miner max_space overflow\n");  
   eosio_assert(max_space >= min_miner_space, "miner max_space underflow\n");  

   uint64_t remainder = max_space % one_gb;
   eosio_assert(remainder == 0, "invalid max_space.");   

   require_auth(itminerinfo2->admin);

   name pool_owner = get_miner_pool_owner(minerid);
   require_auth(pool_owner);

   //--- check miner deposit and max_space
   asset deposit = itminerinfo2->deposit;
   check_deposit_enough(deposit, max_space);
   //--- check miner deposit and max_space

   if(max_space > itminerinfo2->max_space) {
      chg_total_space(max_space - itminerinfo2->max_space, true, false);
   } else {
      chg_total_space(itminerinfo2->max_space - max_space , false, false);
   }


   _minerinfo2.modify(itminerinfo2, _self, [&](auto &row) {
      eosio_assert(itminerinfo2->space < max_space, "max_space should greater then miner's profit space");
      storepool_index _storepool( _self , _self );
      auto itmstorepool = _storepool.find(row.pool_id.value);
      eosio_assert(itmstorepool != _storepool.end(), "storepool not exist");  
      _storepool.modify(itmstorepool, _self, [&](auto &rowpool) {
         if(max_space  > row.max_space) {
            eosio_assert(rowpool.space_left >= (max_space - row.max_space), "exceed storepool's max space");      
            rowpool.space_left -= (max_space - row.max_space);
         } else {
            rowpool.space_left += (row.max_space - max_space);
         }
      });  
      row.max_space = max_space;
   });
}

void hddpool::chgdeposit(name user, uint64_t minerid, bool is_increase, asset quant)
{
   require_auth(user); // need hdd official account to sign this action.

   eosio_assert(quant.symbol == CORE_SYMBOL, "must use core asset for hdd deposit.");
   eosio_assert(quant.amount > 0, "must use positive quant");

   minerinfo2_table _minerinfo2( _self , _self );
   auto itminerinfo2 = _minerinfo2.find(minerid);
   eosio_assert(itminerinfo2 != _minerinfo2.end(), "miner not registered");  

   eosio_assert(itminerinfo2->depacc == user, "must use same account to change deposit.");
   depositinfo_index _deposit_info(_self , _self);
   auto itdeposit = _deposit_info.find(user);
   eosio_assert(itdeposit != _deposit_info.end(), "user is not a deposit account");

   if(is_increase) {
      check_token_enough(quant, user);

      _minerinfo2.modify( itminerinfo2, 0, [&]( auto& row ) {
         row.deposit += quant;
         if(row.deposit.amount > row.dep_total.amount)
               row.dep_total.amount = row.deposit.amount; 
      });

      _deposit_info.modify( itdeposit, 0, [&]( auto& row ) {
         row.deposit += quant;
      });

   } else {
      eosio_assert( itminerinfo2->deposit.amount >= quant.amount, "overdrawn deposit" );
      eosio_assert( itdeposit->deposit.amount >= quant.amount, "overdrawn deposit total" );

      _minerinfo2.modify( itminerinfo2, 0, [&]( auto& row ) {
         row.deposit -= quant;
         row.dep_total -= quant;
      });

      _deposit_info.modify( itdeposit, 0, [&]( auto& row ) {
         row.deposit -= quant;
      });
   }

    check_deposit_enough(itminerinfo2->deposit, itminerinfo2->max_space);
}

void hddpool::mchgdepacc(uint64_t minerid, name new_depacc)
{
   require_auth(new_depacc);

   minerinfo2_table _minerinfo2( _self , _self );
   auto itminerinfo2 = _minerinfo2.find(minerid);
   eosio_assert(itminerinfo2 != _minerinfo2.end(), "miner not registered \n");  
   eosio_assert(itminerinfo2->depacc != new_depacc, "must use different account to change deposit user");

   depositinfo_index _depositold_info(_self , _self);
   auto itdepositold = _depositold_info.find(itminerinfo2->depacc);
   eosio_assert(itdepositold != _depositold_info.end(), "user is not a deposit account");
   eosio_assert(itdepositold->deposit.amount >= itminerinfo2->deposit.amount, "overdrawn deposit");
   _depositold_info.modify( itdepositold, 0, [&]( auto& row ) {
      row.deposit -= itminerinfo2->deposit;
   });

   check_token_enough(itminerinfo2->deposit, new_depacc);

   add_deposit(itminerinfo2->deposit, new_depacc);
}

void hddpool::mchgadminacc(uint64_t minerid, name new_adminacc)
{ 
   minerinfo2_table _minerinfo2( _self , _self );
   auto itminerinfo2 = _minerinfo2.find(minerid);
   eosio_assert(itminerinfo2 != _minerinfo2.end(), "miner not registered \n");  

   require_auth(itminerinfo2->admin);

   eosio_assert(is_account(new_adminacc), "new admin is not an account.");

   _minerinfo2.modify(itminerinfo2, _self, [&](auto &row) {
      row.admin = new_adminacc;
   });
   
}

void hddpool::mchgowneracc(uint64_t minerid, name new_owneracc)
{
   minerinfo2_table _minerinfo2( _self , _self );
   auto itminerinfo2 = _minerinfo2.find(minerid);
   eosio_assert(itminerinfo2 != _minerinfo2.end(), "miner not registered \n");  
   eosio_assert(is_account(new_owneracc), "new owner is not an account");
   eosio_assert(itminerinfo2->owner.value != 0, "no owner for this miner");

   name pool_owner = get_miner_pool_owner(minerid);

   require_auth(itminerinfo2->admin);
   require_auth(pool_owner);

   //禁矿机不能变更收益账户,没有被采购任何空间的矿机也不能变更
   eosio_assert(itminerinfo2->status == 0, "invalidate miner status");  
   eosio_assert(itminerinfo2->space == 0, "invalidate miner production space");  

   maccount2_index _maccount2_old(_self, itminerinfo2->owner.value);
   auto itmaccount2_old = _maccount2_old.find(minerid);
   eosio_assert(itmaccount2_old != _maccount2_old.end(), "minerid not register");

   uint64_t space = itmaccount2_old->space;
   uint64_t tmp_t = current_time();

   //结算旧owner账户当前的收益
   usermhdd_index _usermhdd_old(_self, itminerinfo2->owner.value);
   chg_owner_space(_usermhdd_old, itminerinfo2->owner, space, false, true, tmp_t);

   //结算新owner账户当前的收益
   usermhdd_index _usermhdd_new(_self, new_owneracc.value);
   auto usermhdd_new_itr = _usermhdd_new.find(new_owneracc.value);
   if (usermhdd_new_itr == _usermhdd_new.end())
   {
      new_userm_hdd(_usermhdd_new, new_owneracc, _self);
      usermhdd_new_itr = _usermhdd_new.find(new_owneracc.value);
   }
   chg_owner_space(_usermhdd_new, new_owneracc, space, true, true, tmp_t);

   maccount2_index _maccount2_new(_self, new_owneracc.value);
   auto itmaccount2_new = _maccount2_new.find(minerid);
   eosio_assert(itmaccount2_new == _maccount2_new.end(), "new owner already own this miner");

   //将该矿机加入新的收益账户的矿机收益列表中   
   account_name payer = _self;
   _maccount2_new.emplace(payer, [&](auto &row) {
      row.minerid = minerid;
      row.owner = new_owneracc;
      row.space = itmaccount2_old->space;
      row.hdd_per_cycle_profit = itmaccount2_old->hdd_per_cycle_profit;
      row.hdd_balance = itmaccount2_old->hdd_balance;
      row.last_hdd_time = itmaccount2_old->last_hdd_time;
   });

   //将该矿机从旧的收益账户的矿机收益列表中删除
   _maccount2_old.erase(itmaccount2_old);

   _minerinfo2.modify(itminerinfo2, _self, [&](auto &rowminer) {
      //变更矿机表的收益账户名称
      rowminer.owner = new_owneracc;
   });
}

void hddpool::fixownspace(name owner, uint64_t space)
{
   eosio_assert(false, "not support now!");
   require_auth( N(hddpooladmin) );

   usermhdd_index usermhdd(_self, owner.value);
   auto usermhdd_itr = usermhdd.find(owner.value);
   eosio_assert(usermhdd_itr != usermhdd.end(), "no owner exists in userhdd table");

   usermhdd.modify(usermhdd_itr, _self, [&](auto &row) {
      eosio_assert(space <= max_profit_space, "execeed max profie space");
      row.hdd_space_profit = space;
      row.hdd_per_cycle_profit = (int64_t)(((double)space / (double)one_gb) * ((double)fee_cycle / (double)milliseconds_in_one_year) * 100000000);
   });
}

void hddpool::chg_total_space(uint64_t space, bool is_increate, bool is_profit)
{
   ghddtotal_singleton _gtotal(_self, _self);
   hdd_total_info  _gtotal_state;
   if (_gtotal.exists()) {
      _gtotal_state = _gtotal.get();
   } else {
      _gtotal_state = hdd_total_info{};
   }
   //_gtotal_state.hdd_macc_user += 1;
   uint64_t delta = space / 65536;
   uint64_t remainder = space % one_gb;
   if(is_increate) {
      if(remainder != 0)
         delta = delta + 1;
   }

   if(is_increate) {
      if(is_profit)
         _gtotal_state.total_profit_space += delta;
      else 
         _gtotal_state.total_sapce += delta;
   } else {
      if(is_profit)
         if(_gtotal_state.total_profit_space >= delta)
            _gtotal_state.total_profit_space -= delta;
      else 
         if(_gtotal_state.total_sapce >= delta)
         _gtotal_state.total_sapce -= delta;
   }
   _gtotal.set(_gtotal_state,_self);
}

void hddpool::check_bp_account(account_name bpacc, uint64_t id, bool isCheckId) {
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

void hddpool::calc_deposit_rate() {   
   gusdprice_singleton _gusdprice(_self, _self);
   usd_price  _gusdprice_state;
   if (_gusdprice.exists()) {
      _gusdprice_state = _gusdprice.get();
   } else {
      _gusdprice_state = usd_price{};
   }

   gparams_singleton _gparams(_self, _self);
   hdd_global_param  _gparmas_state;
   if (_gparams.exists()) {
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
   if (_grate.exists()) {
      _grate_state = _grate.get();
   } else {
      _grate_state = deposit_rate{};
   }
   _grate_state.rate = rate;
   _grate.set(_grate_state,_self);

}

void hddpool::setusdprice(uint64_t price, uint8_t acc_type) {
   eosio_assert( acc_type >= 1, "invalid acc_type" );
   require_auth( N(hddpooladmin) );

   eosio_assert( price > 0, "invalid price" );

   gusdprice_singleton _gusdprice(_self, _self);
   usd_price  _gusdprice_state;
   if (_gusdprice.exists()) {
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

   if(acc_type == 1) {    

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



EOSIO_ABI(hddpool, (getbalance)(buyhdd)(transhdds)(sellhdd)(sethfee)(subbalance)(addhspace)(subhspace)(addmprofit)(delminer)
                  (delstrpool)(regstrpool)(chgpoolspace)(redeposit)(regminer)(mforfeit)
                  (mchgspace)(mchgstrpool)(mchgadminacc)(mchgowneracc)(chgdeposit)(calcprofit)(fixownspace)(mchgdepacc)
                  (mdeactive)(mactive)(setusdprice)(sethddprice)(setytaprice)(setdrratio)(setdrdratio)(addhddcnt))
