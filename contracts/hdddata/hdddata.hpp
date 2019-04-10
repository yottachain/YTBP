#include <eosiolib/eosio.hpp>
#include <eosiolib/asset.hpp>
#include <eosiolib/time.hpp>
#include <eosiolib/singleton.hpp>

using namespace eosio;
typedef double real_type;

CONTRACT hdddata : public contract
{
	private:
	//todo copy from eosio.system 
	/**
	*  Uses Bancor math to create a 50/50 relay between two asset types. The state of the
	*  bancor exchange is entirely contained within this struct. There are no external
	*  side effects associated with using this API.
	*/
	struct  [[eosio::table, eosio::contract("eosio.system")]] exchange_state {
	  asset    supply;

	  struct connector {
		 asset balance;
		 double weight = .5;

		 EOSLIB_SERIALIZE( connector, (balance)(weight) )
	  };

	  connector base;
	  connector quote;

	  uint64_t primary_key()const { return supply.symbol.raw(); }

	  asset convert_to_exchange( connector& c, asset in ); 
	  asset convert_from_exchange( connector& c, asset in );
	  asset convert( asset from, const symbol& to );

	  EOSLIB_SERIALIZE( exchange_state, (supply)(base)(quote) )
	};
	
	//comment old style declaration 
	typedef multi_index<"hddmarket"_n, exchange_state> hddmarket_table;
	
    public:
		
    using contract::contract;
	
	hdddata( );
	
    ~hdddata();


    ACTION get_hdd_balance(name owner);
	
	ACTION get_hdd_sum_balance();
	
    ACTION set_hdd_per_cycle_fee(name owner, uint64_t fee);

    ACTION create_mining_account(name mining_name, name owner);

    ACTION add_mining_profit(name mining_name, uint64_t space);

    ACTION sub_hdd_balance(name owner, uint64_t balance);

    ACTION buyhdd(name buyer, name receiver, asset quant);
	
    ACTION sellhdd(name account, uint64_t quant);
	
    ACTION add_hdd_space(name owner, name hddaccount, uint64_t space);

    ACTION sub_hdd_space(name owner, name hddaccount, uint64_t space);

	static constexpr symbol hddcore_symbol = symbol(symbol_code("HDDCORE"), 4);
	static constexpr symbol hdd_symbol     = symbol(symbol_code("HDD"), 0);
	static constexpr symbol yta_symbol     = symbol(symbol_code("YTA"), 4);
    static constexpr eosio::name token_account{"eosio.token"_n};
	static constexpr eosio::name hdd_account{"eosio.hdd"_n};
	static constexpr eosio::name hddfee_account{"eosio.hddfee"_n};
	
	static symbol get_core_symbol( name system_account = "hddofficial"_n ) {
	hddmarket_table rm(system_account, system_account.value);
	const static auto sym = get_core_symbol( rm );
	return sym;
 }
	private:	
	 // Implementation details:

	 static symbol get_core_symbol( const hddmarket_table& hdd ) {
		auto itr = hdd.find(hddcore_symbol.raw());
		check(itr != hdd.end(), "system contract must first be initialized");
		return itr->quote.balance.symbol;
	 }
	 
	 symbol core_symbol()const;
	 
	TABLE  hddbalance {
		name                  owner;
		uint64_t              last_hdd_balance=0;
		uint64_t              hdd_per_cycle_fee=0;
		uint64_t              hdd_per_cycle_profit=0;
		uint64_t              hdd_space=0;
		time_point_sec   last_hdd_time;
		uint64_t              primary_key() const { return owner.value; }
		uint64_t              get_last_hdd_balance() const { return last_hdd_balance; }
		uint64_t              get_hdd_per_cycle_fee() const { return hdd_per_cycle_fee; }
        uint64_t              get_hdd_per_cycle_profit() const { return hdd_per_cycle_profit; }
		uint64_t              get_hdd_space() const { return hdd_space; }
 	};
	typedef multi_index<"hddbalance"_n, hddbalance,
		indexed_by<"bybalance"_n, const_mem_fun<hddbalance, uint64_t, &hddbalance::get_last_hdd_balance>>,
		indexed_by<"byfee"_n, const_mem_fun<hddbalance, uint64_t, &hddbalance::get_hdd_per_cycle_fee>>,
        indexed_by<"byprofit"_n, const_mem_fun<hddbalance, uint64_t, &hddbalance::get_hdd_per_cycle_profit>>,
        indexed_by<"byspace"_n, const_mem_fun<hddbalance, uint64_t, &hddbalance::get_hdd_space>>>
        hddbalance_table;
	
     TABLE mining {
        uint64_t ming_id;
        name owner;

        uint64_t primary_key() const { return ming_id; }
        uint64_t get_owner() const { return owner.value; }
    };
   
    typedef multi_index<"mining"_n, mining,
        indexed_by<"byowner"_n, const_mem_fun<mining, uint64_t, &mining::get_owner>>>
        mining_table;
	
		TABLE producer {
			name                       owner;
			eosio::public_key      producer_key;
			
			uint64_t primary_key() const { return owner.value;   }
		};
		
    typedef multi_index<"producer"_n, producer> producer_table;
	
    private:
		
	hddbalance_table                             t_hddbalance;
	mining_table                                     t_miningaccount;
	producer_table                                  t_producer;
    hddmarket_table                               t_hddmarket;
 };