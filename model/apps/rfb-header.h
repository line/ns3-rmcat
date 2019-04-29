/******************************************************************************
 * Copyright 2016-2018 Cisco Systems, Inc.                                    *
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
 * Header interface of RTP Feedback (no related standard)
 *
 * @author Jungnam Gwock
 */

#ifndef RFB_HEADER_H
#define RFB_HEADER_H

#include "ns3/header.h"
#include "ns3/type-id.h"
#include "rtp-header.h"
#include <map>
#include <set>

namespace ns3 {

//------------------- RTP FEEDBACK HEADER -------------------------//
//   0                   1                   2                   3
//   0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |V=2|P| FMT=RFB | PT=RTPFB=205  |          length               |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |                 SSRC of RTCP packet sender                    |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |                           SSRC = 0                            |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |                  Synced Report Time(ms)                       |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |         fb_seq                |   Monitored time (ms)         |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |                   SSRC of 1st RTP Stream                      |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |          count                |             end_seq           |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |L|ECN| Arrival time offset(ms) | ...                           .
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  .                                                               .
//  .                                                               .
//  .                                                               .
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |                   SSRC of nth RTP Stream                      |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |          count                |             end_seq           |
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//  |L|ECN| Arrival time offset(ms) | ...                           |
//  .                                                               .
//  .                                                               .
//  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
class RfbHeader : public RtcpHeader
{
public:
    class MetricBlock
    {
    public:
        uint8_t m_ecn;
        uint64_t m_timestampMs;
    };

    enum AddResult{
        RFB_ADD_SRC,      /**< Feedback was added as new ssrc */
        RFB_ADD_ONE,      /**< Feedback was added into existing ssrc */
        RFB_ADD_DUPLICATE, /**< Feedback of duplicate packet */
        RFB_ADD_FAIL_BAD_ECN,   /**< ECN value takes more than two bits */
        RFB_ADD_FAIL_TOO_LONG,  /**< Adding this sequence number would make the packet too long */
    };
    typedef std::map<uint16_t /* sequence */, MetricBlock> ReportBlock_t;

    RfbHeader ();
    virtual ~RfbHeader ();

    static ns3::TypeId GetTypeId ();
    virtual ns3::TypeId GetInstanceTypeId () const;
    virtual uint32_t GetSerializedSize () const;
    virtual void Serialize (ns3::Buffer::Iterator start) const;
    virtual uint32_t Deserialize (ns3::Buffer::Iterator start);
    virtual void Print (std::ostream& os) const;

    AddResult AddFeedback (uint32_t ssrc, uint16_t seq, uint64_t currentTimeMs, uint8_t ecn=0);
    void GetSsrcList (std::set<uint32_t>& rv) const;

    bool GetMetricList (uint32_t ssrc, std::vector<std::pair<uint16_t, MetricBlock> >& rv) const;
    bool GetMetricBlock (uint32_t ssrc, uint16_t seq,  MetricBlock& mb) const;

    void IncreaseFbSeq();

    void     CleanReportBlocks();
    void     SetReportTime(uint64_t currentTimeMs);
    void     SetMonitoredTime(uint16_t duration);

    uint32_t GetTotalReportCount();
    uint32_t GetReportCountInSsrc(uint32_t ssrc);
    uint16_t GetEndSeqInSsrc(uint32_t ssrc);
    uint32_t GetReportTime();
    uint16_t GetFbSeq();
    uint16_t GetMonitoredTime();

    void CalcCountEndSeq(uint32_t ssrc, const ReportBlock_t& rb);
    std::pair<uint16_t, uint16_t> GetCountEndSeq(uint32_t ssrc) const;

    uint64_t AtoToTs (uint16_t ato) const;
    uint16_t TsToAto (uint64_t tsUs) const;

protected:
    bool Update();
    std::map<uint32_t /* SSRC */, ReportBlock_t> m_reportBlocks;
    std::map<uint32_t /* SSRC */, std::pair<uint16_t /* count */, uint16_t /*end_seq */> > m_countEndSeq;
    uint64_t m_reportTimeMs;

    uint16_t m_fbSeq;
    uint16_t m_monitoredMs;
    uint16_t m_wrapCount;
};

}

#endif /* RFB_HEADER_H */
