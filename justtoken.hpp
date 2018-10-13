#include <ctime>
#include <eosiolib/asset.hpp>
#include <eosiolib/currency.hpp>
#include <eosiolib/eosio.hpp>
#include <eosiolib/multi_index.hpp>
#include <string>
#include <vector>

using namespace eosio;
using namespace std;

class justtoken : public eosio::contract {
public:
  justtoken(account_name self) : contract(self) {}

  // @abi action
  void hi(account_name user);

  // @abi action
  void sfrozen(asset quantity);

  // @abi action
  void create(account_name issuer, asset maximum_supply);
  // @abi action
  void issue(account_name to, asset quantity, string memo);
  // @abi action
  void transfer(account_name from, account_name to, asset quantity,
                string memo);

  inline asset get_supply(symbol_name sym) const;

  inline asset get_balance(account_name owner, symbol_name sym) const;

private:
  //@abi table accounts i64
  struct account {
    asset balance;
    uint64_t primary_key() const { return balance.symbol.name(); }
  };
  typedef eosio::multi_index<N(accounts), account> accounts;

  //@abi table holder i64
  struct holder {
    account_name user;
    asset balance;
    account_name primary_key() const { return user; }
    uint64_t byamount() const { return balance.amount; }
    EOSLIB_SERIALIZE(holder, (user)(balance))
  };
  typedef eosio::multi_index<
      N(holder), holder,
      indexed_by<N(byamount),
                 const_mem_fun<holder, uint64_t, &holder::byamount>>>
      holders;

  //@abi table stat i64
  struct cstats {
    asset supply;
    asset max_supply;
    account_name issuer;
    uint64_t primary_key() const { return supply.symbol.name(); }
  };
  typedef eosio::multi_index<N(stat), cstats> stats;

  uint64_t get_frozen_part(asset quantity);
  void sub_balance(account_name owner, asset value);
  void add_balance(account_name owner, asset value, account_name ram_payer);

public:
  struct transfer_args {
    account_name from;
    account_name to;
    asset quantity;
    string memo;
  };
};

asset justtoken::get_supply(symbol_name sym) const {
  stats statstable(_self, sym);
  const auto &st = statstable.get(sym);
  return st.supply;
}

asset justtoken::get_balance(account_name owner, symbol_name sym) const {
  accounts accountstable(_self, owner);
  const auto &ac = accountstable.get(sym);
  return ac.balance;
}