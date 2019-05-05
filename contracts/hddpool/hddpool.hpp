#include <eosiolib/eosio.hpp>
#include <eosiolib/asset.hpp>
#include <eosiolib/singleton.hpp>


using namespace eosio;
using namespace std;

CONTRACT hddpool : public contract {
  public:
    using contract::contract;

    hddpool( name s, name code, datastream<const char*> ds );
    ~hddpool();    

    ACTION getbalance( name user );
    ACTION buyhdd( name user , asset quant);
    ACTION sellhdd( name user, int64_t mount);
    ACTION sethfee( name user, uint64_t fee);
    ACTION subbalance ( name user, uint64_t balance);
    ACTION addhspace(name user, uint64_t space);
    ACTION subhspace(name user, uint64_t space);
    ACTION newmaccount(name owner, uint64_t minerid);
    ACTION addmprofit(uint64_t minderid, uint64_t space);
    ACTION clearall();

    // accessor for external contracts to easily send inline actions to your contract
    using getbalance_action = action_wrapper<"getbalance"_n, &hddpool::getbalance>;
    using buyhdd_action = action_wrapper<"buyhdd"_n, &hddpool::buyhdd>;
    using sellhdd_action = action_wrapper<"sellhdd"_n, &hddpool::sellhdd>;
    using sethfee_action = action_wrapper<"sethfee"_n, &hddpool::sethfee>;
    using subbalance_action = action_wrapper<"subbalance"_n, &hddpool::subbalance>;
    using addhspace_action = action_wrapper<"addhspace"_n, &hddpool::addhspace>;
    using subhspace_action = action_wrapper<"subhspace"_n, &hddpool::subhspace>;
    using newmaccount_action = action_wrapper<"newmaccount"_n, &hddpool::newmaccount>;
    using addmprofit_action = action_wrapper<"addmprofit"_n, &hddpool::addmprofit>;
    using clearall_action = action_wrapper<"clearall"_n, &hddpool::clearall>;

  private:

    TABLE userhdd {
      name       account_name; //账户名
      uint64_t   hdd_balance; //余额
      uint64_t   hdd_per_cycle_fee; //每周期费用
      uint64_t   hdd_per_cycle_profit; //每周期收益
      uint64_t   hdd_space; //占用存储空间
    	uint64_t   last_hdd_time; //上次余额计算时间 microseconds from 1970
      uint64_t primary_key() const { return account_name.value; }
    }; 

    TABLE maccount {
      uint64_t minerid; //矿机id
      name     owner; //拥有矿机的矿主的账户名
      uint64_t space; //生产空间
      uint64_t primary_key() const { return minerid; }
      //uint64_t get_owner() const { return owner.value; }
    };
    
    struct [[eosio::table("hddglobal"), eosio::contract("hddpool")]]  hdd_global_state {
      uint64_t hdd_total_balance = 0;
    };      

    typedef eosio::multi_index< "userhdd"_n, userhdd > userhdd_index;
    typedef eosio::multi_index< "maccount"_n, maccount > maccount_index;
    typedef eosio::singleton<"hddglobal"_n, hdd_global_state> global_state_singleton;

    global_state_singleton  _global;
    hdd_global_state        _gstate;    


    bool is_bp_account(uint64_t uservalue);

    static hdd_global_state get_default_param();
    
    bool calculate_balance(uint64_t oldbalance, uint64_t hdd_per_cycle_fee, 
          uint64_t hdd_per_cycle_profit, uint64_t last_hdd_time, uint64_t current_time, 
          uint64_t &new_balance);
    
    void update_hddofficial( userhdd_index& _hbalance, const int64_t _balance,
          const int64_t _fee, const int64_t _profit, 
          const int64_t _space );      
};
