#pragma once

#include <eosiolib/asset.hpp>
#include <eosiolib/eosio.hpp>

namespace hdd {

    using namespace eosio;
    using std::string;

    static uint64_t     SYMBOL = string_to_symbol(4, "HDD");
    static int64_t      MAX_SUPPLY = 10'000'000'000'0000;

    class token : public contract {
    public:
        token( account_name self ):contract(self){}

        void create();

        void claim( account_name claimer );

        void signup( account_name to, string memo );

        void issue( account_name to, asset quantity, string memo );

        void transfer( account_name from,
                       account_name to,
                       asset        quantity,
                       string       memo );

        inline asset get_supply( symbol_name sym )const;

        inline asset get_balance( account_name owner, symbol_name sym )const;

    private:
        //@abi table accounts i64
        struct account {
            asset    balance;

            uint64_t primary_key()const { return balance.symbol.name(); }
        };

        //@abi table stat i64
        struct currencystat {
            asset          supply;
            asset          max_supply;
            account_name   issuer;

            uint64_t primary_key()const { return supply.symbol.name(); }
        };

        typedef eosio::multi_index<N(accounts), account> accounts;
        typedef eosio::multi_index<N(stat), currencystat> stats;

        void sub_balance( account_name owner, asset value );
        void add_balance( account_name owner, asset value, account_name ram_payer );

    };

    asset token::get_supply( symbol_name sym )const
    {
        stats statstable( _self, sym );
        const auto& st = statstable.get( sym );
        return st.supply;
    }

    asset token::get_balance( account_name owner, symbol_name sym )const
    {
        accounts accountstable( _self, owner );
        const auto& ac = accountstable.get( sym );
        return ac.balance;
    }

}
