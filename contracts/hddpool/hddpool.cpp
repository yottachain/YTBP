#include "hddpool.hpp"
#include <eosiolib/action.hpp>
#include <eosiolib/chain.h>
#include <eosiolib/symbol.hpp>
#include <eosiolib/eosio.hpp>
#include <eosiolib/print.hpp>
#include <eosiolib/serialize.hpp>
#include <eosiolib/multi_index.hpp>
//#include <eosio.token/eosio.token.hpp>

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
//const int64_t  useconds_per_day      = 24 * 3600 * int64_t(1000000);

const uint32_t fee_cycle = 30 ;     //计费周期(秒为单位)

const uint32_t one_gb  = 1024*1024*1024;   //1GB
const uint32_t data_slice_size = 16*1024;   // among 4k-32k,set it as 16k

static constexpr eosio::name HDD_OFFICIAL{N(hddofficial)};

static constexpr eosio::name active_permission{N(active)};
static constexpr eosio::name token_account{N(eosio.token)};
static constexpr eosio::name hdd_account{N(hddpool12345)};

const int64_t inc_hdd_amount = 1000000000;


hddpool::hddpool( account_name s)
:contract(s),
 _global(_self, _self)
{
   //print("------hddpool::hddpool-----\n");
   if(_global.exists())
      _gstate = _global.get();
   else 
      _gstate = hdd_global_state{};
}

hddpool::~hddpool()
{
   _global.set(_gstate, _self);
}


hddpool::hdd_global_state hddpool::get_default_param() {
   hdd_global_state dp;
   //print("------get_default_param-----\n");
   dp.hdd_total_balance = 0;
   return dp;
}

void hddpool::getbalance(name user)
{
   //require_auth( user );
   //require_auth(_self);
   //require_auth2(user.value, "active"_n);
   userhdd_index _userhdd(_self, _self);
   auto it = _userhdd.find(user.value);
   if(it == _userhdd.end()){
      _userhdd.emplace(_self, [&](auto &row) {
         row.account_name = user;
         row.hdd_balance = inc_hdd_amount;
         row.hdd_per_cycle_fee = 0;
         row.hdd_per_cycle_profit = 0;
         row.hdd_space = 0;
         row.last_hdd_time = current_time();
      });
      print("{\"balance\":" , inc_hdd_amount, "}");
   }
   else {
      _userhdd.modify(it, _self, [&](auto &row) {
         uint64_t tmp_t = current_time();
         int64_t tmp_last_balance = it->hdd_balance;
         int64_t new_balance;
         if(calculate_balance(tmp_last_balance, it->hdd_per_cycle_fee, it->hdd_per_cycle_profit, it->last_hdd_time, tmp_t, new_balance)) {
            row.hdd_balance = new_balance;
            row.last_hdd_time = tmp_t;      
         }             
         print("{\"balance\":" , it->hdd_balance, "}");
      });
   }   
}

bool hddpool::calculate_balance(int64_t oldbalance, int64_t hdd_per_cycle_fee, int64_t hdd_per_cycle_profit, uint64_t last_hdd_time, uint64_t current_time, int64_t &new_balance) {
   uint64_t slot_t = (current_time - last_hdd_time)/1000000ll;   //convert to seconds
   new_balance = 0;
   //print( "oldbalance: ", oldbalance, "\n" );
   //print("hdd_per_cycle_fee:", hdd_per_cycle_fee, "\n");
   //print( "hdd_per_cycle_profit: ", hdd_per_cycle_profit, "\n" );
   //print( "slot_t: ", slot_t, "\n" );
   //print( "fee_cycle: ", fee_cycle, "\n" );

   //double delta = (int64_t)(((double)slot_t / (double)fee_cycle) * ( (int64_t)hdd_per_cycle_profit - (int64_t)hdd_per_cycle_fee ));
   //print( "delta: ", (int64_t)delta, "\n" );
   double tick = (double)((double)slot_t/fee_cycle);
   //if(tick == 0)
     // return false;
   new_balance = oldbalance;
   int64_t delta = (int64_t)(tick * (hdd_per_cycle_profit - hdd_per_cycle_fee ));
   if(delta < 0 && oldbalance <= -delta)
      new_balance = 0;
   else 
      new_balance += delta;
      
   //print( "new_balance: ", new_balance, "\n" );

   return true;
}

void hddpool::update_hddofficial( userhdd_index& _hbalance, const int64_t _balance,
        const int64_t _fee, const int64_t _profit, 
        const int64_t _space )
{
   auto hbalance_itr = _hbalance.find(HDD_OFFICIAL.value);
   if(hbalance_itr == _hbalance.end()) {
      _hbalance.emplace(_self, [&](auto &row) {
         row.account_name = HDD_OFFICIAL;
         row.hdd_balance = inc_hdd_amount;
         row.hdd_per_cycle_fee = _fee;
         row.hdd_per_cycle_profit = _profit;

         if(_space >= 0)
            row.hdd_space = (uint64_t)_space;
         else
            row.hdd_space = (uint64_t)(-_space);

         row.last_hdd_time = current_time();
      });

   }  else {
      _hbalance.modify(hbalance_itr, _self, [&](auto &row) {
         //todo  check overflow and time cycle
         row.hdd_balance += _balance;
         if(_fee != 0 || _profit != 0) {
            uint64_t tmp_t = current_time();
            int64_t tmp_last_balance = hbalance_itr->hdd_balance;
            int64_t new_balance;
            if(calculate_balance(tmp_last_balance, hbalance_itr->hdd_per_cycle_fee, hbalance_itr->hdd_per_cycle_profit, hbalance_itr->last_hdd_time, tmp_t, new_balance)) {
               row.hdd_balance = new_balance;
               row.last_hdd_time = tmp_t;      
            }
         } 
         row.hdd_per_cycle_fee += _fee;
         row.hdd_per_cycle_profit += _profit;
         if(_space >= 0)
            row.hdd_space += (uint64_t)_space;
         else
            row.hdd_space -= (uint64_t)(-_space);   
      });
   } 
}

void hddpool::buyhdd( name user , asset quant)
{
   require_auth( user );
   
   eosio_assert(is_account(user), "user not a account");
   eosio_assert(is_account(hdd_account), "to not a account");
   eosio_assert(quant.is_valid(), "asset is invalid");
   eosio_assert(quant.symbol == CORE_SYMBOL, "must use YTA to buy HDD");
   eosio_assert(quant.amount > 0, "must transfer positive quantity");
   print( "quant.amount: ", quant.amount , "\n" );

   action(
      permission_level{user, active_permission},
      token_account, N(transfer),
      std::make_tuple(user, hdd_account, quant, std::string("buy hdd"))
   ).send();
   
   int64_t _hdd_amount = quant.amount * 10000;
   userhdd_index _userhdd(_self, _self);
   auto it = _userhdd.find(user.value);
   if(it == _userhdd.end()){
      _userhdd.emplace(_self, [&](auto &row) {
         row.account_name = user;
         row.hdd_balance = _hdd_amount;
         //row.hdd_balance = inc_hdd_amount;
         row.hdd_per_cycle_fee = 0;
         row.hdd_per_cycle_profit = 0;
         row.hdd_space = 0;
         row.last_hdd_time = current_time();
      });
   }
   else {
      _userhdd.modify(it, _self, [&](auto &row) {
         row.hdd_balance += _hdd_amount;
         //row.hdd_balance += inc_hdd_amount;
      });  
   }

   update_hddofficial(_userhdd, _hdd_amount , 0, 0, 0);
   //update_hddofficial(_userhdd, inc_hdd_amount , 0, 0, 0);
}

void hddpool::sellhdd (name user, int64_t amount)
{
   require_auth( user );

   userhdd_index _userhdd(_self, _self);
   auto it = _userhdd.find(user.value);
   eosio_assert( it != _userhdd.end(), "user not exists in userhdd table." );
   eosio_assert( it->hdd_balance >= amount, "hdd overdrawn." );

   _userhdd.modify(it, _self, [&](auto &row) {
        row.hdd_balance -= amount;
        //row.hdd_balance -= inc_hdd_amount;
   });   

   int64_t _yta_amount = (int64_t)((double)amount/10000);
   
   asset quant{_yta_amount, CORE_SYMBOL};
   action(
      permission_level{hdd_account, active_permission},
      token_account, N(transfer),
      std::make_tuple(hdd_account, user, quant, std::string("sell hdd"))
   ).send();
   

   update_hddofficial(_userhdd, -amount , 0, 0, 0);
   //update_hddofficial(_userhdd, -inc_hdd_amount , 0, 0, 0);

}


void hddpool::sethfee( name user, int64_t fee)
{
   eosio_assert(is_account(user), "user invalidate");
   userhdd_index _userhdd(_self, _self);
   auto it = _userhdd.find(user.value);
   eosio_assert( it != _userhdd.end(), "user not exists in userhdd table" );
   eosio_assert(fee != it->hdd_per_cycle_fee, " the fee is the same \n");
   //每周期费用 <= （占用存储空间*数据分片大小/1GB）*（记账周期/ 1年）
   //bool istrue = fee <= (int64_t)(((double)(it->hdd_space * data_slice_size)/(double)one_gb) * ((double)fee_cycle/(double)seconds_in_one_year));
   //eosio_assert(istrue , "the fee verification is not right \n");   
   int64_t delta_fee = 0;
   _userhdd.modify(it, _self, [&](auto &row) {
      //设置每周期费用之前可能需要将以前的余额做一次计算，然后更改last_hdd_time
      uint64_t tmp_t = current_time();
      int64_t tmp_last_balance = it->hdd_balance; 
      int64_t new_balance;
      if(calculate_balance(tmp_last_balance, it->hdd_per_cycle_fee, it->hdd_per_cycle_profit, it->last_hdd_time, tmp_t, new_balance)) {
         row.hdd_balance = new_balance;
         row.last_hdd_time = tmp_t;      
      }         
      delta_fee = fee - it->hdd_per_cycle_fee;
      row.hdd_per_cycle_fee = fee;
    });  
    
    //变更总账户的每周期费用
    update_hddofficial(_userhdd, 0, delta_fee, 0, 0);
}

void hddpool::subbalance ( name user, int64_t balance)
{
   require_auth(user);

   eosio_assert(is_account(user), "user invalidate");
   userhdd_index _userhdd(_self, _self);
   auto it = _userhdd.find(user.value);
   eosio_assert( it != _userhdd.end(), "user not exists in userhdd table" );
    
   _userhdd.modify(it, _self, [&](auto &row) {
        row.hdd_balance -= balance;
   });

   update_hddofficial(_userhdd, -balance, 0, 0, 0);
   
}

void hddpool::addhspace(name user, uint64_t space)
{
   eosio_assert(is_account(user), "user invalidate");
   userhdd_index _userhdd(_self, _self);
   auto it = _userhdd.find(user.value);
   eosio_assert( it != _userhdd.end(), "user not exists in userhdd table" );
    
   _userhdd.modify(it, _self, [&](auto &row) {
        row.hdd_space += space;
   });   

   update_hddofficial(_userhdd, 0, 0, 0, (int64_t)space);
}

void hddpool::subhspace(name user, uint64_t space)
{
   eosio_assert(is_account(user), "user invalidate");
   userhdd_index _userhdd(_self, _self);
   auto it = _userhdd.find(user.value);
   eosio_assert( it != _userhdd.end(), "user not exists in userhdd table" );
    
   _userhdd.modify(it, _self, [&](auto &row) {
        row.hdd_space -= space;
   });

   update_hddofficial(_userhdd, 0, 0, 0, (int64_t)(-space));

}

void hddpool::newmaccount(name owner, uint64_t minerid)
{
   eosio_assert(is_account(owner), "owner invalidate");
   maccount_index _maccount(_self, _self);
    auto it = _maccount.find(owner.value);
    eosio_assert( it == _maccount.end(), "minerid already exist in maccount table \n" );

    _maccount.emplace(_self, [&](auto &row) {
        row.minerid = minerid;
        row.owner = owner;
    });

    userhdd_index _userhdd(_self, _self);
    auto userhdd_itr = _userhdd.find(owner.value);
    if(userhdd_itr == _userhdd.end()) {
      _userhdd.emplace(_self, [&](auto &row) {
         row.account_name = owner;
         row.hdd_balance = inc_hdd_amount;
         row.hdd_per_cycle_fee = 0;
         row.hdd_per_cycle_profit = 0;
         row.hdd_space = 0;
         row.last_hdd_time = current_time();
      });            
    }
}

void hddpool::addmprofit(uint64_t minderid, uint64_t space)
{
   maccount_index _maccount(_self, _self);
   auto it = _maccount.find(minderid);
   eosio_assert( it != _maccount.end(), "minerid not register \n" );
   name owner = it->owner;
   _maccount.modify(it, _self, [&](auto &row) {
        row.space = space;
   });     
   
   //计算owner的每周期收益，计算之间先计算一下该用户上次的HDD余额
   userhdd_index _userhdd(_self, _self);
   auto userhdd_itr = _userhdd.find(owner.value); 
   eosio_assert( userhdd_itr != _userhdd.end(), "no owner exists in userhdd table" );
   int64_t delta_profit = 0;
   _userhdd.modify(userhdd_itr, _self, [&](auto &row) {
      uint64_t tmp_t = current_time();
      int64_t tmp_last_balance = userhdd_itr->hdd_balance;
      int64_t new_balance;
      if(calculate_balance(tmp_last_balance, userhdd_itr->hdd_per_cycle_fee, userhdd_itr->hdd_per_cycle_profit, userhdd_itr->last_hdd_time, tmp_t, new_balance)) {
         row.hdd_balance = new_balance;
         row.last_hdd_time = tmp_t;      
      }           
      int64_t profit = 0;
      //每周期收益 += (生产空间*数据分片大小/1GB）*（记账周期/ 1年）
      profit = (int64_t)(((double)(space*data_slice_size)/(double)one_gb) * ((double)fee_cycle / (double)seconds_in_one_year));
      delta_profit = profit - userhdd_itr->hdd_per_cycle_profit;
      row.hdd_per_cycle_profit = profit;
    });     

   //变更总账户的每周期费用
   update_hddofficial(_userhdd, 0, 0, delta_profit, 0);
}

void hddpool::clearall()
{
   userhdd_index _userhdd( _self , _self );
   while (_userhdd.begin() != _userhdd.end())
      _userhdd.erase(_userhdd.begin());   

   maccount_index _maccount( _self , _self );
   while (_maccount.begin() != _maccount.end())
      _maccount.erase(_maccount.begin());   
   
   //_gstate.hdd_total_balance += 100000;
}

bool hddpool::is_bp_account(uint64_t uservalue)
{
   account_name producers[21];
   uint32_t bytes_populated = get_active_producers(producers, sizeof(account_name)*21);
   uint32_t count = bytes_populated/sizeof(account_name);
   for(uint32_t i = 0 ; i < count; i++ ) {
      if(producers[i] == uservalue)
         return true;
   }
   return false;
}

EOSIO_ABI( hddpool, (getbalance)(buyhdd)(sellhdd)(sethfee)(subbalance)(addhspace)(subhspace)(newmaccount)(addmprofit)(clearall))
