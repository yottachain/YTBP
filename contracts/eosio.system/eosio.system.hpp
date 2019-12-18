/**
 *  @file
 *  @copyright defined in eos/LICENSE
 */
#pragma once

#include <eosio.system/native.hpp>
#include <eosiolib/asset.hpp>
#include <eosiolib/time.hpp>
#include <eosiolib/privileged.hpp>
#include <eosiolib/singleton.hpp>
#include <eosio.system/exchange_state.hpp>

#include <string>

namespace eosiosystem {

   using eosio::asset;
   using eosio::indexed_by;
   using eosio::const_mem_fun;
   using eosio::block_timestamp;

   struct name_bid {
     account_name            newname;
     account_name            high_bidder;
     int64_t                 high_bid = 0; ///< negative high_bid == closed auction waiting to be claimed
     uint64_t                last_bid_time = 0;

     auto     primary_key()const { return newname;                          }
     uint64_t by_high_bid()const { return static_cast<uint64_t>(-high_bid); }
   };

   typedef eosio::multi_index< N(namebids), name_bid,
                               indexed_by<N(highbid), const_mem_fun<name_bid, uint64_t, &name_bid::by_high_bid>  >
                               >  name_bid_table;


   struct eosio_global_state : eosio::blockchain_parameters {
      uint64_t free_ram()const { return max_ram_size - total_ram_bytes_reserved; }

      uint64_t             max_ram_size = 64ll*1024 * 1024 * 1024;
      uint64_t             total_ram_bytes_reserved = 0;
      int64_t              total_ram_stake = 0;

      block_timestamp      last_producer_schedule_update;
      uint64_t             last_pervote_bucket_fill = 0;
      int64_t              pervote_bucket = 0;
      int64_t              perblock_bucket = 0;
      uint32_t             total_unpaid_blocks = 0; /// all blocks which have been produced but not paid
      int64_t              total_activated_stake = 0;
      uint64_t             thresh_activated_stake_time = 0;
      uint16_t             last_producer_schedule_size = 0;
      double               total_producer_vote_weight = 0; /// the sum of all producer votes
      block_timestamp      last_name_close;

      // explicit serialization macro is not necessary, used here only to improve compilation time
      EOSLIB_SERIALIZE_DERIVED( eosio_global_state, eosio::blockchain_parameters,
                                (max_ram_size)(total_ram_bytes_reserved)(total_ram_stake)
                                (last_producer_schedule_update)(last_pervote_bucket_fill)
                                (pervote_bucket)(perblock_bucket)(total_unpaid_blocks)(total_activated_stake)(thresh_activated_stake_time)
                                (last_producer_schedule_size)(total_producer_vote_weight)(last_name_close) )
   };

   //##YTA-Change  start:
   struct eosio_global_state2 {
      int64_t               perbase_bucket = 0;
      uint32_t              total_unpaid_base_cnt = 0;
      uint64_t              last_claim_time = 0;

      EOSLIB_SERIALIZE( eosio_global_state2, (perbase_bucket)(total_unpaid_base_cnt)(last_claim_time))
   };

   struct eosio_global_count {
      uint64_t    total_accounts = 1;

      EOSLIB_SERIALIZE( eosio_global_count, (total_accounts) )
   };

   struct eosio_global_state3 {
      bool    is_schedule = true;

      EOSLIB_SERIALIZE( eosio_global_state3, (is_schedule) )
   };

   //##YTA-Change  end:

   struct producer_info {
      account_name          owner;
      double                total_votes = 0;
      eosio::public_key     producer_key; /// a packed public key object
      bool                  is_active = true;
      std::string           url;
      uint32_t              unpaid_blocks = 0;
      uint64_t              last_claim_time = 0;
      uint16_t              location = 0;

      uint64_t primary_key()const { return owner;                                   }
      double   by_votes()const    { return is_active ? -total_votes : total_votes;  }
      bool     active()const      { return is_active;                               }
      void     deactivate()       { producer_key = public_key(); is_active = false; }

      // explicit serialization macro is not necessary, used here only to improve compilation time
      EOSLIB_SERIALIZE( producer_info, (owner)(total_votes)(producer_key)(is_active)(url)
                        (unpaid_blocks)(last_claim_time)(location) )
   };

   //##YTA-Change  start:
   struct producer_info_ext {
      account_name          owner;
      uint16_t              seq_num = 1; // from 1 to 21      
      uint8_t               level = 3;
      uint32_t              unpaid_base_cnt = 0;
      int64_t               unpaid_amount = 0;  
      account_name          shadow = 0;
      uint64_t primary_key()const { return owner; }          

      EOSLIB_SERIALIZE( producer_info_ext, (owner)(seq_num)(level)(unpaid_base_cnt)(unpaid_amount)(shadow))

   };
   //##YTA-Change  end:

   //##YTA-Change  start:
   struct yta_prod_info {
      account_name         owner;
      double               total_votes = 0; // total votes
      eosio::public_key    producer_key; /// a packed public key object
      bool                 is_active = true;
      uint16_t             location = 0;
      bool                 is_in_grace;            //是否处在补齐投票的宽限期
      uint64_t             grace_start_time = 0;   //宽限期的开始时间

      EOSLIB_SERIALIZE( yta_prod_info, (owner)(total_votes)(producer_key)(is_active)(location)(is_in_grace)(grace_start_time) )      

   };

   struct all_prods_level {
      std::vector<yta_prod_info>        prods_l1;  //max 21
      std::vector<yta_prod_info>        prods_l2;  //max 105

      EOSLIB_SERIALIZE( all_prods_level, (prods_l1)(prods_l2) )      
   };
   typedef eosio::singleton<N(prodslevel), all_prods_level> all_prods_singleton;

   //##YTA-Change  end:

   struct voter_info {
      account_name                owner = 0; /// the voter
      account_name                proxy = 0; /// the proxy set by the voter, if any
      std::vector<account_name>   producers; /// the producers approved by this voter if no proxy set
      int64_t                     staked = 0;

      /** 
       *  Every time a vote is cast we must first "undo" the last vote weight, before casting the
       *  new vote weight.  Vote weight is calculated as:
       *
       *  stated.amount * 2 ^ ( weeks_since_launch/weeks_per_year)
       */
      double                      last_vote_weight = 0; /// the vote weight cast the last time the vote was updated

      /**
       * Total vote weight delegated to this voter.
       */
      double                      proxied_vote_weight= 0; /// the total vote weight delegated to this voter as a proxy
      bool                        is_proxy = 0; /// whether the voter is a proxy for others


      uint32_t                    reserved1 = 0;
      time                        reserved2 = 0;
      eosio::asset                reserved3;

      uint64_t primary_key()const { return owner; }

      // explicit serialization macro is not necessary, used here only to improve compilation time
      EOSLIB_SERIALIZE( voter_info, (owner)(proxy)(producers)(staked)(last_vote_weight)(proxied_vote_weight)(is_proxy)(reserved1)(reserved2)(reserved3) )
   };

   typedef eosio::multi_index< N(voters), voter_info>  voters_table;


   typedef eosio::multi_index< N(producers), producer_info,
                               indexed_by<N(prototalvote), const_mem_fun<producer_info, double, &producer_info::by_votes>  >
                               >  producers_table;

   //##YTA-Change  start:  
   typedef eosio::multi_index< N(producersext), producer_info_ext>  producers_ext_table;
   //##YTA-Change  end:  

   typedef eosio::singleton<N(global), eosio_global_state> global_state_singleton;

   //##YTA-Change  start:
   typedef eosio::singleton<N(globalext), eosio_global_state2> global_state2_singleton;
   typedef eosio::singleton<N(globalext2), eosio_global_state3> global_state3_singleton;
   typedef eosio::singleton<N(gcount), eosio_global_count> global_count_singleton;
   //##YTA-Change  end:

   //   static constexpr uint32_t     max_inflation_rate = 5;  // 5% annual inflation
   static constexpr uint32_t     seconds_per_day = 24 * 3600;
   static constexpr uint64_t     system_token_symbol = CORE_SYMBOL;

   class system_contract : public native {
      private:
         voters_table           _voters;
         producers_table        _producers;
         global_state_singleton _global;
         eosio_global_state     _gstate;
         rammarket              _rammarket;

         //##YTA-Change  start:  
         producers_ext_table     _producersext;
         eosio_global_state2     _gstateex; 
         global_state2_singleton _globalex;
         //##YTA-Change  end:           

      public:
         system_contract( account_name s );
         ~system_contract();

         // Actions:
         void onblock( block_timestamp timestamp, account_name producer );
                      // const block_header& header ); /// only parse first 3 fields of block header

         // functions defined in delegate_bandwidth.cpp

         /**
          *  Stakes SYS from the balance of 'from' for the benfit of 'receiver'.
          *  If transfer == true, then 'receiver' can unstake to their account
          *  Else 'from' can unstake at any time.
          */
         void delegatebw( account_name from, account_name receiver,
                          asset stake_net_quantity, asset stake_cpu_quantity, bool transfer );


         /**
          *  Decreases the total tokens delegated by from to receiver and/or
          *  frees the memory associated with the delegation if there is nothing
          *  left to delegate.
          *
          *  This will cause an immediate reduction in net/cpu bandwidth of the
          *  receiver.
          *
          *  A transaction is scheduled to send the tokens back to 'from' after
          *  the staking period has passed. If existing transaction is scheduled, it
          *  will be canceled and a new transaction issued that has the combined
          *  undelegated amount.
          *
          *  The 'from' account loses voting power as a result of this call and
          *  all producer tallies are updated.
          */
         void undelegatebw( account_name from, account_name receiver,
                            asset unstake_net_quantity, asset unstake_cpu_quantity );


         /**
          * Increases receiver's ram quota based upon current price and quantity of
          * tokens provided. An inline transfer from receiver to system contract of
          * tokens will be executed.
          */
         void buyram( account_name buyer, account_name receiver, asset tokens );
         void buyrambytes( account_name buyer, account_name receiver, uint32_t bytes );

         /**
          *  Reduces quota my bytes and then performs an inline transfer of tokens
          *  to receiver based upon the average purchase price of the original quota.
          */
         void sellram( account_name receiver, int64_t bytes );

         /**
          *  This action is called after the delegation-period to claim all pending
          *  unstaked tokens belonging to owner
          */
         void refund( account_name owner );

         // functions defined in voting.cpp

         void regproducer( const account_name producer, const public_key& producer_key, const std::string& url, uint16_t location );

         void unregprod( const account_name producer );

//##YTA-Change  start:  
         void seqproducer( const account_name producer, const account_name shadow, uint16_t seq , uint8_t level );

         void reprodlevel();       

         void changevotes( const account_name voter_name );  

         void setautosche( bool auto_sche);
//##YTA-Change  end:           

         void setram( uint64_t max_ram_size );

         void voteproducer( const account_name voter, const account_name proxy, const std::vector<account_name>& producers );

         void regproxy( const account_name proxy, bool isproxy );

         void setparams( const eosio::blockchain_parameters& params );

         // functions defined in producer_pay.cpp
         void claimrewards( const account_name& owner );
         void rewardprods();
         void startreward(); 

         void setpriv( account_name account, uint8_t ispriv );

         void rmvproducer( account_name producer );

         void bidname( account_name bidder, account_name newname, asset bid );
      private:
         void update_elected_producers( block_timestamp timestamp );

//##YTA-Change  start:  
         void update_elected_producers_yta( block_timestamp timestamp );

         void update_producer_level();

         void rm_producer_yta( const account_name producer );

         void add_producer_yta( const account_name producer, uint8_t level );

         void change_producer_yta_info( const account_name producer, const eosio::public_key& producer_key, bool isactive);

         void update_producers_yta_totalvotes( account_name owner, double total_votes);              

         void delproducer( const account_name producer );

         void check_yta_account( const account_name user );
//##YTA-Change  end:  

         // Implementation details:

         //defind in delegate_bandwidth.cpp
         void changebw( account_name from, account_name receiver,
                        asset stake_net_quantity, asset stake_cpu_quantity, bool transfer );

         //defined in voting.hpp
         static eosio_global_state get_default_parameters();

         void update_votes( const account_name voter, const account_name proxy, const std::vector<account_name>& producers, bool voting );

         // defined in voting.cpp
         void propagate_weight_change( const voter_info& voter );
   };

   bool isActiveVoter( account_name owner ) {
      voters_table voters(N(eosio), N(eosio));
      auto voter = voters.find(owner);
      if( voter == voters.end() ) {
         return false;
      }
      if( voter->producers.size() == 0 ) {
         return false;
      }

      return true;
   }

   uint16_t getProducerSeq(account_name producer, account_name &shadow){
      producers_ext_table _producer_ext(N(eosio), N(eosio));
      auto prod = _producer_ext.find(producer);
      if(prod != _producer_ext.end()) {
         shadow = prod->shadow;
         return prod->seq_num;
      }
      return 0;
   }


} /// eosiosystem
