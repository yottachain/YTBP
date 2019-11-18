/**
 *  @file
 *  @copyright defined in eos/LICENSE
 */
#include "eosio.system.hpp"

#include <eosiolib/eosio.hpp>
#include <eosiolib/crypto.h>
#include <eosiolib/print.hpp>
#include <eosiolib/datastream.hpp>
#include <eosiolib/serialize.hpp>
#include <eosiolib/multi_index.hpp>
#include <eosiolib/privileged.hpp>
#include <eosiolib/singleton.hpp>
#include <eosiolib/transaction.hpp>
//#include <eosio.token/eosio.token.hpp>
//#include <hdddeposit/hdddeposit.hpp>

#include <algorithm>
#include <cmath>

const uint64_t useconds_per_day_v      = 24 * 3600 * uint64_t(1000000);
//const uint64_t useconds_per_day_v      = 10 * uint64_t(1000000);
#define LEVEL_ONE_MAX 21
#define LEVEL_TWO_MAX 105

namespace eosiosystem {
   using eosio::indexed_by;
   using eosio::const_mem_fun;
   using eosio::bytes;
   using eosio::print;
   using eosio::singleton;
   using eosio::transaction;

   /**
    *  This method will create a producer_config and producer_info object for 'producer'
    *
    *  @pre producer is not already registered
    *  @pre producer to register is an account
    *  @pre authority of producer to register
    *
    */
   void system_contract::regproducer( const account_name producer, const eosio::public_key& producer_key, const std::string& url, uint16_t location ) {
      eosio_assert( url.size() < 512, "url too long" );
      eosio_assert( producer_key != eosio::public_key(), "public key should not be the default value" );

      //require_auth( _self );
      require_auth(N(ytaadminuser));


      auto prod = _producers.find( producer );

      if ( prod != _producers.end() ) {
         _producers.modify( prod, _self, [&]( producer_info& info ){
               info.producer_key = producer_key;
               info.is_active    = true;
               info.url          = url;
               info.location     = location;
            });
         
         change_producer_yta_info(producer, producer_key, true);
      } else {
         _producers.emplace( _self, [&]( producer_info& info ){
               info.owner         = producer;
               info.total_votes   = 0;
               info.producer_key  = producer_key;
               info.is_active     = true;
               info.url           = url;
               info.location      = location;
         });
      }

   }

   void system_contract::unregprod( const account_name producer ) {
      eosio_assert(1 == 2, "unregprod is disabled");
      return;

      require_auth( producer );
      const auto& prod = _producers.get( producer, "producer not found" );
      _producers.modify( prod, 0, [&]( producer_info& info ){
            info.deactivate();
      });

      change_producer_yta_info(producer, public_key(), false);
   }
//##YTA-Change  start: 
   void system_contract::clsprods2() {
      //require_auth( _self );
      require_auth(N(ytaadminuser));

      while (_producersext.begin() != _producersext.end()) {
         auto it = _producersext.begin();
         //it->out_votes
         const auto& prod = _producers.get( it->owner, "producer not found" );
         _producers.modify( prod, 0, [&]( producer_info& info ){
            info.total_votes -= it->out_votes;
         });
         _gstate.total_producer_vote_weight -= it->out_votes;
         _producersext.erase(_producersext.begin()); 
      }

      _gstateex.total_unpaid_base_cnt = 0;

      all_prods_singleton _all_prods(_self, _self);
      all_prods_level     _all_prods_state;

      if (_all_prods.exists()) {
         _all_prods_state = _all_prods.get();
         _all_prods_state.prods_l1.clear();
         _all_prods_state.prods_l2.clear();
         _all_prods.set(_all_prods_state,_self);
      }

   }

   void system_contract::delproducer( const account_name producer ) {
      auto itp = _producers.find(producer);
      if( itp != _producers.end() ) {
         _gstate.total_unpaid_blocks -= itp->unpaid_blocks;
         _gstate.total_producer_vote_weight -= itp->total_votes;
         _producers.erase(itp);
      }

      auto itpex = _producersext.find(producer);
      if( itpex != _producersext.end() ) {
         _gstateex.total_unpaid_base_cnt -= itpex->unpaid_base_cnt;
         _producersext.erase(itpex);
      } else {
         return;
      }
      rm_producer_yta(producer);
   }

   void system_contract::seqproducer( const account_name producer, const account_name shadow, uint16_t seq , uint8_t level ) {
      //require_auth( _self );
      require_auth(N(ytaadminuser));
      
      //const auto& prod = _producers.get( producer, "producer not found" );
      auto itp = _producers.find(producer);
      eosio_assert( itp != _producers.end() , "producer not found");      

      eosio_assert(seq >= 1 && seq <= 21 , "invalidate seq number");
      eosio_assert(level >= 1 && level <= 3 , "invalidate level number");
      //const auto& prod = _producers.get( producer, "producer not found" );
      
      auto it = _producersext.find(producer);
      if (it == _producersext.end()) {
         _producersext.emplace(_self, [&](auto &row) {
            row.owner = producer;
            row.seq_num = seq;
            row.level = level;
            row.shadow = shadow;            
         });
         add_producer_yta(producer, level);
      } else {
         _producersext.modify(it, _self, [&](auto &row) {
            row.seq_num = seq;
            row.level = level;
            row.shadow = shadow;            
         });
         rm_producer_yta(producer);
         add_producer_yta(producer, level);
      }
   }

   void system_contract::rm_producer_yta(const account_name producer) {

      bool is_all_prods_level_change = false;
      all_prods_singleton _all_prods(_self, _self);
      all_prods_level     _all_prods_state;
      if (_all_prods.exists()) {
         do {
            _all_prods_state = _all_prods.get();
            for( auto it1=  _all_prods_state.prods_l1.begin(); it1 !=  _all_prods_state.prods_l1.end(); it1++ ) {
               if(it1->owner == producer) {
                  _all_prods_state.prods_l1.erase(it1);
                  is_all_prods_level_change = true;
                  break;
               } 
            }
            if(is_all_prods_level_change) 
               break;
            for( auto it2=  _all_prods_state.prods_l2.begin(); it2 !=  _all_prods_state.prods_l2.end(); it2++ ) {
               if(it2->owner == producer) {
                  _all_prods_state.prods_l2.erase(it2);
                  is_all_prods_level_change = true;
                  break;
               } 
            }
         } while(false);   

         if(is_all_prods_level_change) {
            _all_prods.set(_all_prods_state,_self);
         }
      }

   }

   void system_contract::add_producer_yta( const account_name producer, uint8_t level ) {
      
      //need retrive from system producers table
      const auto& prod = _producers.get( producer, "producer not found" );

      yta_prod_info prodyta;
      prodyta.grace_start_time = 0;
      prodyta.is_active = prod.is_active;
      prodyta.is_in_grace = false;
      prodyta.location = prod.location;
      prodyta.owner = producer;
      prodyta.producer_key = prod.producer_key;
      prodyta.total_votes = prod.total_votes;

      all_prods_singleton _all_prods(_self, _self);
      all_prods_level     _all_prods_state;
      if (_all_prods.exists()) {
         _all_prods_state = _all_prods.get();
      } else {
         _all_prods_state = all_prods_level{};
      }
      if(level == 1) {
         eosio_assert( _all_prods_state.prods_l1.size() < LEVEL_ONE_MAX, "too many level one bp");
         _all_prods_state.prods_l1.push_back(prodyta);
      } else if(level == 2) {
         eosio_assert( _all_prods_state.prods_l2.size() < LEVEL_TWO_MAX, "too many level two bp");
         _all_prods_state.prods_l2.push_back(prodyta);
      } 
      _all_prods.set(_all_prods_state,_self);
   }

   void system_contract::change_producer_yta_info( const account_name producer, const eosio::public_key& producer_key, bool isactive) {
      
      if(!isactive) {
         delproducer(producer);
         return;
      }

      bool is_all_prods_level_change = false;
      all_prods_singleton _all_prods(_self, _self);
      all_prods_level     _all_prods_state;
      if (_all_prods.exists()) {
         do {
            _all_prods_state = _all_prods.get();
            for( auto it1=  _all_prods_state.prods_l1.begin(); it1 !=  _all_prods_state.prods_l1.end(); it1++ ) {
               if(it1->owner == producer) {
                  it1->is_active = isactive;
                  it1->producer_key = producer_key;
                  is_all_prods_level_change = true;
                  break;
               } 
            }
            if(is_all_prods_level_change) 
               break;
            for( auto it2=  _all_prods_state.prods_l2.begin(); it2 !=  _all_prods_state.prods_l2.end(); it2++ ) {
               if(it2->owner == producer) {
                  it2->is_active = isactive;
                  it2->producer_key = producer_key;
                  is_all_prods_level_change = true;
                  break;
               } 
            }
         } while(false);

         if(is_all_prods_level_change) {
            _all_prods.set(_all_prods_state,_self);
         }
      }

   }

   void system_contract::update_producers_yta_totalvotes( account_name owner, double total_votes) {

      bool is_all_prods_level_change = false;
      all_prods_singleton _all_prods(_self, _self);
      all_prods_level     _all_prods_state;
      if (_all_prods.exists()) {
         do {
            _all_prods_state = _all_prods.get();
            for( auto it1=  _all_prods_state.prods_l1.begin(); it1 !=  _all_prods_state.prods_l1.end(); it1++ ) {
               if(it1->owner == owner) {
                  it1->total_votes = total_votes;
                  is_all_prods_level_change = true;
                  break;
               } 
            }
            if(is_all_prods_level_change) 
               break;
            for( auto it2=  _all_prods_state.prods_l2.begin(); it2 !=  _all_prods_state.prods_l2.end(); it2++ ) {
               if(it2->owner == owner) {
                  it2->total_votes = total_votes;
                  is_all_prods_level_change = true;
                  break;
               } 
            }
         } while(false);

         if(is_all_prods_level_change) {
            _all_prods.set(_all_prods_state,_self);
         }
      }
   }

   void system_contract::reprodlevel() {
      //require_auth( _self );
      require_auth(N(ytarewardusr));

      update_producer_level();
   }

   void system_contract::update_producer_level() {

      all_prods_singleton _all_prods(_self, _self);
      all_prods_level     _all_prods_state;

      if (!_all_prods.exists())
         return;

      _all_prods_state = _all_prods.get();

      bool ischanged = false;

      for( auto it =_all_prods_state.prods_l1.begin(); it != _all_prods_state.prods_l1.end();) {
         bool is_remove = false;
         if(!it->is_active)
            is_remove = true;

         if((int64_t)it->total_votes < 50000000000) {
            if(it->is_in_grace) {
               if(current_time() - it->grace_start_time > useconds_per_day_v) {
                  is_remove = true;
                  it->is_in_grace = false;
                  ischanged = true;
               }
               
            } else {
               it->is_in_grace = true;
               auto ct = current_time();
               it->grace_start_time =  ct;
               ischanged = true;
            }
         } else {
            if(it->is_in_grace) {
               ischanged = true;
               it->is_in_grace = false;
            }
         }

         if(is_remove) {
            //_all_prods_state.prods_l3.push_back(*it);
            it = _all_prods_state.prods_l1.erase(it);
            ischanged = true;
         } else {
            ++it;
         }
      }


      for( auto it =_all_prods_state.prods_l2.begin(); it != _all_prods_state.prods_l2.end();) {
         bool is_remove = false;
         if(!it->is_active)
            is_remove = true;
         if((int64_t)it->total_votes < 20000000000) {
            if(it->is_in_grace) {
               if(current_time() - it->grace_start_time > useconds_per_day_v) {
                  is_remove = true;
                  it->is_in_grace = false;
                  ischanged = true;

               }
               
            } else {
               it->is_in_grace = true;
               auto ct = current_time();
               it->grace_start_time =  ct;
               ischanged = true;

            }
         } else {
            if(it->is_in_grace) {
               ischanged = true;
               it->is_in_grace = false;
            }
         }

         if(is_remove) {
            //_all_prods_state.prods_l3.push_back(*it);
            it =  _all_prods_state.prods_l2.erase(it);
            ischanged = true;
         } else {
            ++it;
         }
      }

      if(_all_prods_state.prods_l1.size() < LEVEL_ONE_MAX) {
         std::sort(_all_prods_state.prods_l2.begin(), _all_prods_state.prods_l2.end(), [&](yta_prod_info lhs, yta_prod_info rhs){return lhs.total_votes > rhs.total_votes;}); 
         for( auto it =_all_prods_state.prods_l2.begin(); it != _all_prods_state.prods_l2.end();) {
            if((int64_t)it->total_votes >= 50000000000) {
               if(_all_prods_state.prods_l1.size() < LEVEL_ONE_MAX) {
                  _all_prods_state.prods_l1.push_back(*it);
                  it = _all_prods_state.prods_l2.erase(it);
                  ischanged = true;
               } else {
                  break;
               }
            } else {
               ++it;
            }
         }
      }     
      
      if(ischanged) 
      {
         _all_prods.set(_all_prods_state, _self);
      }

   }

   void system_contract::update_elected_producers_yta( block_timestamp block_time ) {
 
      all_prods_singleton _all_prods(_self, _self);
      all_prods_level     _all_prods_state;

      if (!_all_prods.exists())
         return;

      _all_prods_state = _all_prods.get();

      _gstate.last_producer_schedule_update = block_time;

      std::vector< std::pair<eosio::producer_key,uint16_t> > top_producers;
      top_producers.reserve(21);

      for( auto it =_all_prods_state.prods_l1.begin(); it != _all_prods_state.prods_l1.end(); it++ ) {
         top_producers.emplace_back( std::pair<eosio::producer_key,uint16_t>({{it->owner, it->producer_key}, it->location}) );
      }

      if(top_producers.size() < 5)
         return;

      if ( top_producers.size() < _gstate.last_producer_schedule_size ) {
         //if(top_producers.size() < 15)
            return;
      }

      /// sort by producer name
      std::sort( top_producers.begin(), top_producers.end() );

      std::vector<eosio::producer_key> producers;

      producers.reserve(top_producers.size());
      for( const auto& item : top_producers )
         producers.push_back(item.first);

      bytes packed_schedule = pack(producers);


      if( set_proposed_producers( packed_schedule.data(),  packed_schedule.size() ) >= 0 ) {
         _gstate.last_producer_schedule_size = static_cast<decltype(_gstate.last_producer_schedule_size)>( top_producers.size() );
      }


   }

//##YTA-Change  end:  

   void system_contract::update_elected_producers( block_timestamp block_time ) {
      _gstate.last_producer_schedule_update = block_time;

      auto idx = _producers.get_index<N(prototalvote)>();

      std::vector< std::pair<eosio::producer_key,uint16_t> > top_producers;
      top_producers.reserve(21);

      for ( auto it = idx.cbegin(); it != idx.cend() && top_producers.size() < 21 && 0 < it->total_votes && it->active(); ++it ) {
         top_producers.emplace_back( std::pair<eosio::producer_key,uint16_t>({{it->owner, it->producer_key}, it->location}) );
      }


      if ( top_producers.size() < _gstate.last_producer_schedule_size ) {
         return;
      }

      /// sort by producer name
      std::sort( top_producers.begin(), top_producers.end() );

      std::vector<eosio::producer_key> producers;

      producers.reserve(top_producers.size());
      for( const auto& item : top_producers )
         producers.push_back(item.first);

      bytes packed_schedule = pack(producers);


      if( set_proposed_producers( packed_schedule.data(),  packed_schedule.size() ) >= 0 ) {
         _gstate.last_producer_schedule_size = static_cast<decltype(_gstate.last_producer_schedule_size)>( top_producers.size() );
      }
   }

   double stake2vote( int64_t staked ) {
      /// TODO subtract 2080 brings the large numbers closer to this decade
      //double weight = int64_t( (now() - (block_timestamp::block_timestamp_epoch / 1000)) / (seconds_per_day * 7) )  / double( 52 );
      //return double(staked) * std::pow( 2, weight );
      return double(staked);
   }
   /**
    *  @pre producers must be sorted from lowest to highest and must be registered and active
    *  @pre if proxy is set then no producers can be voted for
    *  @pre if proxy is set then proxy account must exist and be registered as a proxy
    *  @pre every listed producer or proxy must have been previously registered
    *  @pre voter must authorize this action
    *  @pre voter must have previously staked some EOS for voting
    *  @pre voter->staked must be up to date
    *
    *  @post every producer previously voted for will have vote reduced by previous vote weight
    *  @post every producer newly voted for will have vote increased by new vote amount
    *  @post prior proxy will proxied_vote_weight decremented by previous vote weight
    *  @post new proxy will proxied_vote_weight incremented by new vote weight
    *
    *  If voting for a proxy, the producer votes will not change until the proxy updates their own vote.
    */
   void system_contract::voteproducer( const account_name voter_name, const account_name proxy, const std::vector<account_name>& producers ) {
      require_auth( voter_name );
      ///@@@@@@@@@@@@@@@@@@@@@
      //eosio_assert(1 == 2, "can not vote now.");
      //return;
      ///@@@@@@@@@@@@@@@@@@@@

      update_votes( voter_name, proxy, producers, true );
   }

//##YTA-Change  start:
   void system_contract::changevotes( const account_name voter_name ) {
         require_auth( voter_name );
         auto from_voter = _voters.find(voter_name);
         if( from_voter == _voters.end() ) {
            return;
         }
         if( from_voter->producers.size() || from_voter->proxy ) {
            update_votes( voter_name, from_voter->proxy, from_voter->producers, false );
         }
   }         
//##YTA-Change  end:

   void system_contract::update_votes( const account_name voter_name, const account_name proxy, const std::vector<account_name>& producers, bool voting ) {
      //validate input
      if ( proxy ) {
         eosio_assert( producers.size() == 0, "cannot vote for producers and proxy at same time" );
         eosio_assert( voter_name != proxy, "cannot proxy to self" );
         require_recipient( proxy );
      } else {
         //##YTA-Change  start:         
         //eosio_assert( producers.size() <= 30, "attempt to vote for too many producers" );
         // One voter can only vote for one producer
         eosio_assert( producers.size() <= 1, "attempt to vote for too many producers" );
         //##YTA-Change  end:
         for( size_t i = 1; i < producers.size(); ++i ) {
            eosio_assert( producers[i-1] < producers[i], "producer votes must be unique and sorted" );
         }
      }

      auto voter = _voters.find(voter_name);
      eosio_assert( voter != _voters.end(), "user must stake before they can vote" ); /// staking creates voter object
      eosio_assert( !proxy || !voter->is_proxy, "account registered as a proxy is not allowed to use a proxy" );

      /**
       * The first time someone votes we calculate and set last_vote_weight, since they cannot unstake until
       * after total_activated_stake hits threshold, we can use last_vote_weight to determine that this is
       * their first vote and should consider their stake activated.
       */
      if( voter->last_vote_weight <= 0.0 ) {
         _gstate.total_activated_stake += voter->staked;
         if( _gstate.total_activated_stake >= min_activated_stake && _gstate.thresh_activated_stake_time == 0 ) {
            _gstate.thresh_activated_stake_time = current_time();
         }
      }

      auto new_vote_weight = stake2vote( voter->staked );
      new_vote_weight += hdddeposit(hdd_deposit_account).get_deposit(voter_name).amount;
      if( voter->is_proxy ) {
         new_vote_weight += voter->proxied_vote_weight;
      }

      boost::container::flat_map<account_name, pair<double, bool /*new*/> > producer_deltas;
      if ( voter->last_vote_weight > 0 ) {
         if( voter->proxy ) {
            auto old_proxy = _voters.find( voter->proxy );
            eosio_assert( old_proxy != _voters.end(), "old proxy not found" ); //data corruption
            _voters.modify( old_proxy, 0, [&]( auto& vp ) {
                  vp.proxied_vote_weight -= voter->last_vote_weight;
               });
            propagate_weight_change( *old_proxy );
         } else {
            for( const auto& p : voter->producers ) {
               auto& d = producer_deltas[p];
               d.first -= voter->last_vote_weight;
               d.second = false;
            }
         }
      }

      if( proxy ) {
         auto new_proxy = _voters.find( proxy );
         eosio_assert( new_proxy != _voters.end(), "invalid proxy specified" ); //if ( !voting ) { data corruption } else { wrong vote }
         eosio_assert( !voting || new_proxy->is_proxy, "proxy not found" );
         if ( new_vote_weight >= 0 ) {
            _voters.modify( new_proxy, 0, [&]( auto& vp ) {
                  vp.proxied_vote_weight += new_vote_weight;
               });
            propagate_weight_change( *new_proxy );
         }
      } else {
         if( new_vote_weight >= 0 ) {
            for( const auto& p : producers ) {
               auto& d = producer_deltas[p];
               d.first += new_vote_weight;
               d.second = true;
            }
         }
      }

      account_name producer_not_fount = 0;

      for( const auto& pd : producer_deltas ) {
         double total_votes = 0;
         auto pitr = _producers.find( pd.first );
         if( pitr != _producers.end() ) {
            eosio_assert( !voting || pitr->active() || !pd.second.second /* not from new set */, "producer is not currently registered" );
            _producers.modify( pitr, 0, [&]( auto& p ) {
               p.total_votes += pd.second.first;
               if ( p.total_votes < 0 ) { // floating point arithmetics can give small negative numbers
                  p.total_votes = 0;
               }
               _gstate.total_producer_vote_weight += pd.second.first;
               //eosio_assert( p.total_votes >= 0, "something bad happened" );
               total_votes = p.total_votes;
            });
         } else {
            if(voting) {
               eosio_assert( !pd.second.second /* not from new set */, "producer is not registered" ); //data corruption
            }
            producer_not_fount = pd.first;
         }
         //##YTA-Change  start:
         auto pitr2 = _producersext.find( pd.first );
         if( pitr2 != _producersext.end() ) {
            //pitr2->seq_num   
            update_producers_yta_totalvotes(pd.first, total_votes); 
         }  else {
            if(voting) {
               eosio_assert( !pd.second.second /* not from new set */, "producer is not registered" ); //data corruption
            }
         }     
         //##YTA-Change  end:         
      }

      //delete the last missing producer 
      _voters.modify( voter, 0, [&]( auto& av ) {
         av.last_vote_weight = new_vote_weight;
         av.producers = producers;
         if(producer_not_fount) {
            for( auto it=  av.producers.begin(); it !=  av.producers.end(); it++ ) {
               if(*it == producer_not_fount) {
                  av.producers.erase(it);
                  break;
               } 
            }
         }
         av.proxy     = proxy;
      });
   }

   /**
    *  An account marked as a proxy can vote with the weight of other accounts which
    *  have selected it as a proxy. Other accounts must refresh their voteproducer to
    *  update the proxy's weight.
    *
    *  @param isproxy - true if proxy wishes to vote on behalf of others, false otherwise
    *  @pre proxy must have something staked (existing row in voters table)
    *  @pre new state must be different than current state
    */
   void system_contract::regproxy( const account_name proxy, bool isproxy ) {
      
      eosio_assert(1 == 2, "proxy not supported.");

      require_auth( proxy );

      auto pitr = _voters.find(proxy);
      if ( pitr != _voters.end() ) {
         eosio_assert( isproxy != pitr->is_proxy, "action has no effect" );
         eosio_assert( !isproxy || !pitr->proxy, "account that uses a proxy is not allowed to become a proxy" );
         _voters.modify( pitr, 0, [&]( auto& p ) {
               p.is_proxy = isproxy;
            });
         propagate_weight_change( *pitr );
      } else {
         _voters.emplace( proxy, [&]( auto& p ) {
               p.owner  = proxy;
               p.is_proxy = isproxy;
            });
      }
   }

   void system_contract::propagate_weight_change( const voter_info& voter ) {
      eosio_assert( voter.proxy == 0 || !voter.is_proxy, "account registered as a proxy is not allowed to use a proxy" );
      double new_weight = stake2vote( voter.staked );
      if ( voter.is_proxy ) {
         new_weight += voter.proxied_vote_weight;
      }

      /// don't propagate small changes (1 ~= epsilon)
      if ( fabs( new_weight - voter.last_vote_weight ) > 1 )  {
         if ( voter.proxy ) {
            auto& proxy = _voters.get( voter.proxy, "proxy not found" ); //data corruption
            _voters.modify( proxy, 0, [&]( auto& p ) {
                  p.proxied_vote_weight += new_weight - voter.last_vote_weight;
               }
            );
            propagate_weight_change( proxy );
         } else {
            auto delta = new_weight - voter.last_vote_weight;
            for ( auto acnt : voter.producers ) {
               auto& pitr = _producers.get( acnt, "producer not found" ); //data corruption
               _producers.modify( pitr, 0, [&]( auto& p ) {
                     p.total_votes += delta;
                     _gstate.total_producer_vote_weight += delta;
               });
            }
         }
      }
      _voters.modify( voter, 0, [&]( auto& v ) {
            v.last_vote_weight = new_weight;
         }
      );
   }

} /// namespace eosiosystem
