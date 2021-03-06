### Files Included in Zyrk Core Directory

* banlist.dat: stores the IPs/Subnets of banned nodes
* zyrk.conf: contains configuration settings for zyrkd or zyrk-qt
* zyrkd.pid: stores the process id of zyrkd while running
* blocks/blk000??.dat: block data (custom, 128 MiB per file); since 1.0.0
* blocks/rev000??.dat; block undo data (custom); since 1.0.0
* blocks/index/*; block index (LevelDB); since 1.0.0
* chainstate/*; block chain state database (LevelDB); since 1.0.0
* database/*: BDB database environment; only used for wallet since 1.0.0; moved to wallets/ directory on new installs since 1.0.0
* db.log: wallet database log file; moved to wallets/ directory on new installs since 1.0.0
* debug.log: contains debug information and general logging generated by zyrkd or zyrk-qt
* fee_estimates.dat: stores statistics used to estimate minimum transaction fees and priorities required for confirmation; since 1.0.0
* mempool.dat: dump of the mempool's transactions; since 1.0.0.
* peers.dat: peer IP address database (custom format); since 1.0.0
* wallet.dat: personal wallet (BDB) with keys and transactions; moved to wallets/ directory on new installs since 1.0.0
* wallets/database/*: BDB database environment; used for wallets since 1.0.0
* wallets/db.log: wallet database log file; since 1.0.0
* wallets/wallet.dat: personal wallet (BDB) with keys and transactions; since 1.0.0
* .cookie: session RPC authentication cookie (written at start when cookie authentication is used, deleted on shutdown): since 1.0.0
* onion_private_key: cached Tor hidden service private key for `-listenonion`: since 1.0.0
* guisettings.ini.bak: backup of former GUI settings after `-resetguisettings` is used

