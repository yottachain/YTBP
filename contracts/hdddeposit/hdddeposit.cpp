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
    require_auth(N(hddpooladml1));

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

void hdddeposit::unpaydeppool2(account_name user, asset quant) {
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

    action(
       permission_level{N(hdddeposit12), active_permission},
       token_account, N(transfer),
       std::make_tuple(N(store.sys), N(channel.sys), quant, std::string("unpaydeppool2")))
       .send(); //需要注意这里memo的格式

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
    require_auth(user);

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


void hdddeposit::paydeposit(account_name user, uint64_t minerid, asset quant) {
    require_auth(user);

    account_name payer = _self;

    eosio_assert(hddpool::is_miner_exist(minerid), "miner not registered");

    bool is_frozen = hddlock(hdd_lock_account).is_frozen(user);  
    eosio_assert( !is_frozen, "user is frozen" );

    eosio_assert(quant.symbol == CORE_SYMBOL, "must use core asset for hdd deposit.");
    eosio_assert(quant.amount > 0, "must use positive quant");
    eosio_assert(is_deposit_enough(quant, min_miner_space),"deposit quant is too low!");


    //check if user has enough YTA balance for deposit
    depositpool_table _deposit(_self, user);
    const auto& it = _deposit.get( user, "no deposit pool record for this user.");

    eosio_assert( it.deposit_free.amount >= quant.amount, "free deposit not enough." );

    //update depositpool table
    _deposit.modify( it, 0, [&]( auto& a ) {
        a.deposit_free -= quant;
    });

    //insert minerdeposit table
    minerdeposit_table _mdeposit(_self, _self);
    auto miner = _mdeposit.find( minerid );
    eosio_assert(miner == _mdeposit.end(), "already deposit.");
    if ( miner == _mdeposit.end() ) {
        _mdeposit.emplace( payer, [&]( auto& a ){
            a.minerid = minerid;
            a.miner_type = 0;
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

void hdddeposit::incdeposit(uint64_t minerid, asset quant) {
    //require_auth(N(hddpooladmin));

    eosio_assert(quant.symbol == CORE_SYMBOL, "must use core asset for hdd deposit.");
    eosio_assert(quant.amount > 0, "must use positive quant");

    minerdeposit_table _mdeposit(_self, _self);
    const auto& miner = _mdeposit.get( minerid, "no deposit record for this minerid.");
    require_auth(miner.account_name);    
    depositpool_table _deposit(_self, miner.account_name);
    const auto& acc = _deposit.get( miner.account_name.value, "no deposit pool record for this user.");

    eosio_assert( acc.deposit_free.amount >= quant.amount, "free deposit not enough." );

    _mdeposit.modify( miner, 0, [&]( auto& a ) {
        a.deposit += quant;
        if(a.deposit.amount > a.dep_total.amount)
            a.dep_total.amount = a.deposit.amount; 
    });

    _deposit.modify( acc, 0, [&]( auto& a ) {
        a.deposit_free -= quant;
    });

    if( eosiosystem::isActiveVoter(miner.account_name) ) {
        action(
            permission_level{miner.account_name, active_permission},
            system_account, N(changevotes),
            std::make_tuple(miner.account_name)).send();
    }        
}

void hdddeposit::chgdeposit(name user, uint64_t minerid, bool is_increase, asset quant) {
    require_auth(user); // need hdd official account to sign this action.

    eosio_assert(quant.symbol == CORE_SYMBOL, "must use core asset for hdd deposit.");
    eosio_assert(quant.amount > 0, "must use positive quant");
    
    bool is_frozen = hddlock(hdd_lock_account).is_frozen(user);  
    eosio_assert( !is_frozen, "user is frozen" );

    minerdeposit_table _mdeposit(_self, _self);
    const auto& miner = _mdeposit.get( minerid, "no deposit record for this minerid.");
    eosio_assert(miner.account_name == user, "must use same account to change deposit.");
    depositpool_table _deposit(_self, user);
    const auto& acc = _deposit.get( user.value, "no deposit pool record for this user.");

    if(!is_increase) {
        eosio_assert( miner.deposit.amount >= quant.amount, "overdrawn deposit." );

        _mdeposit.modify( miner, 0, [&]( auto& a ) {
            a.deposit -= quant;
            a.dep_total -= quant;
        });

        _deposit.modify( acc, 0, [&]( auto& a ) {
            a.deposit_free += quant;
        });
    } else {
        eosio_assert( acc.deposit_free.amount >= quant.amount, "free deposit not enough." );

        _mdeposit.modify( miner, 0, [&]( auto& a ) {
            a.deposit += quant;
            if(a.deposit.amount > a.dep_total.amount)
                a.dep_total.amount = a.deposit.amount; 
        });

        _deposit.modify( acc, 0, [&]( auto& a ) {
            a.deposit_free -= quant;
        });
    }

    //--- check miner deposit and max_space
    asset deposit = miner.deposit;
    uint64_t max_space = hddpool::get_miner_max_space(minerid);
    eosio_assert(is_deposit_enough(deposit, max_space),"deposit not enough for miner's max_space -- chgdeposit");
    //--- check miner deposit and max_space

    if( eosiosystem::isActiveVoter(user) ) {
        action(
            permission_level{user, active_permission},
            system_account, N(changevotes),
            std::make_tuple(user)).send();
    }    

}

void hdddeposit::mforfeit(name user, uint64_t minerid, asset quant, std::string memo, uint8_t acc_type, name caller) {

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
    eosio_assert( quant.amount > 0, "must use positive quant" );

    minerdeposit_table _mdeposit(_self, _self);
    depositpool_table   _deposit(_self, user.value);
    const auto& miner = _mdeposit.get( minerid, "no deposit record for this minerid.");

    asset quatreal = quant;
    if(miner.deposit.amount < quatreal.amount)
        quatreal.amount = miner.deposit.amount;

    //eosio_assert( miner.deposit.amount >= quant.amount, "overdrawn deposit." );
    eosio_assert(miner.account_name == user, "must use same account to pay forfeit.");
    const auto& acc = _deposit.get( user.value, "no deposit pool record for this user.");
    eosio_assert( acc.deposit_total.amount - acc.deposit_free.amount >= quatreal.amount, "overdrawn deposit." );

    _mdeposit.modify( miner, 0, [&]( auto& a ) {
        a.deposit.amount -= quatreal.amount;
    });

    _deposit.modify( acc, 0, [&]( auto& a ) {
        a.deposit_total -= quatreal;
    });  

    action(
       permission_level{user, active_permission},
       token_account, N(transfer),
       std::make_tuple(user, N(yottaforfeit), quatreal, memo))
       .send();

}

void hdddeposit::delminer(uint64_t minerid) {
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

    if( eosiosystem::isActiveVoter(miner->account_name) ) {
        action(
            permission_level{miner->account_name, active_permission},
            system_account, N(changevotes),
            std::make_tuple(miner->account_name)).send();
    }      

    _mdeposit.erase( miner );
  
}

void hdddeposit::setrate(int64_t rate) {
    //require_auth(_self);
    require_auth(N(hddpooladmin));

    grate_singleton _rate(_self, _self);
    deposit_rate    _rateState;

   if (_rate.exists())
      _rateState = _rate.get();
   else
      _rateState = deposit_rate{};

    _rateState.rate = rate;

    _rate.set(_rateState, _self);

}

void hdddeposit::mchgdepacc(uint64_t minerid, name new_depacc) {
    require_auth(new_depacc);

    minerdeposit_table _mdeposit(_self, _self);
    const auto& miner = _mdeposit.get( minerid, "no deposit record for this minerid");
    eosio_assert(miner.account_name != new_depacc, "must use different account to change deposit user");

    depositpool_table   _deposit_old(_self, miner.account_name.value);
    const auto& acc_old = _deposit_old.get( miner.account_name, "no deposit pool record for original deposit user");

    depositpool_table   _deposit_new(_self, new_depacc.value);
    const auto& acc_new = _deposit_new.get( new_depacc.value, "no deposit pool record for new deposit user");

    eosio_assert( acc_new.deposit_free.amount >= miner.dep_total.amount, "new deposit user free deposit not enough" );
    
    //变更原抵押账户的押金数量
    _deposit_old.modify( acc_old, 0, [&]( auto& a ) {
        a.deposit_free += miner.deposit;
    });

    if( eosiosystem::isActiveVoter(miner.account_name) ) {
        action(
            permission_level{miner.account_name, active_permission},
            system_account, N(changevotes),
            std::make_tuple(miner.account_name)).send();
    }        

    //将矿机的押金数量重新恢复到未扣罚金的初始额度
    _mdeposit.modify( miner, 0, [&]( auto& a ) {
        a.account_name = new_depacc;
        a.deposit = a.dep_total;
    });

    _deposit_new.modify( acc_new, 0, [&]( auto& a ) {
        a.deposit_free -= miner.dep_total;
    });

    if( eosiosystem::isActiveVoter(new_depacc) ) {
        action(
            permission_level{new_depacc, active_permission},
            system_account, N(changevotes),
            std::make_tuple(new_depacc)).send();
    }        
}

void hdddeposit::updatevote(name user)
{
    require_auth(N(hddpooladmin));

    if( eosiosystem::isActiveVoter(user) ) {
        action(
            permission_level{user, active_permission},
            system_account, N(changevotes),
            std::make_tuple(user)).send();
    }        
}

void hdddeposit::check_bp_account(account_name bpacc, uint64_t id, bool isCheckId) {
    account_name shadow;
    uint64_t seq_num = eosiosystem::getProducerSeq(bpacc, shadow);
    eosio_assert(seq_num > 0 && seq_num < 22, "invalidate bp account");
    if(isCheckId) {
      eosio_assert( (id%21) == (seq_num-1), "can not access this id");
    }
    require_auth(shadow);
    //require_auth(bpacc);
}



EOSIO_ABI( hdddeposit, (paydeppool)(unpaydeppool)(paydeppool2)(unpaydeppool2)(depstore)(undepstore)(paydeposit)(chgdeposit)(mforfeit)(delminer)(setrate)(mchgdepacc)(updatevote)(incdeposit))
