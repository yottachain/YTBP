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


#include <cmath>
#include <string>
#include <type_traits>
#include <optional>

using namespace eosio;

const uint32_t hours_in_one_day = 24;
const uint32_t minutes_in_one_day = hours_in_one_day * 60;
const uint32_t seconds_in_one_day = minutes_in_one_day * 60;
//const uint32_t seconds_in_one_week = seconds_in_one_day * 7;
const uint32_t seconds_in_one_year = seconds_in_one_day * 365;

const uint32_t fee_cycle = seconds_in_one_day; //计费周期(秒为单位)

const uint32_t one_gb = 1024 * 1024 * 1024; //1GB
const uint32_t data_slice_size = 16 * 1024; // among 4k-32k,set it as 16k

//以下空间量按照16k一个分片大小为单位
const uint64_t max_userspace = 64 * 1024 * 1024 * uint64_t(1024 * 500);  //500P 最大用户存储空间量
const uint64_t max_minerspace = 64 * 1024 * uint64_t(1024 * 100); //100T 单个矿机最大的采购空间量
//const uint32_t per_profit_space = 64 * 1024; //1G //每次增加的采购空间容量


static constexpr eosio::name active_permission{N(active)};
static constexpr eosio::name token_account{N(eosio.token)};
static constexpr eosio::name hdd_exchg_acc{N(hddpoolexchg)};
static constexpr eosio::name hdd_deposit{N(hdddeposit12)};

static constexpr int64_t max_hdd_amount    = (1LL << 62) - 1;
bool is_hdd_amount_within_range(int64_t amount) { return -max_hdd_amount <= amount && amount <= max_hdd_amount; }

//const int64_t price_delta = 1;

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
   userhdd_index _userhdd(_self, user.value);
   auto it = _userhdd.find(user.value);
   eosio_assert(it != _userhdd.end(), "user is not a storage provider");

   _userhdd.modify(it, _self, [&](auto &row) {
      uint64_t tmp_t = current_time();
      int64_t tmp_last_balance = it->hdd_minerhdd;
      int64_t new_balance;
      if (calculate_balance(tmp_last_balance, 0, 0, it->last_hddprofit_time, tmp_t, new_balance))
      {
         row.hdd_minerhdd = new_balance;
         eosio_assert(is_hdd_amount_within_range(row.hdd_minerhdd), "magnitude of user hdd_minerhdd must be less than 2^62");      
         row.last_hddprofit_time = tmp_t;
      }
      print("{\"balance\":", it->hdd_minerhdd, "}");
   });
}


void hddpool::getbalance(name user, uint8_t acc_type, name caller)
{
   eosio_assert(is_account(user), "user not a account.");

   account_name payer = _self;

   if(acc_type == 1) {
      require_auth( user );
      payer = user.value;
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
      new_user_hdd(_userhdd, user, 0, payer);
      print("{\"balance\":", 0, "}");
   }
   else
   {
      _userhdd.modify(it, _self, [&](auto &row) {
         uint64_t tmp_t = current_time();
         int64_t tmp_last_balance = it->hdd_storehhdd;
         int64_t new_balance;
         if (calculate_balance(tmp_last_balance, it->hdd_per_cycle_fee, 0, it->last_hddstore_time, tmp_t, new_balance))
         {
            row.hdd_storehhdd = new_balance;
            eosio_assert(is_hdd_amount_within_range(row.hdd_storehhdd), "magnitude of user hdd_storehhdd must be less than 2^62");      
            row.last_hddstore_time = tmp_t;
         }
         
         print("{\"balance\":", it->hdd_storehhdd, "}");
      });
   }
}

bool hddpool::calculate_balance(int64_t oldbalance, int64_t hdd_per_cycle_fee, int64_t hdd_per_cycle_profit, uint64_t last_hdd_time, uint64_t current_time, int64_t &new_balance)
{

   uint64_t slot_t = (current_time - last_hdd_time) / 1000000ll; //convert to seconds
   new_balance = 0;

   double tick = (double)((double)slot_t / fee_cycle);
   new_balance = oldbalance;
   int64_t delta = (int64_t)(tick * (hdd_per_cycle_profit - hdd_per_cycle_fee));
   new_balance += delta;

   return true;
}

void hddpool::buyhdd(name from, name receiver, int64_t amount)
{
   require_auth(from);

   eosio_assert(is_account(from), "user not a account");
   eosio_assert(is_account(receiver), "receiver not a account");
   eosio_assert(is_account(hdd_exchg_acc), "to not a account");
   eosio_assert(amount > 0, "must buy positive quantity");
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

   int64_t _yta_amount =(int64_t)(((amount/10000)*(((double)_gparmas_state.hdd_price)/10000))/(((double)_gparmas_state.yta_price)/10000));

   asset quant{_yta_amount, CORE_SYMBOL};
   action(
       permission_level{from, active_permission},
       token_account, N(transfer),
       std::make_tuple(from, hdd_exchg_acc, quant, std::string("buy hdd")))
       .send();


   //_ghddpriceState.price = (int64_t)( ( (double)(quant.amount * 10000) / _hdd_amount ) * 100000000);

   userhdd_index _userhdd(_self, receiver.value);
   auto it = _userhdd.find(receiver.value);
   if (it == _userhdd.end())
   {
      new_user_hdd(_userhdd, receiver, amount, from);
   }
   else
   {
      _userhdd.modify(it, _self, [&](auto &row) {
         row.hdd_storehhdd += amount;
         eosio_assert(is_hdd_amount_within_range(row.hdd_storehhdd), "magnitude of user hdd_storehhdd must be less than 2^62");      
      });
   }

}

void hddpool::sellhdd(name user, int64_t amount)
{
   require_auth(user);

   eosio_assert(is_hdd_amount_within_range(amount), "magnitude of user hdd amount must be less than 2^62");      

   eosio_assert( amount > 0, "cannot sell negative hdd amount" );

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

   int64_t _yta_amount =(int64_t)((((amount/10000)*(((double)_gparmas_state.hdd_price)/10000))*(((double)_gparmas_state.dup_remove_ratio)/10000)*(((double)_gparmas_state.dup_remove_dist_ratio)/10000))/(((double)_gparmas_state.yta_price)/10000));


   asset quant{_yta_amount, CORE_SYMBOL};
   action(
       permission_level{hdd_exchg_acc, active_permission},
       token_account, N(transfer),
       std::make_tuple(hdd_exchg_acc, user, quant, std::string("sell hdd")))
       .send();

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


   //每周期费用 <= （占用存储空间*数据分片大小/1GB）*（记账周期/ 1年）
   //bool istrue = fee <= (int64_t)(((double)(it->hdd_space_store * data_slice_size)/(double)one_gb) * ((double)fee_cycle/(double)seconds_in_one_year) * 100000000);
   //eosio_assert(istrue , "the fee verification is not right \n");
   _userhdd.modify(it, _self, [&](auto &row) {
      //设置每周期费用之前可能需要将以前的余额做一次计算，然后更改last_hdd_time
      uint64_t tmp_t = current_time();
      int64_t tmp_last_balance = it->hdd_storehhdd;
      int64_t new_balance;
      if (calculate_balance(tmp_last_balance, it->hdd_per_cycle_fee, 0, it->last_hddstore_time, tmp_t, new_balance))
      {
         row.hdd_storehhdd = new_balance;
         eosio_assert(is_hdd_amount_within_range(row.hdd_storehhdd), "magnitude of user hdd_storehhdd must be less than 2^62");            
         row.last_hddstore_time = tmp_t;
      }
      row.hdd_per_cycle_fee = fee;
   });

}

void hddpool::subbalance(name user, int64_t balance, uint8_t acc_type, name caller)
{
   eosio_assert(is_account(user), "user invalidate");

   if(acc_type == 1) {
      require_auth( user );
   }
   else if(acc_type == 2) {
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
      eosio_assert(row.hdd_space_store <= max_userspace , "overflow max_userspace");
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

void hddpool::addmprofit(name owner, uint64_t minerid, uint64_t space, name caller)
{
   eosio_assert(is_account(owner), "owner invalidate");
   eosio_assert(is_account(caller), "caller not an account.");
   check_bp_account(caller.value, minerid, true);

   maccount_index _maccount(_self, owner.value);
   auto it = _maccount.find(minerid);
   eosio_assert(it != _maccount.end(), "minerid not register");

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

   _maccount.modify(it, _self, [&](auto &row) {
      int64_t tmp_last_balance = it->hdd_balance;
      int64_t new_balance;
      if (calculate_balance(tmp_last_balance, 0, it->hdd_per_cycle_profit, it->last_hdd_time, tmp_t, new_balance))
      {
         eosio_assert(is_hdd_amount_within_range(new_balance), "magnitude of miner hddbalance must be less than 2^62");
         row.hdd_balance = new_balance;
         row.last_hdd_time = tmp_t;
      }
      uint64_t newspace = row.space + space;
      row.space = newspace;
      //每周期收益 += (生产空间*数据分片大小/1GB）*（记账周期/ 1年）
      row.hdd_per_cycle_profit = (int64_t)(((double)(newspace * data_slice_size) / (double)one_gb) * ((double)fee_cycle / (double)seconds_in_one_year) * 100000000);
   });

   userhdd_index _userhdd(_self, owner.value);
   auto userhdd_itr = _userhdd.find(owner.value);
   eosio_assert(userhdd_itr != _userhdd.end(), "no owner exists in userhdd table");
   _userhdd.modify(userhdd_itr, _self, [&](auto &row) {
      int64_t tmp_last_balance = userhdd_itr->hdd_minerhdd;
      int64_t new_balance;
      if (calculate_balance(tmp_last_balance, 0, userhdd_itr->hdd_per_cycle_profit, userhdd_itr->last_hddprofit_time, tmp_t, new_balance))
      {
         eosio_assert(is_hdd_amount_within_range(new_balance), "magnitude of new_balance must be less than 2^62");
         row.hdd_minerhdd = new_balance;
         row.last_hddprofit_time = tmp_t;
      }
      uint64_t newspace = row.hdd_space_profit + space;
      row.hdd_space_profit = newspace;
      row.hdd_per_cycle_profit = (int64_t)(((double)(newspace * data_slice_size) / (double)one_gb) * ((double)fee_cycle / (double)seconds_in_one_year) * 100000000);
   });

}

void hddpool::calcmbalance(name owner, uint64_t minerid)
{
   require_auth(owner);

   maccount_index _maccount(_self, owner.value);
   auto it = _maccount.find(minerid);
   eosio_assert(it != _maccount.end(), "minerid not register \n");

   _maccount.modify(it, _self, [&](auto &row) {
      uint64_t tmp_t = current_time();
      int64_t tmp_last_balance = it->hdd_balance;
      int64_t new_balance;
      if (calculate_balance(tmp_last_balance, 0, it->hdd_per_cycle_profit, it->last_hdd_time, tmp_t, new_balance))
      {
         eosio_assert(is_hdd_amount_within_range(new_balance), "magnitude of miner hddbalance must be less than 2^62");
         row.hdd_balance = new_balance;
         row.last_hdd_time = tmp_t;
      }
   });
}

void hddpool::delminer(uint64_t minerid, uint8_t acc_type, name caller)
{
   minerinfo_table _minerinfo( _self , _self );
   auto itminerinfo = _minerinfo.find(minerid);

   eosio_assert(itminerinfo != _minerinfo.end(), "minerid not exist in minerinfo table");

   if(acc_type == 1) {
      eosio_assert(is_account(caller), "caller not a account.");
      check_bp_account(caller.value, minerid, true);

   } else if(acc_type == 2) {
      require_auth(itminerinfo->admin);
   } else {
      //require_auth(_self);
      require_auth(N(hddpooladmin));
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

   //删除该矿机信息
   _minerinfo.erase( itminerinfo );
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
   uint64_t tmp_t = current_time();   
   _maccount.modify(it, _self, [&](auto &row) {
      int64_t tmp_last_balance = it->hdd_balance;
      int64_t new_balance;
      if (calculate_balance(tmp_last_balance, 0, it->hdd_per_cycle_profit, it->last_hdd_time, tmp_t, new_balance))
      {
         eosio_assert(is_hdd_amount_within_range(new_balance), "magnitude of miner hddbalance must be less than 2^62");
         row.hdd_balance = new_balance;
         row.last_hdd_time = tmp_t;
      }
      row.hdd_per_cycle_profit = 0;
   });

   userhdd_index _userhdd(_self, owner.value);
   auto userhdd_itr = _userhdd.find(owner.value);
   eosio_assert(userhdd_itr != _userhdd.end(), "no owner exists in userhdd table");
   _userhdd.modify(userhdd_itr, _self, [&](auto &row) {
      int64_t tmp_last_balance = userhdd_itr->hdd_minerhdd;
      int64_t new_balance;
      if (calculate_balance(tmp_last_balance, 0, userhdd_itr->hdd_per_cycle_profit, userhdd_itr->last_hddprofit_time, tmp_t, new_balance))
      {
         eosio_assert(is_hdd_amount_within_range(new_balance), "magnitude of new_balance must be less than 2^62");
         row.hdd_minerhdd = new_balance;
         row.last_hddprofit_time = tmp_t;
      }
      uint64_t newspace = 0;
      if(row.hdd_space_profit >= space) {
         newspace = row.hdd_space_profit - space;
      }
      row.hdd_space_profit = newspace;
      row.hdd_per_cycle_profit = (int64_t)(((double)(newspace * data_slice_size) / (double)one_gb) * ((double)fee_cycle / (double)seconds_in_one_year) * 100000000);
   });
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
   profit = (int64_t)(((double)(it->space * data_slice_size) / (double)one_gb) * ((double)fee_cycle / (double)seconds_in_one_year) * 100000000);

   uint64_t tmp_t = current_time();
   uint64_t space = it->space;
   _maccount.modify(it, _self, [&](auto &row) {
      row.hdd_per_cycle_profit = profit;
      row.last_hdd_time = tmp_t;
   });

   userhdd_index _userhdd(_self, owner.value);
   auto userhdd_itr = _userhdd.find(owner.value);
   eosio_assert(userhdd_itr != _userhdd.end(), "no owner exists in userhdd table");
   _userhdd.modify(userhdd_itr, _self, [&](auto &row) {
      int64_t tmp_last_balance = userhdd_itr->hdd_minerhdd;
      int64_t new_balance;
      if (calculate_balance(tmp_last_balance, 0, userhdd_itr->hdd_per_cycle_profit, userhdd_itr->last_hddprofit_time, tmp_t, new_balance))
      {
         eosio_assert(is_hdd_amount_within_range(new_balance), "magnitude of new_balance must be less than 2^62");
         row.hdd_minerhdd = new_balance;
         row.last_hddprofit_time = tmp_t;
      }
      uint64_t newspace = 0;
      newspace = row.hdd_space_profit + space;
      row.hdd_space_profit = newspace;
      row.hdd_per_cycle_profit = (int64_t)(((double)(newspace * data_slice_size) / (double)one_gb) * ((double)fee_cycle / (double)seconds_in_one_year) * 100000000);
   });

}

void hddpool::newminer(uint64_t minerid, name adminacc, name dep_acc, asset dep_amount)
{
   require_auth(dep_acc);

   eosio_assert(is_account(adminacc), "adminacc invalidate");
   eosio_assert(is_account(dep_acc), "dep_acc invalidate");
   eosio_assert( dep_amount.amount > 0, "must use positive dep_amount" );

   storepool_index _storepool( _self , _self );
   auto itmstorepool = _storepool.find(dep_acc.value);
   eosio_assert(itmstorepool != _storepool.end(), "dep_acc must use a stroepool name");  

   minerinfo_table _minerinfo( _self , _self );
   auto itminerinfo = _minerinfo.find(minerid);
   eosio_assert(itminerinfo == _minerinfo.end(), "miner already registered \n");  

   _minerinfo.emplace(dep_acc.value, [&](auto &row) {      
      row.minerid    = minerid;
      row.admin      = adminacc;
      row.pool_id    = dep_acc;
      row.max_space  = 0;
      row.space_left = 0;
   });       

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

void hddpool::regstrpool(name pool_id, name pool_owner, uint64_t max_space)
{
   ((void)max_space);

   eosio_assert(is_account(pool_owner), "pool_owner invalidate");
   eosio_assert(pool_owner == pool_id, "pool_owner and pool_id must equal");

   require_auth(pool_owner);

   storepool_index _storepool( _self , _self );
   auto itmstorepool = _storepool.find(pool_id.value);
   eosio_assert(itmstorepool == _storepool.end(), "storepool already registered");  
   _storepool.emplace(_self, [&](auto &row) {
      row.pool_id    = pool_id;
      row.pool_owner = pool_owner;
      //row.max_space  = max_space;
      //row.space_left = max_space;
      row.max_space  = 0;
      row.space_left = 0;
   });       
}

void hddpool::chgpoolspace(name pool_id, uint64_t max_space)
{ 
//   require_auth(N(hddpooladml1));
   require_auth(N(hddpooladmin));

   storepool_index _storepool( _self , _self );
   auto itmstorepool = _storepool.find(pool_id.value);
   eosio_assert(itmstorepool != _storepool.end(), "storepool not exist");  

   _storepool.modify(itmstorepool, _self, [&](auto &row) {
      uint64_t space_used = row.max_space - row.space_left;
      row.max_space = max_space;
      eosio_assert(space_used <= max_space, "invalid max_space");
      row.space_left = max_space - space_used;    
   });  
}

void hddpool::addm2pool(uint64_t minerid, name pool_id, name minerowner, uint64_t max_space) 
{
   eosio_assert(is_account(minerowner), "minerowner invalidate");

   storepool_index _storepool(_self, _self);
   auto itstorepool = _storepool.find(pool_id.value);
   eosio_assert(itstorepool != _storepool.end(), "storepool not registered");

   require_auth(itstorepool->pool_owner);

   minerinfo_table _minerinfo( _self , _self );
   auto itminerinfo = _minerinfo.find(minerid);
   eosio_assert(itminerinfo != _minerinfo.end(), "miner not registered \n");  
   eosio_assert(itminerinfo->pool_id == pool_id, "pool_id invalidate \n");  

   require_auth(itminerinfo->admin);
   

   eosio_assert(itminerinfo->max_space == 0, "miner already join to a pool(@@err:alreadyinpool@@)\n");  
   eosio_assert(max_space <= max_minerspace, "miner max_space overflow\n");  
   eosio_assert((itstorepool->space_left > 0 && itstorepool->space_left > max_space),"pool space not enough");

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

   _maccount.emplace(_self, [&](auto &row) {
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
}

void hddpool::mchgstrpool(uint64_t minerid, name new_poolid)
{
   minerinfo_table _minerinfo( _self , _self );
   auto itminerinfo = _minerinfo.find(minerid);
   eosio_assert(itminerinfo != _minerinfo.end(), "miner not registered \n");  

   storepool_index _storepool(_self, _self);
   auto itstorepool = _storepool.find(new_poolid.value);
   eosio_assert(itstorepool != _storepool.end(), "storepool not registered");

   //require_auth(itminerinfo->admin);
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
   

   //修改minerinfo表中该矿机的矿机id 
   _minerinfo.modify(itminerinfo, _self, [&](auto &row) {
      row.pool_id = new_poolid;
   });  

   //变更矿池其实就是变更抵押   
   action(
       permission_level{itstorepool->pool_owner, active_permission},
       hdd_deposit, N(mchgdepacc),
       std::make_tuple(minerid, itstorepool->pool_owner))
       .send(); 
}

void hddpool::mchgspace(uint64_t minerid, uint64_t max_space)
{
   minerinfo_table _minerinfo( _self , _self );
   auto itminerinfo = _minerinfo.find(minerid);
   eosio_assert(itminerinfo != _minerinfo.end(), "miner not registered \n");  

   require_auth(itminerinfo->admin);

   //--- check miner deposit and max_space
   asset deposit = hdddeposit(hdd_deposit).get_miner_deposit(minerid);
   eosio_assert(hdddeposit(hdd_deposit).is_deposit_enough(deposit, max_space),"deposit not enough for miner's max_space -- addm2pool");
   //--- check miner deposit and max_space


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
         } else {
            rowpool.space_left += (row.max_space - max_space);
         }
      });  

      uint64_t space_used = row.max_space - row.space_left;
      row.max_space = max_space;
      eosio_assert(space_used <= max_space, "invalid max_space");      
      row.space_left = max_space - space_used;
   });
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

}

void hddpool::mchgowneracc(uint64_t minerid, name new_owneracc)
{
   minerinfo_table _minerinfo( _self , _self );
   auto itminerinfo = _minerinfo.find(minerid);
   eosio_assert(itminerinfo != _minerinfo.end(), "miner not registered \n");  

   require_auth(itminerinfo->admin);

   eosio_assert(is_account(new_owneracc), "new owner is not an account.");

   eosio_assert(itminerinfo->owner.value != 0, "no owner for this miner");

   maccount_index _maccount_old(_self, itminerinfo->owner.value);
   auto itmaccount_old = _maccount_old.find(minerid);
   eosio_assert(itmaccount_old != _maccount_old.end(), "minerid not register");
   uint64_t space = itmaccount_old->space;

   uint64_t tmp_t = current_time();

   //结算旧owner账户当前的收益
   userhdd_index _userhdd_old(_self, itminerinfo->owner.value);
   auto userhdd_old_itr = _userhdd_old.find(itminerinfo->owner.value);
   eosio_assert(userhdd_old_itr != _userhdd_old.end(), "old owner not exists in userhdd table");
   _userhdd_old.modify(userhdd_old_itr, _self, [&](auto &row) {
      int64_t tmp_last_balance = userhdd_old_itr->hdd_minerhdd;
      int64_t new_balance;
      if (calculate_balance(tmp_last_balance, 0, userhdd_old_itr->hdd_per_cycle_profit, userhdd_old_itr->last_hddprofit_time, tmp_t, new_balance))
      {
         eosio_assert(is_hdd_amount_within_range(new_balance), "magnitude of new_balance must be less than 2^62");
         row.hdd_minerhdd = new_balance;
         row.last_hddprofit_time = tmp_t;
      }
      uint64_t newspace = 0;
      if(row.hdd_space_profit >= space) {
         newspace = row.hdd_space_profit - space;
      }
      row.hdd_space_profit = newspace;
      row.hdd_per_cycle_profit = (int64_t)(((double)(newspace * data_slice_size) / (double)one_gb) * ((double)fee_cycle / (double)seconds_in_one_year) * 100000000);
   });

   //结算新owner账户当前的收益
   userhdd_index _userhdd_new(_self, new_owneracc.value);
   auto userhdd_new_itr = _userhdd_new.find(new_owneracc.value);
   if (userhdd_new_itr == _userhdd_new.end())
   {
      new_user_hdd(_userhdd_new, new_owneracc, 0, _self);
      userhdd_new_itr = _userhdd_new.find(new_owneracc.value);
   }
   _userhdd_new.modify(userhdd_new_itr, _self, [&](auto &row) {
      int64_t tmp_last_balance = userhdd_new_itr->hdd_minerhdd;
      int64_t new_balance;
      if (calculate_balance(tmp_last_balance, 0, userhdd_new_itr->hdd_per_cycle_profit, userhdd_new_itr->last_hddprofit_time, tmp_t, new_balance))
      {
         eosio_assert(is_hdd_amount_within_range(new_balance), "magnitude of new_balance must be less than 2^62");
         row.hdd_minerhdd = new_balance;
         row.last_hddprofit_time = tmp_t;
      }
      uint64_t newspace = 0;
      newspace = row.hdd_space_profit + space;
      row.hdd_space_profit = newspace;
      row.hdd_per_cycle_profit = (int64_t)(((double)(newspace * data_slice_size) / (double)one_gb) * ((double)fee_cycle / (double)seconds_in_one_year) * 100000000);
   });

   maccount_index _maccount_new(_self, new_owneracc.value);
   auto itmaccount_new = _maccount_new.find(minerid);
   eosio_assert(itmaccount_new == _maccount_new.end(), "new owner already own this miner");

   //将该矿机加入新的收益账户的矿机收益列表中   
   _maccount_new.emplace(_self, [&](auto &row) {
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
    account_name shadow;
    uint64_t seq_num = eosiosystem::getProducerSeq(bpacc, shadow);
    eosio_assert(seq_num > 0 && seq_num < 22, "invalidate bp account");
    if(isCheckId) {
      eosio_assert( (id%21) == (seq_num-1), "can not access this id");
    }
    require_auth(shadow);
    //require_auth(bpacc);
}

void hddpool::sethddprice(uint64_t price) {
   require_auth(_self);

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

void hddpool::setytaprice(uint64_t price, uint8_t acc_type) {
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




EOSIO_ABI(hddpool, (getbalance)(buyhdd)(sellhdd)(sethfee)(subbalance)(addhspace)(subhspace)(addmprofit)(delminer)
                  (calcmbalance)(delstrpool)(regstrpool)(chgpoolspace)(newminer)(addm2pool)
                  (mchgspace)(mchgadminacc)(mchgowneracc)(calcprofit)
                  (mdeactive)(mactive)(sethddprice)(setytaprice)(setdrratio)(setdrdratio)(addhddcnt))
