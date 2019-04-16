#include "hdddata.hpp"
#include <eosiolib/eosio.hpp>
#include <eosiolib/print.hpp>
#include <eosiolib/serialize.hpp>
#include <eosiolib/multi_index.hpp>
#include <eosio.token/eosio.token.hpp>

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
	//todo initialize produers ?
     
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
		});
	}
	
	auto itr = _hmarket.find(hddcore_symbol.raw());
    if( itr == _hmarket.end() ) {
        //auto system_token_supply   = eosio::token(token_account).get_supply(eosio::symbol_type(system_token_symbol).name()).amount;
       //auto system_token_supply   = eosio::token::get_supply(token_account, yta_symbol.code() );
		auto system_token_supply = 0;
	   if( system_token_supply > 0 ) {
            itr = _hmarket.emplace( _self, [&]( auto& m ) {
               m.supply.amount = 100000000000000ll;
               m.supply.symbol = hddcore_symbol;
               //m.base.balance.amount = int64_t(_gstate.free_ram());
               m.base.balance.symbol = hdd_symbol;
               //m.quote.balance.amount = system_token_supply.amount / 1000;
               m.quote.balance.symbol = yta_symbol;
            });
         }
      } else {
         print( "hdd market already created" );
      }
	
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
			// todo ((uint64_t)( now()-hbalance_itr->last_hdd_time ))
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
	//hbalance_table  hbalance(_self, _self.value);
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
	//hbalance_table  hbalance(_self, _self.value);
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

	auto fee = quant;
	fee.amount = ( fee.amount + 199 ) / 200; /// .5% fee (round up)
	// fee.amount cannot be 0 since that is only possible if quant.amount is 0 which is not allowed by the assert above.
	// If quant.amount == 1, then fee.amount == 1,
	// otherwise if quant.amount > 1, then 0 < fee.amount < quant.amount.
	auto quant_after_fee = quant;
	quant_after_fee.amount -= fee.amount;
	INLINE_ACTION_SENDER(eosio::token, transfer)( token_account, {buyer,active_permission},
		{ buyer, hdd_account, quant_after_fee, std::string("buy hdd") } );

	if( fee.amount > 0 ) {
		INLINE_ACTION_SENDER(eosio::token, transfer)( token_account, {buyer,active_permission},
											   { buyer, hddfee_account, fee, std::string("hdd fee") } );
	}

	int64_t bytes_out;

	const auto& market = _hmarket.get(hddcore_symbol.raw(), "hdd market does not exist");
	_hmarket.modify( market, same_payer, [&]( auto& es ) {
		//bytes_out = es.convert( quant_after_fee,  S(0,HDD) ).amount;
	});

	eosio_assert( bytes_out > 0, "must reserve a positive amount" );
	//todo modify the hbalance table
}

//@abi action
void hdddata::sellhdd(name account, uint64_t quant){
	require_auth(account);
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
