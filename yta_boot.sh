YTA_BIN_DIR="/Users/wangzhi/yottachain/YTAChain/build/bin"
YTA_CONTRACT_DIR="/Users/wangzhi/YTBP/build/contracts"
YTA_SN_URL="http://152.136.11.212:8888/"

#${YTA_BIN_DIR}/cleos -u ${YTA_SN_URL}  get info
#${YTA_BIN_DIR}/cleos -u ${YTA_SN_URL} set contract hdddeposit12 ${YTA_CONTRACT_DIR}/hdddeposit

#解锁钱包
printf "\\n\\tUlock your wallet, please input your wallet password...\\n\\n"
${YTA_BIN_DIR}/cleos wallet unlock
PW5JdTGYVAc3fvNYEMArqGchRK8vkPotRazpVwtwLhSCMrFfWvQkE

#创建账户
printf "\\n\\tCreate hdd related account...\\n\\n"
${YTA_BIN_DIR}/cleos -u ${YTA_SN_URL} create account eosio hddpool12345 YTA7EVUeMmQdQqdkDmir3iw7bNe49R9SrcfXw6secCfY2ve1Tgfzh YTA5dGkW3paV4se44bBH7ise2vLt6BWPBs5dxDU9RFYauxwPoQ6qz
${YTA_BIN_DIR}/cleos -u ${YTA_SN_URL} create account eosio hdddeposit12 YTA7sBCYKDum3morjrh8ueWE6asMheWFcTMEuF4JpDjyWYqAiGMRJ YTA5xjUZ3XvzDj5C2FZJPh5H5MZpW67Zrje86FvEL1AXDAhRBFEed
${YTA_BIN_DIR}/cleos -u ${YTA_SN_URL} create account eosio hddlock12345 YTA8aWmhGELYK8diCVY9ioRRq2i7ZAnJC1uH5TrFSLSbtRjgQcEFA YTA7WCpKgdfPimyJCdXkH95njbYcBgMKhJ6sxoQwvS1pubcNoPFoV
${YTA_BIN_DIR}/cleos -u ${YTA_SN_URL} create account eosio username1234 YTA68Lb6h941HvVnaJCkV9958BRqBiiauAxQbTCy6iS5H8mtxE29f YTA4uvDGmiLVpodsnXG4D2J8o66gP3HZxT9TC4wDmtdoBHNmZsjUg
${YTA_BIN_DIR}/cleos -u ${YTA_SN_URL} create account eosio storename123 YTA69XDMXsk8Rvva5bVZgPkJ7NryxLi9dp8KPAQa2K6RT2jj2qyZz YTA6aeqepyGGEcwVkYg444LbxrQuBFefowXCCiCxCZvLHgM3LR7d2
${YTA_BIN_DIR}/cleos -u ${YTA_SN_URL} create account eosio hddbasefound YTA6s6CfKM1pLucJLGgMwmD6Nkj2szHviKkWuAHEAdL9N1QfuRMN1 YTA6dMmm5c2e4uSWB5FoYYdosmoUmut3ooDYh5w3crFqGtncFaiJi
#创建9大系统账户
printf "\\n\\tCreate YTA system account...\\n\\n"
${YTA_BIN_DIR}/cleos -u ${YTA_SN_URL} create account eosio eosio.bpay YTA6btVGr2y4GEEFSjg2STUXnH1h2KEBVL2gq7wrSb4vnJLeFxQDW
${YTA_BIN_DIR}/cleos -u ${YTA_SN_URL} create account eosio eosio.msig YTA6btVGr2y4GEEFSjg2STUXnH1h2KEBVL2gq7wrSb4vnJLeFxQDW
${YTA_BIN_DIR}/cleos -u ${YTA_SN_URL} create account eosio eosio.names YTA6btVGr2y4GEEFSjg2STUXnH1h2KEBVL2gq7wrSb4vnJLeFxQDW
${YTA_BIN_DIR}/cleos -u ${YTA_SN_URL} create account eosio eosio.ram YTA6btVGr2y4GEEFSjg2STUXnH1h2KEBVL2gq7wrSb4vnJLeFxQDW
${YTA_BIN_DIR}/cleos -u ${YTA_SN_URL} create account eosio eosio.ramfee YTA6btVGr2y4GEEFSjg2STUXnH1h2KEBVL2gq7wrSb4vnJLeFxQDW
${YTA_BIN_DIR}/cleos -u ${YTA_SN_URL} create account eosio eosio.saving YTA6btVGr2y4GEEFSjg2STUXnH1h2KEBVL2gq7wrSb4vnJLeFxQDW
${YTA_BIN_DIR}/cleos -u ${YTA_SN_URL} create account eosio eosio.stake YTA6btVGr2y4GEEFSjg2STUXnH1h2KEBVL2gq7wrSb4vnJLeFxQDW
${YTA_BIN_DIR}/cleos -u ${YTA_SN_URL} create account eosio eosio.token YTA6btVGr2y4GEEFSjg2STUXnH1h2KEBVL2gq7wrSb4vnJLeFxQDW
${YTA_BIN_DIR}/cleos -u ${YTA_SN_URL} create account eosio eosio.upay YTA6btVGr2y4GEEFSjg2STUXnH1h2KEBVL2gq7wrSb4vnJLeFxQDW
${YTA_BIN_DIR}/cleos -u ${YTA_SN_URL} create account eosio eosio.vpay YTA6btVGr2y4GEEFSjg2STUXnH1h2KEBVL2gq7wrSb4vnJLeFxQDW

#创建账户hdd.pool
printf "\\n\\tCreate account -- hdd.pool ...\\n\\n"
${YTA_BIN_DIR}/cleos -u ${YTA_SN_URL} create account eosio hdd.pool YTA6btVGr2y4GEEFSjg2STUXnH1h2KEBVL2gq7wrSb4vnJLeFxQDW

#创建账户hdd.deposit   存储押金合约部署账户
printf "\\n\\tCreate account -- hdd.deposit ...\\n\\n"
${YTA_BIN_DIR}/cleos -u ${YTA_SN_URL} create account eosio hdd.deposit YTA6btVGr2y4GEEFSjg2STUXnH1h2KEBVL2gq7wrSb4vnJLeFxQDW

#创建账户hdd.lock  锁仓合约部署账户
printf "\\n\\tCreate account -- hdd.lock ...\\n\\n"
${YTA_BIN_DIR}/cleos -u ${YTA_SN_URL} create account eosio hdd.lock YTA6btVGr2y4GEEFSjg2STUXnH1h2KEBVL2gq7wrSb4vnJLeFxQDW

#创建账户hdd.base  基础奖励基金
${YTA_BIN_DIR}/cleos -u ${YTA_SN_URL} create account eosio hdd.base YTA6btVGr2y4GEEFSjg2STUXnH1h2KEBVL2gq7wrSb4vnJLeFxQDW

#加载系统合约eosio.token
printf "\\n\\tLoad eosio.token ...\\n\\n"
${YTA_BIN_DIR}/cleos -u ${YTA_SN_URL} set contract eosio.token ${YTA_CONTRACT_DIR}/eosio.token

#创建50亿个YTT代币（这里的YTT币相当于主网中的EOS）
printf "\\n\\tCreate YTT token...\\n\\n"
${YTA_BIN_DIR}/cleos -u ${YTA_SN_URL} push action eosio.token create '["eosio","5000000000.0000 YTT",0,0,0]' -p eosio.token

#发行代币，数量：40000000000，代币符号：YTT
printf "\\n\\tIssue YTT to eosio...\\n\\n"
${YTA_BIN_DIR}/cleos -u ${YTA_SN_URL} push action eosio.token issue '["eosio","4000000000.0000 YTT","issue"]' -p eosio

#转账给某用户hddpool12345
printf "\\n\\tTransfer some YTT to hddpool12345...\\n\\n"
${YTA_BIN_DIR}/cleos -u ${YTA_SN_URL} push action eosio.token transfer '["eosio", "hddpool12345","10000.0000 YTT","memo"]' -p eosio

#转账给某用户username1234
printf "\\n\\tTransfer some YTT to username1234...\\n\\n"
${YTA_BIN_DIR}/cleos -u ${YTA_SN_URL} push action eosio.token transfer '["eosio", "username1234","10000.0000 YTT","memo"]' -p eosio

#加载系统合约eosio.system
printf "\\n\\tLoad eosio.system ...\\n\\n"
${YTA_BIN_DIR}/cleos -u ${YTA_SN_URL} set contract eosio ${YTA_CONTRACT_DIR}/eosio.system

#将hdd.pool用户设为特权用户 （否则调用inline actio会有复杂的授权问题）
printf "\\n\\tSet hdd.pool to privilege account...\\n\\n"
${YTA_BIN_DIR}/cleos -u ${YTA_SN_URL}  push action eosio setpriv '["hdd.pool", 1]' -p eosio

#将hdd.deposit 用户设为特权用户 （否则调用inline actio会有复杂的授权问题）
printf "\\n\\tSet hdd.deposit to privilege account...\\n\\n"
${YTA_BIN_DIR}/cleos -u ${YTA_SN_URL}  push action eosio setpriv '["hdd.deposit", 1]' -p eosio

#将hdd.lock 用户设为特权用户 （否则调用inline actio会有复杂的授权问题）
printf "\\n\\tSet hdd.lock to privilege account...\\n\\n"
${YTA_BIN_DIR}/cleos -u ${YTA_SN_URL}  push action eosio setpriv '["hdd.lock", 1]' -p eosio

#将hddpool12345用户设为特权用户 （否则调用inline actio会有复杂的授权问题）
printf "\\n\\tSet hddpool12345 to privilege account...\\n\\n"
${YTA_BIN_DIR}/cleos -u ${YTA_SN_URL}  push action eosio setpriv '["hddpool12345", 1]' -p eosio

#将hdddeposit12用户设为特权用户 （否则调用inline actio会有复杂的授权问题）
printf "\\n\\tSet hdddeposit12 to privilege account...\\n\\n"
${YTA_BIN_DIR}/cleos -u ${YTA_SN_URL}  push action eosio setpriv '["hdddeposit12", 1]' -p eosio

#将hddlock12345用户设为特权用户 （否则调用inline actio会有复杂的授权问题）
printf "\\n\\tSet hddlock12345 to privilege account...\\n\\n"
${YTA_BIN_DIR}/cleos -u ${YTA_SN_URL}  push action eosio setpriv '["hddlock12345", 1]' -p eosio

#部署hddpool合约
printf "\\n\\tLoad hddpool contract...\\n\\n"
${YTA_BIN_DIR}/cleos -u ${YTA_SN_URL} set contract hddpool12345 ${YTA_CONTRACT_DIR}/hddpool

#部署hdddeposit 押金合约
printf "\\n\\tLoad hdddeposit contract...\\n\\n"
${YTA_BIN_DIR}/cleos -u ${YTA_SN_URL} set contract hdddeposit12 ${YTA_CONTRACT_DIR}/hdddeposit

