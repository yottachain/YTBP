#include <eosiolib/eosio.hpp>
#include <eosiolib/asset.hpp>
#include <eosiolib/time.hpp>
#include <eosiolib/singleton.hpp>

using namespace eosio;
using eosio::name;
using eosio::asset;
using eosio::symbol_type;
using eosio::indexed_by;
using eosio::const_mem_fun;
using eosio::microseconds;
using eosio::datastream;
typedef double real_type;

//todo copy from eosio.system 
/**
*  Uses Bancor math to create a 50/50 relay between two asset types. The state of the
*  bancor exchange is entirely contained within this struct. There are no external
*  side effects associated with using this API.
*/
   struct exchange_state {
      asset    supply;

      struct connector {
         asset balance;
         double weight = .5;

         EOSLIB_SERIALIZE( connector, (balance)(weight) )
      };

      connector base;
      connector quote;

      uint64_t primary_key()const { return supply.symbol; }

      asset convert_to_exchange( connector& c, asset in ); 
      asset convert_from_exchange( connector& c, asset in );
      asset convert( asset from, symbol_type to );

      EOSLIB_SERIALIZE( exchange_state, (supply)(base)(quote) )
   };

//comment old style declaration 
typedef multi_index<N(hmarket), exchange_state> hmarket_table;


class hdddata : public contract {
public:
    using contract::contract;
    
    hdddata( account_name s);
    
    ~hdddata();
        
    void gethbalance(name owner);
    
    void gethsum();
    
    void sethfee(name owner, uint64_t fee);

    void newmaccount(name mname, name owner);

    void addmprofit(name mname, uint64_t space);

    void subhbalance(name owner, uint64_t balance);

    void buyhdd(name buyer, name receiver, asset quant);
    
    void sellhdd(name account, uint64_t quant);
    
    void addhspace(name owner, name hddaccount, uint64_t space);

    void subhspace(name owner, name hddaccount, uint64_t space);

    //static constexpr symbol hddcore_symbol = symbol(symbol_code("HDDCORE"), 4);
    //static constexpr symbol hdd_symbol     = symbol(symbol_code("HDD"), 0);
    //static constexpr symbol yta_symbol     = symbol(symbol_code("YTA"), 4);
    //static constexpr eosio::name token_account{N(eosio.token)};
    //static constexpr eosio::name hdd_account{N(eosio.hdd)};
    //static constexpr eosio::name hddfee_account{N(eosio.hddfee)};
    //static constexpr eosio::name active_permission{N(active)};
    
/*    
    static symbol get_core_symbol( name system_account = N(hddofficial) ) {
        hmarket_table rm(system_account, system_account.value);
        const static auto sym = get_core_symbol( rm );
        return sym;
    }
*/

private:    
    // Implementation details:
/*
    static symbol get_core_symbol( const hmarket_table& hdd ) {
        auto itr = hdd.find(hddcore_symbol.raw());
        check(itr != hdd.end(), "system contract must first be initialized");
        return itr->quote.balance.symbol;
    }
    
    symbol core_symbol() const;
*/    
    struct  hbalance {
        name                  owner = name{N(hddofficial)};
        uint64_t              last_hdd_balance=0;
        uint64_t              hdd_per_cycle_fee=0;
        uint64_t              hdd_per_cycle_profit=0;
        uint64_t              hdd_space=0;
		uint64_t              last_hdd_time = current_time();                 //microseconds from 1970
        uint64_t              primary_key() const { return owner.value; }
        uint64_t              get_last_hdd_balance() const { return last_hdd_balance; }
        uint64_t              get_hdd_per_cycle_fee() const { return hdd_per_cycle_fee; }
        uint64_t              get_hdd_per_cycle_profit() const { return hdd_per_cycle_profit; }
        uint64_t              get_hdd_space() const { return hdd_space; }
    };
    typedef multi_index<N(hbalance), hbalance,
    indexed_by<N(bybalance), const_mem_fun<hbalance, uint64_t, &hbalance::get_last_hdd_balance>>,
    indexed_by<N(byfee), const_mem_fun<hbalance, uint64_t, &hbalance::get_hdd_per_cycle_fee>>,
    indexed_by<N(byprofit), const_mem_fun<hbalance, uint64_t, &hbalance::get_hdd_per_cycle_profit>>,
    indexed_by<N(byspace), const_mem_fun<hbalance, uint64_t, &hbalance::get_hdd_space>>>
    hbalance_table;
    
    // ming account 
    struct maccount {
        uint64_t ming_id;
        name     owner;

        uint64_t primary_key() const { return ming_id; }
        uint64_t get_owner() const { return owner.value; }
    };

    typedef multi_index<N(maccount), maccount,
    indexed_by<N(byowner), const_mem_fun<maccount, uint64_t, &maccount::get_owner>>>
    maccount_table;
    
    struct producer {
        name                       owner;
        name                       producer_key;
        
        uint64_t primary_key() const { return owner.value;   }
    };
    
    typedef multi_index<N(producer), producer> producer_table;
    
private:
    void update_hddofficial(hbalance_table& _hblance, const uint64_t _hb, const uint64_t time);
    
    
    //hbalance_table                                   _hbalance;
    maccount_table                                 _maccount;
    producer_table                                  _producer;
    hmarket_table                                   _hmarket;
};