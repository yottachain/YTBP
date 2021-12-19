#include "hdddeposit.hpp"
#include <eosiolib/action.hpp>
#include <eosiolib/chain.h>
#include <eosiolib/symbol.hpp>
#include <eosiolib/eosio.hpp>
#include <eosiolib/print.hpp>
#include <eosiolib/serialize.hpp>
#include <eosiolib/multi_index.hpp>
#include <eosio.token/eosio.token.hpp>
#include <eosio.system/eosio.system.hpp>
#include <hddlock/hddlock.hpp>
#include <hddpool/hddpool.hpp>

using namespace eosio;

static constexpr eosio::name active_permission{N(active)};
static constexpr eosio::name token_account{N(eosio.token)};
static constexpr eosio::name system_account{N(eosio)};
static constexpr eosio::name hdd_lock_account{N(hddlock12345)};

const uint32_t min_miner_space = 100 * (1024 * 64); //100GB，以16k分片为单位

void hdddeposit::paydeppool(account_name user, asset quant) {
    require_auth(user);

    account_name payer = user;

    bool is_frozen = hddlock(hdd_lock_account).is_frozen(user);  
    eosio_assert( !is_frozen, "frozen user can not create deposit pool" );

    eosio_assert(is_account(user), "user is not an account.");
    eosio_assert(quant.symbol == CORE_SYMBOL, "must use core asset for hdd deposit.");
    eosio_assert(quant.amount > 0, "must use positive quant" );

    //check if user has enough YTA balance for deposit
    auto balance   = eosio::token(N(eosio.token)).get_balance( user , quant.symbol.name() );
    asset real_balance = balance;
    depositpool_table _deposit(_self, user);
    auto it = _deposit.find( user );    
    if ( it != _deposit.end() ) {
        real_balance.amount -= it->deposit_total.amount;
    }
    depositpool2_table _deposit2(_self, user);
    auto it2 = _deposit2.find( user );    
    if ( it2 != _deposit2.end() ) {
        real_balance.amount -= it2->deposit_total.amount;
    }
    eosio_assert( real_balance.amount >= quant.amount, "user balance not enough." );

    //insert or update accdeposit table
    if ( it == _deposit.end() ) {
        //_deposit.emplace( _self, [&]( auto& a ){
        _deposit.emplace( payer, [&]( auto& a ){
            a.account_name = name{user};
            a.pool_type = 0;
            a.deposit_total = quant;
            a.deposit_free = quant;
            a.deposit_his = quant;
        });
    } else {
        _deposit.modify( it, 0, [&]( auto& a ) {
            a.deposit_total += quant;
            a.deposit_free += quant;
            a.deposit_his += quant;
        });
    }

    if( eosiosystem::isActiveVoter(user) ) {
        action(
            permission_level{user, active_permission},
            system_account, N(changevotes),
            std::make_tuple(user)).send();
    }    
}

void hdddeposit::unpaydeppool(account_name user, asset quant) {
    require_auth(user);

    eosio_assert(quant.symbol == CORE_SYMBOL, "must use core asset for hdd deposit.");
    eosio_assert(quant.amount > 0, "must use positive quant");

    bool is_frozen = hddlock(hdd_lock_account).is_frozen(user);  
    eosio_assert( !is_frozen, "user is frozen" );

    depositpool_table _deposit(_self, user);
    const auto& it = _deposit.get( user, "no deposit pool record for this user.");

    eosio_assert( it.deposit_free.amount >= quant.amount, "free deposit not enough." );
    eosio_assert( it.deposit_total.amount >= quant.amount, "deposit not enough." );
    eosio_assert( it.deposit_his.amount >= quant.amount, "deposit not enough." );

    _deposit.modify( it, 0, [&]( auto& a ) {
        a.deposit_free -= quant;
        a.deposit_total -= quant;
        a.deposit_his -= quant;

    });

    if( eosiosystem::isActiveVoter(user) ) {
        action(
            permission_level{user, active_permission},
            system_account, N(changevotes),
            std::make_tuple(user)).send();
    }    
}

void hdddeposit::paydeppool2(account_name user, asset quant) {
    require_auth(N(channel.sys));

    action(
       permission_level{N(channel.sys), active_permission},
       token_account, N(transfer),
       std::make_tuple(N(channel.sys), user, quant, std::string("paydeppool2")))
       .send();

    account_name payer = _self;

    bool is_frozen = hddlock(hdd_lock_account).is_frozen(user);  
    eosio_assert( !is_frozen, "frozen user can not create deposit pool2" );

    auto lock_asset = hddlock(hdd_lock_account).get_lock_asset(user);
    eosio_assert( lock_asset.amount == 0, "user has lock asset" );

    eosio_assert(is_account(user), "user is not an account.");
    eosio_assert(quant.symbol == CORE_SYMBOL, "must use core asset for hdd deposit.");
    eosio_assert(quant.amount > 0, "must use positive quant" );

    //insert or update accdeposit table
    depositpool2_table _deposit2(_self, user);
    auto it2 = _deposit2.find( user );    
    if ( it2 == _deposit2.end() ) {
        //_deposit.emplace( _self, [&]( auto& a ){
        _deposit2.emplace( payer, [&]( auto& a ){
            a.account_name = name{user};
            a.pool_type = 0;
            a.deposit_total = quant;
            a.deposit_free = quant;
            a.deposit_his = quant;
        });
    } else {
        _deposit2.modify( it2, 0, [&]( auto& a ) {
            a.deposit_total += quant;
            a.deposit_free += quant;
            a.deposit_his += quant;
        });
    }

    if( eosiosystem::isActiveVoter(user) ) {
        action(
            permission_level{user, active_permission},
            system_account, N(changevotes),
            std::make_tuple(user)).send();
    }    
}

void hdddeposit::undeppool2(account_name user, asset quant) {
    require_auth(user);

    eosio_assert(quant.symbol == CORE_SYMBOL, "must use core asset for hdd deposit.");
    eosio_assert(quant.amount > 0, "must use positive quant");

    bool is_frozen = hddlock(hdd_lock_account).is_frozen(user);  
    eosio_assert( !is_frozen, "user is frozen" );

    depositpool2_table _deposit2(_self, user);
    const auto& it2 = _deposit2.get( user, "no deposit pool record for this user.");

    eosio_assert( it2.deposit_free.amount >= quant.amount, "free deposit not enough." );
    eosio_assert( it2.deposit_total.amount >= quant.amount, "deposit not enough." );
    eosio_assert( it2.deposit_his.amount >= quant.amount, "deposit not enough." );

    _deposit2.modify( it2, 0, [&]( auto& a ) {
        a.deposit_free -= quant;
        a.deposit_total -= quant;
        a.deposit_his -= quant;

    });

    if( eosiosystem::isActiveVoter(user) ) {
        action(
            permission_level{user, active_permission},
            system_account, N(changevotes),
            std::make_tuple(user)).send();
    }

    //这里需要转账到绿色通道    
    action(
       permission_level{user, active_permission},
       token_account, N(transfer),
       std::make_tuple(user, N(store.sys), quant, std::string("unpaydeppool2")))
       .send();
    
    std::string memo;
    memo = "3:" + (name{user}).to_string() + ":0";
    action(
       permission_level{N(store.sys), active_permission},
       token_account, N(transfer),
       std::make_tuple(N(store.sys), N(channel.sys), quant, memo))
       .send(); //需要注意这里memo的格式


   action(
      permission_level{_self, N(active)},
      _self, N(channellog),
      std::make_tuple((uint8_t)3, quant, user))
      .send(); 

}

void hdddeposit::channellog(uint8_t type, asset quant, account_name user) 
{
   require_auth(_self);
   ((void)type);
   ((void)quant);

   require_recipient(user);
}


void hdddeposit::depstore(account_name user, asset quant) {
    require_auth(user);

    eosio_assert(quant.symbol == CORE_SYMBOL, "must use core asset for hdd deposit.");
    eosio_assert(quant.amount > 0, "must use positive quant" );

    storedeposit_table _deposit(_self, user);
    auto it = _deposit.find( user );

    account_name payer = user;
    if ( it == _deposit.end() ) {
        _deposit.emplace( payer, [&]( auto& a ){
            a.account_name = name{user};
            a.deposit_total = quant;
            a.last_deposit_time = current_time();
        });
    } else  {
        _deposit.modify( it, 0, [&]( auto& a ) {
            a.deposit_total += quant;
            a.last_deposit_time = current_time();
        });
    }

    action(
       permission_level{user, active_permission},
       token_account, N(transfer),
       std::make_tuple(user, N(pool.sys), quant, std::string("deopsit for data storeage")))
       .send();
}


void hdddeposit::undepstore(account_name user, asset quant) {
    require_auth(N(hddpooladml1));

    eosio_assert(quant.symbol == CORE_SYMBOL, "must use core asset for hdd deposit.");
    eosio_assert(quant.amount > 0, "must use positive quant" );

    storedeposit_table _deposit(_self, user);
    auto it = _deposit.find( user );
    eosio_assert(it != _deposit.end(), "can not find deposit record");

    eosio_assert(it->deposit_total.amount >= quant.amount, "deposit not enough");
    //eosio_assert(it->last_deposit_time + some_day <= current_time(), "can not return deposit now);

    _deposit.modify( it, 0, [&]( auto& a ) {
        a.deposit_total -= quant;
    });

    action(
       permission_level{N(pool.sys), active_permission},
       token_account, N(transfer),
       std::make_tuple(N(pool.sys), user, quant, std::string("return deopsit for data storeage")))
       .send();

}

void hdddeposit::setrate(int64_t rate) {
    //require_auth(_self);
    require_auth(N(hddpooladmin));
    //return;

    grate_singleton _rate(_self, _self);
    deposit_rate    _rateState;

   if (_rate.exists())
      _rateState = _rate.get();
   else
      _rateState = deposit_rate{};

    _rateState.rate = rate;

    _rate.set(_rateState, _self);

}


#include "nmdeposit.cpp"

EOSIO_ABI( hdddeposit, (paydeppool)(unpaydeppool)(paydeppool2)(undeppool2)(depstore)(undepstore)(paydeposit)(chgdeposit)(mforfeit)(delminer)(setrate)(mchgdepacc)(updatevote)(incdeposit)(channellog))
