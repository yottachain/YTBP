#include <eosiolib/eosio.hpp>
#include <eosiolib/asset.hpp>
#include <eosiolib/singleton.hpp>
#include <eosiolib/symbol.hpp>


using eosio::name;
using eosio::asset;
using eosio::multi_index;
using eosio::indexed_by;
using eosio::const_mem_fun;


class hdddeposit : public eosio::contract {
    
    public:
        using contract::contract;

        void paydeppool(account_name user, asset quant);
        void unpaydeppool(account_name user, asset quant);

        void paydeppool2(account_name user, asset quant);
        void undeppool2(account_name user, asset quant);

        void depstore(account_name user, asset quant);
        void undepstore(account_name user, account_name caller);

        void paydeposit(account_name user, uint64_t minerid, asset quant);
        void incdeposit(uint64_t minerid, asset quant);
        void chgdeposit(name user, uint64_t minerid, bool is_increase, asset quant);
        void mforfeit(name user, uint64_t minerid, asset quant, std::string memo, uint8_t acc_type, name caller);
        void delminer(uint64_t minerid);
        void setrate(int64_t rate);

        void mchgdepacc(uint64_t minerid, name new_depacc);

        void updatevote(name user);

        void channellog(uint8_t type, asset quant, account_name user);

        
        inline asset get_deposit( account_name user )const;
        inline asset get_depositfree( account_name user )const;
        inline asset get_miner_deposit( uint64_t minerid )const;
        inline asset get_miner_forfeit( uint64_t minerid )const;
        inline name  get_miner_depacc(uint64_t minerid)const;
        inline bool is_deposit_enough( asset deposit, uint64_t max_space ) const;
        inline asset calc_deposit( uint64_t space )const;


    private:

        void check_bp_account(account_name bpacc, uint64_t id, bool isCheckId);

        //记录某个账户的存储押金池信息
        struct depositpool {
            name        account_name;
            uint8_t     pool_type = 0;
            asset       deposit_total;  //用户押金池总额(扣除了罚金后的总额)
            asset       deposit_free;   //用户押金池剩余的可以给矿机缴纳押金的总额
            asset       deposit_his;    //用户总抵押量(包含罚金)
            uint64_t    primary_key()const { return account_name.value; }
        };        
        typedef multi_index<N(depositpool), depositpool> depositpool_table; 

        //记录某个账户的存储押金池信息(跨链token存储抵押)
        struct depositpool2 {
            name        account_name;
            uint8_t     pool_type = 0;
            asset       deposit_total;  //用户押金池总额(扣除了罚金后的总额)
            asset       deposit_free;   //用户押金池剩余的可以给矿机缴纳押金的总额
            asset       deposit_his;    //用户总抵押量(包含罚金)
            uint64_t    primary_key()const { return account_name.value; }
        };        
        typedef multi_index<N(depositpool2), depositpool2> depositpool2_table; 

        struct storedeposit {
            name        account_name;
            asset       deposit_total;
            asset       reserved1;
            uint64_t    reserved2;
            uint64_t    reserved3;
            uint64_t    primary_key()const { return account_name.value; }
        };       
        typedef multi_index<N(depstore), storedeposit> storedeposit_table; 

        //记录哪个账户为哪个矿机抵押了多少钱
        struct miner2dep {
            uint64_t    minerid;    //矿机ID
            uint8_t     miner_type = 0;
            name        account_name; 
            asset       deposit;
            asset       dep_total;
            uint64_t    primary_key()const { return minerid; }
            uint64_t    by_accname()const    { return account_name.value;  }
        };
        typedef multi_index<N(miner2dep), miner2dep,
                            indexed_by<N(accname), const_mem_fun<miner2dep, uint64_t, &miner2dep::by_accname>  >
                            > minerdeposit_table;   

        struct deposit_rate
        {
            int64_t rate = 100;
        };
        typedef eosio::singleton<N(gdepositrate), deposit_rate> grate_singleton;
};

//these const functions will be called by eosio.token transfer action to check where a user has deposit


asset hdddeposit::get_deposit( account_name user ) const
{
    asset deposit{0, CORE_SYMBOL};

    depositpool_table _deposit(_self, user);    
    auto acc = _deposit.find( user );    
    if ( acc != _deposit.end() ) {
        deposit += acc->deposit_total;
    } 
    depositpool2_table _deposit2(_self, user);    
    auto acc2 = _deposit2.find( user );    
    if ( acc2 != _deposit2.end() ) {
        deposit += acc2->deposit_total;
    } 

    return deposit;
}

asset hdddeposit::get_depositfree( account_name user ) const
{
    asset deposit{0, CORE_SYMBOL};

    depositpool_table _deposit(_self, user);
    auto acc = _deposit.find( user );    
    if ( acc != _deposit.end() ) {
        deposit += acc->deposit_free;
    } 

    depositpool2_table _deposit2(_self, user);    
    auto acc2 = _deposit2.find( user );    
    if ( acc2 != _deposit2.end() ) {
        deposit += acc2->deposit_free;
    } 

    return deposit;
}


asset hdddeposit::get_miner_deposit( uint64_t minerid ) const 
{
    minerdeposit_table _mdeposit(_self, _self);
    auto miner = _mdeposit.find( minerid );
    if(miner != _mdeposit.end())
        return miner->deposit;

    asset zero{0, CORE_SYMBOL};
    return zero;
}

asset hdddeposit::get_miner_forfeit( uint64_t minerid ) const 
{
    minerdeposit_table _mdeposit(_self, _self);
    auto miner = _mdeposit.find( minerid );
    if(miner != _mdeposit.end()) {
        if(miner->dep_total.amount > miner->deposit.amount)
           return (miner->dep_total - miner->deposit);
    }
        
    asset zero{0, CORE_SYMBOL};
    return zero;
}

name hdddeposit::get_miner_depacc( uint64_t minerid ) const 
{
    minerdeposit_table _mdeposit(_self, _self);
    name depacc = {0};
    auto miner = _mdeposit.find( minerid );
    if(miner != _mdeposit.end()) {
        depacc = miner->account_name;
    }
        
    return depacc;
}


bool hdddeposit::is_deposit_enough( asset deposit, uint64_t max_space ) const 
{
    grate_singleton grate(_self, _self);
    deposit_rate    rate_state;
    int64_t rate;

    if (!grate.exists()) {
        rate = 100;
    } else {
        rate = grate.get().rate;
    }
    double drate = ((double)rate)/100;
    uint32_t one_gb = 64 * 1024; //1GB, 以16k一个分片大小为单位

    int64_t am = (int64_t)((((double)max_space)/one_gb) * drate * 10000);
    if(deposit.amount >= am)
        return true;

    return false;
}

asset hdddeposit::calc_deposit( uint64_t space )const
{
    grate_singleton grate(_self, _self);
    deposit_rate    rate_state;
    int64_t rate;

    if (!grate.exists()) {
        rate = 100;
    } else {
        rate = grate.get().rate;
    }

    double drate = ((double)rate)/100;
    uint32_t one_gb = 64 * 1024; //1GB, 以16k一个分片大小为单位

    int64_t am = (int64_t)((((double)space)/one_gb) * drate * 10000);
    asset dep{am,CORE_SYMBOL};

    return dep;
}

