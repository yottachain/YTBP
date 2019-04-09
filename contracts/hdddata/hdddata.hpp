#include <eosiolib/eosio.hpp>
#include <eosiolib/asset.hpp>
#include <eosiolib/time.hpp>

using namespace eosio;

CONTRACT hdddata : public contract
{
    public:
    using contract::contract;
    struct hdd_parameter {
		uint64_t              last_hdd_balance=0,
		uint64_t              hdd_per_cycle_fee=0,
		uint64_t              hdd_per_cycle_profit=0,
		uint64_t              hdd_spacel=0,
		time_point_sec   last_hdd_time
    };

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

    private:

	TABLE  hddbalance {
		name                  owner;
		uint64_t              last_hdd_balance=0;
		uint64_t              hdd_per_cycle_fee=0;
		uint64_t              hdd_per_cycle_profit=0;
		uint64_t              hdd_spacel=0;
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
	
    TABLE mining_account {
        uint64_t ming_id;
        name owner;

        uint64_t primary_key() const { return ming_id; }
        uint64_t get_owner() const { return owner.value; }
    };
   
    typedef multi_index<"mining_account"_n, mining_account,
        indexed_by<"byowner"_n, const_mem_fun<mining_account, uint64_t, &mining_account::get_owner>>>
        mining_account_table;
	
		TABLE producer {
			name                       owner;
			eosio::public_key     producer_key;
			
			uint64_t primary_key() const { return owner.value;   }
		};
		
		typedef multi_index<"producer"_n, producer> producer_table;

 };