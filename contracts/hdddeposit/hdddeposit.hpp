#include <eosiolib/eosio.hpp>
#include <eosiolib/asset.hpp>
#include <eosiolib/singleton.hpp>
#include <eosiolib/symbol.hpp>

using eosio::name;
using eosio::asset;
using eosio::multi_index;


class hdddeposit : public eosio::contract {
    
    public:
        using contract::contract;

        void paydeposit(name user, uint64_t minerid, asset quant);
        void undeposit(name user, uint64_t minerid, asset quant);
        void clearminer(uint64_t minerid);
        void clearacc(name user);

        inline asset get_deposit( account_name user )const;


    private:
        //记录某个账户缴纳的押金总量
        struct accdeposit {
            name        account_name;
            asset       amount; 
            uint64_t    primary_key()const { return account_name.value; }
        };
        typedef multi_index<N(accdeposit), accdeposit> accdeposit_table; 

        //记录哪个账户为哪个矿机抵押了多少钱
        struct minerdeposit {
            uint64_t    minerid;    //矿机ID
            name        account_name; 
            asset       deposit;
            uint64_t    primary_key()const { return minerid; }
        };
        typedef multi_index<N(minerdeposit), minerdeposit> minerdeposit_table;       
};

//thid const function will be called by eosio.token transfer action to check where a user has hdd deposit
asset hdddeposit::get_deposit( account_name user ) const
{
    accdeposit_table _deposit(_self, user);
    auto acc = _deposit.find( user );    
    if ( acc != _deposit.end() ) {
        return acc->amount;
    } 
    asset zero{0, CORE_SYMBOL};
    return zero;
}
