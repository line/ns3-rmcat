/******************************************************************************
 * Copyright 2016-2017 Cisco Systems, Inc.                                    *
 *                                                                            *
 * Licensed under the Apache License, Version 2.0 (the "License");            *
 * you may not use this file except in compliance with the License.           *
 *                                                                            *
 * You may obtain a copy of the License at                                    *
 *                                                                            *
 *     http://www.apache.org/licenses/LICENSE-2.0                             *
 *                                                                            *
 * Unless required by applicable law or agreed to in writing, software        *
 * distributed under the License is distributed on an "AS IS" BASIS,          *
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.   *
 * See the License for the specific language governing permissions and        *
 * limitations under the License.                                             *
 ******************************************************************************/

/**
 * @file
 * CCFS controller implementation for rmcat ns3 module.
 *
 * Draft:
 * https://datatracker.ietf.org/doc/draft-gwock-rmcat-ccfs/
 *
 * TODO:
 * - Use correlation (with window)
 *
 * @author Jungnam gwock
 */

#include "ns3/rtp-header.h"
#include "ns3/rfb-header.h"
#include "ns3/rmcat-utils.h"
#include "ccfs-controller.h"
#include "ns3/log.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <cassert>
#include <cmath>
#include <list>



/*******************************************************************
 * Constants
 */

const uint32_t  kCcfsQDelayWindowTimeMs = 4000;
const uint64_t  kCcfsBrFractionWindowTimeMs = 500;

const int32_t   kCcfsTargetQDelayMs   = 50;
const int32_t   kCcfsTargetQDelayMsMin   = 0;
const int32_t   kCcfsTargetQDelayMsMax   = 250;

const int32_t   kCcfsCrossTrafficTargetQDelayMs = 50;
const int32_t   kCcfsCompeteTargetQDelayMs   = kCcfsCrossTrafficTargetQDelayMs + kCcfsTargetQDelayMs ;

const uint64_t  kCcfsCompeteMaintainMs   = 4000;


const uint64_t  kCcfsInitWaitToProbeMs = kCcfsQDelayWindowTimeMs;

/*
 * TODO: adaptive target qdelay
const float     kCcfsTargetQDelayDescRate    = 0.95;
const float     kCcfsTargetQDelayIncrRate    = 1.05;
const int32_t   kCcfsTargetQDelayForThrottleMs = 50;
*/

const int32_t    KCcfsTrigCompQIncrTimeMs = 2000;

const float      kCcfsTrigThroQDFractMin = 1.5;
const float      kCcfsTrigThroBrFractMax = 0.9;

const int32_t    kCcfsTrigCompQMRangeMin         = 150;
const int32_t    kCcfsTrigCompQDelayMin         = kCcfsTrigCompQMRangeMin;


const int32_t    kCcfsTrigStopCompQDelayMax = 50;
const int32_t    kCcfsTrigStopCompQMRangeMax = 20;

const float      kCcfsTrigProbQDelayMax  = 20;
const float      kCcfsTrigProbBrFractMin  = 1.0f;
const int32_t    kCcfsTrigProbQMRangeMax  = 15;
const float      kCcfsTrigProbLossRtMax    = 0.001f;
const float      kCcfsTrigProbECNRtMax     = 0.001f;

const float      kCcfsTrigStopProbQDFractMin = 1.3f;
const float      kCcfsTrigStopProbBrFractMax = 0.8f;
const float      kCcfsTrigStopProbLossRtMax    = 0.03f;
const float      kCcfsTrigStopProbECNRtMax     = 0.001f;

const float      kCcfsCompTargetBrUpRate = 1.3f;
const float      kCcfsThroTargetBrRate= 0.5f;

const uint64_t  kCcfsSentRtpKeepTimeMs       = 10000;    // 10 sec

const float     kCcfsFwdBwdEstMAFactor       = 0.9f;

const float     kCcfsFwdBwdRoom              = 0.1f;


const uint64_t kCcfsQIncrDetectorMinDurationMs = 500;

const uint64_t kCcfsProbingTimeoutMs = 4000;


const float kCcfsDfltBrCtrlQDFractLow = 0.5;
const float kCcfsDfltBrCtrlQDFractHi = 1.05;
const float kCcfsDfltBrCtrlIncrRate = 1.005;
const float kCcfsDfltBrCtrlDecrRate = 0.98;

/* TODO: xqFraction
const float kCcfsCompBrCtrlQDFractHi = 1.0;
const float kCcfsCompBrCtrlXQFractLow = 0.8;
 */

#if 0
const float kCcfsCompBrCtrlQDelayBoundLow = 50;
#endif
const float kCcfsCompBrCtrlDecrRate = 0.95;
const float kCcfsCompBrCtrlIncrRate = 1.02;

const int32_t kCcfsVqBaseDelayUs = 20000; // 20msec

NS_LOG_COMPONENT_DEFINE ("CcfsController");

namespace rmcat {


CcfsController::CcfsController() :
    m_nqMonitor(NULL),
    m_netDataRate(0),
    m_minQDelay(100000),
    m_targetQDelay(kCcfsTargetQDelayMs),
    m_targetSendBps{m_initBw/8}
{
    NS_LOG_INFO("INIT: TargetTxBps=" << ns3::utilConvertKbps(m_targetSendBps)
                                  << "(" << ns3::utilConvertKbps(m_minBw/8) << "~" << ns3::utilConvertKbps(m_maxBw/8) << ")"
                << " TargetQDelay=" << m_targetQDelay);
}


CcfsController::~CcfsController() {}



void CcfsController::setCurrentBw(float newBw)
{
    NS_LOG_INFO("Do not use this");
}

float CcfsController::getBandwidth(uint64_t now) const
{
    return 8.0 * m_targetSendBps ;
}


void CcfsController::reset()
{
    m_rxedFbmCount = 0;

    /**
     * TODO: Manage max FBM seq for multiple feedback senders(media receiver)
     *
    m_maxFbmSeq = -1;
    m_wrapCount = 0;
     */

    m_targetSendBps = m_initBw/8;
    m_inflightPktCount = 0;
    m_minQDelay = 10000;
    m_lastQDelay = 0;
    m_minQPktSize = 0;

    m_qdelayWindow.clear();

    m_targetQDelay = kCcfsTargetQDelayMs;
    m_maXQDelay = 0.0f;

    m_ivqMinQptUs = 10000000;
    m_ivqQptUs = 0;
    m_ivqStUs = 0;
    m_ivqDelayUs = 0;

    m_incrCount = 0;
    m_incrLastMinQDelay = 0;
    m_incrStartTime = 0;



    m_status = SENDER_STATUS_DEFAULT;

    m_estFwdBwBps = 0.0;
    m_lastReceivedBps = 0.0;
    m_throEstFwdBwBps = 0.0;


    m_totSentBytes = 0;
    m_totRcvdBytes = 0;
    m_brFractionWindow.clear();

    m_lastCompeteTime = 0;

    m_ccfsStartTime = 0;

    /* Do not reset refPoint, senderSsrc, rxedFbmCount */
    SenderBasedController::reset();
}

/**
 * TODO: Manage max FBM seq for multiple feedback senders(media receiver)
 */
#if 0
bool CcfsController::updateFbmMaxSeq(uint16_t seq)
{
    /* Check seq */
    int32_t  fbSeq = (65535*m_wrapCount) + int32_t(seq) ;

    if(m_maxFbmSeq == fbSeq) 
    {
        NS_LOG_INFO("Received duplicated fbseq(" << fbSeq 
                        << "). Ignore"); 
        return false; 
    } 
    else if(m_maxFbmSeq > fbSeq) 
    {
        if((m_maxFbmSeq - fbSeq) > (65535/3)) {
            m_wrapCount++;
            fbSeq += 65535;
        }
        else {
            NS_LOG_INFO("Received stale:  maxFb=" << m_maxFbmSeq
                        << "received seq=" << seq);
            return false;

        }
    }

    if(m_maxFbmSeq >= fbSeq) {
        NS_LOG_INFO("Strange seq: max=" << m_maxFbmSeq 
                    << ", fbSeq=" << fbSeq
                    << ", seq16=" << seq);

        NS_ASSERT(m_maxFbmSeq < fbSeq);
    }


    if(fbSeq - m_maxFbmSeq > 1) {
        NS_LOG_INFO("Detect FB loss! max=" << m_maxFbmSeq
                    << " Just received seq=" << fbSeq);
    }

    m_maxFbmSeq = fbSeq;

    return true;
}
#endif



uint64_t CcfsController::findLatestRtp(ns3::RfbHeader &feedback, std::pair<uint32_t, uint16_t> &ssrcSeq)
{
    uint64_t latest = 0;

    std::set<uint32_t> ssrcList{};
    feedback.GetSsrcList (ssrcList);

    for(auto ssrc : ssrcList) 
    {
        std::vector<std::pair<uint16_t, ns3::RfbHeader::MetricBlock> > rv{};

        const bool res = feedback.GetMetricList (ssrc, rv);

        NS_ASSERT (res);
        for (auto& item : rv)
        {
            if(latest < item.second.m_timestampMs) {
                latest = item.second.m_timestampMs;
                ssrcSeq = std::make_pair(ssrc, item.first);
            }
        }
    }

    return latest;

}

bool CcfsController::findEndSequences(ns3::RfbHeader &feedback, std::vector<std::pair<uint32_t, uint16_t>> &ssrcSeqs)
{
    std::set<uint32_t> ssrcList{};
    feedback.GetSsrcList(ssrcList);

    ssrcSeqs.clear();

    for(auto ssrc : ssrcList)
    {
        ssrcSeqs.push_back( std::make_pair(ssrc, feedback.GetEndSeqInSsrc(ssrc)) );
    }


    return ssrcSeqs.size() == 0 ? false : true;
}

std::pair<uint32_t, uint64_t> CcfsController::getNetRemains(ns3::RfbHeader &feedback, std::vector < std::pair< uint32_t, uint16_t > > &ssrcSeqs)
{
    uint64_t totRxedBytes = 0;
    uint64_t totSentBytes = 0;
    std::vector<std::pair<uint32_t, uint16_t>>::iterator rxItr;

    for(rxItr = ssrcSeqs.begin(); rxItr != ssrcSeqs.end(); rxItr++) {
        totRxedBytes += m_sentRtpSsrcMap[rxItr->first][rxItr->second].totSentBytes;
    }

    std::map<uint32_t, SentRtpStream>::iterator txItr;
    for(txItr = m_sentRtpSsrcMap.begin(); txItr != m_sentRtpSsrcMap.end(); txItr++) {
        totSentBytes += txItr->second[m_lastSentRtpMap[txItr->first]].totSentBytes;
    }


    auto reportedPktCount = feedback.GetTotalReportCount();

    NS_ASSERT(totSentBytes >= totRxedBytes);
    NS_ASSERT(m_inflightPktCount >= reportedPktCount);


    m_inflightPktCount -= reportedPktCount;
    return std::make_pair(m_inflightPktCount, totSentBytes-totRxedBytes);
}



void CcfsController::parseFeedback(ns3::RfbHeader &feedback, const uint32_t beginMs, const uint32_t endMs, ParsedFBData &parsed)
{
    std::set<uint32_t> ssrcList{};
    feedback.GetSsrcList (ssrcList);

    parsed.rxedSentBytes = 0;
    parsed.rxedBytes = 0;
    parsed.txedBytes = 0;
    parsed.lastAddedBytes = 0;
    parsed.lossCount = 0;
    parsed.vq.clear();

    for(auto ssrc : ssrcList)
    {

        auto countEnd = feedback.GetCountEndSeq(ssrc);
        uint16_t beginSeq = countEnd.second - countEnd.first + 1;

        std::vector<std::pair<uint16_t, ns3::RfbHeader::MetricBlock> > rv{};
        feedback.GetMetricList (ssrc, rv);

        for(uint16_t i = 0; i < countEnd.first; i++)
        {
            uint16_t seq = (uint16_t)(beginSeq + i);

            bool find = false;
            std::vector<std::pair<uint16_t, ns3::RfbHeader::MetricBlock> >::iterator item;
            for (item = rv.begin(); item != rv.end(); item++)
            {
                if(seq == item->first) {
                    find = true;
                    break;
                }
            }

            SentRtpRecord record = {0};
            std::pair<uint32_t, uint16_t> ssrcSeq = std::make_pair(ssrc, seq);

            if(getSentRtp(ssrcSeq, record)) 
            {

                if(find) {
                    parsed.rxedBytes += record.size;

                    auto sentMs = record.localTimestampUs/1000;
                    if(beginMs <= sentMs && sentMs < endMs)
                    {
                        parsed.rxedSentBytes += record.size;
                    }

                    QDelayData timeData = {ssrc, seq, record.localTimestampUs, item->second.m_timestampMs, record.size };
                    parsed.vq.push_back(timeData);


                    rv.erase(item);
                }
                else {
                    parsed.lossCount++;
                }

                //NS_LOG_INFO("[ssrc=" << ssrc << "]seq=" << seq
                //            << ", find=" << find
                //            << ", lossCount=" << lossCount
                //            << ", rxedBytes=" << rxedBytes);
            }
            else 
            {
                NS_LOG_INFO("Cannot find from sent list ssrcSeq=" << ssrcSeq.first << "," << ssrcSeq.second);
                if(find) {
                    rv.erase(item);
                }
            }
            
        }


        /* txedBytes */
        auto& stream = m_sentRtpSsrcMap[ssrc];
        SentRtpStream::iterator sitr;
        uint16_t maxSeq  = 0;
        uint64_t lastSentTime = 0;

        for(sitr = stream.begin(); sitr != stream.end(); sitr++)
        {
            auto sentMs = sitr->second.localTimestampUs/1000;
            if(beginMs <= sentMs && sentMs < endMs) 
            {
                parsed.txedBytes += sitr->second.size;

                if(lastSentTime < sentMs)
                {
                    lastSentTime = sentMs;
                    maxSeq = sitr->first;
                }
            }
        }

        /* last Added Bytes */
        SentRtpRecord recordRxedEnd = {0}, recordTxedEnd = {0};
        std::pair<uint32_t, uint16_t> ssrcEndSeq = std::make_pair(ssrc, countEnd.second);
        std::pair<uint32_t, uint16_t> ssrcMaxSeq = std::make_pair(ssrc, maxSeq);

        if(getSentRtp(ssrcEndSeq, recordRxedEnd))
        {
            if(getSentRtp(ssrcMaxSeq, recordTxedEnd)) {
                parsed.lastAddedBytes = recordTxedEnd.totSentBytes - recordRxedEnd.totSentBytes;

            }
            else {
                NS_LOG_INFO("Cannot find ssrcMaxSeq=" << ssrcMaxSeq.first << "," << ssrcMaxSeq.second);
            }
        }
        else
        {
            NS_LOG_INFO("Cannot find from sent list ssrcEndSeq=" << ssrcEndSeq.first << "," << ssrcEndSeq.second);
        }
    }


    NS_LOG_INFO("[ParseFBM] RxedPkt=" << feedback.GetTotalReportCount()
                                      << " RxedBytes=" << parsed.rxedBytes
                                      << " TxedBytes=" << parsed.txedBytes
                                      << " SentRxedBytes=" << parsed.rxedSentBytes
                                      << " AddedBytes=" << parsed.lastAddedBytes
                                      << " Lost=" << parsed.lossCount);

}


void CcfsController::updateTargetQDelay(int32_t newTargetQDelay)
{

    if(m_targetQDelay == newTargetQDelay)
        return;

    if(newTargetQDelay > kCcfsTargetQDelayMsMax) {
        NS_LOG_INFO("Set max target qdelay");
        newTargetQDelay = kCcfsTargetQDelayMsMax;
    }
    
    if(newTargetQDelay < kCcfsTargetQDelayMsMin) {
        NS_LOG_INFO("Set min target qdelay");
        newTargetQDelay = kCcfsTargetQDelayMsMin;
    }

    NS_LOG_INFO("Update TargetQDelay: " << m_targetQDelay << "->" << newTargetQDelay);
    m_targetQDelay = newTargetQDelay;
}
void CcfsController::updateSendRate(float newTargetBps)
{
    float applyBps = newTargetBps;

    if(applyBps < (m_minBw/8.0f))
        applyBps = m_minBw/8.0f;

    if(applyBps > (m_maxBw/8.0f))
        applyBps = m_maxBw/8.0f;

    NS_LOG_INFO("Target: " << ns3::utilConvertKbps(m_targetSendBps) << " ->" << ns3::utilConvertKbps(applyBps)
                           << " estFwdBw=" << ns3::utilConvertKbps(m_estFwdBwBps));
    m_targetSendBps = applyBps;
}
void CcfsController::setFwdBw(float roomedInputBwBps)
{
    float newFwdBwBps = 0.0f;

    if(m_estFwdBwBps <  (m_minBw/8.0))
    {
        newFwdBwBps = roomedInputBwBps/(1.0 - kCcfsFwdBwdRoom);
    }
    else
    {
        newFwdBwBps = (kCcfsFwdBwdEstMAFactor * m_estFwdBwBps) + ((1.0 - kCcfsFwdBwdEstMAFactor) * roomedInputBwBps);

        if(newFwdBwBps > m_maxBw/8.0f)
            newFwdBwBps = m_maxBw/8.0f;

    }

    NS_LOG_INFO("update FwdBw:" << ns3::utilConvertKbps(m_estFwdBwBps)
                                << "-->"     << ns3::utilConvertKbps(newFwdBwBps)
                                << " input:" << ns3::utilConvertKbps(roomedInputBwBps));
    m_estFwdBwBps = newFwdBwBps;

}


uint64_t CcfsController::getIncreasingQTime(uint64_t nowUs, int32_t minQDelay, uint64_t periodEndMs)
{
    if(m_incrLastMinQDelay < minQDelay)
    {
        m_incrCount++;

        if(m_incrCount == 1)
            m_incrStartTime = periodEndMs;


        m_incrLastMinQDelay = (m_incrLastMinQDelay*85 + minQDelay*15)/100;

        if(m_incrCount >= 3)
        {
            const auto duration = periodEndMs - m_incrStartTime;
            if(duration >= kCcfsQIncrDetectorMinDurationMs)
            {
                NS_LOG_INFO("Increasing Set:" << m_incrStartTime
                << " ~ " << periodEndMs
                << ":" << duration << "ms"
                << " incrCount=" << m_incrCount);

                return duration;
            }
        }


        return 0;

    }

    m_incrCount = 0;
    m_incrStartTime = 0;
    m_incrLastMinQDelay = minQDelay;


    return 0;

}

bool CcfsController::getSentRtp(std::pair<uint32_t, uint16_t> &ssrcSeq, SentRtpRecord &sentRtp)
{
    if(m_sentRtpSsrcMap.find(ssrcSeq.first) == m_sentRtpSsrcMap.end()) {
        return false;
    }

    auto& stream = m_sentRtpSsrcMap[ssrcSeq.first];
    if(stream.find(ssrcSeq.second) == stream.end()) {
        return false;
    }

    sentRtp = stream[ssrcSeq.second];
    return true;
}

float CcfsController::getBpsForProbing()
{
    return (200000.0/8.0);   // 200kbps
}

void CcfsController::handleEvtStartCompete(uint64_t nowUs)
{
    updateSendRate( m_targetSendBps*kCcfsCompTargetBrUpRate );

    m_status = SENDER_STATUS_COMPETING;

    updateTargetQDelay(kCcfsCompeteTargetQDelayMs);

    m_lastCompeteTime = nowUs/1000;
}

void CcfsController::handleEvtStartProbing(uint64_t nowUs)
{
    updateSendRate( m_estFwdBwBps + getBpsForProbing() );

    m_probingStartTime = nowUs/1000;
    m_status = SENDER_STATUS_PROBING;
}

bool CcfsController::validateFeedback(uint64_t nowUs, ns3::RfbHeader &feedback)
{


#if 0
    std::stringstream ss;
    feedback.Print(ss);
    NS_LOG_INFO("Recieved Feedback \n" << ss.rdbuf() );
#endif
    std::pair<uint32_t, uint64_t> netRemains = std::make_pair<uint32_t, uint64_t>(0,0);
    std::vector< std::pair<uint32_t, uint16_t> > ssrcEndSeqs;

    if(!findEndSequences(feedback, ssrcEndSeqs)) {
        NS_LOG_INFO("[ERROR] Cannot find end seq...");
        return false;
    }
    else {
        netRemains = getNetRemains(feedback, ssrcEndSeqs);
    }



    // TODO: find proper ssrc/seq pair
    uint32_t ssrc = ssrcEndSeqs[0].first;
    uint16_t seq = ssrcEndSeqs[0].second;

    uint32_t QPkts = 0;
    uint32_t QBytes = 0;
    uint64_t QMs = 0;
    ns3::UtilNQMonitor::Entry qentry;

    if(m_nqMonitor)
    {
        qentry = m_nqMonitor->GetEntry( ssrc, seq );

        QPkts = qentry.remainPkts;
        QBytes = qentry.remainBytes;
        QMs = qentry.delayUs/1000;
    }

    NS_LOG_INFO("[StartProcessFBM] NetRemains:" << netRemains.first << "pkt, " << netRemains.second << "bytes"
                << " QPkts=" << QPkts << " QBytes=" << QBytes << " QMs=" << QMs
                << " At (" << ssrc << ", " << seq << ") rcvd"
                << " FbmCount=" << m_rxedFbmCount);


    if(m_nqMonitor) {
        m_nqMonitor->DelOlds(qentry.timeUs);
    }

    if(feedback.GetTotalReportCount() == 0)
    {
        NS_LOG_INFO("TBD: handle if no reported count");
        return false;
    }

    return true;
}

float CcfsController::updateBrFractionWindow(const uint32_t beginMs,
                                             const uint32_t endMs,
                                             const uint64_t rxedBytes,
                                             const uint64_t txedBytes)
{

    BrFractionData brData = { beginMs, txedBytes, rxedBytes};

    m_totSentBytes += txedBytes;
    m_totRcvdBytes += rxedBytes;

    m_brFractionWindow.push_back(brData);

    while (m_brFractionWindow.size())
    {
        const uint64_t timeMs = m_brFractionWindow.front().beginTimeMs;

        if(timeMs + kCcfsBrFractionWindowTimeMs < endMs)
        {
            m_totSentBytes -= m_brFractionWindow.front().sentBytes;
            m_totRcvdBytes -= m_brFractionWindow.front().rcvdBytes;

            m_brFractionWindow.pop_front();

        }
        else
        {
            break;
        }
    }

    float ret = float(double(m_totRcvdBytes) / double(m_totSentBytes));
    NS_LOG_INFO("[BrFraction] brFraction=" << ret
                << " sent=" << m_totSentBytes
                << " rcvd=" << m_totRcvdBytes
                << " wndCount = " << m_brFractionWindow.size());
    return ret;
}

std::pair<uint32_t /* beginMs */, uint32_t /*endMs*/>
CcfsController::getLastPeriodMs(ns3::RfbHeader &feedback)
{
    SentRtpRecord rtp = { 0, 0, 0 };
    std::pair<uint32_t, uint16_t> ssrcSeq = { 0, 0 };

    uint64_t rxTimeMs = findLatestRtp(feedback, ssrcSeq);
    if(rxTimeMs == 0)
    {
        NS_LOG_INFO("ERROR(" << __LINE__
                             << "): Cannot find sentRtp of latest rtp in fb. (ssrc,seq)=(" << ssrcSeq.first
                             << "," << ssrcSeq.second << ")");

    }
    else
    {
        if(!getSentRtp(ssrcSeq, rtp)) {
            NS_LOG_INFO("ERROR(" << __LINE__
                                 << "): Cannot get Rtp record (ssrc,seq)=(" << ssrcSeq.first
                                 << "," << ssrcSeq.second << ")");
        }
        else {
            uint64_t offset = feedback.GetReportTime() - rxTimeMs;
            uint64_t sentLatestRtpMs = rtp.localTimestampUs/1000;

            uint64_t endMs = sentLatestRtpMs + offset;


            return std::make_pair(endMs - feedback.GetMonitoredTime(), endMs);

        }

    }

    return std::make_pair(0,0);


}

std::pair<uint64_t /* st */, int64_t /* qpt */>
CcfsController::calcIVQDelay(const ParsedFBData &parsed, float fwdbw, std::vector<int64_t> &vqdelays)
{
    /*
     * always vqd < owd
     *   vqd: virtual q delay = q processing time + extra q delay
     *   owd: one way delay = network q delay(q processing time + xod) + propagation + error
     *
     *   And
     *   owd = xod + base_delay
     *
     *   So
     *   vqd[i] < xod[i] + base_delay
     *   Although we cannot know base_delay,
     *   kCcfsVqBaseDelayUs is base_delay in virtual q delay and constant.
     *
     *   So, vqd[i] - kCcfsVqBaseDelayUs < xod[i]
     *
     *
     * qdt: q departure time
     * qpt: q processing time(=vqd)
     *
     * qdt[i] = MAX(st[i], qdt[i-1]) + (size[i] / bw)
     * qpt[i] = qdt[i] - st[i]
     *        = Max(0, qdt[i-1] - st[i]) + (size[i] / bw)
     */

    int64_t     preQpt;
    uint64_t    preQdt;

    if(m_lastQDelay == 0) {
        preQpt = 0;
        preQdt = 0;
    }
    else {
        preQpt = m_lastQDelay*1000 + kCcfsVqBaseDelayUs;
        preQdt = m_ivqStUs + preQpt;

    }

    int64_t     currQpt = 0;
    uint64_t    currSt = 0;
    int64_t     sumQpt = 0;
    int64_t     off = 0;

    for(int i = 0; i < parsed.vq.size(); i++)
    {
        currSt = parsed.vq[i].localSentUs;
        currQpt = int64_t(1000000.0f * float(parsed.vq[i].pktSize) / fwdbw);

        if(preQdt > currSt) {
            off = int64_t(preQdt - currSt);
            currQpt += off;
        }

/*
        if(currQpt < m_ivqMinQptUs)
            m_ivqMinQptUs = currQpt;
        currQpt -= m_ivqMinQptUs;
*/

        sumQpt += currQpt;

        NS_LOG_INFO("[TOBEDEL] ivqQptMs=" << currQpt / 1000 << "." << currQpt % 1000
                                          << " offMs=" << off / 1000 << "." << off % 1000
                                          << " ivqSt=" << currSt / 1000 << "." << currSt % 1000
                                          << " BW=" << ns3::utilConvertKbps(fwdbw) << " Size=" << parsed.vq[i].pktSize);

        preQpt = currQpt;
        preQdt = currSt + currQpt;

        vqdelays.push_back(currQpt/1000);
    }

    return std::make_pair(currSt, currQpt);

};

void CcfsController::updateInternalVQDelay(const rmcat::CcfsController::ParsedFBData &parsed, int32_t qdelay, std::vector<int64_t> &nqdelays)
{

    if(m_estFwdBwBps == 0.0f)
        return ;

    float ori_fwdbw ;
    float fwdbw = float(m_estFwdBwBps / (1.0f - kCcfsFwdBwdRoom));
    float corr = 0.0f;
    ori_fwdbw = fwdbw;

    int64_t xqptUs = 0;
    std::pair<uint64_t, int64_t> stqpt;
    std::vector<int64_t> vqdelays;

    /// if too small qdelay, error is too large
    if(qdelay == 0)
    {
        m_ivqQptUs = 0;
        m_ivqDelayUs = 0;
        m_ivqStUs = 0;

        goto FUNC_OUT;

    }




    while(1)
    {
        vqdelays.clear();

        stqpt = calcIVQDelay(parsed, fwdbw, vqdelays);
        corr = ns3::utilGetCorrelationCoef(nqdelays, vqdelays);

        NS_LOG_INFO("[TOBEDEL] qpt=" << stqpt.second
                    << " nq.size=" << nqdelays.size()
                    << " vq.size=" << vqdelays.size()
                    << " corr=" << corr);

        if(stqpt.second > kCcfsVqBaseDelayUs)
            xqptUs = (stqpt.second  - kCcfsVqBaseDelayUs);
        else
            xqptUs = 0;

        if(qdelay < xqptUs/1000)
        {
            NS_LOG_INFO("[TOBEDEL] bw compensation cont. qdelay=" << qdelay
                    << " xqpt=" << xqptUs/1000
                    << " fwdbw=" << ns3::utilConvertKbps(fwdbw)
                    << " next-fwdbw=" << ns3::utilConvertKbps(fwdbw + 12500)
                    << " ori-fwdbw=" << ns3::utilConvertKbps(ori_fwdbw));
            fwdbw += 12500; // 100 kbps
        }
        else
        {
            break;
        }
    }

    m_ivqQptUs = stqpt.second;
    m_ivqDelayUs = xqptUs;
    m_ivqStUs = stqpt.first;

#if 0
    /*
     * TODO: need survey more graceful calculation to find out fwdbw
     */
    if(ori_fwdbw != fwdbw)
        setFwdBw(fwdbw);
#endif

FUNC_OUT:

    NS_LOG_INFO("[UpdateIVQ] ivqQptMs=" << m_ivqQptUs / 1000 << "." << m_ivqQptUs % 1000
                 << " ivqDelayMs=" << m_ivqDelayUs / 1000 << "." << m_ivqDelayUs % 1000
                 << " netQDelayMs=" << qdelay << " corr=" << fabs(corr)
                 << " properBw:" << ns3::utilConvertKbps(fwdbw)  << "(" << ns3::utilConvertKbps(ori_fwdbw) << ")");


}
rmcat::CcfsController::EstiQDelay CcfsController::estiQDelay(uint64_t periodEndMs, const ParsedFBData &parsed)
{

    /// Remove olds
    while (m_qdelayWindow.size())
    {
        const uint64_t timeMs = m_qdelayWindow.front().second;
        if(timeMs + kCcfsQDelayWindowTimeMs < periodEndMs)
        {
            m_qdelayWindow.pop_front();

        }
        else
        {
            break;
        }
    }
    int32_t wndMaxQDelay = -1;
    EstiQDelay qdelay = {10000, 0, 0, 0};

    /// Find minimum within m_qdelayWindow
    std::deque< std::pair<int32_t, uint64_t> >::iterator itr;

    for(itr = m_qdelayWindow.begin(); itr != m_qdelayWindow.end(); ++itr)
    {
        if(qdelay.wndMinQDelay > itr->first)
            qdelay.wndMinQDelay = itr->first;

        if(wndMaxQDelay < itr->first)
            wndMaxQDelay = itr->first;

    }


    std::string log("");
    char buff[64];

    std::vector<int64_t> nqdelays;

    /// Find latestQDelay and Update m_qdelayWindow
    if(parsed.vq.size() == 0)
    {
        /// If there is no reported pkts, just use the last qdelay
        qdelay.latestQDelay = qdelay.wndMinQDelay;

    }
    else
    {
        int32_t  mindelay = 9000;

        for(int i = 0; i < parsed.vq.size(); i++)
        {
            int64_t qdelay_diff = (int32_t)(parsed.vq[i].rcvdMs - (parsed.vq[i].localSentUs/1000));

            if(m_minQDelay > qdelay_diff)
            {
                m_minQDelay = qdelay_diff;
                m_minQPktSize = parsed.vq[i].pktSize;
                NS_LOG_INFO("Update base qdelay: " << m_minQDelay << "->" << qdelay_diff
                                                   << " base delay pkt size=" << m_minQPktSize);

            }

            int32_t newQDelay = qdelay_diff - m_minQDelay;

            if(mindelay > newQDelay) {
                mindelay = newQDelay;
            }

            nqdelays.push_back(newQDelay);

            snprintf(buff, sizeof(buff), "%d:%dms,", parsed.vq[i].seq, newQDelay);
            log += buff;

        }


        qdelay.latestQDelay = mindelay;

        if(qdelay.wndMinQDelay > qdelay.latestQDelay)
            qdelay.wndMinQDelay = qdelay.latestQDelay;

        if(wndMaxQDelay < qdelay.latestQDelay)
            wndMaxQDelay = qdelay.latestQDelay;

        m_qdelayWindow.push_back(std::make_pair(qdelay.latestQDelay, periodEndMs));


    }


    qdelay.wndMinQRange = wndMaxQDelay - qdelay.wndMinQDelay;

    updateInternalVQDelay(parsed, qdelay.latestQDelay, nqdelays);

    qdelay.currXQDelay = qdelay.latestQDelay - (m_ivqQptUs/1000);

    if(qdelay.currXQDelay > 0) {
        if(m_maXQDelay == 0.0f)
            m_maXQDelay = float(qdelay.currXQDelay);
        else
            m_maXQDelay = 0.9f*m_maXQDelay + 0.1f*float(qdelay.currXQDelay);

    }
    else {
        m_maXQDelay = 0.0f;
    }


    NS_LOG_INFO("[EstQDelay] " << log
                << "\n                                                 "
                << " wndMinQDelayMs=" << qdelay.wndMinQDelay
                << " wndMinQRangeMs=" << qdelay.wndMinQRange
                << " latestQDelay=" << qdelay.latestQDelay
                << " QDelaySampleCount="<< m_qdelayWindow.size()
                << " intQDelay=" << m_ivqDelayUs/1000
                << " currXQDelay=" << qdelay.currXQDelay
                << " MA(XQDelay)=" << m_maXQDelay
                );

    return qdelay;
}

float CcfsController::estiFwdBwPre(const ParsedFBData parsed, int periodMs)
{
    float old = m_estFwdBwBps;
    float lastRxedBps = 0.0f;

    lastRxedBps = (1000.0 * (float)parsed.rxedBytes) / (float)(periodMs) ; /* Byte per sec */
    float lastTxedBps = (1000.0 * (float)parsed.txedBytes) / (float)(periodMs) ; /* Byte per sec */
    float roomedLastRxedBps = lastRxedBps*(1.0 - kCcfsFwdBwdRoom);

    if( m_status != SENDER_STATUS_THROTTLED
        && ( (parsed.txedBytes > parsed.rxedSentBytes)                                       /* Not arrived all yet */
            || (parsed.txedBytes == parsed.rxedSentBytes && m_targetSendBps < lastRxedBps) ) /* Prevent under-estimation */
        && ( m_status != SENDER_STATUS_COMPETING ||
           (m_status == SENDER_STATUS_COMPETING && roomedLastRxedBps > m_estFwdBwBps && parsed.lossCount == 0)) )
    {
        setFwdBw(roomedLastRxedBps);
    }


    m_lastReceivedBps = lastRxedBps;

    NS_LOG_INFO("[EstFwdBw]Estd=" << ns3::utilConvertKbps(old) << "-->" << ns3::utilConvertKbps( m_estFwdBwBps )
                                  << " Txed=" << ns3::utilConvertKbps( (uint32_t)(lastTxedBps) )
                                  << " Rxed=" << ns3::utilConvertKbps( (uint32_t)(lastRxedBps) )
                                  << " Topo=" << ns3::utilConvertKbps( m_netDataRate )
                                  << " Target=" << ns3::utilConvertKbps( m_targetSendBps ));


    return m_estFwdBwBps - old;

}




rmcat::CcfsController::ControlSend CcfsController::getControlSend(const uint64_t nowUs,
                                       const EstiQDelay &eq,
                                       const uint32_t periodEndMs,
                                       const float brFraction)
{
    ControlSend ctrl = { SEND_CONTROL_NOTHING, 1.0f,  0.0f, brFraction };

    uint64_t increasingMs = getIncreasingQTime(nowUs, eq.latestQDelay, periodEndMs);

    if(m_estFwdBwBps == 0.0)
    {
        goto FUNC_OUT;
    }


    ctrl.qdFraction = float(eq.latestQDelay) / float(m_targetQDelay);

    if(eq.currXQDelay > 0)
        ctrl.xqFraction = float(eq.currXQDelay) / float(kCcfsCrossTrafficTargetQDelayMs);


    if(m_status == SENDER_STATUS_THROTTLED)
    {
        if(ctrl.qdFraction < 1.0)
        {
            ctrl.evt = SEND_CONTROL_RESOLV_THROTTLE;
            NS_LOG_INFO("THROTTLE RESOLV");
            goto FUNC_OUT;
        }
    }


    if (increasingMs > 0
        && ctrl.qdFraction >= kCcfsTrigThroQDFractMin
        && ctrl.brFraction < kCcfsTrigThroBrFractMax
        /* TODO ref xqFraction
        && ctrl.xqFraction == 0.0f
         */
            )
    {
        ctrl.evt = SEND_CONTROL_DETECT_THROTTLE;
        NS_LOG_INFO("THROTTLE DETECT");

        goto FUNC_OUT;
    }


    if( (eq.wndMinQRange > kCcfsTrigCompQMRangeMin && increasingMs > KCcfsTrigCompQIncrTimeMs)  /* intrude: Fast increase */
        || (eq.latestQDelay > kCcfsTrigCompQDelayMin && increasingMs > KCcfsTrigCompQIncrTimeMs) )      /* Slow increase */
        /* TODO: ref xqFraction ctrl.xqFraction > 0.2f */
       /*
            || (eq.latestQDelay > kCcfsTrigCompQDelayMin && increasingMs > KCcfsTrigCompQIncrTimeMs)) )
            */
    {
        ctrl.evt = SEND_CONTROL_START_COMPETE;
        NS_LOG_INFO("COMPETE START");

        goto FUNC_OUT;
    }

    if(m_status == SENDER_STATUS_COMPETING
        && periodEndMs - m_lastCompeteTime > kCcfsCompeteMaintainMs
        && eq.latestQDelay < kCcfsTrigStopCompQDelayMax
        && eq.wndMinQRange < kCcfsTrigStopCompQMRangeMax
       /* TODO: ref xqFraction
        && ctrl.xqFraction == 0.0f
        */
            )
    {
        ctrl.evt = SEND_CONTROL_STOP_COMPETE;

        NS_LOG_INFO("COMPETE STOP");
        goto FUNC_OUT;
    }

    if( (nowUs - m_ccfsStartTime)/1000 > kCcfsInitWaitToProbeMs
       && m_status != SENDER_STATUS_PROBING
       && eq.latestQDelay < kCcfsTrigProbQDelayMax
       && ctrl.brFraction >= kCcfsTrigProbBrFractMin
       && eq.wndMinQRange < kCcfsTrigProbQMRangeMax
       && m_ecnRate < kCcfsTrigProbECNRtMax
       && m_lossRate < kCcfsTrigProbLossRtMax)
    {
        ctrl.evt = SEND_CONTROL_START_PROBING;
        NS_LOG_INFO("PROBING START");

        goto FUNC_OUT;
    }

    if(m_status == SENDER_STATUS_PROBING
        && ( increasingMs > 0
             || ctrl.qdFraction > kCcfsTrigStopProbQDFractMin
             || ctrl.brFraction < kCcfsTrigStopProbBrFractMax
             || m_ecnRate > kCcfsTrigStopProbECNRtMax
             || m_lossRate > kCcfsTrigStopProbLossRtMax) )
    {
        ctrl.evt = SEND_CONTROL_STOP_PROBING;
        NS_LOG_INFO("PROBING STOP");
        goto FUNC_OUT;
    }



FUNC_OUT:

    NS_LOG_INFO("[CtrlSend] targetQDelay=" << m_targetQDelay
                << " qdFraction=" << ctrl.qdFraction
                << " brFraction=" << ctrl.brFraction
                << " xqFraction=" << ctrl.xqFraction
                << " ctrl.evt=" << ctrl.evt
                << " increasingMs=" << increasingMs);


    return ctrl;
}
void CcfsController::handleSendControl(uint64_t nowUs, const ParsedFBData &parsed, const EstiQDelay &eq, const ControlSend &ctrl)
{
    float currBps = m_targetSendBps;

    SenderStatus beforeStatus = m_status;

    uint64_t nowMs = nowUs/1000;

    /*
     * Control by status
     */
    switch(m_status)
    {
        case SENDER_STATUS_DEFAULT:

            if(ctrl.evt == SEND_CONTROL_NOTHING)
            {
                if(ctrl.qdFraction < kCcfsDfltBrCtrlQDFractLow && m_targetSendBps < m_estFwdBwBps)
                {
                    updateSendRate( m_targetSendBps*kCcfsDfltBrCtrlIncrRate );
                }
                else if(ctrl.qdFraction > kCcfsDfltBrCtrlQDFractHi)
                {
                    updateSendRate( m_targetSendBps*kCcfsDfltBrCtrlDecrRate );
                }
            }

            else if(ctrl.evt == SEND_CONTROL_START_PROBING)
            {
                handleEvtStartProbing(nowUs);
            }

            else if(ctrl.evt == SEND_CONTROL_START_COMPETE)
            {
                handleEvtStartCompete(nowUs);
            }


            else if(ctrl.evt == SEND_CONTROL_DETECT_THROTTLE)
            {
                updateSendRate( m_targetSendBps * kCcfsThroTargetBrRate );
                m_throEstFwdBwBps = m_estFwdBwBps;
                m_status = SENDER_STATUS_THROTTLED;

            }

            break;

        case SENDER_STATUS_COMPETING:

            if(ctrl.evt == SEND_CONTROL_NOTHING)
            {
                if (m_targetSendBps < m_estFwdBwBps * 0.95)
                        /* TODO: ref xqFraction
                        || (ctrl.qdFraction < kCcfsCompBrCtrlQDFractHi && ctrl.xqFraction > kCcfsCompBrCtrlXQFractLow)))
                         */
                {
                    updateSendRate( m_targetSendBps * kCcfsCompBrCtrlIncrRate );
                }

                if(m_targetSendBps > m_estFwdBwBps * 1.05)
                /* TODO: ref xqFraction
                        && (0.001 < ctrl.xqFraction && ctrl.xqFraction < 0.4))
                */
                {
                    updateSendRate( m_targetSendBps * kCcfsCompBrCtrlDecrRate );
                }
#if 0

                else if(eq.latestQDelay < kCcfsCompBrCtrlQDelayBoundLow) {
                    updateSendRate( m_targetSendBps * kCcfsCompBrCtrlIncrRate );
                }
#endif
            }
            else if(ctrl.evt == SEND_CONTROL_START_COMPETE)
            {

                // JUST update last event time
                m_lastCompeteTime = nowUs/1000;
            }
            else if(ctrl.evt == SEND_CONTROL_STOP_COMPETE)
            {
                updateTargetQDelay(kCcfsTargetQDelayMs);
                m_lastCompeteTime = 0;

                m_status = SENDER_STATUS_DEFAULT;

                NS_LOG_INFO("COMPETING Stop");
            }

            else if(ctrl.evt == SEND_CONTROL_DETECT_THROTTLE)
            {
                NS_LOG_INFO("THROTTLED during COMPETE: stay as COMPETE");

                /*
                 *
                updateTargetQDelay(kCcfsTargetQDelayMs);
                m_lastCompeteTime = 0;

                updateSendRate( m_targetSendBps * kCcfsThroTargetBrRate );
                m_throEstFwdBwBps = m_estFwdBwBps;
                m_status = SENDER_STATUS_THROTTLED;

                NS_LOG_INFO("COMPETING Stop --> THROTTLED");
                 */
            }

            break;

        case SENDER_STATUS_PROBING:

            if(ctrl.evt == SEND_CONTROL_NOTHING)
            {
                if( nowMs > m_probingStartTime + kCcfsProbingTimeoutMs )
                {

                    m_status = SENDER_STATUS_DEFAULT;

                    NS_LOG_INFO("PROBING Complete");
                }

            }
            else if(ctrl.evt == SEND_CONTROL_STOP_PROBING)
            {
                m_status = SENDER_STATUS_DEFAULT;

                NS_LOG_INFO("PROBING Quit --> DEFAULT");

                updateSendRate(m_estFwdBwBps);
            }
            else if(ctrl.evt == SEND_CONTROL_START_COMPETE)
            {
                NS_LOG_INFO("PROBING Quit");

                handleEvtStartCompete(nowUs);
            }
            else if(ctrl.evt == SEND_CONTROL_DETECT_THROTTLE)
            {
                NS_LOG_INFO("PROBING Quit --> THROTTLED");

                updateSendRate( (m_estFwdBwBps * kCcfsThroTargetBrRate) );
                m_throEstFwdBwBps = m_estFwdBwBps;
                m_status = SENDER_STATUS_THROTTLED;

            }

            break;

        case SENDER_STATUS_THROTTLED:

            if(ctrl.evt == SEND_CONTROL_RESOLV_THROTTLE)
            {
                NS_LOG_INFO("THROTTLED Complete");
                updateSendRate( m_throEstFwdBwBps );
                m_status = SENDER_STATUS_DEFAULT;

            }
            else if(ctrl.evt == SEND_CONTROL_START_COMPETE)
            {
                NS_LOG_INFO("THROTTLED --> COMPETING");
                updateSendRate( m_throEstFwdBwBps );
                handleEvtStartCompete(nowUs);
            }
            else if(ctrl.evt == SEND_CONTROL_DETECT_THROTTLE) {
                NS_LOG_INFO("THROTTLED Again");
                /// updateSendRate( m_targetSendBps * kCcfsThroTargetBrRate );
                m_throEstFwdBwBps = m_estFwdBwBps;
            } break;
    }


    NS_LOG_INFO("[HndlTxEvt] status=" << beforeStatus << "->" << m_status
                << " evt=" << ctrl.evt
                << " Target: " << ns3::utilConvertKbps(currBps) << "->" << ns3::utilConvertKbps(m_targetSendBps)
    );


}
bool CcfsController::processFeedback2(uint64_t nowUs, ns3::RfbHeader &feedback)
{
    if(validateFeedback(nowUs, feedback) == false) {
        NS_LOG_INFO("Validation fail");
        return true;

    }

    /**
     * TODO: Manage max FBM seq for multiple feedback senders(media receiver)
     *
    if(m_rxedFbmCount == 0)
    {
        m_maxFbmSeq = int32_t(feedback.GetFbSeq());
    }
    else 
    {
        if(!updateFbmMaxSeq(feedback.GetFbSeq())) {
            NS_LOG_INFO("Update MaxSeq fail");
            return false;
        }
    }
    */

    m_rxedFbmCount++;


    /*
     * parse Feedback
     */
    auto periodBeginEnd = getLastPeriodMs(feedback);
    if(periodBeginEnd.first == periodBeginEnd.second) {
        NS_LOG_INFO(" Parse fail. Cannot get the period from the feedback");
        return false;
    }
    uint32_t lastPeriodBeginMs = periodBeginEnd.first;
    uint32_t lastPeriodEndMs = periodBeginEnd.second;


    ParsedFBData parsed = { 0};
    parseFeedback(feedback, lastPeriodBeginMs, lastPeriodEndMs, parsed );


    /*
     * Bitrate fraction
     */
    const auto brFraction = updateBrFractionWindow(lastPeriodBeginMs, lastPeriodEndMs, parsed.rxedBytes, parsed.txedBytes);

    /*
     * TODO: Update Loss/Ecn rate
     */
    m_lossRate = 0.0;
    m_ecnRate = 0.0;

    /*
     * First Estimate Fwd bandwidth
     */
    float diffEstBw = estiFwdBwPre(parsed, (lastPeriodEndMs - lastPeriodBeginMs));

    /*
     * Estimate QDelay
     */
    EstiQDelay qdelay = estiQDelay(lastPeriodEndMs, parsed);


    /*
     * Findout Send control
     */
    ControlSend ctrlSend = getControlSend(nowUs, qdelay, lastPeriodEndMs, brFraction);


    /*
     * Second Estimate Fwd bandwidth
     */
    if(m_status == SENDER_STATUS_COMPETING && ctrlSend.qdFraction >= 1.5 && diffEstBw > 0) {
        float old = m_estFwdBwBps;
        m_estFwdBwBps -= diffEstBw;
        NS_LOG_INFO("[EstFwdBw2] FullQ: Estd=" << ns3::utilConvertKbps(old) << "-->" << ns3::utilConvertKbps( m_estFwdBwBps ));
    }



    /*
     * Send control
     */
    handleSendControl(nowUs, parsed, qdelay, ctrlSend);


    /*
     * Backup
     */
    m_lastQDelay = qdelay.latestQDelay;


    logStats(nowUs, feedback.GetMonitoredTime()*1000);

    return true;
}



bool CcfsController::processSendPacket2(uint32_t ssrc,
                                    uint64_t localTimestampUs,
                                    uint16_t sequence,
                                    uint32_t size)
{


    auto& stream = m_sentRtpSsrcMap[ssrc];
    SentRtpRecord lastSentRtp = { 0, 0, 0};


    if(m_firstSend == false) {
        lastSentRtp = stream[m_lastSentRtpMap[ssrc]];
    }
    else {
        NS_LOG_INFO("Start CCFS");
        m_ccfsStartTime = localTimestampUs;

    }

    m_lastSentRtpMap[ssrc] = sequence;

    SenderBasedController::processSendPacket( localTimestampUs/1000, sequence, size );

    SentRtpRecord sentRtp = { localTimestampUs, size, lastSentRtp.totSentBytes + size };


    if(m_sentRtpSsrcMap.find(ssrc) == m_sentRtpSsrcMap.end()) {
        SentRtpStream newstream;
        newstream[sequence] = sentRtp;
        m_sentRtpSsrcMap[ssrc] = newstream;
    }
    else {
        auto& stream = m_sentRtpSsrcMap[ssrc];
        if(stream.find(sequence) != stream.end()) {
            NS_LOG_INFO("Duplicated sentRtp! ssrc=" << ssrc << ",seq=" << sequence);
        }
        stream[sequence] = sentRtp;
    }



    /* Remove old data */
    SentRtpStream::iterator sitr;
    std::list< SentRtpStream::iterator > removes;
    for(sitr = stream.begin(); sitr != stream.end(); ++sitr)
    {
        auto sentMs = sitr->second.localTimestampUs/1000;
        auto currMs = localTimestampUs/1000;

        if(sentMs + kCcfsSentRtpKeepTimeMs < currMs) {
            //NS_LOG_INFO("Remove old data: [ssrc=" << ssrc
            //            << ", (seq, pkt_size)=(" << sitr->first << ", " << sitr->second.size
            //            << ")");
            removes.push_back(sitr);
        }
    }

    std::list< SentRtpStream::iterator >::iterator ritr;
    for(ritr = removes.begin(); ritr != removes.end(); ++ritr) {
        stream.erase(*ritr);
        /// NS_LOG_INFO("remove [ssrc=" << ssrc << "]stream size=" << stream.size());
    }


    m_inflightPktCount++;

    return true;
}





/**
 * The following function retrieves updated
 * estimates of delay, loss, and receiving
 * rate from the base class SenderBasedController
 * and saves them to local member variables.
 */
void CcfsController::updateMetrics(uint64_t now)
{
    NS_LOG_INFO("tobedel");
}

void CcfsController::logStats(uint64_t nowUs, uint64_t deltaUs) const
{

    uint32_t invalid = 0;
    float invalidf = 0.0;
    std::ostringstream os;
    os << std::fixed;
    os.precision(RMCAT_LOG_PRINT_PRECISION);

    /* log packet stats: including common stats
     * (e.g., receiving rate, loss, delay) needed
     * by all controllers and algorithm-specific
     * ones (e.g., xcurr for NADA) */
    os << " algo:ccfs " << m_id
       << " ts: "     << (nowUs / 1000)
       << " loglen: " << m_qdelayWindow.size()
       << " qdel: "   << m_lastQDelay
       << " rtt: "    << invalid
       << " ploss: "  << invalid
       << " plr: "    << invalidf
       << " xcurr: "  << invalidf
       << " rrate: "  << m_lastReceivedBps * 8.0
       << " srate: "  << getBandwidth(nowUs)
       << " avgint: " << invalidf
       << " curint: " << invalid
       << " delta: "  << (deltaUs / 1000);
    logMessage(os.str());
}
void CcfsController::setNetworkAttributes(uint32_t dataRate)
{
    m_netDataRate = dataRate/8;
}
void CcfsController::setNQMonitor(ns3::Ptr<ns3::UtilNQMonitor> monitor)
{
    m_nqMonitor = monitor;
    NS_LOG_INFO("Set NQMonitor: " << monitor);
}

#if 0
    void CcfsController::byParent(uint64_t nowUs, ns3::RfbHeader &feedback)
{
    std::set<uint32_t> ssrcList{};
    feedback.GetSsrcList (ssrcList);

    if(ssrcList.size() != 1) {
        NS_LOG_INFO("Not support multiple ssrc! count of ssrc=" << ssrcList.size());
    }

    uint32_t ssrc = (*ssrcList.begin());


    std::vector<std::pair<uint16_t, ns3::RfbHeader::MetricBlock> > rv{};
    std::vector<std::pair<uint16_t, ns3::RfbHeader::MetricBlock> >::iterator itr;

    feedback.GetMetricList (ssrc, rv);

    for (itr = rv.begin(); itr != rv.end(); itr++)
    {
        uint64_t rxTimestamp = itr->second.m_timestampMs;
        if(!SenderBasedController::processFeedback(nowUs/1000, itr->first, rxTimestamp, itr->second.m_ecn)) {
            NS_LOG_INFO("ERROR(" << __LINE__ << "): SenderBasedController::processFeedback return fail"
                                 << ", seq=" << itr->first << ", rxTimestamp=" << rxTimestamp);

        }
    }

}
#endif
}
