#include "eosio.system.hpp"
#include <eosiolib/print.hpp>
#include <eosio.token/eosio.token.hpp>

namespace eosiosystem {

   const int64_t  min_pervote_daily_pay = 100'0000;
   //##YTA-Change  start:
   //Change total vote rate from 15% to 1% for test network
   //const int64_t  min_activated_stake   = 150'000'000'0000;
   const int64_t  min_activated_stake   = 5'000'0000;
   //##YTA-Change  end:
   const uint32_t blocks_per_year       = 52*7*24*2*3600;   // half seconds per year
   const uint32_t seconds_per_year      = 52*7*24*3600;
   const uint32_t blocks_per_day        = 2 * 24 * 3600;
   const uint32_t blocks_per_hour       = 2 * 3600;
   const uint64_t useconds_per_day      = 24 * 3600 * uint64_t(1000000);
   const uint64_t useconds_per_year     = seconds_per_year*1000000ll;

   const int64_t block_initial_timestamp = 1551369600ll;  // epoch year 2019.03.01    unix timestamp 1551369600s
   //yta seo total= yta_seo_year[i] * YTA_SEO_BASE
   const uint32_t YTA_SEO_BASE = 10'0000;
   const double YTA_PRECISION =10000.0000;
   const uint32_t yta_seo_year[62] = {
            1000, 900, 800, 700,
            600, 600, 500, 500,
            400, 400, 300, 300,
            200, 200, 200,
            100, 100, 100,
            90, 90, 90,
            80, 80, 80,
            70, 70, 70, 70,
            60, 60, 60, 60,
            50, 50, 50, 50, 50,
            40, 40, 40, 40, 40,
            30, 30, 30, 30, 30,
            20, 20, 20, 20, 20,
            10, 10, 10, 10, 10,
            9, 9, 9, 9, 9
    };

   void system_contract::onblock( block_timestamp timestamp, account_name producer ) {
      using namespace eosio;

      require_auth(N(eosio));

      /** until activated stake crosses this threshold no new rewards are paid */
      if( _gstate.total_activated_stake < min_activated_stake )
         return;

      if( _gstate.last_pervote_bucket_fill == 0 )  /// start the presses
         _gstate.last_pervote_bucket_fill = current_time();


      /**
       * At startup the initial producer may not be one that is registered / elected
       * and therefore there may be no producer object for them.
       */
      auto prod = _producers.find(producer);
      if ( prod != _producers.end() ) {
         _gstate.total_unpaid_blocks++;
         _producers.modify( prod, 0, [&](auto& p ) {
               p.unpaid_blocks++;
         });
      }

      //##YTA-Change  start:  
      if( timestamp.slot - _gstate.last_producer_schedule_update.slot > 120 ) {
         for( auto it = _producersext.begin(); it != _producersext.end(); it++ ) {
            _producersext.modify( it, 0, [&](auto& p ) {
                  p.unpaid_base_cnt++;
            });
            _gstateex.total_unpaid_base_cnt++;  
         }
      }
      //##YTA-Change  end:  


      //##YTA-Change  start:         
      /// only update block producers once every minute, block_timestamp is in half seconds
      //if( timestamp.slot - _gstate.last_producer_schedule_update.slot > 120 ) {
      /// update block producers once every two minute due to election strategy is more complex than before
      if( timestamp.slot - _gstate.last_producer_schedule_update.slot > 120 ) {
      //##YTA-Change  end:         

         update_elected_producers( timestamp );
         //update_elected_producers_yta( timestamp );


         if( (timestamp.slot - _gstate.last_name_close.slot) > blocks_per_day ) {
            name_bid_table bids(_self,_self);
            auto idx = bids.get_index<N(highbid)>();
            auto highest = idx.begin();
            if( highest != idx.end() &&
                highest->high_bid > 0 &&
                highest->last_bid_time < (current_time() - useconds_per_day) &&
                _gstate.thresh_activated_stake_time > 0 &&
                (current_time() - _gstate.thresh_activated_stake_time) > 14 * useconds_per_day ) {
                   _gstate.last_name_close = timestamp;
                   idx.modify( highest, 0, [&]( auto& b ){
                         b.high_bid = -b.high_bid;
               });
            }
         }
      }
   }

   using namespace eosio;
   void system_contract::claimrewards( const account_name& owner ) {
      require_auth(_self);
      //require_auth(owner);

      //@@@@@@@@@@@@@@@@@@@@

      const auto& prod = _producers.get( owner );
      eosio_assert( prod.active(), "producer does not have an active key" );

      eosio_assert( _gstate.total_activated_stake >= min_activated_stake,
                    "cannot claim rewards until the chain is activated (at least 15% of all tokens participate in voting)" );
      
      auto ct = current_time();

      eosio_assert( ct - prod.last_claim_time > useconds_per_day, "already claimed rewards within past day" );

      const asset token_supply   = token( N(eosio.token)).get_supply(symbol_type(system_token_symbol).name() );
      const auto usecs_since_last_fill = ct - _gstate.last_pervote_bucket_fill;

      print("usecs_since_last_fill: ", usecs_since_last_fill, "\n");   
      print("_gstate.last_pervote_bucket_fill: ", _gstate.last_pervote_bucket_fill, "\n");
      print("now(): ", now(), "\n");
       
      int idx_year = (int)((now()- block_initial_timestamp) / seconds_per_year);
      auto seo_token = yta_seo_year[idx_year] * YTA_SEO_BASE;
       
      print("idx_year: ", idx_year, "\n");
      print("yta_seo_year[idx_year]: ", yta_seo_year[idx_year], "\n");
      print( "token_supply: ", token_supply, "\n");
      print("seo_token: ", seo_token, "\n");
		 
      if( usecs_since_last_fill > 0 && _gstate.last_pervote_bucket_fill > 0 ) {
         auto new_tokens = static_cast<int64_t>(seo_token * YTA_PRECISION * double(usecs_since_last_fill)/double(useconds_per_year));
         print("new_token: ", new_tokens, "\n");
         auto to_per_base_pay    = new_tokens / 5;
         auto to_producers       = new_tokens - to_per_base_pay;
         auto to_per_block_pay   = to_producers / 4;
         auto to_per_vote_pay    = to_producers - to_per_block_pay;

         INLINE_ACTION_SENDER(eosio::token, issue)( N(eosio.token), {{N(eosio),N(active)}},
                                                    {N(eosio), asset(new_tokens), std::string("issue tokens for producer pay")} );

         INLINE_ACTION_SENDER(eosio::token, transfer)( N(eosio.token), {N(eosio),N(active)},
                                                       { N(eosio), N(hddbasefound), asset(to_per_base_pay), "fund per-base bucket" } );

         INLINE_ACTION_SENDER(eosio::token, transfer)( N(eosio.token), {N(eosio),N(active)},
                                                       { N(eosio), N(eosio.bpay), asset(to_per_block_pay), "fund per-block bucket" } );

         INLINE_ACTION_SENDER(eosio::token, transfer)( N(eosio.token), {N(eosio),N(active)},
                                                       { N(eosio), N(eosio.vpay), asset(to_per_vote_pay), "fund per-vote bucket" } );

         _gstateex.perbase_bucket   += to_per_base_pay;
         _gstate.pervote_bucket     += to_per_vote_pay;
         _gstate.perblock_bucket    += to_per_block_pay;

         _gstate.last_pervote_bucket_fill = ct;
      }

      const auto& prodext = _producersext.get( owner );

      int64_t producer_per_base_pay = 0;
      if( _gstateex.total_unpaid_base_cnt > 0 ) {
         producer_per_base_pay = (_gstateex.perbase_bucket * prodext.unpaid_base_cnt) / _gstateex.total_unpaid_base_cnt;
      }

      int64_t producer_per_block_pay = 0;
      if( _gstate.total_unpaid_blocks > 0 ) {
         producer_per_block_pay = (_gstate.perblock_bucket * prod.unpaid_blocks) / _gstate.total_unpaid_blocks;
      }
      int64_t producer_per_vote_pay = 0;
      if( _gstate.total_producer_vote_weight > 0 ) {
         producer_per_vote_pay  = int64_t((_gstate.pervote_bucket * prod.total_votes ) / _gstate.total_producer_vote_weight);
      }
      if( producer_per_vote_pay < min_pervote_daily_pay ) {
         producer_per_vote_pay = 0;
      }

      _gstateex.perbase_bucket    -= producer_per_base_pay;
      _gstate.pervote_bucket      -= producer_per_vote_pay;
      _gstate.perblock_bucket     -= producer_per_block_pay;
      _gstate.total_unpaid_blocks -= prod.unpaid_blocks;
      _gstateex.total_unpaid_base_cnt -= prodext.unpaid_base_cnt;

      _producers.modify( prod, 0, [&](auto& p) {
          p.last_claim_time = ct;
          p.unpaid_blocks = 0;
      });

      _producersext.modify( prodext, 0, [&](auto& p) {
          p.unpaid_base_cnt = 0;
      });      

      if( producer_per_base_pay > 0 ) {
         INLINE_ACTION_SENDER(eosio::token, transfer)( N(eosio.token), {N(hddbasefound),N(active)},
                                                       { N(hddbasefound), owner, asset(producer_per_base_pay), std::string("producer base pay") } );
      }
      if( producer_per_block_pay > 0 ) {
         INLINE_ACTION_SENDER(eosio::token, transfer)( N(eosio.token), {N(eosio.bpay),N(active)},
                                                       { N(eosio.bpay), owner, asset(producer_per_block_pay), std::string("producer block pay") } );
      }
      if( producer_per_vote_pay > 0 ) {
         INLINE_ACTION_SENDER(eosio::token, transfer)( N(eosio.token), {N(eosio.vpay),N(active)},
                                                       { N(eosio.vpay), owner, asset(producer_per_vote_pay), std::string("producer vote pay") } );
      }
   }


   void system_contract::rewardprods( ) {
      require_auth(_self);

      auto ct = current_time();
      //eosio_assert( ct - _gstateex.last_claim_time > useconds_per_day, "already claimed rewards within past day" );
      _gstateex.last_claim_time = ct;
      const auto usecs_since_last_fill = ct - _gstate.last_pervote_bucket_fill;
      int idx_year = (int)((now()- block_initial_timestamp) / seconds_per_year);
      auto seo_token = yta_seo_year[idx_year] * YTA_SEO_BASE;

      if( usecs_since_last_fill > 0 && _gstate.last_pervote_bucket_fill > 0 ) {
         auto new_tokens = static_cast<int64_t>(seo_token * YTA_PRECISION * double(usecs_since_last_fill)/double(useconds_per_year));
         print("new_token: ", new_tokens, "\n");
         auto to_per_base_pay    = new_tokens / 5;
         auto to_producers       = new_tokens - to_per_base_pay;
         auto to_per_block_pay   = to_producers / 4;
         auto to_per_vote_pay    = to_producers - to_per_block_pay;


         INLINE_ACTION_SENDER(eosio::token, issue)( N(eosio.token), {{N(eosio),N(active)}},
                                                    {N(eosio), asset(new_tokens), std::string("issue tokens for producer pay")} );


         INLINE_ACTION_SENDER(eosio::token, transfer)( N(eosio.token), {N(eosio),N(active)},
                                                       { N(eosio), N(hddbasefound), asset(to_per_base_pay), "fund per-base bucket" } );


         INLINE_ACTION_SENDER(eosio::token, transfer)( N(eosio.token), {N(eosio),N(active)},
                                                       { N(eosio), N(eosio.bpay), asset(to_per_block_pay), "fund per-block bucket" } );


         INLINE_ACTION_SENDER(eosio::token, transfer)( N(eosio.token), {N(eosio),N(active)},
                                                       { N(eosio), N(eosio.vpay), asset(to_per_vote_pay), "fund per-vote bucket" } );


         _gstateex.perbase_bucket   += to_per_base_pay;
         _gstate.pervote_bucket     += to_per_vote_pay;
         _gstate.perblock_bucket    += to_per_block_pay;

         _gstate.last_pervote_bucket_fill = ct;
      }

      int64_t producer_total_base_pay = _gstateex.perbase_bucket;
      int64_t producer_total_block_pay = _gstate.perblock_bucket;
      int64_t producer_total_vote_pay = _gstate.pervote_bucket;

      print("producer_total_base_pay -- ", producer_total_base_pay , "\n");
      print("producer_total_block_pay -- ", producer_total_block_pay , "\n");
      print("producer_total_vote_pay -- ", producer_total_vote_pay , "\n");


      int64_t producer_already_base_pay = 0;
      int64_t producer_already_block_pay = 0;
      int64_t producer_already_vote_pay = 0;


      uint32_t total_unpaid_blocks  =  _gstate.total_unpaid_blocks;
      uint32_t total_unpaid_base_cnt = _gstateex.total_unpaid_base_cnt;

      for( auto it = _producers.begin(); it != _producers.end(); it++ ) {
         if(!(it->active()))
            continue;
         auto& prodex = _producersext.get(it->owner);
         int64_t producer_per_base_pay = 0;
         if( _gstateex.total_unpaid_base_cnt > 0 ) {
            producer_per_base_pay = (_gstateex.perbase_bucket * prodex.unpaid_base_cnt) / total_unpaid_base_cnt;
         }
         int64_t producer_per_block_pay = 0;
         if( _gstate.total_unpaid_blocks > 0 ) {
            producer_per_block_pay = (_gstate.perblock_bucket * it->unpaid_blocks) / total_unpaid_blocks;
         }
         int64_t producer_per_vote_pay = 0;
         if( _gstate.total_producer_vote_weight > 0 ) {
            producer_per_vote_pay  = int64_t((_gstate.pervote_bucket * it->total_votes ) / _gstate.total_producer_vote_weight);
         }

         print("producer_per_base_pay -- ", producer_per_base_pay , "\n");
         print("producer_per_block_pay -- ", producer_per_block_pay , "\n");
         print("producer_per_vote_pay -- ", producer_per_vote_pay , "\n");


         _gstate.total_unpaid_blocks -= it->unpaid_blocks;
         _gstateex.total_unpaid_base_cnt -= prodex.unpaid_base_cnt;

         _producers.modify( it, 0, [&](auto& p) {
             p.last_claim_time = ct;
             p.unpaid_blocks = 0;
         });

         _producersext.modify( prodex, 0, [&](auto& p) {
             p.unpaid_base_cnt = 0;
         });   

         
         if( producer_per_base_pay > 0 && ((producer_per_base_pay + producer_already_base_pay) <= producer_total_base_pay) ) {
            producer_already_base_pay += producer_per_base_pay;
            INLINE_ACTION_SENDER(eosio::token, transfer)( N(eosio.token), {N(hddbasefound),N(active)},
                                                          { N(hddbasefound), it->owner, asset(producer_per_base_pay), std::string("producer base pay") } );
         }

         if( producer_per_block_pay > 0 && ((producer_per_block_pay + producer_already_block_pay) <= producer_total_block_pay) ) {
            producer_already_block_pay += producer_per_block_pay;            
            INLINE_ACTION_SENDER(eosio::token, transfer)( N(eosio.token), {N(eosio.bpay),N(active)},
                                                        { N(eosio.bpay), it->owner, asset(producer_per_block_pay), std::string("producer block pay") } );
         }

         if( producer_per_vote_pay > 0 && ((producer_per_vote_pay + producer_already_vote_pay) <= producer_total_vote_pay) ) {
            producer_already_vote_pay += producer_per_vote_pay;            
            INLINE_ACTION_SENDER(eosio::token, transfer)( N(eosio.token), {N(eosio.vpay),N(active)},
                                                       { N(eosio.vpay), it->owner, asset(producer_per_vote_pay), std::string("producer vote pay") } );
         }
         
      }
      

      _gstateex.perbase_bucket    -= producer_already_base_pay;
      _gstate.pervote_bucket      -= producer_already_vote_pay;
      _gstate.perblock_bucket     -= producer_already_block_pay;

      if(_gstateex.perbase_bucket < 0 )
         _gstateex.perbase_bucket = 0;

      if(_gstate.pervote_bucket < 0 )
         _gstate.pervote_bucket = 0;

      if(_gstate.perblock_bucket < 0 )
         _gstate.perblock_bucket = 0;

   }


} //namespace eosiosystem
