#include "hdddeposit.hpp"
#include <eosiolib/action.hpp>
#include <eosiolib/chain.h>
#include <eosiolib/symbol.hpp>
#include <eosio.token/eosio.token.hpp>


void hdddeposit::paydeposit(name user, uint64_t minerid, asset quant) {
    
    require_auth(user);

    eosio_assert(is_account(user), "user is not an account.");
    eosio_assert(quant.symbol == CORE_SYMBOL, "must use YTA for hdd deposit.");

    //check if user has enough YTA balance for deposit
    auto balance   = eosio::token(N(eosio.token)).get_balance( user.value , quant.symbol.name() );
    asset real_balance = balance;
    accdeposit_table _deposit(_self, user.value);
    auto acc = _deposit.find( user.value );    
    if ( acc != _deposit.end() ) {
        eosio_assert( real_balance.amount >= acc->amount.amount, "user balance not enough." );
        real_balance -= acc->amount;
    }
    //to do : also need sub lock_token in futuer
    //......

    eosio_assert( real_balance.amount >= quant.amount, "user balance not enough." );

    //insert or update minerdeposit table
    minerdeposit_table _mdeposit(_self, minerid);
    auto miner = _mdeposit.find( minerid );
    if ( miner == _mdeposit.end() ) {
        _mdeposit.emplace( _self, [&]( auto& a ){
            a.minerid = minerid;
            a.account_name = user;
            a.deposit = quant;
        });
    } else {
        _mdeposit.modify( miner, 0, [&]( auto& a ) {
            eosio_assert(a.account_name == user, "must use same account to increase deposit.");
            a.deposit += quant;
        });
    }

    //insert or update accdeposit table
    if ( acc == _deposit.end() ) {
        _deposit.emplace( _self, [&]( auto& a ){
            a.account_name = user;
            a.amount = quant;
        });
    } else {
        _deposit.modify( acc, 0, [&]( auto& a ) {
            a.amount += quant;
        });
    }
}

void hdddeposit::undeposit(name user, uint64_t minerid, asset quant) {

    require_auth(_self); // need hdd official account to sign this action.

    eosio_assert(is_account(user), "user is not an account.");
    eosio_assert(quant.symbol == CORE_SYMBOL, "must use YTA for hdd deposit.");

    minerdeposit_table _mdeposit(_self, minerid);
    accdeposit_table   _deposit(_self, user.value);
    const auto& miner = _mdeposit.get( minerid, "no deposit record for this minerid.");
    eosio_assert( miner.deposit.amount >= quant.amount, "overdrawn deposit." );
    eosio_assert(miner.account_name == user, "must use same account to decrease deposit.");
    const auto& acc = _deposit.get( user.value, "no deposit record for this user.");
    eosio_assert( acc.amount.amount >= quant.amount, "overdrawn deposit." );

    if( miner.deposit.amount == quant.amount ) {
        _mdeposit.erase( miner );
    } else {
        _mdeposit.modify( miner, 0, [&]( auto& a ) {
            a.deposit -= quant;
        });
    }

    if( acc.amount.amount == quant.amount ) {
        _deposit.erase( acc );
    } else {
        _deposit.modify( acc, 0, [&]( auto& a ) {
            a.amount -= quant;
        });
    }

}

void hdddeposit::clearminer(uint64_t minerid) {
    require_auth(_self);
    minerdeposit_table _mdeposit(_self, minerid);
    const auto& miner = _mdeposit.get( minerid, "no deposit record for this minerid.");
     _mdeposit.erase( miner );
}

void hdddeposit::clearacc(name user) {
    require_auth(_self); 
    accdeposit_table   _deposit(_self, user.value);
    const auto& acc = _deposit.get( user.value, "no deposit record for this user.");
    _deposit.erase( acc );
}

EOSIO_ABI( hdddeposit, (paydeposit)(undeposit)(clearminer)(clearacc))
