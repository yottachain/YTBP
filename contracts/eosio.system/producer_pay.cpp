#include "eosio.system.hpp"
#include <eosiolib/print.hpp>
#include <eosio.token/eosio.token.hpp>
#include <hddpool/hddpool.hpp>

namespace eosiosystem {

   const int64_t  min_pervote_daily_pay = 100'0000;
   //##YTA-Change  start:
   //Change total vote rate from 15% to 1% for test network
   //const int64_t  min_activated_stake   = 150'000'000'0000;
   //const int64_t  min_activated_stake   = 5'000'0000;
   const int64_t  min_activated_stake   = 0;
   //##YTA-Change  end:
   const uint32_t blocks_per_year       = 360*24*2*3600;   // half seconds per year
   const uint32_t seconds_per_year      = 360*24*3600;
   const uint32_t blocks_per_day        = 2 * 24 * 3600;
   const uint32_t blocks_per_hour       = 2 * 3600;
   const uint64_t useconds_per_day      = 24 * 3600 * uint64_t(1000000);
   const uint64_t useconds_per_year     = seconds_per_year*1000000ll;

   const int64_t block_initial_timestamp = 1578621600ll;  // epoch year 2020.01.10 10:00    unix timestamp 1578621600s
   
   //yta seo total= yta_seo_year[i] * YTA_SEO_BASE
   const uint32_t YTA_SEO_BASE = 10'0000;
   const double YTA_PRECISION =10000.0000;
   const uint32_t yta_seo_year[12] = {
            300, 300, 300, 300,
            300, 300, 300, 300,
            300, 300, 300, 300
    };

   void system_contract::onblock( block_timestamp timestamp, account_name producer ) {
      using namespace eosio;

      require_auth(N(eosio));

      ((void)producer);

      //##YTA-Change  start:         
      /// only update block producers once every minute, block_timestamp is in half seconds
      //if( timestamp.slot - _gstate.last_producer_schedule_update.slot > 120 ) {
      /// update block producers once every two minute due to election strategy is more complex than before
      if( timestamp.slot - _gstate.last_producer_schedule_update.slot > 240 ) {
      //##YTA-Change  end:         

         //update_elected_producers( timestamp );
         update_elected_producers_yta( timestamp );


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
      } else {
         if( timestamp.slot >= hddpool::get_next_reward_slot() ) {
            action(
               permission_level{N(hddpool12345), N(active)},
               N(hddpool12345), N(onrewardt),
               std::make_tuple(timestamp.slot )).send();
         }
      }
   }

   using namespace eosio;
   void system_contract::claimrewards( const account_name& owner ) {
      
      eosio_assert(1 == 2, "can not claimrewards by self");
      return;
      
      ((void)owner);

      require_auth(_self);
      //require_auth(owner);
   }

   void system_contract::startreward( ) {
      require_auth(N(ytarewardusr));

      eosio_assert( _gstate.last_pervote_bucket_fill == 0, "already start reward");
      all_prods_singleton _all_prods(_self, _self);
      all_prods_level     _all_prods_state;

      eosio_assert ( _all_prods.exists(), "can not start reward" );

      _all_prods_state = _all_prods.get();

      eosio_assert( _all_prods_state.prods_l1.size() >= 21, "main susper producers not enough" );
      eosio_assert( _all_prods_state.prods_l2.size() >= 42, "super producers not enouth" );

      _gstate.last_pervote_bucket_fill = current_time();

   }

   void system_contract::rewardprods( ) {
      require_auth(N(ytarewardusr));

      if( _gstate.last_pervote_bucket_fill == 0 ) 
         return;

      all_prods_singleton _all_prods(_self, _self);
      all_prods_level     _all_prods_state;

      if (!_all_prods.exists())
         return;

      _all_prods_state = _all_prods.get();

      int64_t num_main_producers = _all_prods_state.prods_l1.size();
      int64_t num_producers = _all_prods_state.prods_l1.size() + _all_prods_state.prods_l2.size();
      if(num_main_producers < 1)
         return;


      auto ct = current_time();
      //eosio_assert( ct - _gstateex.last_claim_time > useconds_per_day, "already claimed rewards within past day" );
      _gstateex.last_claim_time = ct;
      const auto usecs_since_last_fill = ct - _gstate.last_pervote_bucket_fill;
      int idx_year = (int)((now()- block_initial_timestamp) / seconds_per_year);
      auto seo_token = yta_seo_year[idx_year] * YTA_SEO_BASE;

      if( usecs_since_last_fill > 0 && _gstate.last_pervote_bucket_fill > 0 ) {
         auto new_tokens = static_cast<int64_t>(seo_token * YTA_PRECISION * double(usecs_since_last_fill)/double(useconds_per_year));
         print("new_token: ", new_tokens, "\n");
         auto to_per_base_pay    = static_cast<int64_t>((new_tokens * 3) / 5);
         auto to_producers       = new_tokens - to_per_base_pay;
         auto to_per_block_pay   = to_producers / 2;
         auto to_per_vote_pay    = to_producers - to_per_block_pay;


         INLINE_ACTION_SENDER(eosio::token, issue)( N(eosio.token), {{N(eosio),N(active)}},
                                                    {N(eosio), asset(new_tokens), std::string("issue tokens for producer pay")} );

         INLINE_ACTION_SENDER(eosio::token, transfer)( N(eosio.token), {N(eosio),N(active)},
                                                       { N(eosio), N(hddbasefound), asset(new_tokens), "fund total bucket" } );

         _gstateex.perbase_bucket   += to_per_base_pay;
         _gstate.pervote_bucket     += to_per_vote_pay;
         _gstate.perblock_bucket    += to_per_block_pay;

         _gstate.last_pervote_bucket_fill = ct;
      }

      int64_t producer_total_base_pay = _gstateex.perbase_bucket;
      int64_t producer_total_block_pay = _gstate.perblock_bucket;
      int64_t producer_total_vote_pay = _gstate.pervote_bucket;
      int64_t producer_total_pay = producer_total_base_pay + producer_total_block_pay + producer_total_vote_pay;

      print("producer_total_base_pay -- ", producer_total_base_pay , "\n");
      print("producer_total_block_pay -- ", producer_total_block_pay , "\n");
      print("producer_total_vote_pay -- ", producer_total_vote_pay , "\n");

      int64_t producer_already_base_pay = 0;
      int64_t producer_already_block_pay = 0;
      int64_t producer_already_vote_pay = 0;

      int64_t producer_already_total_pay = 0;

      double total_vote_weight = 0;
      for( auto it =_all_prods_state.prods_l1.begin(); it != _all_prods_state.prods_l1.end(); it++) {
         total_vote_weight += it->total_votes;
      }
      for( auto it =_all_prods_state.prods_l2.begin(); it != _all_prods_state.prods_l2.end(); it++) {
         total_vote_weight += it->total_votes;
      }

      for( auto it =_all_prods_state.prods_l1.begin(); it != _all_prods_state.prods_l1.end(); it++) {
         int64_t producer_per_base_pay = 0;
         producer_per_base_pay =  _gstateex.perbase_bucket / num_producers;

         int64_t producer_per_block_pay = 0;
         producer_per_block_pay = _gstate.perblock_bucket / num_main_producers;

         int64_t producer_per_vote_pay = 0;
         if( total_vote_weight > 0 ) {
            producer_per_vote_pay  = int64_t((_gstate.pervote_bucket * it->total_votes ) / total_vote_weight);
         }

         print("producer_per_base_pay -- ", producer_per_base_pay , "\n");
         print("producer_per_block_pay -- ", producer_per_block_pay , "\n");
         print("producer_per_vote_pay -- ", producer_per_vote_pay , "\n");

         int64_t producer_per_total_pay = 0;
         producer_per_total_pay = producer_per_base_pay + producer_per_block_pay + producer_per_vote_pay;

         if( producer_per_total_pay > 0 && ((producer_per_total_pay + producer_already_total_pay) <= producer_total_pay) ) {
            producer_already_total_pay += producer_per_total_pay;
            producer_already_base_pay += producer_per_base_pay;
            producer_already_block_pay += producer_per_block_pay;
            producer_already_vote_pay += producer_per_vote_pay;
            INLINE_ACTION_SENDER(eosio::token, transfer)( N(eosio.token), {N(hddbasefound),N(active)},
                                                          { N(hddbasefound), it->owner, asset(producer_per_total_pay), std::string("main producer daily pay") } );
         }

      }


      for( auto it =_all_prods_state.prods_l2.begin(); it != _all_prods_state.prods_l2.end(); it++) {
         int64_t producer_per_base_pay = 0;
         producer_per_base_pay =  _gstateex.perbase_bucket / num_producers;

         int64_t producer_per_vote_pay = 0;
         if( total_vote_weight > 0 ) {
            producer_per_vote_pay  = int64_t((_gstate.pervote_bucket * it->total_votes ) / total_vote_weight);
         }

         int64_t producer_per_block_pay = 0;

         print("producer_per_base_pay -- ", producer_per_base_pay , "\n");
         print("producer_per_block_pay -- ", producer_per_block_pay , "\n");
         print("producer_per_vote_pay -- ", producer_per_vote_pay , "\n");

         int64_t producer_per_total_pay = 0;
         producer_per_total_pay = producer_per_base_pay + producer_per_block_pay + producer_per_vote_pay;

         if( producer_per_total_pay > 0 && ((producer_per_total_pay + producer_already_total_pay) <= producer_total_pay) ) {
            producer_already_total_pay += producer_per_total_pay;
            producer_already_base_pay += producer_per_base_pay;
            producer_already_block_pay += producer_per_block_pay;
            producer_already_vote_pay += producer_per_vote_pay;
            INLINE_ACTION_SENDER(eosio::token, transfer)( N(eosio.token), {N(hddbasefound),N(active)},
                                                          { N(hddbasefound), it->owner, asset(producer_per_total_pay), std::string("producer daily pay") } );
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
