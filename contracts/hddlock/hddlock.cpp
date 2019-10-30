#include "hddlock.hpp"
#include <eosiolib/action.hpp>
#include <eosiolib/chain.h>
#include <eosiolib/symbol.hpp>
#include <eosiolib/eosio.hpp>
#include <eosiolib/print.hpp>
#include <eosiolib/serialize.hpp>
#include <eosiolib/multi_index.hpp>
//#include <eosio.token/eosio.token.hpp>
//#include <eosio.system/eosio.system.hpp>

using namespace eosio;

static constexpr eosio::name active_permission{N(active)};
static constexpr eosio::name token_account{N(eosio.token)};


void hddlock::init() {

}

void hddlock::addaccbig(account_name user, std::string& desc) {
    require_auth(_self);

    accbig_table _accbig( _self , _self );
    auto itmaccbig = _accbig.find(user);
    eosio_assert(itmaccbig == _accbig.end(), "user already registered as big account");  

    _accbig.emplace(_self, [&](auto &row) {
        row.user = user;
        row.desc = desc;
    });       
}

void hddlock::rmvaccbig(account_name user) {
    require_auth(_self);

    accbig_table _accbig( _self , _self );
    auto itmaccbig = _accbig.find(user);
    if(itmaccbig != _accbig.end()) {
        _accbig.erase(itmaccbig);
    }
}


void hddlock::addrule(uint64_t lockruleid, std::vector<uint64_t>& times, std::vector<uint8_t>& percentage, std::string& desc) 
{
    require_auth(_self);

    eosio_assert(times.size() >= 2, "invalidate size of times array");
    eosio_assert(times.size() == percentage.size(), "times and percentage in different size.");

    lockrule_table _lockrule(_self, _self);
    auto itrule = _lockrule.find(lockruleid);
    eosio_assert(itrule == _lockrule.end(), "the id already existed in rule table");  

    _lockrule.emplace(_self, [&](auto &row) {
        row.lockruleid   = lockruleid;
        row.times        = times;
        row.percentage   = percentage;
        row.desc         = desc;
    });          
}

void hddlock::rmvrule(uint64_t lockruleid)
{
    require_auth(_self);

    lockrule_table _lockrule(_self, _self);
    auto itrule = _lockrule.find(lockruleid);
    eosio_assert(itrule != _lockrule.end(), "the lockruleid not exist");  
    _lockrule.erase(itrule);      

}


void hddlock::locktransfer(uint64_t lockruleid, account_name from, account_name to, asset quantity, asset amount, std::string memo) 
{
    require_auth(from);
    eosio_assert( quantity.symbol == CORE_SYMBOL , "only core symbole support this lock transsfer");  
    eosio_assert( amount.symbol == CORE_SYMBOL , "only core symbole support this lock transsfer");  

    accbig_table _accbig( _self , _self );
    auto itmaccbig = _accbig.find(from);
    eosio_assert(itmaccbig != _accbig.end(), "from can not locktransfer");  

    lockrule_table _lockrule(_self, _self);
    auto itrule = _lockrule.find(lockruleid);
    eosio_assert(itrule != _lockrule.end(), "lockruleid not existed in rule table");  

    eosio_assert(quantity.amount >= amount.amount, "overdrawn quantity");  

    action(
       permission_level{from, active_permission},
       token_account, N(transfer),
       std::make_tuple(from, to, amount, memo))
       .send();

    acclock_table _acclock(_self, to);
    _acclock.emplace(_self, [&](auto &row) {
        row.quantity    = quantity;
        row.lockruleid  = lockruleid;
        row.user        = to;
        row.from        = from;
        row.memo        = memo;
        row.time        = current_time();
    });
}


void hddlock::clearall(account_name user) {
    require_auth(_self);

    acclock_table _acclock(_self, user);
    while (_acclock.begin() != _acclock.end()) {
      _acclock.erase(_acclock.begin());      
    }  
}


EOSIO_ABI( hddlock, (init)(addrule)(rmvrule)(locktransfer)(clearall)(addaccbig)(rmvaccbig))
