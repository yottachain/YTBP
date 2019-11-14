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
const uint64_t max_userspace = 64 * 1024 * 1024 * uint64_t(1024 * 50);  //50P 最大用户存储空间量
const uint64_t max_minerspace = 64 * 1024 * uint64_t(1024 * 100); //100T 单个矿机最大的采购空间量
//const uint32_t per_profit_space = 64 * 1024; //1G //每次增加的采购空间容量


static constexpr eosio::name active_permission{N(active)};
static constexpr eosio::name token_account{N(eosio.token)};
static constexpr eosio::name hdd_account{N(hddpool12345)};
static constexpr eosio::name hdd_deposit{N(hdddeposit12)};

static constexpr int64_t max_hdd_amount    = (1LL << 62) - 1;
bool is_hdd_amount_within_range(int64_t amount) { return -max_hdd_amount <= amount && amount <= max_hdd_amount; }

#define HDD_SYMBOL_BANCOR S(4, HDD)
#define HDDCORE_SYMBOL_BANCOR S(4, HDDCORE)

const int64_t inc_hdd_amount = 0;//1000000000;

//const int64_t price_delta = 1;

hddpool::hddpool(account_name s)
    : contract(s),
      _global(_self, _self),
      _global2(_self, _self),
      _global3(_self, _self),
      _ghddprice(_self, _self),
      _hmarket(_self, _self)
{

   if (_global.exists())
      _gstate = _global.get();
   else
      _gstate = hdd_global_state{};

   if (_global2.exists())
      _gstate2 = _global2.get();
   else
      _gstate2 = hdd_global_state2{};

   if (_global3.exists())
      _gstate3 = _global3.get();
   else
      _gstate3 = hdd_global_state3{};

   if (_ghddprice.exists())
      _ghddpriceState = _ghddprice.get();
   else
      _ghddpriceState = hdd_global_price{};

   auto itr = _hmarket.find(HDDCORE_SYMBOL_BANCOR);

   if (itr == _hmarket.end())
   {
      auto system_token_supply = eosio::token(N(eosio.token)).get_supply(eosio::symbol_type(CORE_SYMBOL).name()).amount;
      if (system_token_supply > 0)
      {
         itr = _hmarket.emplace(_self, [&](auto &m) {
            m.supply.amount = 100000000000000ll;
            m.supply.symbol = HDDCORE_SYMBOL_BANCOR;
            m.base.balance.amount = 40000000000000ll / 10;
            m.base.weight = 0.35;
            m.base.balance.symbol = HDD_SYMBOL_BANCOR;
            ;
            m.quote.balance.amount = system_token_supply / 10;
            m.quote.balance.symbol = CORE_SYMBOL;
            m.quote.weight = 0.5;
         });
      }
   }
}

hddpool::~hddpool()
{
   _global.set(_gstate, _self);
   _global2.set(_gstate2, _self);
   _global3.set(_gstate3, _self);
   _ghddprice.set(_ghddpriceState, _self);
}

void hddpool::new_user_hdd(userhdd_index& userhdd, name user, int64_t balance, account_name payer)
{
      userhdd.emplace(payer, [&](auto &row) {
         row.account_name = user;
         row.hdd_balance = balance;
         row.hdd_per_cycle_fee = 0;
         row.hdd_per_cycle_profit = 0;
         row.hdd_space = 0;
         row.last_hdd_time = current_time();

         _gstate2.hdd_total_user += 1;
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
      check_bp_account(caller.value, 0, false);
   } else {
      require_auth( _self );
   }

   userhdd_index _userhdd(_self, user.value);
   auto it = _userhdd.find(user.value);
   if (it == _userhdd.end())
   {
      new_user_hdd(_userhdd, user, inc_hdd_amount, payer);
      print("{\"balance\":", inc_hdd_amount, "}");
   }
   else
   {
      _userhdd.modify(it, _self, [&](auto &row) {
         uint64_t tmp_t = current_time();
         int64_t tmp_last_balance = it->hdd_balance;
         int64_t new_balance;
         if (calculate_balance(tmp_last_balance, it->hdd_per_cycle_fee, it->hdd_per_cycle_profit, it->last_hdd_time, tmp_t, new_balance))
         {
            row.hdd_balance = new_balance;
            row.last_hdd_time = tmp_t;
         }
         
         /* 
         //--- 去掉遍历该账户下所有矿机并计算收益的操作，以避免事务超过最大CPU时限。外部系统需要自行调用相关的action分别计算某个矿机的收益
         //计算该账户下所有矿机的收益
         maccount_index _maccount(_self, user.value);
         for (auto it_m = _maccount.begin(); it_m != _maccount.end(); it_m++)
         {
            int64_t balance_delta_m = 0;
            _maccount.modify(it_m, _self, [&](auto &row_m) {
               uint64_t tmp_t_m = current_time();
               int64_t tmp_last_balance_m = it_m->hdd_balance;
               int64_t new_balance_m = 0;
               if (calculate_balance(tmp_last_balance_m, 0, it_m->hdd_per_cycle_profit, it_m->last_hdd_time, tmp_t_m, new_balance_m))
               {
                  eosio_assert(is_hdd_amount_within_range(new_balance_m), "magnitude of miner hddbalance must be less than 2^62");
                  balance_delta_m = new_balance_m - row_m.hdd_balance;
                  row_m.hdd_balance = new_balance_m;
                  row_m.last_hdd_time = tmp_t;
               }
            });
            row.hdd_balance += balance_delta_m;
            eosio_assert(is_hdd_amount_within_range(row.hdd_balance), "magnitude of user hddbalance must be less than 2^62");

         }
         */
         print("{\"balance\":", it->hdd_balance, "}");
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
   //avoid under zero
   if (delta < 0 && oldbalance <= -delta) {
      if(oldbalance < 0) {
         delta = 0;
      } else {
         delta = -oldbalance;
      }
   }
      
   new_balance += delta;

   update_total_hdd_balance(delta);

   return true;
}

void hddpool::update_total_hdd_balance(int64_t _balance_delta)
{
   _gstate.hdd_total_balance += _balance_delta;

   if (_gstate.hdd_total_balance < 0)
      _gstate.hdd_total_balance = 0;

   eosio_assert(is_hdd_amount_within_range(_gstate.hdd_total_balance), "magnitude of hdd_total_balance must be less than 2^62");
}

void hddpool::buyhdd(name from, name receiver, asset quant)
{
   require_auth(from);

   eosio_assert(is_account(from), "user not a account");
   eosio_assert(is_account(receiver), "receiver not a account");
   eosio_assert(is_account(hdd_account), "to not a account");
   eosio_assert(quant.is_valid(), "asset is invalid");
   eosio_assert(quant.symbol == CORE_SYMBOL, "must use core asset to buy HDD");
   eosio_assert(quant.amount > 0, "must transfer positive quantity");
   //print( "quant.amount: ", quant.amount , "\n" );

   action(
       permission_level{from, active_permission},
       token_account, N(transfer),
       std::make_tuple(from, hdd_account, quant, std::string("buy hdd")))
       .send();

   int64_t _hdd_amount = 0;   
   const auto &market = _hmarket.get(HDDCORE_SYMBOL_BANCOR, "hdd market does not exist");
   _hmarket.modify(market, 0, [&](auto &es) {
      _hdd_amount = (es.convert(quant, HDD_SYMBOL_BANCOR).amount) * 10000;
   });
   //print("_hdd_amount:  ", _hdd_amount, "\n");
   //_ghddpriceState.price = (quant.amount * 100 ) / (_hdd_amount/10000);
   eosio_assert(is_hdd_amount_within_range(_hdd_amount), "magnitude of _hdd_amount must be less than 2^62");
   eosio_assert(is_hdd_amount_within_range(quant.amount * 10000), "magnitude of [quant.amount * 10000] must be less than 2^62");

   _ghddpriceState.price = (int64_t)( ( (double)(quant.amount * 10000) / _hdd_amount ) * 100000000);
   print("_hdd_amount:  ", _hdd_amount, "  price: ", _ghddpriceState.price ,"\n");

   userhdd_index _userhdd(_self, receiver.value);
   auto it = _userhdd.find(receiver.value);
   if (it == _userhdd.end())
   {
      new_user_hdd(_userhdd, receiver, _hdd_amount, from);
   }
   else
   {
      _userhdd.modify(it, _self, [&](auto &row) {
         row.hdd_balance += _hdd_amount;
         eosio_assert(is_hdd_amount_within_range(row.hdd_balance), "magnitude of user hddbalance must be less than 2^62");      
      });
   }

   update_total_hdd_balance(_hdd_amount);
}

void hddpool::sellhdd(name user, int64_t amount)
{
   require_auth(user);

   eosio_assert(is_hdd_amount_within_range(amount), "magnitude of user hdd amount must be less than 2^62");      

   eosio_assert( amount > 0, "cannot sell negative hdd amount" );

   userhdd_index _userhdd(_self, user.value);
   auto it = _userhdd.find(user.value);
   eosio_assert(it != _userhdd.end(), "user not exists in userhdd table.");
   eosio_assert(it->hdd_balance >= amount, "hdd overdrawn.");

   _userhdd.modify(it, _self, [&](auto &row) {
      row.hdd_balance -= amount;
      eosio_assert(is_hdd_amount_within_range(row.hdd_balance), "magnitude of user hddbalance must be less than 2^62");      
   });

   int64_t _yta_amount = 0;
   const auto &market = _hmarket.get(HDDCORE_SYMBOL_BANCOR, "hdd market does not exist");
   _hmarket.modify(market, 0, [&](auto &es) {
      /// the cast to int64_t of quant is safe because we certify quant is <= quota which is limited by prior purchases
      _yta_amount = es.convert(asset(amount / 10000, HDD_SYMBOL_BANCOR), CORE_SYMBOL).amount;
   });
   _ghddpriceState.price = (int64_t) (((double)(_yta_amount * 10000 ) / amount ) * 100000000);
   print("_yta_amount:  ", _yta_amount, "  price: ", _ghddpriceState.price ,"\n");

   asset quant{_yta_amount, CORE_SYMBOL};
   action(
       permission_level{hdd_account, active_permission},
       token_account, N(transfer),
       std::make_tuple(hdd_account, user, quant, std::string("sell hdd")))
       .send();

   update_total_hdd_balance(-amount);
}


void hddpool::sethfee(name user, int64_t fee, name caller, uint64_t userid)
{
   eosio_assert(is_account(user), "user invalidate");
   eosio_assert(is_account(caller), "caller not an account.");
   eosio_assert( fee >= 0, "must use positive fee value" );


   userhdd_index _userhdd(_self, user.value);
   auto it = _userhdd.find(user.value);
   eosio_assert(it != _userhdd.end(), "user not exists in userhdd table");
   eosio_assert(fee != it->hdd_per_cycle_fee, " the fee is the same \n");

   check_userid(user.value, userid);
   check_bp_account(caller.value, userid, true);

   eosio_assert(is_hdd_amount_within_range(fee), "magnitude of fee must be less than 2^62");      


   //每周期费用 <= （占用存储空间*数据分片大小/1GB）*（记账周期/ 1年）
   bool istrue = fee <= (int64_t)(((double)(it->hdd_space * data_slice_size)/(double)one_gb) * ((double)fee_cycle/(double)seconds_in_one_year) * 100000000);
   eosio_assert(istrue , "the fee verification is not right \n");
   _userhdd.modify(it, _self, [&](auto &row) {
      //设置每周期费用之前可能需要将以前的余额做一次计算，然后更改last_hdd_time
      uint64_t tmp_t = current_time();
      int64_t tmp_last_balance = it->hdd_balance;
      int64_t new_balance;
      if (calculate_balance(tmp_last_balance, it->hdd_per_cycle_fee, it->hdd_per_cycle_profit, it->last_hdd_time, tmp_t, new_balance))
      {
         row.hdd_balance = new_balance;
         row.last_hdd_time = tmp_t;
      }
      row.hdd_per_cycle_fee = fee;
   });

}

void hddpool::subbalance(name user, int64_t balance, uint64_t userid, uint8_t acc_type, name caller)
{
   eosio_assert(is_account(user), "user invalidate");

   if(acc_type == 1) {
      require_auth( user );
   }
   else if(acc_type == 2) {
      eosio_assert(is_account(caller), "caller not a account.");
      check_bp_account(caller.value, userid, false);

   } else {
      require_auth( _self );
   }

   eosio_assert(is_hdd_amount_within_range(balance), "magnitude of hddbalance must be less than 2^62");  
   eosio_assert( balance >= 0, "must use positive balance value" );


   userhdd_index _userhdd(_self, user.value);
   auto it = _userhdd.find(user.value);
   eosio_assert(it != _userhdd.end(), "user not exists in userhdd table");

   check_userid(user.value, userid);

   _userhdd.modify(it, _self, [&](auto &row) {
      row.hdd_balance -= balance;
      eosio_assert(is_hdd_amount_within_range(row.hdd_balance), "magnitude of user hddbalance must be less than 2^62");
   });   

   update_total_hdd_balance(-balance);
}

void hddpool::addhspace(name user, uint64_t space, name caller, uint64_t userid)
{
   eosio_assert(is_account(user), "user invalidate");
   eosio_assert(is_account(caller), "caller not an account.");

   userhdd_index _userhdd(_self, user.value);
   auto it = _userhdd.find(user.value);
   eosio_assert(it != _userhdd.end(), "user not exists in userhdd table");

   check_userid(user.value, userid);
   check_bp_account(caller.value, userid, true);

   _userhdd.modify(it, _self, [&](auto &row) {
      row.hdd_space += space;
      eosio_assert(row.hdd_space <= max_userspace , "overflow max_userspace");
   });

}

void hddpool::subhspace(name user, uint64_t space, name caller, uint64_t userid)
{
   eosio_assert(is_account(user), "user invalidate");
   eosio_assert(is_account(caller), "caller not an account.");

   userhdd_index _userhdd(_self, user.value);
   auto it = _userhdd.find(user.value);
   eosio_assert(it != _userhdd.end(), "user not exists in userhdd table");

   check_userid(user.value, userid);
   check_bp_account(caller.value, userid, true);

   _userhdd.modify(it, _self, [&](auto &row) {
      //eosio_assert(row.hdd_space >= space , "overdraw user hdd_space");
      if(row.hdd_space >= space)
         row.hdd_space -= space;
      else 
         row.hdd_space = 0;
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

   int64_t profit_delta = 0;
   //每周期收益 += (生产空间*数据分片大小/1GB）*（记账周期/ 1年）
   profit_delta = (int64_t)(((double)(space * data_slice_size) / (double)one_gb) * ((double)fee_cycle / (double)seconds_in_one_year) * 100000000);

   int64_t balance_delta = 0;

   _maccount.modify(it, _self, [&](auto &row) {
      uint64_t tmp_t = current_time();
      int64_t tmp_last_balance = it->hdd_balance;
      int64_t new_balance;
      if (calculate_balance(tmp_last_balance, 0, it->hdd_per_cycle_profit, it->last_hdd_time, tmp_t, new_balance))
      {
         eosio_assert(is_hdd_amount_within_range(new_balance), "magnitude of miner hddbalance must be less than 2^62");
         balance_delta = new_balance - row.hdd_balance;
         row.hdd_balance = new_balance;
         row.last_hdd_time = tmp_t;
      }

      row.space += space;
      row.hdd_per_cycle_profit += profit_delta;
   });

   userhdd_index _userhdd(_self, owner.value);
   auto userhdd_itr = _userhdd.find(owner.value);
   eosio_assert(userhdd_itr != _userhdd.end(), "no owner exists in userhdd table");
   _userhdd.modify(userhdd_itr, _self, [&](auto &row) {
      row.hdd_balance += balance_delta;
      eosio_assert(is_hdd_amount_within_range(row.hdd_balance), "magnitude of user hddbalance must be less than 2^62");
      row.hdd_per_cycle_profit = 0;
   });

}

void hddpool::calcmbalance(name owner, uint64_t minerid)
{
   require_auth(owner);

   maccount_index _maccount(_self, owner.value);
   auto it = _maccount.find(minerid);
   eosio_assert(it != _maccount.end(), "minerid not register \n");

   int64_t balance_delta = 0;

   _maccount.modify(it, _self, [&](auto &row) {
      uint64_t tmp_t = current_time();
      int64_t tmp_last_balance = it->hdd_balance;
      int64_t new_balance;
      if (calculate_balance(tmp_last_balance, 0, it->hdd_per_cycle_profit, it->last_hdd_time, tmp_t, new_balance))
      {
         eosio_assert(is_hdd_amount_within_range(new_balance), "magnitude of miner hddbalance must be less than 2^62");
         balance_delta = new_balance - row.hdd_balance;
         row.hdd_balance = new_balance;
         row.last_hdd_time = tmp_t;
      }
   });

   //userhdd_index _userhdd(_self, _self);
   userhdd_index _userhdd(_self, owner.value);
   auto userhdd_itr = _userhdd.find(owner.value);
   eosio_assert(userhdd_itr != _userhdd.end(), "no owner exists in userhdd table");
   _userhdd.modify(userhdd_itr, _self, [&](auto &row) {
      row.hdd_balance += balance_delta;
      eosio_assert(is_hdd_amount_within_range(row.hdd_balance), "magnitude of user hddbalance must be less than 2^62");
      row.hdd_per_cycle_profit = 0;
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
      auto itmaccount = _maccount.find(itminerinfo->owner.value);
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

   int64_t balance_delta = 0;
   _maccount.modify(it, _self, [&](auto &row) {
      uint64_t tmp_t = current_time();
      int64_t tmp_last_balance = it->hdd_balance;
      int64_t new_balance;
      if (calculate_balance(tmp_last_balance, 0, it->hdd_per_cycle_profit, it->last_hdd_time, tmp_t, new_balance))
      {
         eosio_assert(is_hdd_amount_within_range(new_balance), "magnitude of miner hddbalance must be less than 2^62");
         balance_delta = new_balance - row.hdd_balance;
         row.hdd_balance = new_balance;
         row.last_hdd_time = tmp_t;
      }
      row.hdd_per_cycle_profit = 0;
   });

   userhdd_index _userhdd(_self, owner.value);
   auto userhdd_itr = _userhdd.find(owner.value);
   eosio_assert(userhdd_itr != _userhdd.end(), "no owner exists in userhdd table");
   _userhdd.modify(userhdd_itr, _self, [&](auto &row) {
      row.hdd_balance += balance_delta;
      eosio_assert(is_hdd_amount_within_range(row.hdd_balance), "magnitude of user hddbalance must be less than 2^62");
      row.hdd_per_cycle_profit = 0;
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

   _maccount.modify(it, _self, [&](auto &row) {
      uint64_t tmp_t = current_time();
      row.hdd_per_cycle_profit = profit;
      row.last_hdd_time = tmp_t;
   });
}

void hddpool::newminer(uint64_t minerid, name adminacc, name dep_acc, asset dep_amount)
{
   require_auth(dep_acc);

   eosio_assert(is_account(adminacc), "adminacc invalidate");
   eosio_assert(is_account(dep_acc), "dep_acc invalidate");
   eosio_assert( dep_amount.amount > 0, "must use positive dep_amount" );


   minerinfo_table _minerinfo( _self , _self );
   auto itminerinfo = _minerinfo.find(minerid);
   eosio_assert(itminerinfo == _minerinfo.end(), "miner already registered \n");  

   action(
       permission_level{dep_acc, active_permission},
       hdd_deposit, N(paydeposit),
       std::make_tuple(dep_acc, minerid, dep_amount))
       .send(); 

   //_minerinfo.emplace(_self, [&](auto &row) {
   _minerinfo.emplace(dep_acc.value, [&](auto &row) {      
      row.minerid    = minerid;
      row.admin      = adminacc;
      row.max_space  = 0;
      row.space_left = 0;
   });       
}

void hddpool::delstrpool(name poolid)
{
   require_auth(_self);

   storepool_index _storepool( _self , _self );
   auto itmstorepool = _storepool.find(poolid.value);
   if(itmstorepool != _storepool.end()) {
      eosio_assert(itmstorepool->max_space == itmstorepool->space_left == 0,  "can not delete this storepool.");
      _storepool.erase(itmstorepool);
   }

   /* 
   while (_storepool.begin() != _storepool.end()) {
      _storepool.erase(_storepool.begin());      

   }  */  
}

void hddpool::regstrpool(name pool_id, name pool_owner, uint64_t max_space)
{
   ((void)max_space);

   eosio_assert(is_account(pool_owner), "pool_owner invalidate");

   //require_auth(_self);
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
   //require_auth(_self);  
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

   require_auth(itminerinfo->admin);

   eosio_assert(itminerinfo->pool_id.value == 0, "miner already join to a pool(@@err:alreadyinpool@@)\n");  

   eosio_assert(max_space <= max_minerspace, "miner max_space overflow\n");  

   /* 
   if(itminerinfo->pool_id.value != 0) {
      if(itminerinfo->pool_id.value != pool_id.value || itminerinfo->max_space != max_space) {
         eosio_assert(false, "miner already join to another pool\n");
      }
   }
   */

   eosio_assert((itstorepool->space_left > 0 && itstorepool->space_left > max_space),"pool space not enough");

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
      _gstate3.hdd_macc_user += 1;
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
      new_user_hdd(_userhdd, minerowner, inc_hdd_amount, _self);
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

}


void hddpool::mchgspace(uint64_t minerid, uint64_t max_space)
{
   minerinfo_table _minerinfo( _self , _self );
   auto itminerinfo = _minerinfo.find(minerid);
   eosio_assert(itminerinfo != _minerinfo.end(), "miner not registered \n");  

   require_auth(itminerinfo->admin);

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

   _minerinfo.modify(itminerinfo, _self, [&](auto &rowminer) {
      //如果每周期收益计算是挂在收益账户名下的时候，需要考虑更该新旧收益账号的每周期收益字段
      //目前是挂在收益计算是挂在矿机名下，所以不需要
      maccount_index _maccount_old(_self, rowminer.owner.value);
      auto itmaccount_old = _maccount_old.find(minerid);
      eosio_assert(itmaccount_old != _maccount_old.end(), "miner was not complete registration");

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

      //变更矿机表的收益账户名称
      rowminer.owner = new_owneracc;

   });
}



void hddpool::check_userid(uint64_t namevalue, uint64_t userid)
{
   userhdd2_index _userhdd2(_self, _self);
   auto it = _userhdd2.find(namevalue);
   if(it != _userhdd2.end()) {
      eosio_assert(it->userid == userid, "invalidate userid");      
   } else {
      _userhdd2.emplace(_self, [&](auto &row) {
         row.account_name = name{namevalue};
         row.userid = userid;
      });
   }
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


asset exchange_state::convert_to_exchange(connector &c, asset in)
{
   real_type R(supply.amount);
   real_type C(c.balance.amount + in.amount);
   real_type F(c.weight / 1000.0);
   real_type T(in.amount);
   real_type ONE(1.0);

   real_type E = -R * (ONE - std::pow(ONE + T / C, F));
   //print( "E: ", E, "\n");
   int64_t issued = int64_t(E);

   supply.amount += issued;
   c.balance.amount += in.amount;

   return asset(issued, supply.symbol);
}

asset exchange_state::convert_from_exchange(connector &c, asset in)
{
   eosio_assert(in.symbol == supply.symbol, "unexpected asset symbol input");

   real_type R(supply.amount - in.amount);
   real_type C(c.balance.amount);
   real_type F(1000.0 / c.weight);
   real_type E(in.amount);
   real_type ONE(1.0);

   // potentially more accurate:
   // The functions std::expm1 and std::log1p are useful for financial calculations, for example,
   // when calculating small daily interest rates: (1+x)n
   // -1 can be expressed as std::expm1(n * std::log1p(x)).
   // real_type T = C * std::expm1( F * std::log1p(E/R) );

   real_type T = C * (std::pow(ONE + E / R, F) - ONE);
   //print( "T: ", T, "\n");
   int64_t out = int64_t(T);

   supply.amount -= in.amount;
   c.balance.amount -= out;

   return asset(out, c.balance.symbol);
}

asset exchange_state::convert(asset from, symbol_type to)
{
   auto sell_symbol = from.symbol;
   auto ex_symbol = supply.symbol;
   auto base_symbol = base.balance.symbol;
   auto quote_symbol = quote.balance.symbol;

   //print( "From: ", from, " TO ", asset( 0,to), "\n" );
   //print( "base: ", base_symbol, "\n" );
   //print( "quote: ", quote_symbol, "\n" );
   //print( "ex: ", supply.symbol, "\n" );

   if (sell_symbol != ex_symbol)
   {
      if (sell_symbol == base_symbol)
      {
         from = convert_to_exchange(base, from);
      }
      else if (sell_symbol == quote_symbol)
      {
         from = convert_to_exchange(quote, from);
      }
      else
      {
         eosio_assert(false, "invalid sell");
      }
   }
   else
   {
      if (to == base_symbol)
      {
         from = convert_from_exchange(base, from);
      }
      else if (to == quote_symbol)
      {
         from = convert_from_exchange(quote, from);
      }
      else
      {
         eosio_assert(false, "invalid conversion");
      }
   }

   if (to != from.symbol)
      return convert(from, to);

   return from;
}

EOSIO_ABI(hddpool, (getbalance)(buyhdd)(sellhdd)(sethfee)(subbalance)(addhspace)(subhspace)(addmprofit)(delminer)
                  (calcmbalance)(delstrpool)(regstrpool)(chgpoolspace)(newminer)(addm2pool)
                  (mchgspace)(mchgstrpool)(mchgadminacc)(mchgowneracc)
                  (mdeactive)(mactive))
