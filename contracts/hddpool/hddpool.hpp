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

class hddpool : public eosio::contract
{
public:
  using contract::contract;

  void getbalance(name user, uint8_t acc_type, name caller);
  void calcprofit(name user);
  void buyhdd(name from, name receiver, int64_t amount, std::string memo);
  void transhdds(name from, name to, int64_t amount, std::string memo);
  void sellhdd(name user, int64_t amount, std::string memo);
  void sethfee(name user, int64_t fee, name caller);
  void subbalance(name user, int64_t balance, uint8_t acc_type, name caller);
  void addhspace(name user, uint64_t space, name caller);
  void subhspace(name user, uint64_t space, name caller);
  void addmprofit(uint64_t minerid, uint64_t space, name caller);
  void delminer(uint64_t minerid, uint8_t acc_type, name caller);
  void regminer(uint64_t minerid, name adminacc, name dep_acc, asset dep_amount, name pool_id, name minerowner, uint64_t max_space);
  void redeposit(uint64_t minerid, name dep_acc, asset dep_amount);
  void mforfeit(uint64_t minerid, asset quant, name caller);

  //store pool related actions -- start
  void delstrpool(name poolid);
  void regstrpool(name pool_id, name pool_owner);
  void chgpoolspace(name pool_id, bool is_increase, uint64_t delta_space);
  //store pool related actions -- end

  void mdeactive(uint64_t minerid, name caller);
  void mactive(uint64_t minerid, name caller);

  //change miner info related actions
  void mchgadminacc(uint64_t minerid, name new_adminacc);
  void mchgowneracc(uint64_t minerid, name new_owneracc);
  void mchgstrpool(uint64_t minerid, name new_poolid);
  void mchgspace(uint64_t minerid, uint64_t max_space);
  void chgdeposit(name user, uint64_t minerid, bool is_increase, asset quant);


  //update hddpool params
  void sethddprice(uint64_t price);
  void setytaprice(uint64_t price, uint8_t acc_type);
  void setdrratio(uint64_t ratio, uint8_t acc_type);
  void setdrdratio(uint64_t ratio);
  void addhddcnt(int64_t count, uint8_t acc_type);

  //update usd price
  void setusdprice(uint64_t price, uint8_t acc_type);

  void fixownspace(name owner, uint64_t space);
 
private:
  //-------------------------------------------------------------------------------
  //旧模型数据，作为合约内部表暂时保留用作数据迁移 ----- start 
  //-------------------------------------------------------------------------------

  struct userhdd
  {
    name      account_name;         //账户名
    int64_t   hdd_storehhdd;        //用户数据存储的HDD数量
    int64_t   hdd_per_cycle_fee;    //用户存储数据的每周期费用
    uint64_t  hdd_space_store;      //用户存储数据占用的存储空间
    uint64_t  last_hddstore_time;   //上次存储hdd余额计算时间 microseconds from 1970
    int64_t   hdd_minerhdd;         //废弃
    int64_t   hdd_per_cycle_profit; //废弃
    uint64_t  hdd_space_profit;     //废弃
    uint64_t  last_hddprofit_time;  //废弃
    uint64_t  primary_key() const { return account_name.value; }
  };
  typedef multi_index<N(userhddinfo), userhdd> userhdd1_index;

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
  typedef multi_index<N(maccount), maccount> maccount1_index;

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
                     > minerinfo1_table;   


  //-------------------------------------------------------------------------------
  //旧模型数据，作为合约内部表暂时保留用作数据迁移 ----- end 
  //-------------------------------------------------------------------------------

  struct storepool
  {
    name        pool_id;  //pool id use eos name type  
    name        pool_owner; //pool owner is a ytachain account
    uint64_t    max_space;  //max space for this pool
    uint64_t    space_left; //space left for this pool 
    uint64_t    primary_key() const { return pool_id.value; }
  };
  typedef multi_index<N(storepool), storepool> storepool_index;

  //新模型存储用户hdd信息表
  struct usershdd 
  {
    name      account_name;         //账户名
    int64_t   hdd_storehhdd;        //用户数据存储的HDD数量
    int64_t   hdd_per_cycle_fee;    //用户存储数据的每周期费用
    uint64_t  hdd_space_store;      //用户存储数据占用的存储空间
    uint64_t  last_hddstore_time;   //上次存储hdd余额计算时间 microseconds from 1970
    uint64_t  primary_key() const { return account_name.value; }
  };
  typedef multi_index<N(usershddinfo), usershdd> usershdd_index;

  //新模型收益账户hdd信息表
  struct usermhdd
  {
    name      account_name;         //账户名
    int64_t   hdd_minerhdd;         //存储服务提供者的HDD收益数量  
    int64_t   hdd_per_cycle_profit; //每周期收益
    uint64_t  hdd_space_profit;     //该收益账户名下所有矿机的总生产空间
    uint64_t  last_hddprofit_time;  //上次收益hdd余额计算时间 microseconds from 1970
    uint64_t  primary_key() const { return account_name.value; }
  };
  typedef multi_index<N(usermhddinfo), usermhdd> usermhdd_index;

  //新模型收益账户miner信息表
  struct maccount2
  {
    uint64_t minerid;             //矿机id
    name owner;                   //拥有矿机的矿主的账户名
    uint64_t space;               //生产空间
    int64_t hdd_per_cycle_profit; //每周期收益
    int64_t hdd_balance;          //余额
    uint64_t last_hdd_time;       //上次余额计算时间 microseconds from 1970
    uint64_t primary_key() const { return minerid; }
  };
  typedef multi_index<N(maccount2), maccount2> maccount2_index;

  //新模型miner信息表
  struct minerinfo2
  {
    uint64_t  minerid;
    name      owner;      //收益账号
    name      admin;      //管理员账号
    name      depacc;     //抵押账户
    name      pool_id;    //所在的矿池子id
    asset     deposit;    //当前剩余押金
    asset     dep_total;  //总押金  
    uint64_t  max_space;  //最大可被采购生产空间
    uint64_t  space;      //当前的生产空间
    uint64_t  internal_id = 0;    //(预留字段,用来表示以后的内部紧凑重构矿机id)
    uint8_t   miner_type = 0; //矿机类型(预留)
    uint8_t   status = 0; //矿机状态 0-正常 1-封禁 2-永久封禁
    uint64_t  primary_key() const { return minerid; }
  };
  typedef multi_index<N(minerinfo2), minerinfo2> minerinfo2_table;   

  struct depositinfo
  {
    name      user;               //抵押账户
    asset     deposit;            //当前剩余押金
    uint64_t  primary_key() const { return user.value; }
  };
  typedef multi_index<N(depositinfo), depositinfo> depositinfo_index;

  struct hdd_global_param
  {
    uint64_t  hdd_price = 5760;
    uint64_t  yta_price = 8000;
    uint64_t  dup_remove_ratio = 10000;
    uint64_t  dup_remove_dist_ratio = 10000;
    int64_t   hdd_counter = 2 * 1024 * 1024 * 100000000ll;

    EOSLIB_SERIALIZE( hdd_global_param, (hdd_price)(yta_price)(dup_remove_ratio)(dup_remove_dist_ratio)(hdd_counter))
  };

  struct hdd_price_guard
  {
    uint64_t  last_yta_guard_price1 = 8000;
    uint64_t  last_ytaprice_guard_time1 = current_time(); 
    uint64_t  last_yta_guard_price2 = 8000;
    uint64_t  last_ytaprice_guard_time2 = current_time(); 
    uint64_t  last_dup_remove_guard_ratio1 = 10000;
    uint64_t  last_duprmv_ratio_guard_time1 = current_time();
    uint64_t  last_dup_remove_guard_ratio2 = 10000;
    uint64_t  last_duprmv_ratio_guard_time2 = current_time();  
    uint64_t  yta_price_delta_1 = 15; 
    uint64_t  yta_price_timespan_1 = 8*60; 
    uint64_t  yta_price_delta_2 = 50;
    uint64_t  yta_price_timespan_2 = 60 * 60 * 24; 
    uint64_t  duprmv_ratio_delta_1 = 10;
    uint64_t  duprmv_ratio_timespan_1 = 60 * 60 * 24; 
    uint64_t  duprmv_ratio_delta_2 = 50;
    uint64_t  duprmv_ratio_timespan_2 = 60 * 60 * 240; 


    EOSLIB_SERIALIZE( hdd_price_guard,  (last_yta_guard_price1)(last_ytaprice_guard_time1)
                                        (last_yta_guard_price2)(last_ytaprice_guard_time2)
                                        (last_dup_remove_guard_ratio1)(last_duprmv_ratio_guard_time1)
                                        (last_dup_remove_guard_ratio2)(last_duprmv_ratio_guard_time2)
                                        (yta_price_delta_1)(yta_price_timespan_1)
                                        (yta_price_delta_2)(yta_price_timespan_2)
                                        (duprmv_ratio_delta_1)(duprmv_ratio_timespan_1)
                                        (duprmv_ratio_delta_2)(duprmv_ratio_timespan_2))
  };

  typedef eosio::singleton<N(gparams), hdd_global_param> gparams_singleton;
  typedef eosio::singleton<N(paramguard), hdd_price_guard> paramguard_singleton;

  struct hdd_total_info
  {
    uint64_t hdd_total_user = 0;
    uint64_t hdd_macc_user = 0;
    uint64_t total_sapce = 0; //以G为单位 
  };
  typedef eosio::singleton<N(ghddtotal), hdd_total_info> ghddtotal_singleton;

  struct deposit_rate
  {
    int64_t rate = 10000;
  };
  typedef eosio::singleton<N(gdeprate), deposit_rate> gdeprate_singleton;

  struct usd_price
  {
    uint64_t usdprice = 64200;
  };
  typedef eosio::singleton<N(gusdprice), usd_price> gusdprice_singleton;

  void check_bp_account(account_name bpacc, uint64_t id, bool isCheckId);

  int64_t calculate_balance(int64_t oldbalance, int64_t hdd_per_cycle_fee, int64_t hdd_per_cycle_profit,
                          uint64_t last_hdd_time, uint64_t current_time);

  //新增存储用户
  void new_users_hdd(usershdd_index& usershdd, name user, int64_t balance, account_name payer);
  //新增矿工用户
  void new_userm_hdd(usermhdd_index& usermhdd, name user, account_name payer);
  //结算矿工旧模式下的收益到指定的时间点(该时间点需呀小于当前时间)
  int64_t cacl_old_hddm(name user);
  void addmprofitnew(uint64_t minerid, uint64_t space);
  void addmprofitold(uint64_t minerid, uint64_t space);
  void mdeactivenew(uint64_t minerid);
  void mdeactiveold(uint64_t minerid);
  void mactivenew(uint64_t minerid);
  void mactiveold(uint64_t minerid);

  //计算抵押系数
  void calc_deposit_rate();
  void check_deposit_enough( asset deposit, uint64_t max_space ); 
  void check_token_enough( asset deposit, name user ); 
  void add_deposit( asset deposit, name user );


  void chg_owner_space(usermhdd_index& usermhdd, name minerowner, uint64_t space_delta, bool is_increase, bool is_calc, uint64_t ct);

public:  

  static int64_t get_dep_lock( account_name user) {
    depositinfo_index _deposit_info( N(hddpool12345) , N(hddpool12345) );
    auto it = _deposit_info.find(user);
    if(it != _deposit_info.end()) {
      return it->deposit.amount;
    }
    return 0;
  }

  static name get_miner_pool_owner(uint64_t minerid)
  {
    name pool_owner;
    minerinfo2_table _minerinfo2( N(hddpool12345) , N(hddpool12345) );
    auto itminerinfo = _minerinfo2.find(minerid);
    if(itminerinfo != _minerinfo2.end()) {
     storepool_index _storepool( N(hddpool12345) , N(hddpool12345));
     auto itstorepool = _storepool.find(itminerinfo->pool_id);
     if(itstorepool != _storepool.end()) {
       pool_owner = itstorepool->pool_owner;
     }
    }
    return pool_owner;  
  }

};

