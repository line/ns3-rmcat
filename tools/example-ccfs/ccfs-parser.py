#!/usr/bin/env python
import sys
from parse import *
from parse import compile



"""
59.9006s CcfsController:printProcessFbmLog(): [StartProcessFBM] NetRemains:18pkt, 17045bytes QPkts=0 QBytes=0 QMs=0 FbmCount=59
"""
def get_csvhdr_start_fbm():
    return "time(s),rem_pkt,rem_bytes,q_pkt,q_bytes,q_ms"

def parse_start_fbm(line):
    if line.find("[StartProcessFBM]") == -1:
        return 

    data = line[0:line.find("s ")]
    ret = search("NetRemains:{:w}pkt, {:w}bytes QPkts={:w} QBytes={:w} QMs={:w} ", line)

    data += "," + ret[0] + "," + ret[1] + "," + ret[2] + "," + ret[3] + "," + ret[4]
    return data

"""
1.50146s CcfsController:parseFeedback(): [ParseFBM] RxedPkt=8 RxedBytes=6691 TxedBytes=6675 SentRxedBytes=5964 AddedBytes=0 Lost=1
"""
def get_csvhdr_parsefbm():
    return "lost"

def parse_fbm(line):
    if line.find("[ParseFBM]") == -1:
        return

    ret = search("[ParseFBM] RxedPkt={:w} RxedBytes={:w} TxedBytes={:w} SentRxedBytes={:w} AddedBytes={:w} Lost={:w}", line)

    if ret:
        return ret[5]

    print("Error. parse_fbm() fail:{}".format(line))
    return

"""
0.200936s CcfsController:estiFwdBw(): [EstFwdBw]Estd=0kbps-->0kbps Txed=0kbps Rxed=0kbps Topo=1000kbps Target=150kbps
"""
def get_csvhdr_estbw():
    return "est-bw(kbps),txed(kbps),rxed(kbps),topo(kbps)"

def parse_estbw(line):
    if line.find("[EstFwdBw]") == -1:
        return

    ret = search("[EstFwdBw]Estd={:w}kbps-->{:w}kbps Txed={:w}kbps Rxed={:w}kbps Topo={:w}kbps Target={:w}kbps", line)

    if ret:
        data = ret[1] + "," + ret[2] + "," + ret[3] + "," + ret[4]
        return data

    return
"""
8.90133s CcfsController:updateInternalVQDelay(): [UpdateIVQ] ivqQptMs=0.419 ivqDelayMs=0.0 netQDelayMs=1 corr=0.927173 properBw:291kbps(191kbps)
"""
def get_csvhdr_ivq():
    return "corr,vbw,obw"

def parse_ivq(line):
    if line.find("[UpdateIVQ]") == -1:
        return

    ret = search("[UpdateIVQ] ivqQptMs={:g} ivqDelayMs={:g} netQDelayMs={:g} corr={:g} properBw:{:w}kbps({:w}kbps)", line)

    if ret:
        data = str(ret[3]) + "," + ret[4] + "," + ret[5]
        return data

    return


"""
39.4015s CcfsController:estiQDelay(): [EstQDelay] 57048:283ms,57049:243ms,57050:201ms,57051:183ms,57052:152ms,57053:134ms,57054:122ms,57055:122ms,
"""
pktqdelay_comp = compile("{:w}:{:w}ms")

def get_cvshdr_pktqdelay():
    return "seq,xod"

def parse_pktqdelay(line):

    key = "[EstQDelay] "
    pos = line.find(key)
    if pos == -1:
        return

    subline = line[pos + len(key):]
    pktqdelays = pktqdelay_comp.findall(subline)

    data = ""
    for each in pktqdelays:
        data += each[0] + "," + each[1] + "\n"

    return data



"""
EstQDelay Next line:
wndMinQDelayMs=1 wndMinQRangeMs=7 latestQDelay=2 QDelaySampleCount=40 intQDelay=1 currXQDelay=1 MA(XQDelay)=149.44
"""
def get_csvhdr_estqdelay():
    return "wnd_qdelay,wnd_mrange,qdelay,vqdelay,xqdelay,ma(XQ)"


def parse_estqdelay(line):
    ret = search("wndMinQDelayMs={:w} wndMinQRangeMs={:w} latestQDelay={:w} QDelaySampleCount={:w} intQDelay={:w} currXQDelay={:g} MA(XQDelay)={:g}", line)

    if ret:
        return ret[0] + "," + ret[1] + "," + ret[2] + "," + ret[4] + "," + str(ret[5]) + "," + str(ret[6])

    print("Error. parse_estqdelay() fail:{}".format(line))

    return 


"""
0.2014s CcfsController:getControlSend(): [CtrlSend] targetQDelay=50 qdFraction=1 brFraction=0 xqFraction=1.5 ctrl.evt=0 increasingMs=0
"""
def get_csvhdr_ctrl():
    return "target,qdFract,brFract,xqFract,incr(ms)"

def parse_ctrl(line):
    if line.find("[CtrlSend]") == -1:
        return

    ret = search("[CtrlSend] targetQDelay={:w} qdFraction={} brFraction={} xqFraction={:g} ctrl.evt={:w} increasingMs={:w}", line)

    if ret:
        data = ret[0] + "," + ret[1] + "," + ret[2] + "," + str(ret[3]) + "," + ret[5]
        return data

    print("Error. parse_ctrl() fail:{}".format(line))

    return


"""
59.3014s CcfsController:handleSendControlEvent(): [HndlTxEvt] status=1->1 evt=0 Target: 122kbps->122kbps
"""
def get_csvhdr_txevt():
    return "status,evt,target(kbps)"

def parse_txevt(line):
    if line.find("[HndlTxEvt]") == -1:
        return

    ret = search("[HndlTxEvt] status={:w}->{:w} evt={:w} Target: {:w}kbps->{:w}kbps", line)

    if ret:
        data = ret[1] + "," + ret[2] + "," + ret[4]
        return data

    return

def write_qdelay_csv(input_file, output_file):
    csv1_name = output_file

    csv1 = open(csv1_name, 'w')
    csv_head = get_csvhdr_start_fbm() + ", "
    csv_head += get_csvhdr_parsefbm() + ", "
    csv_head += get_csvhdr_estbw() + ", "
    csv_head += get_csvhdr_ivq() + ", "
    csv_head += get_csvhdr_estqdelay() + ","
    csv_head += get_csvhdr_ctrl() + ","
    csv_head += get_csvhdr_txevt() + "\n"
    csv1.write(csv_head)
    # print("    >> HEADER est_qd: {}".format(csv_head))

    # csv2_name = "pktqd_" + filename + ".csv"
    # csv2 = open(csv2_name, 'w')
    # csv_head = get_cvshdr_pktqdelay()
    # csv_head += "\n"
    # csv2.write(csv_head)
    # print("    >> HEADER pktqd {}".format(csv_head))


    fbmcount = 0

    with open(input_file, 'r') as log:
        while True:
            line = log.readline()
            if not line: break

            
            data = parse_start_fbm(line)
            if data:

                while True:
                    line = log.readline()
                    if not line: break

                    parsefbm = parse_fbm(line)
                    if parsefbm:
                        data += ","
                        data += parsefbm

                    estbw = parse_estbw(line)
                    if estbw:
                        data += ","
                        data += estbw

                    ivq = parse_ivq(line)
                    if ivq:
                        data +=","
                        data += ivq

                    csv2data = parse_pktqdelay(line)
                    if csv2data:
                        # csv2.write(csv2data)
                        line = log.readline()
                        estqdelay =  parse_estqdelay(line)

                        if estqdelay:
                            data += ","
                            data += estqdelay

                    ctrl = parse_ctrl(line)
                    if ctrl:
                        data += ","
                        data += ctrl

                    txevt = parse_txevt(line)
                    if txevt:
                        data += ","
                        data += txevt

                        fbmcount += 1
                        data += "\n"
                        csv1.write(data)
                        break



    
    csv1.close()
    #csv2.close()
    print('    End write_qdelay_csv()')
    print('    >> Intput={} FBMCount={}'.format(input_file, fbmcount))
    print('    >> Outputs={}'.format(csv1_name))
    #print('    >> Outputs={}, {}'.format(csv1_name, csv2_name))
    return


if __name__=='__main__':
    print("    Start...")
    write_qdelay_csv(sys.argv[1], sys.argv[2])



