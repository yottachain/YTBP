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

using namespace eosio;

static constexpr eosio::name active_permission{N(active)};
static constexpr eosio::name token_account{N(eosio.token)};
static constexpr eosio::name system_account{N(eosio)};
static constexpr eosio::name hdd_deposit_account{N(hdddeposit12)};

void hdddeposit::paydeposit(account_name user, uint64_t minerid, asset quant) {
    
    require_auth(user);

    eosio_assert(is_account(user), "user is not an account.");
    eosio_assert(quant.symbol == CORE_SYMBOL, "must use core asset for hdd deposit.");

    //check if user has enough YTA balance for deposit
    auto balance   = eosio::token(N(eosio.token)).get_balance( user , quant.symbol.name() );
    asset real_balance = balance;
    accdeposit_table _deposit(_self, user);
    auto acc = _deposit.find( user );    
    if ( acc != _deposit.end() ) {
        real_balance.amount -= acc->deposit.amount;
        real_balance.amount -= acc->forfeit.amount;     
    }
    eosio_assert( real_balance.amount >= quant.amount, "user balance not enough." );

    //insert or update accdeposit table
    if ( acc == _deposit.end() ) {
        _deposit.emplace( _self, [&]( auto& a ){
            a.account_name = name{user};
            a.deposit = quant;
        });
    } else {
        _deposit.modify( acc, 0, [&]( auto& a ) {
            a.deposit += quant;
        });
    }

    //insert or update minerdeposit table
    minerdeposit_table _mdeposit(_self, _self);
    auto miner = _mdeposit.find( minerid );
    eosio_assert(miner == _mdeposit.end(), "already deposit.");
    if ( miner == _mdeposit.end() ) {
        _mdeposit.emplace( _self, [&]( auto& a ){
            a.minerid = minerid;
            a.account_name = name{user};
            a.deposit = quant;
            a.dep_total = quant;
        });
    }

    if( eosiosystem::isActiveVoter(user) ) {
        action(
            permission_level{user, active_permission},
            system_account, N(changevotes),
            std::make_tuple(user)).send();
    }
}

void hdddeposit::chgdeposit(name user, uint64_t minerid, bool is_increace, asset quant) {

    require_auth(_self); // need hdd official account to sign this action.

    eosio_assert(is_account(user), "user is not an account.");
    eosio_assert(quant.symbol == CORE_SYMBOL, "must use core asset for hdd deposit.");

    minerdeposit_table _mdeposit(_self, _self);
    accdeposit_table   _deposit(_self, user.value);
    const auto& miner = _mdeposit.get( minerid, "no deposit record for this minerid.");
    eosio_assert(miner.account_name == user, "must use same account to decrease deposit.");
    const auto& acc = _deposit.get( user.value, "no deposit record for this user.");

    if(!is_increace) {
        eosio_assert( miner.deposit.amount >= quant.amount, "overdrawn deposit." );
        eosio_assert( acc.deposit.amount >= quant.amount, "overdrawn deposit." );
        
        _mdeposit.modify( miner, 0, [&]( auto& a ) {
            a.deposit.amount -= quant.amount;
            a.dep_total.amount -= quant.amount;
        });

        _deposit.modify( acc, 0, [&]( auto& a ) {
            a.deposit.amount -= quant.amount;
        });
    } else {
        auto balance   = eosio::token(N(eosio.token)).get_balance( user , quant.symbol.name() );
        asset real_balance = balance;
        real_balance.amount -= acc.deposit.amount;
        real_balance.amount -= acc.forfeit.amount;     
        eosio_assert( real_balance.amount >= quant.amount, "user balance not enough." );
        _mdeposit.modify( miner, 0, [&]( auto& a ) {
            a.deposit.amount += quant.amount;
            if(a.deposit.amount > a.dep_total.amount)
                a.dep_total.amount = a.deposit.amount; 
        });

        _deposit.modify( acc, 0, [&]( auto& a ) {
            a.deposit.amount += quant.amount;
        });

    }


    if( eosiosystem::isActiveVoter(user) ) {
        action(
            permission_level{user, active_permission},
            system_account, N(changevotes),
            std::make_tuple(user)).send();
    }

}

void hdddeposit::payforfeit(name user, uint64_t minerid, asset quant, uint8_t acc_type, name caller) {

    if(acc_type == 2) {
        eosio_assert(is_account(caller), "caller not a account.");
        //eosio_assert(is_bp_account(caller.value), "caller not a BP account.");
        //require_auth( caller );
        check_bp_account(caller.value, minerid, true);
    } else {
        require_auth( _self );
    }

    eosio_assert(is_account(user), "user is not an account.");
    eosio_assert(quant.symbol == CORE_SYMBOL, "must use core asset for hdd deposit.");

    minerdeposit_table _mdeposit(_self, _self);
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
        //a.forfeit.amount += quant.amount;
    });  

    action(
       permission_level{user, active_permission},
       token_account, N(transfer),
       std::make_tuple(user, hdd_deposit_account, quant, std::string("draw forfeit")))
       .send();

    if( eosiosystem::isActiveVoter(user) ) {
        action(
            permission_level{user, active_permission},
            system_account, N(changevotes),
            std::make_tuple(user)).send();
    }

}

void hdddeposit::delminer(uint64_t minerid) {
    require_auth(_self);
          
    minerdeposit_table _mdeposit(_self, _self);
    auto miner = _mdeposit.find(minerid);
    if(miner == _mdeposit.end())
        return;
    accdeposit_table   _deposit(_self, miner->account_name.value );
    auto acc = _deposit.find(miner->account_name.value);
    if(acc != _deposit.end()) {
        if(acc->deposit.amount >= miner->deposit.amount) {
            _deposit.modify( acc, 0, [&]( auto& a ) {
                a.deposit.amount -= miner->deposit.amount;
            });    
        }            
    }

    if( eosiosystem::isActiveVoter(miner->account_name) ) {
        action(
            permission_level{miner->account_name, active_permission},
            system_account, N(changevotes),
            std::make_tuple(miner->account_name)).send();
    }

    _mdeposit.erase( miner );
}

void hdddeposit::setrate(int64_t rate) {
    require_auth(_self);
    grate_singleton _rate(_self, _self);
    deposit_rate    _rateState;

   if (_rate.exists())
      _rateState = _rate.get();
   else
      _rateState = deposit_rate{};

    _rateState.rate = rate;

    _rate.set(_rateState, _self);

}


void hdddeposit::drawforfeit(name user, uint8_t acc_type, name caller) {
    ((void)user);
    ((void)acc_type);
    ((void)caller);
}

void hdddeposit::cutvote(name user, uint8_t acc_type, name caller) {
    ((void)user);
    ((void)acc_type);
    ((void)caller);
}


void hdddeposit::check_bp_account(account_name bpacc, uint64_t id, bool isCheckId) {
    account_name shadow;
    uint64_t seq_num = eosiosystem::getProducerSeq(bpacc, shadow);
    eosio_assert(seq_num > 0 && seq_num < 22, "invalidate bp account");
    if(isCheckId) {
      eosio_assert( (id%21) == (seq_num-1), "can not access this id");
    }
    //require_auth(shadow);
    require_auth(bpacc);
}



EOSIO_ABI( hdddeposit, (paydeposit)(chgdeposit)(payforfeit)(drawforfeit)(cutvote)(delminer)(setrate))
