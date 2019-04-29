#!/usr/bin/env python
import sys
import os

print ("Start CCFS Example with various parameter")
os.system("source ../../nslog/nslog_ccfs_example")
traffic = ["pure", "tcp", "udp"]
netbw = ["300", "500", "1000", "2000"]

for tr in traffic:
    ctr = ""
    if tr == "tcp":
        ctr = " --tcp=1"
    elif tr == "udp":
        ctr = " --udp=1"

    for bw in netbw:
        name = tr + "_" + bw
        img_title = name + "kbps"
        out_name = "ccfs-results/" + name + ".out"
        csv_name = "ccfs-results/" + name + ".csv"
        svg_name = "ccfs-results/" + name

        print("- Run [" + tr + "/" + bw + "kbps] ")
        os.chdir("../../../../")
        os.system("./waf --run 'rmcat-example --kbps=" + bw + ctr + "'> src/ns3-rmcat/tools/example-ccfs/" + out_name + " 2>&1")
        os.chdir("src/ns3-rmcat/tools/example-ccfs/")
        os.system("python ccfs-parser.py " + out_name + " " + csv_name)
        os.system("python ccfs-plot.py " + img_title + " " + svg_name + " " + csv_name + " " + bw)
        print("- End")


print ("Finish CCFS")

