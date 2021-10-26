#include "channel.sys.hpp"


void mchannel::transfercore( account_name from,
                      account_name to,
                      asset        quantity,
                      string       memo )
{
   eosio_assert(quantity.symbol == CORE_SYMBOL, "invalid symbol");
   /*
   bool is_from_ok = false;
   if( from == N(fund.sys) || from == N(store.sys) || from == N(ytapro.map)) {
      is_from_ok = true;
   }
   eosio_assert(is_from_ok, "invalid from user");
   */
   eosio_assert(to == _self, "invalid to user");
   auto pos1 = memo.find_first_of(':');
   eosio_assert(pos1 != std::string::npos, "invalid memo");
   auto typestr = memo.substr(0,pos1);
   uint8_t type = atoi(typestr.c_str());
   auto pos2 = memo.find_first_of(':',pos1+1);
   eosio_assert(pos2 != std::string::npos, "invalid memo");
   auto namestr = memo.substr(pos1+1,pos2);
   account_name user = eosio::string_to_name(namestr.c_str());
   if(type == 1) {
      eosio_assert(from == N(fund.sys), "invalid from user");

   } else if(type == 2) {
      eosio_assert(from == N(store.sys), "invalid from user");

   } else if(type == 3) {
      eosio_assert(from == N(ytapro.map), "invalid from user");

   } else {
      eosio_assert(false, "invalid memo type");
   }
}


extern "C" { 
   void apply( uint64_t receiver, uint64_t code, uint64_t action ) { 
      auto self = receiver; 
      if(action == N(onerror)) {
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
            //EOSIO_API( mchannel, (xxxx) ) 
         } 
         /* does not allow destructor of thiscontract to run: eosio_exit(0); */ \
      } 
   } 
} 

