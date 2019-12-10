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
  void subbalance(name user, int64_t balance, uint8_t acc_type, name caller);
  void addhspace(name user, uint64_t space, name caller);
  void subhspace(name user, uint64_t space, name caller);
  //void newmaccount(name owner, uint64_t minerid, name caller);
  void addmprofit(name owner, uint64_t minerid, uint64_t space, name caller);
  void delminer(uint64_t minerid, uint8_t acc_type, name caller);
  void calcmbalance(name owner, uint64_t minerid);
  void newminer(uint64_t minerid, name adminacc, name dep_acc, asset dep_amount);

  //store pool related actions -- start
  void delstrpool(name poolid);
  void regstrpool(name pool_id, name pool_owner, uint64_t max_space);
  void chgpoolspace(name pool_id, uint64_t max_space);
  void addm2pool(uint64_t minerid, name pool_id, name minerowner, uint64_t max_space);
  //store pool related actions -- end

  void mdeactive(name owner, uint64_t minerid, name caller);
  void mactive(name owner, uint64_t minerid, name caller);

  //change miner info related actions
  void mchgadminacc(uint64_t minerid, name new_adminacc);
  void mchgowneracc(uint64_t minerid, name new_owneracc);
  void mchgstrpool(uint64_t minerid, name new_poolid);
  void mchgspace(uint64_t minerid, uint64_t max_space);

  inline bool     is_miner_exist(uint64_t minerid)const;
  inline uint64_t get_miner_max_space(uint64_t minerid)const;
private:
  struct userhdd
  {
    name      account_name;         //账户名
    int64_t   hdd_balance;          //余额
    int64_t   hdd_per_cycle_fee;    //每周期费用
    int64_t   hdd_per_cycle_profit; //每周期收益
    uint64_t  hdd_space;            //占用存储空间
    uint64_t  last_hdd_time;        //上次余额计算时间 microseconds from 1970
    uint64_t  primary_key() const { return account_name.value; }
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

  //store poll tables start -----------------
  //store pool (scope : self)
  struct storepool
  {
    name        pool_id;  //pool id use eos name type  
    name        pool_owner; //pool owner is a ytachain account
    uint64_t    max_space;  //max space for this pool
    uint64_t    space_left; //space left for this pool 
    uint64_t    primary_key() const { return pool_id.value; }
  };
  typedef multi_index<N(storepool), storepool> storepool_index;
  //store poll tables end -------------------

  struct minerinfo
  {
    uint64_t  minerid;
    name      owner;  //收益账号
    name      admin;  //管理员账号
    name      pool_id;
    uint64_t  max_space;
    uint64_t  space_left;    
    uint64_t  primary_key() const { return minerid; }
    uint64_t  by_owner()  const { return owner.value; }
    uint64_t  by_admin()  const { return admin.value; }
    uint64_t  by_poolid() const { return pool_id.value; }
  };

  typedef multi_index<N(minerinfo), minerinfo,
                      indexed_by<N(owner),  const_mem_fun<minerinfo, uint64_t, &minerinfo::by_owner> >,
                      indexed_by<N(admin),  const_mem_fun<minerinfo, uint64_t, &minerinfo::by_admin> >,
                      indexed_by<N(poolid), const_mem_fun<minerinfo, uint64_t, &minerinfo::by_poolid> >
                     > minerinfo_table;   

  struct hdd_global_state
  {
    int64_t hdd_total_balance = 0;
  };

  struct hdd_global_state2
  {
    uint64_t hdd_total_user = 0;
  };

  struct hdd_global_state3
  {
    uint64_t hdd_macc_user = 0;
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

  //bool is_bp_account(uint64_t uservalue);
  void check_bp_account(account_name bpacc, uint64_t id, bool isCheckId);

  bool calculate_balance(int64_t oldbalance, int64_t hdd_per_cycle_fee,
                         int64_t hdd_per_cycle_profit, uint64_t last_hdd_time, uint64_t current_time,
                         int64_t &new_balance);

  void update_total_hdd_balance(int64_t _balance_delta);

  void new_user_hdd(userhdd_index& userhdd, name user, int64_t balance, account_name payer);
};

bool hddpool::is_miner_exist(uint64_t minerid) const
{
  minerinfo_table _minerinfo( _self , _self );
  auto itminerinfo = _minerinfo.find(minerid);
  if(itminerinfo != _minerinfo.end())
    return true;    

  return false;
}

uint64_t hddpool::get_miner_max_space(uint64_t minerid) const
{
  minerinfo_table _minerinfo( _self , _self );
  auto itminerinfo = _minerinfo.find(minerid);
  if(itminerinfo != _minerinfo.end())
    return itminerinfo->max_space;    
  return 0;  
}
