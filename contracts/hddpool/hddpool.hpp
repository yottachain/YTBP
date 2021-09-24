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

  hddpool(account_name s);
  ~hddpool();

  void getbalance(name user, uint8_t acc_type, name caller);
  void calcprofit(name user);
  void buyhdd(name from, name receiver, int64_t amount, std::string memo);
  void transhdds(name from, name to, int64_t amount, std::string memo);
  void sellhdd(name user, int64_t amount, std::string memo);
  void sethfee(name user, int64_t fee, name caller);
  void subbalance(name user, int64_t balance, uint8_t acc_type, name caller);
  void addhspace(name user, uint64_t space, name caller);
  void subhspace(name user, uint64_t space, name caller);
  //void newmaccount(name owner, uint64_t minerid, name caller);
  void addmprofit(name owner, uint64_t minerid, uint64_t space, name caller);
  void submprofit(name owner, uint64_t minerid, uint64_t space, name caller);
  void delminer(uint64_t minerid, uint8_t acc_type, name caller);
  void calcmbalance(name owner, uint64_t minerid);
  void newminer(uint64_t minerid, name adminacc, name dep_acc, asset dep_amount);

  //store pool related actions -- start
  void delstrpool(name poolid);
  void regstrpool(name pool_id, name pool_owner);
  void chgpoolspace(name pool_id, bool is_increase, uint64_t delta_space);
  void addm2pool(uint64_t minerid, name pool_id, name minerowner, uint64_t max_space);
  //store pool related actions -- end

  //miner registration itterface
  void regminer(uint64_t minerid,name adminacc, name dep_acc,name pool_id, name minerowner, uint64_t max_space, asset dep_amount, bool is_calc);

  void mdeactive(name owner, uint64_t minerid, name caller);
  void mactive(name owner, uint64_t minerid, name caller);

  //change miner info related actions
  void mchgadminacc(uint64_t minerid, name new_adminacc);
  void mchgowneracc(uint64_t minerid, name new_owneracc);
  void mchgstrpool(uint64_t minerid, name new_poolid);
  void mchgspace(uint64_t minerid, uint64_t max_space);

  //update hddpool params
  void sethddprice(uint64_t price);
  void setytaprice(uint64_t price, uint8_t acc_type);
  void setdrratio(uint64_t ratio, uint8_t acc_type);
  void setdrdratio(uint64_t ratio);
  void addhddcnt(int64_t count, uint8_t acc_type);

  //update usd price
  void setusdprice(uint64_t price, uint8_t acc_type);

  //update miner's level
  void mlevel(uint64_t minerid, uint32_t level, name caller);


  void fixownspace(name owner, uint64_t space);

  void oldsync(uint64_t minerid);

  void onrewardt(uint32_t slot);
  void rewardselt(uint64_t random1, uint64_t random2);
  void rewardlogt(std::string memo);
 
private:
  struct userhdd
  {
    name      account_name;         //账户名
    int64_t   hdd_storehhdd;        //用户数据存储的HDD数量
    int64_t   hdd_per_cycle_fee;    //用户存储数据的每周期费用
    uint64_t  hdd_space_store;      //用户存储数据占用的存储空间
    uint64_t  last_hddstore_time;   //上次存储hdd余额计算时间 microseconds from 1970
    int64_t   hdd_minerhdd;         //存储服务提供者的HDD收益数量  
    int64_t   hdd_per_cycle_profit; //每周期收益
    uint64_t  hdd_space_profit;     //该收益账户名下所有矿机的总生产空间
    uint64_t  last_hddprofit_time;  //上次收益hdd余额计算时间 microseconds from 1970
    uint64_t  primary_key() const { return account_name.value; }
  };
  typedef multi_index<N(userhddinfo), userhdd> userhdd_index;

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


  //新模型的矿机表
  struct miner
  {
    uint64_t  minerid;
    name      owner;              //收益账号
    name      admin;              //管理员账号
    name      pool_id;            //矿池id  
    uint64_t  max_space;          //最大可采购空间
    uint64_t  space;              //当前已采购空间
    uint64_t  last_modify_time;   //内部使用,用来记录该矿机某些数据的最后修改时间
    uint32_t  internal_id = 0;    //内部id
    uint32_t  internal_id2 = 0;   //内部id2,用来进行紧凑表数据维护
    uint32_t  round1 = 0;         //最后被选中的容量算力轮次
    uint32_t  times1 = 0;         //最后被选中的容量算力轮次中被选中的次数(用作本轮次的算力衰减)
    uint32_t  round2 = 0;         //最后被选中的数据存储算力轮次
    uint32_t  times2 = 0;         //最后被选中的数据存储算力轮次中被选中的次数(用作本轮次的算力衰减)
    uint32_t  level = 66438;      //矿机评级 log2(100)*10000取整为66438,要注意存储算力计算溢出的问题
    uint16_t  status;             //当前状态 0-正常收益, 其他-封禁收益

    uint64_t  primary_key() const { return minerid; }
  };

  typedef multi_index<N(miner), miner> miner_table;

  //新模型的矿机内部id表(紧凑数组)
  struct miner2
  {
    uint64_t  internal_id;      //内部id
    uint64_t  minerid;          //矿机id
    uint64_t  next_id;          //下一个内部id，如果minerid不为0，则该字段为0
    
    uint64_t  primary_key() const { return internal_id; }
  };

  typedef multi_index<N(miner2), miner2> miner2_table;

  //内部紧凑表元数据
  struct miner2ex
  {
    uint64_t  next_id = 1;            //下一个内部id
    uint64_t  max_table_count = 0;    //内部表总条目数
    uint64_t  max_miner_count = 0;     //有效矿机数量

  };
  typedef eosio::singleton<N(miner2ex), miner2ex> gminer2ex_singleton;

  //新模型的全局参数
  struct newmparam
  {
    uint64_t  start_time = 0;                 //新经济模型的预定的开启时间(微秒单位)
    uint64_t  next_day_time = 0;              //下一个激励日开始的时间戳(微秒单位)
    uint64_t  task_space = 0;                 //当前需要满足的存储空间(以G为单位)
    uint64_t  last_inc_space = 0;             //上一次的空间增长量(以G为单位)
    int64_t   total_issue = 0;                //当前总增发的激励token  
    int64_t   total_destory = 0;              //当前总销毁的激励token
    int64_t   total_remain = 0;               //当前总剩余未发出的激励token
    int64_t   cur_issue = 0;                  //当次增发的激励token
    int64_t   cur_destory = 0;                //当次销毁的激励token  
    int64_t   cur_reward1 = 0;                //当次每一笔容量激励token
    int64_t   cur_reward2 = 0;                //当次每一笔存储激励token  
    uint64_t  next_round_time = 0;            //下一轮激励周期开始的时间戳(微秒单位)
    uint64_t  round_interval = 0;             //下一个周期的时间间隔
    uint32_t  reward_day = 0;                 //新经济开启的天数(从0开始计数)
    uint32_t  reward_round = 0;               //当前的激励周期
    uint32_t  last_reward_slot = 0;           //最后一次发激励的区块时间槽
    uint32_t  span_slot = 12;                 //每隔多少个区块进行一次激励
    uint8_t   reward_type = 0;                 //激励类型 0-容量激励 1-存储激励
    bool      is_started = false;             //新经济模型是否已经开启
  };
  typedef eosio::singleton<N(newmparam1), newmparam> gnewmparam_singleton;

  struct couterstate
  {
    uint64_t  total_space = 0;      //当前全网注册总空间
    uint64_t  reserved1 = 0;
    uint64_t  reserved2 = 0;
    uint64_t  reserved3 = 0;
  };
  typedef eosio::singleton<N(couterstate1), couterstate> gcouterstate_singleton;

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

  struct hdd_global_state2
  {
    uint64_t hdd_total_user = 0;
  };

  struct hdd_global_state3
  {
    uint64_t hdd_macc_user = 0;
  };

  typedef eosio::singleton<N(gparams), hdd_global_param> gparams_singleton;
  typedef eosio::singleton<N(paramguard), hdd_price_guard> paramguard_singleton;

  typedef eosio::singleton<N(gusercount), hdd_global_state2> gusercount_singleton;
  typedef eosio::singleton<N(gmacccount), hdd_global_state3> gmacccount_singleton;

  gusercount_singleton _globalu;
  hdd_global_state2 _gstateu;

  gmacccount_singleton _globalm;
  hdd_global_state3 _gstatem;

  struct deposit_rate
  {
    int64_t   rate = 10000;
    uint64_t  last_rate_time = current_time();
  };
  typedef eosio::singleton<N(gdeprate), deposit_rate> gdeprate_singleton;

  struct usd_price
  {
    uint64_t usdprice = 64500;
  };
  typedef eosio::singleton<N(gusdprice), usd_price> gusdprice_singleton;

  //bool is_bp_account(uint64_t uservalue);
  void check_bp_account(account_name bpacc, uint64_t id, bool isCheckId);

  int64_t calculate_fee_balance(int64_t oldbalance, int64_t hdd_per_cycle_fee, uint64_t last_hdd_time, uint64_t current_time);

  int64_t calculate_profit_balance(int64_t oldbalance, int64_t hdd_per_cycle_profit, uint64_t last_hdd_time, uint64_t current_time, uint64_t &deadline_time);


  void new_user_hdd(userhdd_index& userhdd, name user, int64_t balance, account_name payer);

  void chg_owner_space(userhdd_index& userhdd, name minerowner, uint64_t space_delta, bool is_increase, bool is_calc, uint64_t ct, uint64_t deadline_time);

  //计算抵押系数
  void calc_deposit_rate();  

  uint32_t insert_miner2(uint64_t minerid);
  void del_miner2(uint64_t internal_id);
  void rewardproc(uint64_t random1, uint64_t random2);

  uint64_t get_newmodel_start_time();

public:  

  static bool is_miner_exist(uint64_t minerid)
  {
    minerinfo_table _minerinfo( N(hddpool12345) , N(hddpool12345) );
    auto itminerinfo = _minerinfo.find(minerid);
    if(itminerinfo != _minerinfo.end())
      return true;    

    return false;
  }

  static uint64_t get_miner_max_space(uint64_t minerid)
  {
    minerinfo_table _minerinfo( N(hddpool12345) , N(hddpool12345) );
    auto itminerinfo = _minerinfo.find(minerid);
    if(itminerinfo != _minerinfo.end())
      return itminerinfo->max_space;    
    return 0;  
  }

  static name get_miner_pool_id(uint64_t minerid)
  {
    name pool_id;
    minerinfo_table _minerinfo( N(hddpool12345) , N(hddpool12345) );
    auto itminerinfo = _minerinfo.find(minerid);
    if(itminerinfo != _minerinfo.end()) {
      pool_id = itminerinfo->pool_id;  
    }
    return pool_id;  
  }

  static name get_miner_pool_owner(uint64_t minerid)
  {
    name pool_owner;
    minerinfo_table _minerinfo( N(hddpool12345) , N(hddpool12345) );
    auto itminerinfo = _minerinfo.find(minerid);
    if(itminerinfo != _minerinfo.end()) {
     storepool_index _storepool( N(hddpool12345) , N(hddpool12345));
     auto itstorepool = _storepool.find(itminerinfo->pool_id);
     if(itstorepool != _storepool.end()) {
       pool_owner = itstorepool->pool_owner;
     }

    }
    return pool_owner;  
  }

  static uint32_t get_next_reward_slot()
  {
    gnewmparam_singleton _gnewmparam( N(hddpool12345), N(hddpool12345));
    newmparam  _gstate;
    if(_gnewmparam.exists()){
      _gstate = _gnewmparam.get();
      return _gstate.last_reward_slot + _gstate.span_slot;
    }
    return 0xffffffff;
  }

};

