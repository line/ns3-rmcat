rmcat-example for CCFS
======================

rmcat-example
-------------
`./waf --run "rmcat-example" > ccfs_out.log 2>&1`

+ Default
  + Number of RMCAT flows: 1
  + RMCAT algorithm: CCFS
  + Number of TCP: 0
  + Number of UDP: 0
  + Throughput: 1000 Kbps
  + RTT: 300ms
  + QDelay : 300ms


#### Related options

+ `--rmcat=N` : Number of RMCAT flows (NADA/CCFS)
+ `--tcp=N`: TCP cross traffic
+ `--udp=N`: UDP cross traffic
+ `--algo=nada/ccfs`: Type of RMCAT flow (NADA, CCFS)
+ `--kbps=xxxx` : config data rate option (Throughput)






There are various tools for running/drawing charts with a ccfs output log file.

See documentation: [RUN-CCFS-VPARAM.md](tools/example-ccfs/RUN-CCFS-VPARA.md)

