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

static constexpr eosio::name active_permission{N(active)};
static constexpr eosio::name token_account{N(eosio.token)};
static constexpr eosio::name hdd_account{N(hddpool12345)};
static constexpr eosio::name hdd_deposit{N(hdddeposit12)};


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

void hddpool::new_user_hdd(userhdd_index& userhdd, name user, int64_t balance)
{
      userhdd.emplace(_self, [&](auto &row) {
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
   if(acc_type == 1) {
      eosio_assert(is_account(user), "user not a account.");
      require_auth( user );
   }
   else if(acc_type == 2) {
      eosio_assert(is_account(caller), "caller not a account.");
      check_bp_account(caller.value, 0, false);

   } else {
      require_auth( _self );
   }

   eosio_assert(is_account(user), "user not a account.");

   userhdd_index _userhdd(_self, user.value);
   auto it = _userhdd.find(user.value);
   if (it == _userhdd.end())
   {
      new_user_hdd(_userhdd, user, inc_hdd_amount);
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
                  balance_delta_m = new_balance_m - row_m.hdd_balance;
                  row_m.hdd_balance = new_balance_m;
                  row_m.last_hdd_time = tmp_t;
               }
            });
            row.hdd_balance += balance_delta_m;
         }
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

   int64_t _hdd_amount = quant.amount * 10000;
   const auto &market = _hmarket.get(HDDCORE_SYMBOL_BANCOR, "hdd market does not exist");
   _hmarket.modify(market, 0, [&](auto &es) {
      _hdd_amount = (es.convert(quant, HDD_SYMBOL_BANCOR).amount) * 10000;
   });
   //print("_hdd_amount:  ", _hdd_amount, "\n");
   //_ghddpriceState.price = (quant.amount * 100 ) / (_hdd_amount/10000);
   _ghddpriceState.price = (int64_t)(((double)(quant.amount * 10000) / _hdd_amount ) * 100000000);
   print("_hdd_amount:  ", _hdd_amount, "  price: ", _ghddpriceState.price ,"\n");

   userhdd_index _userhdd(_self, receiver.value);
   auto it = _userhdd.find(receiver.value);
   if (it == _userhdd.end())
   {
      new_user_hdd(_userhdd, receiver, _hdd_amount);
   }
   else
   {
      _userhdd.modify(it, _self, [&](auto &row) {
         row.hdd_balance += _hdd_amount;
      });
   }

   update_total_hdd_balance(_hdd_amount);
}

void hddpool::sellhdd(name user, int64_t amount)
{
   require_auth(user);

   userhdd_index _userhdd(_self, user.value);
   auto it = _userhdd.find(user.value);
   eosio_assert(it != _userhdd.end(), "user not exists in userhdd table.");
   eosio_assert(it->hdd_balance >= amount, "hdd overdrawn.");

   _userhdd.modify(it, _self, [&](auto &row) {
      row.hdd_balance -= amount;
   });

   int64_t _yta_amount = (int64_t)((double)amount / 10000);
   auto itr = _hmarket.find(HDDCORE_SYMBOL_BANCOR);
   _hmarket.modify(itr, 0, [&](auto &es) {
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

   userhdd_index _userhdd(_self, user.value);
   auto it = _userhdd.find(user.value);
   eosio_assert(it != _userhdd.end(), "user not exists in userhdd table");
   eosio_assert(fee != it->hdd_per_cycle_fee, " the fee is the same \n");

   check_userid(user.value, userid);
   check_bp_account(caller.value, userid, true);

   //每周期费用 <= （占用存储空间*数据分片大小/1GB）*（记账周期/ 1年）
   //bool istrue = fee <= (int64_t)(((double)(it->hdd_space * data_slice_size)/(double)one_gb) * ((double)fee_cycle/(double)seconds_in_one_year));
   //eosio_assert(istrue , "the fee verification is not right \n");
   int64_t delta_fee = 0;
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
      delta_fee = fee - it->hdd_per_cycle_fee;
      row.hdd_per_cycle_fee = fee;
   });

}

void hddpool::subbalance(name user, int64_t balance, uint64_t userid)
{
   require_auth(user);

   eosio_assert(is_account(user), "user invalidate");

   userhdd_index _userhdd(_self, user.value);
   auto it = _userhdd.find(user.value);
   eosio_assert(it != _userhdd.end(), "user not exists in userhdd table");

   check_userid(user.value, userid);

   _userhdd.modify(it, _self, [&](auto &row) {
      row.hdd_balance -= balance;
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
      row.hdd_space -= space;
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
   if(itminerinfo != _minerinfo.end()) {
      eosio_assert(itminerinfo->space_left >= space, "exceed max space");
      _minerinfo.modify(itminerinfo, _self, [&](auto &row) {
         row.space_left -= space;
      });         
   }
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
       permission_level{hdd_deposit, active_permission},
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

   minerinfo_table _minerinfo( _self , _self );
   auto itminerinfo = _minerinfo.find(minerid);
   eosio_assert(itminerinfo == _minerinfo.end(), "miner already registered \n");  

   action(
       permission_level{dep_acc, active_permission},
       hdd_deposit, N(paydeposit),
       std::make_tuple(dep_acc, minerid, dep_amount))
       .send(); 

   _minerinfo.emplace(_self, [&](auto &row) {
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
      eosio_assert(space_used <= max_space, "invalid max_spave");
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
      new_user_hdd(_userhdd, minerowner, inc_hdd_amount);
   }
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
    //require_auth(shadow);
    require_auth(bpacc);
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
                  (mdeactive)(mactive))
