#include "hdddata.hpp"

const uint32_t seconds_in_one_day = 60 * 60 * 24;
const uint32_t seconds_in_one_week = seconds_in_one_day * 7;
const uint32_t seconds_in_one_year = seconds_in_one_day * 365;
const name hdd_account = "hddofficial"_n;

//@abi action
void hdddata::get_hdd_balance(name owner) {
	//			每个YTA用户的HDD余额由HDD账户中的 上次余额、上次余额时间、每周期费用、每周期收益 四个参数计算得到，计算公式为：
	//		当前余额=上次余额+(当前时间-上次余额时间)*（每周期收益-每周期费用）
	require_auth(owner);
	hddbalance_table  t_hddbalance(_self, _self.value);
	auto hddbalance_itr = t_hddbalance.get(owner.value);
	if(hddbalance_itr == t_hddbalance.end()) {
		t_hddbalance.emplace(owner, [&](auto &row) {
			//todo check the 1st time insert
			row.last_hdd_balance=0;
			row.hdd_per_cycle_fee=0;
			row.hdd_per_cycle_profit=0;
			row.hdd_space=0;
			row.ast_hdd_time = now();
		});
	} else {
		t_hddbalance.modify(owner, [&](auto &row) {
			//todo check overflow
			row.last_hdd_balance=
				hddbalance_itr.get_last_hdd_balance()+ (now()-*hddbalance_itr.last_hdd_time)*(hddbalance_itr.get_hdd_per_cycle_profit()-hddbalance_itr.get_hdd_per_cycle_fee());
			row.last_hdd_time = now();
		});
	}

}

//@abi action
void set_hdd_per_cycle_fee(name owner, uint64_t fee) {
	require_auth(owner);
	hddbalance_table  t_hddbalance(_self, _self.value);
}

//@abi action
void sub_hdd_balance(uint64_t balance){
	hddbalance_table  t_hddbalance(_self, _self.value);
}

//@abi action
void buyhdd(name owner, asset value){
	require_auth(owner);
}

//@abi action
void sellhdd(name owner, uint64_t value){
	require_auth(owner);
}

//@abi action
void add_hdd_space(name owner, name hddaccount, uint64_t space){
	require_auth(owner);
	require_auth(hddaccount);
	hddbalance_table  t_hddbalance(_self, _self.value);
	auto hddbalance_itr = t_hddbalance.find(hddaccount.value);
	if(hddbalance_itr != t_hddbalance.end()) {
		t_hddbalance.modify(hddaccount, [&](auto &row) {
			//todo check overflow
			row.hdd_space +=space;
		});
	}
}

//@abi action
void sub_hdd_space(name owner, name hddaccount, uint64_t space){
	require_auth(owner);
	require_auth(hddaccount);
	hddbalance_table  t_hddbalance(_self, _self.value);
	auto hddbalance_itr = t_hddbalance.find(hddaccount.value);
	if(hddbalance_itr != t_hddbalance.end()) {
		t_hddbalance.modify(hddaccount, [&](auto &row) {
			//todo check overflow
			row.hdd_space -=space;
		});
	}
}

//@abi action
void create_mining_account(name mining_name, name owner) {
	require_auth(mining_name);
	mining_account t_miningaccount(_self, _self.value);
	auto mining_account_itr = t_miningaccount.get(mining_name.value, "Mining Id does not exist");
	if(mining_account_itr == t_miningaccount.end()) {
		t_miningaccount.emplace(mining_account, [&](auto &row) {
			row.mining_name = mining_name;
			row.owner = owner;
		 });
	} else {
		return ;
	}
	
	require_auth(owner);
	hddbalance_table  t_hddbalance(_self, _self.value);
	auto hddbalance_itr = t_hddbalance.get(owner.value);
	if(hddbalance_itr == t_hddbalance.end()) {
	t_hddbalance.emplace(owner, [&](auto &row) {
		//todo check the 1st time insert
		row.owner = owner;
		row.hdd_spacel=0;
	});
	} 
}

//@abi action
void add_mining_profit(name mining_name, uint64_t space){
	require_auth(mining_name);
	mining_account t_miningaccount(_self, _self.value);
	auto mining_account_itr = t_miningaccount.get(mining_name.value, "Mining Id does not exist");
	auto owner_id = mining_account_itr.get_owner();
	if( owner_id != mining_account.end()) {
	hddbalance_table  t_hddbalance(_self, _self.value);
	auto hddbalance_itr = t_hddbalance.get(owner_id);
	if(hddbalance_itr == t_hddbalance.end()) {
		t_hddbalance.emplace(owner, [&](auto &row) {
			//todo check the 1st time insert
			row.owner = owner;
			row.hdd_spacel=space;
		});
	} else {
		
	}
	//每周期收益 += (预采购空间*数据分片大小/1GB）*（记账周期/ 1年）
		
}
	
extern "C" {
    void apply(uint64_t receiver, uint64_t code, uint64_t action) {
        if(code==receiver)
        {
            switch(action)
            {
                EOSIO_DISPATCH_HELPER( hdddata, (get_hdd_balance)(set_hdd_per_cycle_fee)(create_mining_account)(add_mining_profit)(sub_hdd_balance)(buyhdd)(sellhdd)(add_hdd_space)(sub_hdd_space) )
            }
        }
        else if(code==hdd_account.value && action=="transfer"_n.value) {
            execute_action( name(receiver), name(code), &hdddata::deposithdd );
        }
    }
};