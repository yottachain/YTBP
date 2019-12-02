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

        void paydeposit(account_name user, uint64_t minerid, asset quant);
        void chgdeposit(name user, uint64_t minerid, bool is_increace, asset quant);
        void payforfeit(name user, uint64_t minerid, asset quant, uint8_t acc_type, name caller);
        void delminer(uint64_t minerid);
        void setrate(int64_t rate);

        void mchgdepacc(uint64_t minerid, name new_depacc);

        inline asset get_deposit( account_name user )const;


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
    depositpool_table _deposit(_self, user);
    auto acc = _deposit.find( user );    
    if ( acc != _deposit.end() ) {
        return acc->deposit_total;
    } 
    asset zero{0, CORE_SYMBOL};
    return zero;
}
