#include <eosiolib/eosio.hpp>
#include <eosiolib/asset.hpp>
#include <eosiolib/singleton.hpp>
#include <eosiolib/symbol.hpp>


using eosio::name;
using eosio::asset;
using eosio::multi_index;
using eosio::indexed_by;
using eosio::const_mem_fun;


class hddlock : public eosio::contract {
    
    public:
        using contract::contract;

        void init();
        void addrule(uint64_t id, std::vector<uint64_t>& times, std::vector<uint8_t>& percentage, std::string& desc);


    private:

        struct lockrule {
            uint64_t    ruleid;
            uint64_t    primary_key()const { return ruleid; }
        };
        typedef multi_index<N(lockrule), lockrule> lockrule_table; 

};


