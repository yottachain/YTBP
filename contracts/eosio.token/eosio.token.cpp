/**
 *  @file
 *  @copyright defined in eos/LICENSE
 */

#include "eosio.token.hpp"
#include <hdddeposit/hdddeposit.hpp>
#include <hddlock/hddlock.hpp>

const account_name hdd_deposit_account = N(hdddeposit12);
const account_name hdd_lock_account = N(hddlock12345);

namespace eosio {

void token::create( account_name issuer,
                    asset        maximum_supply )
{
    require_auth( _self );

    auto sym = maximum_supply.symbol;
    eosio_assert( sym.is_valid(), "invalid symbol name" );
    eosio_assert( maximum_supply.is_valid(), "invalid supply");
    eosio_assert( maximum_supply.amount > 0, "max-supply must be positive");

    stats statstable( _self, sym.name() );
    auto existing = statstable.find( sym.name() );
    eosio_assert( existing == statstable.end(), "token with symbol already exists" );

    statstable.emplace( _self, [&]( auto& s ) {
       s.supply.symbol = maximum_supply.symbol;
       s.max_supply    = maximum_supply;
       s.issuer        = issuer;
    });
}


void token::issue( account_name to, asset quantity, string memo )
{
    auto sym = quantity.symbol;
    eosio_assert( sym.is_valid(), "invalid symbol name" );
    eosio_assert( memo.size() <= 256, "memo has more than 256 bytes" );

    auto sym_name = sym.name();
    stats statstable( _self, sym_name );
    auto existing = statstable.find( sym_name );
    eosio_assert( existing != statstable.end(), "token with symbol does not exist, create token before issue" );
    const auto& st = *existing;

    require_auth( st.issuer );
    eosio_assert( quantity.is_valid(), "invalid quantity" );
    eosio_assert( quantity.amount > 0, "must issue positive quantity" );

    eosio_assert( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );
    eosio_assert( quantity.amount <= st.max_supply.amount - st.supply.amount, "quantity exceeds available supply");

    statstable.modify( st, 0, [&]( auto& s ) {
       s.supply += quantity;
    });

    add_balance( st.issuer, quantity, st.issuer );

    if( to != st.issuer ) {
       SEND_INLINE_ACTION( *this, transfer, {st.issuer,N(active)}, {st.issuer, to, quantity, memo} );
    }
}

void token::transfer( account_name from,
                      account_name to,
                      asset        quantity,
                      string       memo )
{
    eosio_assert( from != to, "cannot transfer to self" );
    require_auth( from );
    eosio_assert( is_account( to ), "to account does not exist");
    auto sym = quantity.symbol.name();
    stats statstable( _self, sym );
    const auto& st = statstable.get( sym );

    require_recipient( from );
    require_recipient( to );

    eosio_assert( quantity.is_valid(), "invalid quantity" );
    eosio_assert( quantity.amount > 0, "must transfer positive quantity" );
    eosio_assert( quantity.symbol == st.supply.symbol, "symbol precision mismatch" );
    eosio_assert( memo.size() <= 256, "memo has more than 256 bytes" );


    //##YTA-Change  start:  
    if( quantity.symbol != CORE_SYMBOL || from == N(hddbasefound))  // no need to consider hdd_deposit and hdd_lock issue  
      sub_balance( from, quantity );
    else
      sub_balance_yta( from, quantity, to );
    //##YTA-Change  end:

    add_balance( to, quantity, from );
}

void token::sub_balance( account_name owner, asset value ) {
   accounts from_acnts( _self, owner );

   const auto& from = from_acnts.get( value.symbol.name(), "no balance object found" );

   eosio_assert( from.balance.amount >= value.amount, "overdrawn balance" );

   if( from.balance.amount == value.amount ) {
      from_acnts.erase( from );
   } else {
      from_acnts.modify( from, owner, [&]( auto& a ) {
          a.balance -= value;
      });
   }
}

//##YTA-Change  start:
void token::sub_balance_yta( account_name owner, asset value , account_name to) {
   accounts from_acnts( _self, owner );

   const auto& from = from_acnts.get( value.symbol.name(), "no balance object found" );

   //todo : need consider lock_token situation
   if( to == hdd_deposit_account) { //缴纳罚金,锁仓币也可以缴纳罚金
      eosio_assert( from.balance.amount >= value.amount, "overdrawn balance" );
   } else if( to == N(eosio.stake) ) { //用来抵押带宽和CPU
      //forfeit can not use to delegatebw and vote
      auto deposit_and_forfeit = hdddeposit(hdd_deposit_account).get_deposit_and_forfeit(owner);
      eosio_assert( deposit_and_forfeit.symbol == value.symbol, "symbol precision mismatch" );
      eosio_assert( from.balance.amount - deposit_and_forfeit.amount >= value.amount, "overdrawn balance" );
   } else { //普通转账，需要考虑锁仓和存储抵押
      auto frozen_asset = hdddeposit(hdd_deposit_account).get_deposit_and_forfeit(owner);
      //also need check lock token issue
      auto lock_asset = hddlock(hdd_lock_account).get_lock_asset(owner);
      eosio_assert( frozen_asset.symbol == lock_asset.symbol, "symbol precision mismatch" );
      frozen_asset.amount += lock_asset.amount;
      eosio_assert( frozen_asset.symbol == value.symbol, "symbol precision mismatch" );
      eosio_assert( from.balance.amount - frozen_asset.amount >= value.amount, "overdrawn balance" );
   }

   if( from.balance.amount == value.amount ) {
      from_acnts.erase( from );
   } else {
      from_acnts.modify( from, owner, [&]( auto& a ) {
          a.balance -= value;
      });
   }
}
//##YTA-Change  end:

void token::add_balance( account_name owner, asset value, account_name ram_payer )
{
   accounts to_acnts( _self, owner );
   auto to = to_acnts.find( value.symbol.name() );
   if( to == to_acnts.end() ) {
      to_acnts.emplace( ram_payer, [&]( auto& a ){
        a.balance = value;
      });
   } else {
      to_acnts.modify( to, 0, [&]( auto& a ) {
        a.balance += value;
      });
   }
}

} /// namespace eosio

EOSIO_ABI( eosio::token, (create)(issue)(transfer) )
