#include <eosiolib/eosio.hpp>
#include <eosiolib/asset.hpp>
#include <eosiolib/time.hpp>
#include <eosiolib/singleton.hpp>

using eosio::asset;
using eosio::const_mem_fun;
using eosio::contract;
using eosio::datastream;
using eosio::indexed_by;
using eosio::microseconds;
using eosio::multi_index;
using eosio::name;
using eosio::symbol_type;
typedef double real_type;

//copy from eosio.system
/**
*  Uses Bancor math to create a 50/50 relay between two asset types. The state of the
*  bancor exchange is entirely contained within this struct. There are no external
*  side effects associated with using this API.
*/
struct exchange_state
{
  asset supply;

  struct connector
  {
    asset balance;
    double weight = .5;

    EOSLIB_SERIALIZE(connector, (balance)(weight))
  };

  connector base;
  connector quote;

  uint64_t primary_key() const { return supply.symbol; }

  asset convert_to_exchange(connector &c, asset in);
  asset convert_from_exchange(connector &c, asset in);
  asset convert(asset from, symbol_type to);

  EOSLIB_SERIALIZE(exchange_state, (supply)(base)(quote))
};

//comment old style declaration
typedef multi_index<N(hmarket), exchange_state> hmarket_table;

class hddpool : public contract
{
public:
  using contract::contract;

  hddpool(account_name s);
  ~hddpool();

  void getbalance(name user, uint8_t acc_type, name caller);
  void buyhdd(name from, name receiver, asset quant);
  //void buyhdd( name user , int64_t amount);
  void sellhdd(name user, int64_t amount);
  void sethfee(name user, int64_t fee, name caller);
  void subbalance(name user, int64_t balance);
  void addhspace(name user, uint64_t space, name caller);
  void subhspace(name user, uint64_t space, name caller);
  void newmaccount(name owner, uint64_t minerid, name caller);
  void addmprofit(name owner, uint64_t minerid, uint64_t space, name caller);
  void clearall(name owner);
  void calcmbalance(name owner, uint64_t minerid);

  //store pool related actions -- start
  void clsallpools();
  void regstrpool(name pool_id, name pool_owner, uint64_t max_space);
  void addm2pool(uint64_t minerid, name pool_id, name minerowner);
  //store pool related actions -- end

private:
  struct userhdd
  {
    name account_name;            //账户名
    int64_t hdd_balance;          //余额
    int64_t hdd_per_cycle_fee;    //每周期费用
    int64_t hdd_per_cycle_profit; //每周期收益
    uint64_t hdd_space;           //占用存储空间
    uint64_t last_hdd_time;       //上次余额计算时间 microseconds from 1970
    uint64_t primary_key() const { return account_name.value; }
  };
  typedef multi_index<N(userhdd), userhdd> userhdd_index;

  struct maccount
  {
    uint64_t minerid;             //矿机id
    name owner;                   //拥有矿机的矿主的账户名
    uint64_t space;               //生产空间
    int64_t hdd_per_cycle_profit; //每周期收益
    int64_t hdd_balance;          //余额
    uint64_t last_hdd_time;       //上次余额计算时间 microseconds from 1970
    uint64_t primary_key() const { return minerid; }
    //uint64_t  get_owner() const { return owner.value; }
  };
  typedef multi_index<N(maccount), maccount> maccount_index;

  struct miner2acc //scope minerid
  {
    uint64_t minerid;             //矿机id
    name owner;                   //拥有矿机的矿主的账户名  
    uint64_t  primary_key() const { return minerid; }  
  };
  typedef multi_index<N(miner2acc), miner2acc> miner2acc_index;

  //store poll tables  (scope : self) start -----------------
  //store pool
  struct storepool
  {
    name        pool_id;  //pool id use eos name type  
    name        pool_owner; //pool owner is a ytachain account
    uint64_t    max_space;  //max space for this pool
    uint64_t    space_left; //space left for this pool 
    uint64_t    primary_key() const { return pool_id.value; }
  };
  typedef multi_index<N(storepool), storepool> storepool_index;

  //storepool's miners ( scope : pool_id )
  struct spoolminers
  {
    name        pool_id;      //pool id use eos name type  
    uint64_t    minerid;      //矿机id
    name        miner_owner;  //拥有矿机的矿主的账户名
    uint64_t    space;        //矿机的生产空间
    uint64_t    primary_key() const { return minerid; }
  };
  typedef multi_index<N(spoolminers), spoolminers> spoolminers_index;

  //miner's storepool ( scope : minerid)
  struct miner2pool
  {
    uint64_t  minerid;
    name      pool_id;
    uint64_t  primary_key() const { return minerid; }
  };
  typedef multi_index<N(miner2pool), miner2pool> miner2pool_index;
  //store poll tables  (scope : self) end -------------------


  struct hdd_global_state
  {
    int64_t hdd_total_balance = 10000000000;
  };

  struct hdd_global_state2
  {
    uint64_t hdd_total_user = 4;
  };

  struct hdd_global_state3
  {
    uint64_t hdd_macc_user = 2;
  };

  struct hdd_global_price
  {
    int64_t price = 70;
  };

  typedef eosio::singleton<N(hddglobal), hdd_global_state> global_state_singleton;
  typedef eosio::singleton<N(gusercount), hdd_global_state2> gusercount_singleton;
  typedef eosio::singleton<N(gmacccount), hdd_global_state3> gmacccount_singleton;
  typedef eosio::singleton<N(ghddprice), hdd_global_price> ghddprice_singleton;

  global_state_singleton _global;
  hdd_global_state _gstate;

  gusercount_singleton _global2;
  hdd_global_state2 _gstate2;

  gmacccount_singleton _global3;
  hdd_global_state3 _gstate3;

  ghddprice_singleton _ghddprice;
  hdd_global_price _ghddpriceState;

  hmarket_table _hmarket;

  bool is_bp_account(uint64_t uservalue);

  bool calculate_balance(int64_t oldbalance, int64_t hdd_per_cycle_fee,
                         int64_t hdd_per_cycle_profit, uint64_t last_hdd_time, uint64_t current_time,
                         int64_t &new_balance);

  void update_hddofficial(const int64_t _balance,
                          const int64_t _fee, const int64_t _profit,
                          const int64_t _space);

  void update_total_hdd_balance(int64_t _balance_delta);
};
