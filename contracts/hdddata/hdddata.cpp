#include "hdddata.hpp"
#include <eosiolib/eosio.hpp>
#include <eosiolib/print.hpp>
#include <eosiolib/serialize.hpp>
#include <eosiolib/multi_index.hpp>
#include <eosio.token/eosio.token.hpp>

#include <cmath>
using namespace eosio;

const uint32_t hours_in_one_day = 24;
const uint32_t minutes_in_one_day = hours_in_one_day * 60;
const uint32_t seconds_in_one_day = minutes_in_one_day * 60;
const uint32_t seconds_in_one_week = seconds_in_one_day * 7;
const uint32_t seconds_in_one_year = seconds_in_one_day * 365;
const name HDD_OFFICIAL = "hddofficial"_n;

// constructor
hdddata::hdddata( name s, name code, datastream<const char*> ds )
 : eosio::contract(s, code, ds),
   _hbalance(_self, _self.value),
   _maccount(_self, _self.value),
   _hmarket(_self, _self.value),
   _producer(_self, _self.value)  {
     
	print( "construct system \n" );
	auto hbalance_itr = _hbalance.find(HDD_OFFICIAL.value);
	if(hbalance_itr == _hbalance.end()) {
		_hbalance.emplace(_self, [&](auto &row) {
			//todo check the 1st time insert
			row.owner = HDD_OFFICIAL;
			row.last_hdd_balance=10;
			row.hdd_per_cycle_fee=10;
			row.hdd_per_cycle_profit=10;
			row.hdd_space=20;
			row.last_hdd_time = current_time();
			
			print( "create owner :  ", HDD_OFFICIAL, " while constructor \n" );
		});
	}
	
	auto itr = _hmarket.find(hddcore_symbol.raw());
	eosio_assert( itr == _hmarket.end(), "hdd contract has already been initialized" );
	
    auto system_token_supply   = eosio::token::get_supply(token_account, yta_symbol.code() );
    eosio_assert( system_token_supply.symbol == yta_symbol, "specified core symbol does not exist (precision mismatch)" ); 

	eosio_assert( system_token_supply.amount > 0, "system token supply must be greater than 0" ); 
	//auto system_token_supply = 0;
	
     _hmarket.emplace( _self, [&]( auto& m ) {
               m.supply.amount = 100000000000000ll;
               m.supply.symbol = hddcore_symbol;
               m.base.balance.amount = 1024ll*1024*1024*1024*1024;
               m.base.balance.symbol = hdd_symbol;
               m.quote.balance.amount = system_token_supply.amount / 1000;
               m.quote.balance.symbol = yta_symbol;
            });
    }

   hdddata:: ~hdddata() {
   }
   
 symbol hdddata::core_symbol()const {
      const static auto sym = get_core_symbol( _hmarket );
      return sym;																						
   }
   
//@abi action
void hdddata::gethbalance(name owner) {
	//	当前余额=上次余额+(当前时间-上次余额时间)*（每周期收益-每周期费用）
	require_auth(owner);	
	//hbalance_table  hbalance(_self, _self.value);
	auto hbalance_itr = _hbalance.find(owner.value);
	if(hbalance_itr == _hbalance.end()) {
		_hbalance.emplace(owner, [&](auto &row) {
			//todo check the 1st time insert
			row.last_hdd_balance=0;
			row.hdd_per_cycle_fee=0;
			row.hdd_per_cycle_profit=0;
			row.hdd_space=0;
			row.last_hdd_time = current_time();
		});
	} else {
		_hbalance.modify(hbalance_itr, _self, [&](auto &row) {
			//todo  check overflow and time cycle 
			row.last_hdd_balance=
				hbalance_itr->get_last_hdd_balance() + 
				//( current_time() - (hbalance_itr->last_hdd_time) )
				10 * ( hbalance_itr->get_hdd_per_cycle_profit()-hbalance_itr->get_hdd_per_cycle_fee() );
				
			row.last_hdd_time = current_time();
		});
	}

}

void hdddata::gethsum() {
	require_auth(_self);
	
	auto hbalance_itr = _hbalance.find(HDD_OFFICIAL.value);
	if(hbalance_itr != _hbalance.end()) {
		//todo check the 1st time insert
		_hbalance.modify(hbalance_itr, _self, [&](auto &row) {
		//todo  check overflow and time cycle 
		row.last_hdd_balance=
			hbalance_itr->get_last_hdd_balance() + 
			// todo ((uint64_t)( current_time() - hbalance_itr->last_hdd_time ))
			10 *( hbalance_itr->get_hdd_per_cycle_profit()-hbalance_itr->get_hdd_per_cycle_fee() );
		row.last_hdd_time = current_time();
		});
	}
}
//@abi action
void hdddata::sethfee(name owner, uint64_t fee) {
	require_auth(_self);
	require_auth(owner);
	//hbalance_table  _hbalance(owner, owner.value);
	auto hbalance_itr = _hbalance.find(owner.value);
	if(hbalance_itr != _hbalance.end()) {
		//每周期费用 <= （占用存储空间*数据分片大小/1GB）*（记账周期/ 1年）
		//eos_assert( xxx, "");
		_hbalance.modify(hbalance_itr, owner, [&](auto &row) {
			//todo check overflow
			row.hdd_per_cycle_fee -=fee;
		});
		}else {
			print( "no owner in _hbalance :  ", owner, "\n" );
			
		}
	
	//每周期费用 <= （占用存储空间*数据分片大小/1GB）*（记账周期/ 1年）
}

//@abi action
void hdddata::subhbalance(name owner,  uint64_t balance){
	require_auth(_self);
	require_auth(owner);
	//hbalance_table  hbalance(owner, owner.value);
	auto hbalance_itr = _hbalance.find(owner.value);
	if(hbalance_itr != _hbalance.end()) {
		_hbalance.modify(hbalance_itr, owner, [&](auto &row) {
			//todo check overflow
			row.last_hdd_balance -=balance;
		});
	}else {
			print( "no owner in _hbalance :  ", owner, "\n" );
			
	}
}

//@abi action
void hdddata::addhspace(name owner, name hddaccount, uint64_t space){
	require_auth(owner);
	require_auth(hddaccount);
	auto hbalance_itr = _hbalance.find(hddaccount.value);
	if(hbalance_itr != _hbalance.end()) {
		_hbalance.modify(hbalance_itr, hddaccount, [&](auto &row) {
			//todo check overflow
			row.hdd_space +=space;
		});
	}else {
			print( "no owner in _hbalance :  ", owner, "\n" );
			
	}
}

//@abi action
void hdddata::subhspace(name owner, name hddaccount, uint64_t space){
	require_auth(owner);
	require_auth(hddaccount);
	auto hbalance_itr = _hbalance.find(hddaccount.value);
	if(hbalance_itr != _hbalance.end()) {
		_hbalance.modify(hbalance_itr, hddaccount, [&](auto &row) {
			//todo check overflow
			row.hdd_space -=space;
		});
	}else {
			print( "no owner in _hbalance :  ", owner, "\n" );
			
	}
}

//@abi action
void hdddata::newmaccount(name mname, name owner) {
	require_auth(mname);
	//maccount_table _maccount(_self, _self.value);
	auto maccount_itr = _maccount.find(mname.value);
	if(maccount_itr == _maccount.end()) {
		_maccount.emplace(mname, [&](auto &row) {
			row.ming_id = mname.value;
			row.owner = owner;
		 });
	} else {
		print( "owner  :  ", owner, "  already exist in  _hbalance \n" );
		return ;
	}
	
	require_auth(owner);
	//hbalance_table  hbalance(_self, _self.value);
	auto hbalance_itr = _hbalance.find(owner.value);
	if(hbalance_itr == _hbalance.end()) {
	_hbalance.emplace(owner, [&](auto &row) {
		//todo check the 1st time insert
		row.owner = owner;
		row.hdd_space=0;
	});
	} 
}

//@abi action
void hdddata::addmprofit(name mname, uint64_t space){
	require_auth(mname);
	//maccount_table _maccount(_self, _self.value);
	auto maccount_itr = _maccount.find(mname.value);
	if( maccount_itr != _maccount.end()) {
	//hbalance_table  _hbalance(_self, _self.value);
		auto owner_id = maccount_itr->get_owner();
		auto hbalance_itr = _hbalance.find(owner_id);
		if(hbalance_itr == _hbalance.end()) {
			_hbalance.emplace(_self, [&](auto &row) {
			//todo check the 1st time insert
			//row.owner = owner;
			row.hdd_space=space;
			});
		} else {
		//todo 
			print( "no owner in _hbalance :  ", owner_id, "\n" );
		}
	}
	//每周期收益 += (预采购空间*数据分片大小/1GB）*（记账周期/ 1年）		
}
	
//@abi action
void hdddata::buyhdd(name buyer, name receiver, asset quant) {
	require_auth(buyer);
	eosio_assert( quant.amount > 0, "must purchase a positive amount" );

	INLINE_ACTION_SENDER(eosio::token, transfer)( token_account, {buyer,active_permission},
		{ buyer, hdd_account, quant, std::string("buy hdd") } );

	int64_t bytes_out;

	const auto& market = _hmarket.get(hddcore_symbol.raw(), "hdd market does not exist");
	_hmarket.modify( market, same_payer, [&]( auto& es ) {
		bytes_out = es.convert( quant, hdd_symbol).amount;
	});

	eosio_assert( bytes_out > 0, "must reserve a positive amount" );
	
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
	
	auto res_itr = _hbalance.find( account.value );
	eosio_assert( res_itr != _hbalance.end(), "no resource row" );
	
	//need to calculate the latest hddbalance
    eosio_assert( res_itr->get_last_hdd_balance() >= quant, "insufficient hdd" );
	
	asset tokens_out;
    auto itr = _hmarket.find(hddcore_symbol.raw());
    _hmarket.modify( itr, same_payer, [&]( auto& es ) {
          /// the cast to int64_t of quant is safe because we certify quant is <= quota which is limited by prior purchases
          tokens_out = es.convert( asset(quant, hdd_symbol), core_symbol());
      });
	  
	 _hbalance.modify( res_itr, account, [&]( auto& res ) {
		   res.last_hdd_balance -= quant;
		   res.last_hdd_time        =  current_time();
	});

	INLINE_ACTION_SENDER(eosio::token, transfer)(
         token_account, { {hdd_account, active_permission}, {account, active_permission} },
         { hdd_account, account, asset(tokens_out), std::string("sell ram") }
    );
}

asset exchange_state::convert_to_exchange( connector& c, asset in ) {

      real_type R(supply.amount);
      real_type C(c.balance.amount+in.amount);
      real_type F(c.weight);
      real_type T(in.amount);
      real_type ONE(1.0);

      real_type E = -R * (ONE - std::pow( ONE + T / C, F) );
      int64_t issued = int64_t(E);

      supply.amount += issued;
      c.balance.amount += in.amount;

      return asset( issued, supply.symbol );
 }

asset exchange_state::convert_from_exchange( connector& c, asset in ) {
      eosio_assert( in.symbol== supply.symbol, "unexpected asset symbol input" );

      real_type R(supply.amount - in.amount);
      real_type C(c.balance.amount);
      real_type F(1.0/c.weight);
      real_type E(in.amount);
      real_type ONE(1.0);


     // potentially more accurate: 
     // The functions std::expm1 and std::log1p are useful for financial calculations, for example, 
     // when calculating small daily interest rates: (1+x)n
     // -1 can be expressed as std::expm1(n * std::log1p(x)). 
     // real_type T = C * std::expm1( F * std::log1p(E/R) );
      
      real_type T = C * (std::pow( ONE + E/R, F) - ONE);
      int64_t out = int64_t(T);

      supply.amount -= in.amount;
      c.balance.amount -= out;

      return asset( out, c.balance.symbol );
}

asset exchange_state::convert( asset from, const symbol& to ) {
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
   
extern "C" {
    void apply(uint64_t receiver, uint64_t code, uint64_t action) {
        if(code==receiver)
        {
            switch(action)
            {
                EOSIO_DISPATCH_HELPER( hdddata, (gethbalance)(gethsum)(sethfee)(newmaccount)(addmprofit)(subhbalance)(buyhdd)(sellhdd)(addhspace)(subhspace) )
            }
        }
        else if(code==HDD_OFFICIAL.value && action=="transfer"_n.value) {
            //execute_action( name(receiver), name(code), &hdddata::deposithdd );
        }
    }
};
