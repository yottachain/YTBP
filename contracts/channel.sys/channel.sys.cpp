#include <eosiolib/action.hpp>
#include "channel.sys.hpp"

using namespace eosio;

void mchannel::transfercore( account_name from,
                      account_name to,
                      asset        quantity,
                      string       memo )
{
   if(from == _self) 
      return;

   ((void)to);

   eosio_assert(quantity.symbol == CORE_SYMBOL, "invalid symbol");

   auto pos1 = memo.find_first_of(':');
   eosio_assert(pos1 != std::string::npos, "invalid memo");
   auto trans_typestr = memo.substr(0,pos1);
   uint8_t trans_type = (uint8_t)atoi(trans_typestr.c_str());
   auto pos2 = memo.find_first_of(':',pos1+1);
   eosio_assert(pos2 != std::string::npos, "invalid memo");
   auto namestr = memo.substr(pos1+1,pos2);
   account_name user = eosio::string_to_name(namestr.c_str());

   if(trans_type == 1 || trans_type == 2) {
      eosio_assert(from == N(fund.sys), "invalid from user");
      cbalances bans(_self, user);
      auto it = bans.find(1);
      if(it != bans.end()) {
         bans.modify( it, _self, [&]( auto& a ) {
            a.balance +=  quantity;
         });
      } else {
         bans.emplace( _self, [&]( auto& a ){
            a.balance = quantity;
            a.type = 1;
         });
      }
   } else if(trans_type == 3) {
      eosio_assert(from == N(store.sys), "invalid from user");
      cbalances bans(_self, user);
      auto it = bans.find(1);
      if(it != bans.end()) {
         bans.modify( it, _self, [&]( auto& a ) {
            a.balance +=  quantity;
         });
      } else {
         bans.emplace( _self, [&]( auto& a ){
            a.balance = quantity;
            a.type = 1;
         });
      }
   } else if(trans_type == 4) {
      eosio_assert(from == N(ytapro.map), "invalid from user");
      
      action(
          permission_level{N(channel.sys), N(active)},
          N(hdddeposit12), N(paydeppool2),
         std::make_tuple(user, quantity))
         .send();

   } else {
      eosio_assert(false, "invalid memo type");
   }
}

void mchannel::mapc(account_name user, asset  quant, asset gas, string bscaddr)
{
   require_auth(user);

   eosio_assert(quant.symbol == CORE_SYMBOL, "invalid quant symbol");
   eosio_assert(gas.symbol == CORE_SYMBOL, "invalid gas symbol");

   asset total_trans_asset = quant + gas;

   cbalances bans(_self, user);
   auto it = bans.find(1);
   eosio_assert(it != bans.end(), "invalid user");
   eosio_assert(it->balance >= total_trans_asset, "overdrawn user channel balance");

   bans.modify( it, _self, [&]( auto& a ) {
      a.balance -=  total_trans_asset;
   });

   std::string memo;
   memo = "BSC" + bscaddr;
   action(
      permission_level{N(channel.sys), N(active)},
      N(eosio.token), N(transfer),
      std::make_tuple(N(channel.sys), N(ytapro.map), quant, memo))
      .send(); 

   action(
      permission_level{N(channel.sys), N(active)},
      N(eosio.token), N(transfer),
      std::make_tuple(N(channel.sys), N(ytapro.map), gas, std::string("fee")))
      .send(); 

   action(
      permission_level{_self, N(active)},
      _self, N(channellog),
      std::make_tuple((uint8_t)4, quant+gas, user))
      .send(); 

}

void mchannel::channellog(uint8_t type, asset quant, account_name user) 
{
   require_auth(_self);
   ((void)type);
   ((void)quant);

   require_recipient(user);
}

extern "C" { 
   void apply( uint64_t receiver, uint64_t code, uint64_t action ) { 
      auto self = receiver; 
      if(action == N(transfer)) {
         if(code == N(eosio.token)) {
            mchannel thiscontract( self ); 
            execute_action( &thiscontract, &mchannel::transfercore );
            return;
         }
      }
      if( action == N(onerror)) { 
         /* onerror is only valid if it is for the "eosio" code account and authorized by "eosio"'s "active permission */ \
         eosio_assert(code == N(eosio), "onerror action's are only valid from the \"eosio\" system account"); 
      } 
      if( code == self || action == N(onerror) ) { 
         mchannel thiscontract( self ); 
         switch( action ) { 
            EOSIO_API( mchannel, (mapc)(channellog) ) 
         } 
         /* does not allow destructor of thiscontract to run: eosio_exit(0); */ \
      } 
   } 
} 

