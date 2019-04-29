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
 * CCFS controller interface for rmcat ns3 module.
 *
 * @author Jungnam gwock
 */

#ifndef CCFS_CONTROLLER_H
#define CCFS_CONTROLLER_H

#include "sender-based-controller.h"
#include "ns3/rfb-header.h"
#include "ns3/rmcat-utils.h"

namespace rmcat {

/**
 * Draft:
 * https://datatracker.ietf.org/doc/draft-gwock-rmcat-ccfs/
 */
class CcfsController: public SenderBasedController
{
public:
    CcfsController();
    virtual ~CcfsController();

    virtual void setCurrentBw(float newBw);

    virtual void reset();

    bool processFeedback2(uint64_t nowUs, ns3::RfbHeader &feedback);

    bool processSendPacket2(uint32_t ssrc, 
                           uint64_t txTimestamp,
                           uint16_t sequence,
                           uint32_t size); // in Bytes

    virtual float getBandwidth(uint64_t now) const;

    /**
     * This is only for the logging
     */
    void setNetworkAttributes(uint32_t dataRate);
    void setNQMonitor(ns3::Ptr<ns3::UtilNQMonitor> monitor);

private:
    class SentRtpRecord {
    public:
        uint64_t localTimestampUs;
        uint32_t size;
        uint64_t totSentBytes;
    };


    class QDelayData {
    public:
        uint32_t ssrc;
        uint16_t seq;
        uint64_t localSentUs;
        uint64_t rcvdMs;
        uint32_t pktSize;
    };

    class ParsedFBData {
    public:
        uint64_t rxedBytes;
        uint64_t rxedSentBytes;
        uint64_t txedBytes;
        uint64_t lastAddedBytes;
        uint32_t lossCount;
        std::vector<QDelayData> vq;

    };

    class BrFractionData {
    public:
        uint64_t beginTimeMs;
        uint64_t sentBytes;
        uint64_t rcvdBytes;
    };

    class EstiQDelay {
    public:
        int32_t wndMinQDelay;
        int32_t wndMinQRange;
        int32_t latestQDelay;
        int32_t currXQDelay;
    };



    enum SendControlEvent {
        SEND_CONTROL_NOTHING ,

        SEND_CONTROL_START_COMPETE,
        SEND_CONTROL_START_PROBING,
        SEND_CONTROL_DETECT_THROTTLE,

        SEND_CONTROL_STOP_COMPETE,
        SEND_CONTROL_STOP_PROBING,
        SEND_CONTROL_RESOLV_THROTTLE,
    };

    class ControlSend {
    public:
        SendControlEvent evt;
        float            qdFraction;
        float            xqFraction;
        float            brFraction;
    };

    enum SenderStatus {
        SENDER_STATUS_DEFAULT,
        SENDER_STATUS_COMPETING,
        SENDER_STATUS_PROBING,
        SENDER_STATUS_THROTTLED
    };

    void updateSendRate(float newTargetBps);


    /**
     * TODO: Manage max FBM seq for multiple feedback senders(media receiver)
     */
    /// bool updateFbmMaxSeq(uint16_t seq);

    /**
     * Function for retrieving updated estimates
     * (by the base class SenderBasedController) of
     * delay, loss, and receiving rate metrics and
     * copying them to local member variables
     *
     * @param [in] now  current timestamp in ms
     */
    void updateMetrics(uint64_t now);

    /**
     * Function for printing losss, delay, and rate
     * metrics to log in a pre-formatted manner
     * @param [in] now  current timestamp in ms
     */
    void logStats(uint64_t nowUs, uint64_t deltaUs) const;


    void setFwdBw(float Bps);

    bool findEndSequences(ns3::RfbHeader &feedback, std::vector<std::pair<uint32_t, uint16_t>> &ssrcSeqs);

    std::pair<uint32_t, uint64_t> getNetRemains(ns3::RfbHeader &feedback, std::vector<std::pair<uint32_t, uint16_t>> &ssrcSeqs);

    uint64_t findLatestRtp(ns3::RfbHeader &feedback, std::pair<uint32_t, uint16_t> &ssrcSeq);

    bool getSentRtp(std::pair<uint32_t, uint16_t> &ssrcSeq, SentRtpRecord &sentRtp);

    void updateTargetQDelay(int32_t newTargetQDelay);

    uint64_t getIncreasingQTime(uint64_t nowUs, int32_t minQDelay, uint64_t periodEndMs);

    float getBpsForProbing();

    void handleEvtStartProbing(uint64_t nowUs);
    void handleEvtStartCompete(uint64_t nowUs);

    std::pair<uint64_t /* st */, int64_t /* qpt */> calcIVQDelay(const ParsedFBData &parsed, float fwdbw, std::vector<int64_t> &vqdelays);
    void updateInternalVQDelay(const ParsedFBData &parsed, int32_t qdelay, std::vector<int64_t> &nqdelays);

    /*
     * Feedback Handlers
     */
    bool validateFeedback(uint64_t nowUs, ns3::RfbHeader &feedback);

    std::pair<uint32_t /* beginMs */, uint32_t /*endMs*/>
    getLastPeriodMs(ns3::RfbHeader &feedback);


    void parseFeedback(ns3::RfbHeader &feedback,
                       const uint32_t beginMs,
                       const uint32_t endMs,
                       ParsedFBData &parsed);


    float updateBrFractionWindow(const uint32_t beginMs, const uint32_t endMs, const uint64_t rxedBytes, const uint64_t txedBytes);


    float estiFwdBwPre(const ParsedFBData parsed, const int periodMs);


    EstiQDelay estiQDelay(uint64_t lastPeriodEndMs, const ParsedFBData &parsed);


    ControlSend getControlSend(const uint64_t nowUs,
                            const EstiQDelay &eq,
                            const uint32_t lastPeriodEndMs,
                            const float brFraction);

    void estiFwdBwPost(float differnce, const ControlSend &ctrl);

    void handleSendControl(uint64_t nowUs, const ParsedFBData &parsed, const EstiQDelay &eq, const ControlSend &ctrl);

    /*
     * Variables
     */
    uint32_t    m_rxedFbmCount;

     /**
      * TODO: Manage max FBM seq for multiple feedback senders(media receiver)
      *
     int32_t     m_maxFbmSeq;
     int32_t     m_wrapCount;
      */

    float       m_lossRate;         /* 0 ~ 1 */
    float       m_ecnRate;          /* 0 ~ 1 */



    typedef std::map<uint16_t /* sequence */, SentRtpRecord> SentRtpStream;
    std::map<uint32_t /* SSRC */, SentRtpStream >           m_sentRtpSsrcMap;


    std::map<uint32_t /* SSRC */, uint16_t /* sequence */>  m_lastSentRtpMap;


    /* Only for debug */
    ns3::Ptr<ns3::UtilNQMonitor> m_nqMonitor;
    uint32_t                   m_netDataRate;
    uint32_t                   m_inflightPktCount;



    /* FwdBw Estimation: Available BW by ME */
    float       m_estFwdBwBps;
    float       m_lastReceivedBps;

    /* Bitrate Fraction */
    std::deque< BrFractionData > m_brFractionWindow;
    uint64_t    m_totSentBytes;
    uint64_t    m_totRcvdBytes;


    /* QDelay Estimation */
    std::deque< std::pair<int32_t /*qdelay*/, uint64_t /*txedTimeMs*/> > m_qdelayWindow;
    int32_t     m_minQDelay;            /* base delay */
    int32_t     m_lastQDelay;
    uint32_t    m_minQPktSize;
    int32_t     m_targetQDelay;
    float       m_maXQDelay;          /* Average of QDelay by cross traffic */


    /* Internal Virtual Q */
    int64_t     m_ivqMinQptUs;
    int64_t     m_ivqQptUs;
    uint64_t    m_ivqStUs;
    int64_t     m_ivqDelayUs;

    /* QDelay Increase Detector */
    int32_t m_incrCount;
    int32_t m_incrLastMinQDelay;
    uint64_t m_incrStartTime;



    /* Send Control */
    SenderStatus m_status;

    float    m_targetSendBps;

    uint64_t m_probingStartTime;


    float   m_throEstFwdBwBps;

    uint64_t m_lastCompeteTime;


    uint64_t m_ccfsStartTime;
};

}

#endif /* CCFS_CONTROLLER_H */
