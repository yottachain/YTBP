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

void hdddeposit::mforfeitold(uint64_t minerid, asset quant) {

    require_auth(N(hddpooladmin));

    eosio_assert(quant.symbol == CORE_SYMBOL, "must use core asset for hdd deposit.");
    eosio_assert( quant.amount > 0, "must use positive quant" );

    minerdeposit_table _mdeposit(_self, _self);
    const auto& miner = _mdeposit.get( minerid, "no deposit record for this minerid.");
    depositpool_table   _deposit(_self, miner.account_name.value);
    asset quatreal = quant;
    if(miner.deposit.amount < quatreal.amount)
        quatreal.amount = miner.deposit.amount;
    //eosio_assert( miner.deposit.amount >= mforfeitold.amount, "overdrawn deposit." );
    const auto& acc = _deposit.get( miner.account_name.value, "no deposit pool record for this user.");
    eosio_assert( acc.deposit_total.amount - acc.deposit_free.amount >= quatreal.amount, "overdrawn deposit." );

    _mdeposit.modify( miner, 0, [&]( auto& a ) {
        a.deposit.amount -= quatreal.amount;
    });

    _deposit.modify( acc, 0, [&]( auto& a ) {
        a.deposit_total -= quatreal;
    });  

    action(
       permission_level{miner.account_name, active_permission},
       token_account, N(transfer),
       std::make_tuple(miner.account_name, N(yottaforfeit), quatreal, std::string("draw forfeit")))
       .send();

    if( eosiosystem::isActiveVoter(miner.account_name) ) {
        action(
            permission_level{miner.account_name, active_permission},
            system_account, N(changevotes),
            std::make_tuple(miner.account_name)).send();
    }

}

void hdddeposit::rmdeposit(uint64_t minerid) {
    require_auth(N(hddpooladmin));
          
    minerdeposit_table _mdeposit(_self, _self);
    auto miner = _mdeposit.find(minerid);
    if(miner == _mdeposit.end())
        return;
    depositpool_table   _deposit(_self, miner->account_name.value );
    auto acc = _deposit.find(miner->account_name.value);
    if(acc != _deposit.end()) {
        _deposit.modify( acc, 0, [&]( auto& a ) {
            a.deposit_free += miner->deposit;
            if(a.deposit_free >= a.deposit_total)
                a.deposit_free = a.deposit_total;
        });    
    }

    _mdeposit.erase( miner );
}


EOSIO_ABI( hdddeposit, (paydeppool)(unpaydeppool)(mforfeitold)(rmdeposit))
