#include <eosiolib/eosio.hpp>
#include <eosiolib/asset.hpp>
#include <eosiolib/time.hpp>
#include <eosiolib/singleton.hpp>

using eosio::contract;
using eosio::name;
using eosio::asset;
using eosio::symbol_type;
using eosio::indexed_by;
using eosio::const_mem_fun;
using eosio::microseconds;
using eosio::datastream;
using eosio::multi_index;
typedef double real_type;

class hddpool : public contract {
  public:
    using contract::contract;

    hddpool( account_name s);
    ~hddpool();    

    void getbalance( name user );
    void buyhdd( name user , asset quant);
    void sellhdd( name user, int64_t amount);
    void sethfee( name user, int64_t fee);
    void subbalance ( name user, int64_t balance);
    void addhspace(name user, uint64_t space);
    void subhspace(name user, uint64_t space);
    void newmaccount(name owner, uint64_t minerid);
    void addmprofit(uint64_t minderid, uint64_t space);
    void clearall();

  private:

    struct userhdd {
      name       account_name; //账户名
      int64_t    hdd_balance; //余额
      int64_t    hdd_per_cycle_fee; //每周期费用
      int64_t    hdd_per_cycle_profit; //每周期收益
      uint64_t   hdd_space; //占用存储空间
    	uint64_t   last_hdd_time; //上次余额计算时间 microseconds from 1970
      uint64_t   primary_key() const { return account_name.value; }
    }; 
    typedef multi_index<N(userhdd), userhdd> userhdd_index; 

    struct maccount {
      uint64_t minerid; //矿机id
      name     owner; //拥有矿机的矿主的账户名
      uint64_t space; //生产空间
      uint64_t primary_key() const { return minerid; }
      //uint64_t get_owner() const { return owner.value; }
    };
    typedef multi_index< N(maccount), maccount > maccount_index;

    
    struct hdd_global_state {
      int64_t    hdd_total_balance = 0;
    };        

    typedef eosio::singleton<N(hddglobal), hdd_global_state> global_state_singleton;

    global_state_singleton  _global;
    hdd_global_state        _gstate;    


    bool is_bp_account(uint64_t uservalue);

    static hdd_global_state get_default_param();
    
    bool calculate_balance(int64_t oldbalance, int64_t hdd_per_cycle_fee, 
          int64_t hdd_per_cycle_profit, uint64_t last_hdd_time, uint64_t current_time, 
          int64_t &new_balance);
    
    void update_hddofficial( userhdd_index& _hbalance, const int64_t _balance,
          const int64_t _fee, const int64_t _profit, 
          const int64_t _space );      
};
