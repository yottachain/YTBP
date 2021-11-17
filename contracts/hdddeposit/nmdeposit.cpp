
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
    depositpool2_table _deposit2(_self, user);
    const auto& it2 = _deposit2.get( user, "no deposit pool record for this user.");

    eosio_assert( it2.deposit_free.amount >= quant.amount, "free deposit not enough." );

    //update depositpool table
    _deposit2.modify( it2, 0, [&]( auto& a ) {
        a.deposit_free -= quant;
    });

    //insert minerdeposit table
    minerdeposit_table _mdeposit(_self, _self);
    auto miner = _mdeposit.find( minerid );
    eosio_assert(miner == _mdeposit.end(), "already deposit.");
    if ( miner == _mdeposit.end() ) {
        _mdeposit.emplace( payer, [&]( auto& a ){
            a.minerid = minerid;
            a.miner_type = 1;
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
    eosio_assert(false, "not support now.");

    eosio_assert(quant.symbol == CORE_SYMBOL, "must use core asset for hdd deposit.");
    eosio_assert(quant.amount > 0, "must use positive quant");

    minerdeposit_table _mdeposit(_self, _self);
    const auto& miner = _mdeposit.get( minerid, "no deposit record for this minerid.");
    require_auth(miner.account_name);

    if(miner.miner_type == 0) {
        depositpool_table _deposit(_self, miner.account_name);
        const auto& acc = _deposit.get( miner.account_name.value, "no deposit pool record for this user.");

        depositpool2_table _deposit2(_self, miner.account_name);
        const auto& acc2 = _deposit2.get( miner.account_name.value, "no deposit pool2 record for this user.");

        //释放depositpool的矿机抵押
        _deposit.modify( acc, 0, [&]( auto& a ) {
            a.deposit_free += miner.deposit;
            if(a.deposit_free >= a.deposit_total)
                a.deposit_free = a.deposit_total;
        });

        _mdeposit.modify( miner, 0, [&]( auto& a ) {
            a.deposit += quant;
            a.miner_type = 1;
            if(a.deposit.amount > a.dep_total.amount)
                a.dep_total.amount = a.deposit.amount; 
        });

        eosio_assert( acc2.deposit_free.amount >= miner.deposit.amount, "free deposit not enough." );

        _deposit2.modify( acc2, 0, [&]( auto& a ) {
            a.deposit_free -= miner.deposit;
        });

    } else {
        depositpool2_table _deposit2(_self, miner.account_name);
        const auto& acc2 = _deposit2.get( miner.account_name.value, "no deposit pool2 record for this user.");
        eosio_assert( acc2.deposit_free.amount >= quant.amount, "free deposit not enough." );

        _mdeposit.modify( miner, 0, [&]( auto& a ) {
            a.deposit += quant;
            a.miner_type = 1;
            if(a.deposit.amount > a.dep_total.amount)
                a.dep_total.amount = a.deposit.amount; 
        });

        _deposit2.modify( acc2, 0, [&]( auto& a ) {
            a.deposit_free -= quant;
        });
    }

    if( eosiosystem::isActiveVoter(miner.account_name) ) {
        action(
            permission_level{miner.account_name, active_permission},
            system_account, N(changevotes),
            std::make_tuple(miner.account_name)).send();
    }        
}

void hdddeposit::chgdeposit(name user, uint64_t minerid, bool is_increase, asset quant) {
    eosio_assert(false, "not support now.");
    ((void)user);
    ((void)minerid);
    ((void)is_increase);
    ((void)quant);
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
    const auto& miner = _mdeposit.get( minerid, "no deposit record for this minerid.");

    asset quatreal = quant;
    if(miner.deposit.amount < quatreal.amount)
        quatreal.amount = miner.deposit.amount;

    //eosio_assert( miner.deposit.amount >= quant.amount, "overdrawn deposit." );
    eosio_assert(miner.account_name == user, "must use same account to pay forfeit.");
    _mdeposit.modify( miner, 0, [&]( auto& a ) {
        a.deposit.amount -= quatreal.amount;
    });

    if(miner.miner_type == 0) {
        depositpool_table   _deposit(_self, user.value);
        const auto& acc = _deposit.get( user.value, "no deposit pool record for this user.");
        eosio_assert( acc.deposit_total.amount - acc.deposit_free.amount >= quatreal.amount, "overdrawn deposit." );
        _deposit.modify( acc, 0, [&]( auto& a ) {
            a.deposit_total -= quatreal;
        });  
    } else {
        depositpool2_table   _deposit2(_self, user.value);
        const auto& acc2 = _deposit2.get( user.value, "no deposit pool2 record for this user.");
        eosio_assert( acc2.deposit_total.amount - acc2.deposit_free.amount >= quatreal.amount, "overdrawn deposit." );
        _deposit2.modify( acc2, 0, [&]( auto& a ) {
            a.deposit_total -= quatreal;
        });  

    }

    if(miner.miner_type == 0) {
        action(
            permission_level{user, active_permission},
            token_account, N(transfer),
            std::make_tuple(user, N(yottaforfeit), quatreal, memo))
        .send();
    } else {
        action(
            permission_level{user, active_permission},
            token_account, N(transfer),
            std::make_tuple(user, N(forfeit.sys), quatreal, memo))
        .send();
    }


}

void hdddeposit::delminer(uint64_t minerid) {
    require_auth(N(hddpooladmin));
          
    minerdeposit_table _mdeposit(_self, _self);
    auto miner = _mdeposit.find(minerid);
    if(miner == _mdeposit.end())
        return;

    if(miner->miner_type == 0) {
        depositpool_table   _deposit(_self, miner->account_name.value );
        auto acc = _deposit.find(miner->account_name.value);
        if(acc != _deposit.end()) {
            _deposit.modify( acc, 0, [&]( auto& a ) {
                a.deposit_free += miner->deposit;
                if(a.deposit_free >= a.deposit_total)
                    a.deposit_free = a.deposit_total;
            });    
        }
    } else {
        depositpool2_table   _deposit2(_self, miner->account_name.value );
        auto acc2 = _deposit2.find(miner->account_name.value);
        if(acc2 != _deposit2.end()) {
            _deposit2.modify( acc2, 0, [&]( auto& a ) {
                a.deposit_free += miner->deposit;
                if(a.deposit_free >= a.deposit_total)
                    a.deposit_free = a.deposit_total;
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

void hdddeposit::mchgdepacc(uint64_t minerid, name new_depacc) {
    eosio_assert(false, "not support now.");
    require_auth(new_depacc);

    minerdeposit_table _mdeposit(_self, _self);
    const auto& miner = _mdeposit.get( minerid, "no deposit record for this minerid");
    eosio_assert(miner.account_name != new_depacc, "must use different account to change deposit user");

    depositpool2_table   _deposit2_new(_self, new_depacc.value);
    const auto& acc2_new = _deposit2_new.get( new_depacc.value, "no deposit pool2 record for new deposit user");

    if(miner.miner_type == 0) {
        depositpool_table   _deposit_old(_self, miner.account_name.value);
        const auto& acc_old = _deposit_old.get( miner.account_name, "no deposit pool record for original deposit user");

        eosio_assert( acc2_new.deposit_free.amount >= miner.dep_total.amount, "new deposit user free deposit not enough" );
    
        //变更原抵押账户的押金数量
        _deposit_old.modify( acc_old, 0, [&]( auto& a ) {
            a.deposit_free += miner.deposit;
        });
    } else {
        depositpool2_table   _deposit2_old(_self, miner.account_name.value);
        const auto& acc2_old = _deposit2_old.get( miner.account_name, "no deposit pool2 record for original deposit user");

        eosio_assert( acc2_new.deposit_free.amount >= miner.dep_total.amount, "new deposit user free deposit not enough" );
    
        //变更原抵押账户的押金数量
        _deposit2_old.modify( acc2_old, 0, [&]( auto& a ) {
            a.deposit_free += miner.deposit;
        });
    }

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
        a.miner_type = 1;
    });

    _deposit2_new.modify( acc2_new, 0, [&]( auto& a ) {
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
    require_auth(bpacc);
    return;    
    account_name shadow;
    uint64_t seq_num = eosiosystem::getProducerSeq(bpacc, shadow);
    eosio_assert(seq_num > 0 && seq_num < 22, "invalidate bp account");
    if(isCheckId) {
      eosio_assert( (id%21) == (seq_num-1), "can not access this id");
    }
    require_auth(shadow);
    //require_auth(bpacc);
}
