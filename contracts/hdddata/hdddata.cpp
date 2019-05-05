#include "hdddata.hpp"
#include <eosiolib/eosio.hpp>
#include <eosiolib/print.hpp>
#include <eosiolib/serialize.hpp>
#include <eosiolib/multi_index.hpp>
#include <eosio.token/eosio.token.hpp>

#include <cmath>
#include <string>
#include <type_traits>
#include <optional>
using namespace eosio;

const uint32_t hours_in_one_day = 24;
const uint32_t minutes_in_one_day = hours_in_one_day * 60;
const uint32_t seconds_in_one_day = minutes_in_one_day * 60;
const uint32_t seconds_in_one_week = seconds_in_one_day * 7;
const uint32_t seconds_in_one_year = seconds_in_one_day * 365;
const int64_t  useconds_per_day      = 24 * 3600 * int64_t(1000000);

const uint32_t one_gb  = 1024*1024*1024;   //1GB
const uint32_t data_slice_size = 8*1024;   // among 4k-32k,set it as 8k
const uint32_t preprocure_space = one_gb;  // set as 1G
const name HDD_OFFICIAL = name{N(hddofficial)};

// constructor

hdddata::hdddata( account_name s )
: eosio::contract(s),
_maccount(_self,  _self),
_hmarket(_self, _self),
_producer(_self, _self)  {

      auto itr = _hmarket.find(S(4,HDDCORE));

      if( itr == _hmarket.end() ) {
         auto system_token_supply   = eosio::token(N(eosio.token)).get_supply(eosio::symbol_type(CORE_SYMBOL).name()).amount;
         if( system_token_supply > 0 ) {
            itr = _hmarket.emplace( _self, [&]( auto& m ) {
               m.supply.amount = 100000000000000ll;
               m.supply.symbol = S(4,HDDCORE);
               m.base.balance.amount = 1024ll*1024*1024*1024*1024;
               m.base.balance.symbol = S(0,HDD);
               m.quote.balance.amount = system_token_supply / 1000;
               m.quote.balance.symbol = CORE_SYMBOL;
            });
         }
      } else {
         //print( "ram market already created" );
      }


}

/*
hdddata::hdddata( account_name s )
: contract(s) 
{

}
*/

hdddata:: ~hdddata() {
}

/*
symbol hdddata::core_symbol()const {
    const static auto sym = get_core_symbol( _hmarket );
    return sym;
}
*/

/*
//@abit action
void hdddata::init(name owner) {
    require_auth( owner ); 
    hbalance_table            _hbalance(_self, _self.value);
    auto hbalance_itr = _hbalance.find(owner.value);
    
    eosio_assert( hbalance_itr == _hbalance.end(), "_hbalance table has already been initialized" );
    
    _hbalance.emplace(_self, [&](auto &row) {
        //todo check the 1st time insert
        row.owner = owner;
        row.last_hdd_balance=10;
        row.hdd_per_cycle_fee=10;
        row.hdd_per_cycle_profit=10;
        row.hdd_space=20;
        row.last_hdd_time = current_time();
    });

    
    auto itr = _hmarket.find(hddcore_symbol.raw());
    
    eosio_assert( itr == _hmarket.end(), "_hmarket has already been initialized" );
    
    auto system_token_supply   = eosio::token::get_supply(token_account, yta_symbol.code() );
    
    eosio_assert( system_token_supply.symbol == yta_symbol, "specified core symbol does not exist (precision mismatch)" ); 
    eosio_assert( system_token_supply.amount > 0, "system token supply must be greater than 0" ); 
    
    _hmarket.emplace( _self, [&]( auto& m ) {
        m.supply.amount = 100000000000000ll;
        m.supply.symbol = hddcore_symbol;
        m.base.balance.amount = 1024ll*1024*1024*1024*1024;
        m.base.balance.symbol = hdd_symbol;
        m.quote.balance.amount = system_token_supply.amount / 1000;
        m.quote.balance.symbol = yta_symbol;
    });
    
}
*/

//@abi action
void hdddata::gethbalance(name owner) {
    require_auth(owner);    
    hbalance_table  _hbalance(_self, _self);
    auto hbalance_itr = _hbalance.find(owner.value);
    if(hbalance_itr == _hbalance.end()) {
        _hbalance.emplace(owner, [&](auto &row) {
            print("A   gethbalance :  ", owner,   " is new  ... \n");
            row.owner = owner;
            row.last_hdd_balance=10;
            row.hdd_per_cycle_fee=5;
            row.hdd_per_cycle_profit=10;
            row.hdd_space=10;
            row.last_hdd_time = current_time();
        });
    } else {
        uint64_t tmp_t = current_time();
        _hbalance.modify(hbalance_itr, _self, [&](auto &row) {
            print("gethbalance  modify  last_hdd_balance :  ", hbalance_itr->get_last_hdd_balance(),  "\n");
            
            uint64_t slot_t = (tmp_t - hbalance_itr->last_hdd_time)/1000000ll;   //convert to seconds
            
            print("gethbalance  modify  slot_t :  ", slot_t,  "\n");
            //todo  check overflow and time cycle 
            row.last_hdd_balance=
            hbalance_itr->get_last_hdd_balance() + 
            slot_t * ( hbalance_itr->get_hdd_per_cycle_profit() - hbalance_itr->get_hdd_per_cycle_fee() ) * seconds_in_one_day;
            
            print("B   gethbalance   .last_hdd_balance :  ", row.last_hdd_balance,  "\n");
            row.last_hdd_time = tmp_t;
        });
        
        
    }

}


void hdddata::update_hddofficial(hbalance_table& _hbalance, const uint64_t _hb, const uint64_t time) {
    auto hbalance_itr = _hbalance.find(HDD_OFFICIAL.value);
    eosio_assert( hbalance_itr != _hbalance.end(), "no HDD_OFFICIAL exists  in  hbalance table" );
    _hbalance.modify(hbalance_itr, _self, [&](auto &row) {
        //todo  check overflow and time cycle 
        row.last_hdd_balance  += _hb;
        row.last_hdd_time = time;
    });
}

void hdddata::gethsum() {
    require_auth(_self);
    hbalance_table            _hbalance(_self, _self);
    auto hbalance_itr = _hbalance.find(HDD_OFFICIAL.value);
    eosio_assert( hbalance_itr != _hbalance.end(), "no HDD_OFFICIAL exists  in  hbalance table" );
    //todo check the 1st time insert
    uint64_t tmp_t = current_time();
    _hbalance.modify(hbalance_itr, _self, [&](auto &row) {
        print("gethbalance  modify  last_hdd_balance :  ", hbalance_itr->get_last_hdd_balance(),  "\n");
        
        uint64_t slot_t = (tmp_t - hbalance_itr->last_hdd_time)/1000000ll;   //convert to seconds
        
        print("gethbalance  modify  slot_t :  ", slot_t,  "\n");
        //todo  check overflow and time cycle 
        row.last_hdd_balance=
        hbalance_itr->get_last_hdd_balance() + 
        slot_t * ( hbalance_itr->get_hdd_per_cycle_profit() - hbalance_itr->get_hdd_per_cycle_fee() ) * seconds_in_one_day;
        
        print("B   gethbalance   .last_hdd_balance :  ", row.last_hdd_balance,  "\n");
        row.last_hdd_time = tmp_t;
    });

}
//@abi action
void hdddata::sethfee(name owner, uint64_t fee) {
    require_auth(_self);
    require_auth(owner);
    hbalance_table  _hbalance(owner, owner.value);
    auto hbalance_itr = _hbalance.find(owner.value);
    
    eosio_assert( hbalance_itr != _hbalance.end(), "no owner exists  in _hbalance table" );
    
    //每周期费用 <= （占用存储空间*数据分片大小/1GB）*（记账周期/ 1年）
    //eos_assert( xxx, "");
    _hbalance.modify(hbalance_itr, owner, [&](auto &row) {
        print("A   row.hdd_per_cycle_fee :  ", row.hdd_per_cycle_fee,  " \n");
        //todo check overflow
        row.hdd_per_cycle_fee -=fee;
    });
    print("B   row.hdd_per_cycle_fee :  ", hbalance_itr->get_hdd_per_cycle_fee(),  " \n");
    
    //每周期费用 <= （占用存储空间*数据分片大小/1GB）*（记账周期/ 1年）
}

//@abi action
void hdddata::subhbalance(name owner,  uint64_t balance){
    require_auth(_self);
    require_auth(owner);
    hbalance_table            _hbalance(_self, _self);
    auto hbalance_itr = _hbalance.find(owner.value);
    
    eosio_assert( hbalance_itr != _hbalance.end(), "no owner exists  in _hbalance table" );

    _hbalance.modify(hbalance_itr, owner, [&](auto &row) {
        //todo check overflow
        row.last_hdd_balance -=balance;
    });

}

//@abi action
void hdddata::addhspace(name owner, name hddaccount, uint64_t space){
    require_auth(owner);
    require_auth(hddaccount);
    hbalance_table            _hbalance(_self, _self);
    auto hbalance_itr = _hbalance.find(hddaccount.value);
    
    eosio_assert( hbalance_itr != _hbalance.end(), "no owner exists  in _hbalance table" );
    
    _hbalance.modify(hbalance_itr, hddaccount, [&](auto &row) {
        //todo check overflow
        row.hdd_space +=space;
    });

}

//@abi action
void hdddata::subhspace(name owner, name hddaccount, uint64_t space){
    require_auth(owner);
    require_auth(hddaccount);
    hbalance_table            _hbalance(_self, _self);
    auto hbalance_itr = _hbalance.find(hddaccount.value);
    
    eosio_assert( hbalance_itr != _hbalance.end(), "no owner exists  in _hbalance table" );
    

    _hbalance.modify(hbalance_itr, hddaccount, [&](auto &row) {
        //todo check overflow
        row.hdd_space -=space;
    });

}

//@abi action
void hdddata::newmaccount(name mname, name owner) {
    require_auth(mname);
    
    //maccount_table _maccount(_self, _self.value);
    auto maccount_itr = _maccount.find(mname.value);
    eosio_assert( maccount_itr == _maccount.end(), "owner already exist in _maccount table \n" );

    _maccount.emplace(mname, [&](auto &row) {
        row.ming_id = mname.value;
        row.owner = owner;
    });

    require_auth(owner);
    hbalance_table  _hbalance(_self, _self);
    auto hbalance_itr = _hbalance.find(owner.value);
    
    eosio_assert( hbalance_itr == _hbalance.end(), "no owner exists  in _hbalance table" );
    
    _hbalance.emplace(owner, [&](auto &row) {
        //todo check the 1st time insert to modify
        row.owner = owner;
        row.last_hdd_balance=10;
        row.hdd_per_cycle_fee=10;
        row.hdd_per_cycle_profit=10;
        row.hdd_space=0;
        row.last_hdd_time=current_time();
    });
    
}

//@abi action
void hdddata::addmprofit(name mname, uint64_t space){
    require_auth(mname);
    
    //maccount_table _maccount(_self, _self.value);
    auto maccount_itr = _maccount.find(mname.value);
    eosio_assert( maccount_itr != _maccount.end(), "no owner exists  in _hbalance table" );

    hbalance_table  _hbalance(_self, _self);
    auto owner_id = maccount_itr->get_owner();
    auto hbalance_itr = _hbalance.find(owner_id);
    
    eosio_assert( hbalance_itr == _hbalance.end(), "no owner exists  in _hbalance table" );

    //space verification    
    eosio_assert(hbalance_itr->get_hdd_space()+ data_slice_size ==space , "not correct verification");
    
    _hbalance.modify(hbalance_itr, _self, [&](auto &row) {
        //todo check the 1st time insert
        //row.owner = owner;
        row.hdd_space += data_slice_size;
        row.hdd_per_cycle_profit += (preprocure_space*data_slice_size/one_gb);
    });
    

    //每周期收益 += (预采购空间*数据分片大小/1GB）*（记账周期/ 1年）        
}

//@abi action
void hdddata::buyhdd(name buyer, name receiver, asset quant) {
    
    require_auth(buyer);
    eosio_assert( quant.amount > 0, "must purchase a positive amount" );

    INLINE_ACTION_SENDER(eosio::token, transfer)( N(eosio.token), {buyer,N(active)},
    { buyer, N(eosio.hdd), quant, std::string("buy hdd") } );


    int64_t bytes_out;

    const auto& market = _hmarket.get(S(4,HDDCORE), "hdd market does not exist");
    _hmarket.modify( market, 0, [&]( auto& es ) {
        bytes_out = es.convert( quant, S(0,HDD)).amount;
    });
    print("bytes_out:  ", bytes_out, "\n");
    eosio_assert( bytes_out > 0, "must reserve a positive amount" );
    hbalance_table            _hbalance(_self, _self);
    auto res_itr = _hbalance.find( receiver.value );
    if( res_itr ==  _hbalance.end() ) {
        res_itr = _hbalance.emplace( receiver, [&]( auto& res ) {
            res.owner                 = receiver;
            res.last_hdd_balance = bytes_out;
            res.last_hdd_time      = current_time();
        });
    } else {
        _hbalance.modify( res_itr, receiver, [&]( auto& res ) {
            res.last_hdd_balance += bytes_out;
            res.last_hdd_time        =  current_time();
        });
    }
    
}

//@abi action
void hdddata::sellhdd(name account, uint64_t quant){
    
    require_auth(account);
    eosio_assert( quant > 0, "cannot sell negative hdd" );
    hbalance_table            _hbalance(_self, _self);
    auto res_itr = _hbalance.find( account.value );
    eosio_assert( res_itr != _hbalance.end(), "no resource row" );
    
    //need to calculate the latest hddbalance
    eosio_assert( res_itr->get_last_hdd_balance() >= quant, "insufficient hdd" );
    
    asset tokens_out;
    auto itr = _hmarket.find(S(4,HDDCORE));
    _hmarket.modify( itr, 0, [&]( auto& es ) {
        /// the cast to int64_t of quant is safe because we certify quant is <= quota which is limited by prior purchases
        tokens_out = es.convert( asset(quant, S(0,HDD)), CORE_SYMBOL);
    });
    
    _hbalance.modify( res_itr, account, [&]( auto& res ) {
        res.last_hdd_balance -= quant;
        res.last_hdd_time        =  current_time();
    });

    INLINE_ACTION_SENDER(eosio::token, transfer)( N(eosio.token), {N(eosio.hdd),N(active)},
    { N(eosio.hdd), account, asset(tokens_out), std::string("sell hdd") } );

}

   asset exchange_state::convert_to_exchange( connector& c, asset in ) {

      real_type R(supply.amount);
      real_type C(c.balance.amount+in.amount);
      real_type F(c.weight/1000.0);
      real_type T(in.amount);
      real_type ONE(1.0);

      real_type E = -R * (ONE - std::pow( ONE + T / C, F) );
      //print( "E: ", E, "\n");
      int64_t issued = int64_t(E);

      supply.amount += issued;
      c.balance.amount += in.amount;

      return asset( issued, supply.symbol );
   }

   asset exchange_state::convert_from_exchange( connector& c, asset in ) {
      eosio_assert( in.symbol== supply.symbol, "unexpected asset symbol input" );

      real_type R(supply.amount - in.amount);
      real_type C(c.balance.amount);
      real_type F(1000.0/c.weight);
      real_type E(in.amount);
      real_type ONE(1.0);


     // potentially more accurate: 
     // The functions std::expm1 and std::log1p are useful for financial calculations, for example, 
     // when calculating small daily interest rates: (1+x)n
     // -1 can be expressed as std::expm1(n * std::log1p(x)). 
     // real_type T = C * std::expm1( F * std::log1p(E/R) );
      
      real_type T = C * (std::pow( ONE + E/R, F) - ONE);
      //print( "T: ", T, "\n");
      int64_t out = int64_t(T);

      supply.amount -= in.amount;
      c.balance.amount -= out;

      return asset( out, c.balance.symbol );
   }

   asset exchange_state::convert( asset from, symbol_type to ) {
      auto sell_symbol  = from.symbol;
      auto ex_symbol    = supply.symbol;
      auto base_symbol  = base.balance.symbol;
      auto quote_symbol = quote.balance.symbol;

      //print( "From: ", from, " TO ", asset( 0,to), "\n" );
      //print( "base: ", base_symbol, "\n" );
      //print( "quote: ", quote_symbol, "\n" );
      //print( "ex: ", supply.symbol, "\n" );

      if( sell_symbol != ex_symbol ) {
         if( sell_symbol == base_symbol ) {
            from = convert_to_exchange( base, from );
         } else if( sell_symbol == quote_symbol ) {
            from = convert_to_exchange( quote, from );
         } else { 
            eosio_assert( false, "invalid sell" );
         }
      } else {
         if( to == base_symbol ) {
            from = convert_from_exchange( base, from ); 
         } else if( to == quote_symbol ) {
            from = convert_from_exchange( quote, from ); 
         } else {
            eosio_assert( false, "invalid conversion" );
         }
      }

      if( to != from.symbol )
         return convert( from, to );

      return from;
   }



EOSIO_ABI( hdddata, (gethbalance)(gethsum)(sethfee)(newmaccount)(addmprofit)(subhbalance)(buyhdd)(sellhdd)(addhspace)(subhspace))
/*
extern "C" {
    void apply(uint64_t receiver, uint64_t code, uint64_t action) {
        if(code==receiver)
        {
            switch(action)
            {
                EOSIO_DISPATCH_HELPER( hdddata, (gethbalance)(gethsum)(sethfee)(newmaccount)(addmprofit)(subhbalance)(buyhdd)(sellhdd)(addhspace)(subhspace) )
            }
        }
    }
};
*/
