#include "justtoken.hpp"

const account_name MINE_CONTRACT = N(justminepool);

void justtoken::hi(account_name user) { require_auth(user); }

void justtoken::create(account_name issuer, asset maximum_supply) {
  require_auth(_self);
  auto sym = maximum_supply.symbol;
  eosio_assert(sym.is_valid(), "invalid symbol name");
  eosio_assert(maximum_supply.is_valid(), "invalid supply");
  eosio_assert(maximum_supply.amount > 0, "max-supply must be positive");

  stats statstable(_self, sym.name());
  auto existing = statstable.find(sym.name());
  eosio_assert(existing == statstable.end(),
               "token with symbol already exists");

  statstable.emplace(_self, [&](auto &s) {
    s.supply.symbol = maximum_supply.symbol;
    s.max_supply = maximum_supply;
    s.issuer = issuer;
  });
}

void justtoken::issue(account_name to, asset quantity, string memo) {
  auto sym = quantity.symbol;
  eosio_assert(sym.is_valid(), "invalid symbol name");
  eosio_assert(memo.size() <= 256, "memo has more than 256 bytes");

  auto sym_name = sym.name();
  stats statstable(_self, sym_name);
  auto existing = statstable.find(sym_name);
  eosio_assert(existing != statstable.end(),
               "token with symbol does not exist, create token before issue");
  const auto &st = *existing;

  require_auth(st.issuer);
  eosio_assert(quantity.is_valid(), "invalid quantity");
  eosio_assert(quantity.amount > 0, "must issue positive quantity");
  eosio_assert(quantity.symbol == st.supply.symbol,
               "symbol precision mismatch");
  
  uint64_t frozen_part=get_frozen_part(quantity);
  uint64_t issue_minus = frozen_part + st.supply.amount;
  eosio_assert(st.max_supply.amount >= issue_minus, "issue overflow");
  uint64_t can_issue = st.max_supply.amount - issue_minus;

  eosio_assert(quantity.amount <= can_issue,
               "quantity exceeds available supply");

  statstable.modify(st, 0, [&](auto &s) { s.supply += quantity; });

  add_balance(st.issuer, quantity, st.issuer);

  if (to != st.issuer) {
    SEND_INLINE_ACTION(*this, transfer, {st.issuer, N(active)},
                       {st.issuer, to, quantity, memo});
  }
}

uint64_t justtoken::get_frozen_part(asset quantity){
  auto sym = quantity.symbol;
  eosio_assert(sym.is_valid(), "invalid symbol name");
  auto sym_name = sym.name();
  time c_time=now();
  uint64_t one_year_sec = 365 * 24 * 60 * 60;
  uint64_t release_time = 1548993600;
  uint64_t bancor_part = 50000000000;
  uint64_t mine_part = 450000000000;
  uint64_t free_part = 100000000000;
  uint64_t team_part = 400000000000;
  uint64_t frozen_part = team_part+free_part;
  uint64_t has_mine = 0;
  //按时间解锁的
  uint64_t time_release = 0;
  //挖矿解锁的
  uint64_t mine_release=0;
  accounts mine_account_inx(_self, MINE_CONTRACT);
  auto itr_mine = mine_account_inx.find(sym_name);
  if (itr_mine != mine_account_inx.end()) {
    has_mine = mine_part - itr_mine->balance.amount;
    mine_release=(has_mine / 10);
  }
  if (c_time > release_time) {
    double percent =
        (double)(c_time - release_time) / ((double)(one_year_sec*2));
    if(percent>0&&percent<=1){
        time_release = (uint64_t)((double)team_part * percent);
    }
  }
  uint64_t real_release=std::max(mine_release,time_release);
  eosio_assert(frozen_part>=real_release, "frozen overflow");
  frozen_part-=real_release;
  return frozen_part;
}

void justtoken::sfrozen(asset quantity) {
  //用于查看当前解锁了多少了
  uint64_t frozen_part=get_frozen_part(quantity);
  eosio_assert(false, std::to_string(frozen_part).c_str());
}
void justtoken::transfer(account_name from, account_name to, asset quantity,
                         string memo) {
  eosio_assert(from != to, "cannot transfer to self");
  require_auth(from);
  eosio_assert(is_account(to), "to account does not exist");
  if (to == MINE_CONTRACT && from != _self) {
    eosio_assert(false, "only eosjusttoken can transfer JUST to justminepool");
  }
  auto sym = quantity.symbol.name();
  stats statstable(_self, sym);
  const auto &st = statstable.get(sym);

  require_recipient(from);
  require_recipient(to);

  eosio_assert(quantity.is_valid(), "invalid quantity");
  eosio_assert(quantity.amount > 0, "must transfer positive quantity");
  eosio_assert(quantity.symbol == st.supply.symbol,
               "symbol precision mismatch");
  eosio_assert(memo.size() <= 256, "memo has more than 256 bytes");

  sub_balance(from, quantity);
  add_balance(to, quantity, from);
}

void justtoken::sub_balance(account_name owner, asset value) {
  accounts from_acnts(_self, owner);
  const auto &from =
      from_acnts.get(value.symbol.name(), "no balance object found");
  eosio_assert(from.balance.amount >= value.amount, "overdrawn balance");

  asset new_balance = from.balance - value;
  if (from.balance.amount == value.amount) {
    from_acnts.erase(from);
  } else {
    from_acnts.modify(from, owner, [&](auto &a) { a.balance = new_balance; });
  }
  holders holdertable(_self, value.symbol.name());
  auto itr_holder = holdertable.find(owner);
  if (itr_holder != holdertable.end()) {
    holdertable.modify(itr_holder, 0,
                       [&](auto &a) { a.balance = new_balance; });
  }
}

void justtoken::add_balance(account_name owner, asset value,
                            account_name ram_payer) {
  accounts to_acnts(_self, owner);
  auto to = to_acnts.find(value.symbol.name());
  asset new_balance = value;
  if (to == to_acnts.end()) {
    new_balance = value;
    to_acnts.emplace(ram_payer, [&](auto &a) { a.balance = new_balance; });
  } else {
    new_balance = to->balance + value;
    to_acnts.modify(to, 0, [&](auto &a) { a.balance = new_balance; });
  }
  holders holdertable(_self, value.symbol.name());
  auto itr_holder = holdertable.find(owner);
  if (itr_holder == holdertable.end()) {
    holdertable.emplace(ram_payer, [&](auto &a) {
      a.user = owner;
      a.balance = new_balance;
    });
  } else {
    holdertable.modify(itr_holder, 0,
                       [&](auto &a) { a.balance = new_balance; });
  }
}

EOSIO_ABI(justtoken, (hi)(sfrozen)(create)(issue)(transfer));