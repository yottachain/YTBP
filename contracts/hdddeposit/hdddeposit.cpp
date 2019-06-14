#include "hdddeposit.hpp"
#include <eosiolib/action.hpp>
#include <eosiolib/chain.h>
#include <eosiolib/symbol.hpp>
#include <eosiolib/eosio.hpp>
#include <eosiolib/print.hpp>
#include <eosiolib/serialize.hpp>
#include <eosiolib/multi_index.hpp>
#include <eosio.token/eosio.token.hpp>

using namespace eosio;

static constexpr eosio::name active_permission{N(active)};
static constexpr eosio::name token_account{N(eosio.token)};
static constexpr eosio::name system_account{N(eosio)};
static constexpr eosio::name hdd_deposit_account{N(hdddeposit12)};

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
        real_balance.amount -= acc->deposit.amount;
        real_balance.amount -= acc->forfeit.amount;     
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
            a.deposit = quant;
        });
    } else {
        _deposit.modify( acc, 0, [&]( auto& a ) {
            a.deposit += quant;
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
    eosio_assert( acc.deposit.amount >= quant.amount, "overdrawn deposit." );

    _mdeposit.modify( miner, 0, [&]( auto& a ) {
        a.deposit.amount -= quant.amount;
    });

    _deposit.modify( acc, 0, [&]( auto& a ) {
        a.deposit.amount -= quant.amount;
    });
}

void hdddeposit::payforfeit(name user, uint64_t minerid, asset quant) {
    require_auth(_self); // need hdd official account to sign this action.

    eosio_assert(is_account(user), "user is not an account.");
    eosio_assert(quant.symbol == CORE_SYMBOL, "must use YTA for hdd deposit.");

    minerdeposit_table _mdeposit(_self, minerid);
    accdeposit_table   _deposit(_self, user.value);
    const auto& miner = _mdeposit.get( minerid, "no deposit record for this minerid.");
    eosio_assert( miner.deposit.amount >= quant.amount, "overdrawn deposit." );
    eosio_assert(miner.account_name == user, "must use same account to pay forfeit.");
    const auto& acc = _deposit.get( user.value, "no deposit record for this user.");
    eosio_assert( acc.deposit.amount >= quant.amount, "overdrawn deposit." );

    _mdeposit.modify( miner, 0, [&]( auto& a ) {
        a.deposit.amount -= quant.amount;
    });

    _deposit.modify( acc, 0, [&]( auto& a ) {
        a.deposit.amount -= quant.amount;
        a.forfeit.amount += quant.amount;
    });    

}

void hdddeposit::drawforfeit(name user) {
    require_auth(_self); // need hdd official account to sign this action.

    eosio_assert(is_account(user), "user is not an account.");
    accdeposit_table   _deposit(_self, user.value);
    const auto& acc = _deposit.get( user.value, "no deposit record for this user.");

    asset quant{acc.forfeit.amount, CORE_SYMBOL};
    action(
       permission_level{user, active_permission},
       token_account, N(transfer),
       std::make_tuple(user, hdd_deposit_account, quant, std::string("draw forfeit")))
       .send();
    
    _deposit.modify( acc, 0, [&]( auto& a ) {
        a.forfeit.amount = 0;
    });      

}

void hdddeposit::cutvote(name user) {
    require_auth(_self); // need hdd official account to sign this action.

    eosio_assert(is_account(user), "user is not an account.");
    accdeposit_table   _deposit(_self, user.value);
    const auto& acc = _deposit.get( user.value, "no deposit record for this user.");

    asset quantb{acc.forfeit.amount/2, CORE_SYMBOL};
    asset quantw{acc.forfeit.amount/2, CORE_SYMBOL};
    
    //asset quantb{10000, CORE_SYMBOL};
    //asset quantw{10000, CORE_SYMBOL};

    action(
       permission_level{user, active_permission},
       system_account, N(undelegatebw),
       std::make_tuple(user, user, quantb, quantw))
       .send();
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

EOSIO_ABI( hdddeposit, (paydeposit)(undeposit)(payforfeit)(drawforfeit)(cutvote)(clearminer)(clearacc))
