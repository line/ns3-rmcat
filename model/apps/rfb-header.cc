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

#include "rtp-header.h"
#include "rfb-header.h"
#include "ns3/log.h"

NS_LOG_COMPONENT_DEFINE ("RfbHeader");

namespace ns3 {

NS_OBJECT_ENSURE_REGISTERED (RfbHeader);


RfbHeader::RfbHeader()
: RtcpHeader{RTP_FB, RTCP_RTPFB_RFB}
, m_reportBlocks{}
, m_countEndSeq{}
, m_reportTimeMs{0}
, m_fbSeq(0)
, m_monitoredMs(0)
, m_wrapCount(0)
{
    ++m_length; // report timestamp field
    ++m_length; // fb_seq & Monitored time
}

RfbHeader::~RfbHeader () {}

TypeId RfbHeader::GetTypeId ()
{
    static TypeId tid = TypeId ("RfbHeader")
      .SetParent<RtcpHeader> ()
      .AddConstructor<RfbHeader> ()
    ;
    return tid;
}

TypeId RfbHeader::GetInstanceTypeId () const
{
    return GetTypeId ();
}


RfbHeader::AddResult
RfbHeader::AddFeedback (uint32_t ssrc, uint16_t seq, uint64_t currentTimeMs, uint8_t ecn)
{
    if (ecn > 0x03) {
        return RFB_ADD_FAIL_BAD_ECN;
    }

    MetricBlock newmb = { ecn, currentTimeMs };
    AddResult res;

    if(m_reportBlocks.find(ssrc) == m_reportBlocks.end()) 
    {
        ReportBlock_t rbmap;

        rbmap[seq] = newmb;

        m_reportBlocks[ssrc] = rbmap;

        res = RFB_ADD_SRC;

    }
    else 
    {
        auto& rbmap = m_reportBlocks[ssrc];
        if (rbmap.find (seq) != rbmap.end ()) 
        {
            return RFB_ADD_DUPLICATE;
        }

        rbmap[seq] = newmb;
        res = RFB_ADD_ONE;
    }

    Update();

    return res;

}

void RfbHeader::GetSsrcList (std::set<uint32_t>& rv) const
{
    rv.clear ();
    for (const auto& rb : m_reportBlocks) {
        rv.insert (rb.first);
    }
}

void RfbHeader::CleanReportBlocks()
{
    m_reportBlocks.clear();
    m_countEndSeq.clear();

    Update();
    NS_LOG_INFO("m_length after clean=" << m_length);

}
void RfbHeader::IncreaseFbSeq()
{
    m_fbSeq++;
}

uint32_t RfbHeader::GetTotalReportCount()
{
    NS_ASSERT (m_length >= 3); 
    uint32_t count = 0;
    for (const auto& rb : m_reportBlocks) 
    {
        count += GetCountEndSeq(rb.first).first;
    }

    return count;
}
uint32_t RfbHeader::GetReportCountInSsrc(uint32_t ssrc)
{
    return GetCountEndSeq(ssrc).first;
}

uint16_t RfbHeader::GetEndSeqInSsrc(uint32_t ssrc)
{
    return GetCountEndSeq(ssrc).second;
}

void RfbHeader::SetReportTime(uint64_t currentTimeMs)
{
    m_reportTimeMs = currentTimeMs;
}
void RfbHeader::SetMonitoredTime(uint16_t duration)
{
    m_monitoredMs = duration;
}

uint32_t RfbHeader::GetReportTime()
{
    return m_reportTimeMs;
}

uint16_t RfbHeader::GetFbSeq()
{
    return m_fbSeq;
}
uint16_t RfbHeader::GetMonitoredTime()
{
    return m_monitoredMs;
}

bool RfbHeader::GetMetricBlock(uint32_t ssrc, uint16_t seq, ns3::RfbHeader::MetricBlock &mb) const
{
    const auto it = m_reportBlocks.find (ssrc);

    if (it == m_reportBlocks.end ()) {
        return false;
    }


    const auto& rb = it->second;

    if(rb.empty ())
        return false;

    const auto& mb_it = rb.find (seq);

    if(mb_it == rb.end())
        return false;

    mb = mb_it->second;
    return true;
}
bool RfbHeader::GetMetricList (uint32_t ssrc,
                                      std::vector<std::pair<uint16_t, MetricBlock> >& rv) const
{
    const auto it = m_reportBlocks.find (ssrc);
    if (it == m_reportBlocks.end ()) {
        return true;
    }
    rv.clear ();

    const auto& rb = it->second;

    if(rb.empty ())
        return true;


    auto countEnd = GetCountEndSeq(ssrc);
    const uint16_t count = countEnd.first;
    const uint16_t endSeq = countEnd.second;
    const uint16_t beginSeq = endSeq - count + 1;

    for (uint16_t i = 0; i < count; i++)
    {
        auto seq = beginSeq + i;

        const auto& mb_it = rb.find (seq);
        const bool received = (mb_it != rb.end ());
        if (received) {
            const auto item = std::make_pair (seq, mb_it->second);
            rv.push_back (item);
        }
    }

    return true;
}

uint32_t RfbHeader::GetSerializedSize () const {
    NS_ASSERT (m_length >= 3);
    return 4*(m_length + 1);
}

void RfbHeader::Serialize (Buffer::Iterator start) const
{
    NS_ASSERT (m_length >= 3); 
    RtcpHeader::SerializeCommon (start);

    uint32_t  hdr_ssrc = 0;
    start.WriteHtonU32 (hdr_ssrc);
    start.WriteHtonU32 (uint32_t(m_reportTimeMs));
    start.WriteHtonU16 (m_fbSeq);
    start.WriteHtonU16 (m_monitoredMs);

    for (const auto& rb : m_reportBlocks) 
    {
        start.WriteHtonU32 (rb.first);

        auto countStop = GetCountEndSeq(rb.first);
        const uint16_t count = countStop.first;
        const uint16_t endSeq = countStop.second;
        start.WriteHtonU16 (count);
        start.WriteHtonU16 (endSeq);

        const uint16_t beginSeq = endSeq - count + 1;
        uint16_t seq;
        for (uint16_t i = 0; i < count; ++i) 
        {
            seq = i + beginSeq;
            const auto& mb_it = rb.second.find (seq);
            uint8_t octet1 = 0;
            uint8_t octet2 = 0;
            const bool received = (mb_it != rb.second.end ());
            RtpHdrSetBit (octet1, 7, received);
            if (received) 
            {
                const auto& mb = mb_it->second;
                NS_ASSERT (mb.m_ecn <= 0x03);
                octet1 |= uint8_t ((mb.m_ecn & 0x03) << 5);
                const uint16_t ato = TsToAto (mb.m_timestampMs);
                NS_ASSERT (ato <= 0x1fff);
                octet1 |= uint8_t (ato >> 8);
                octet2 |= uint8_t (ato & 0xff);
            }
            start.WriteU8 (octet1);
            start.WriteU8 (octet2);
        }
        if (uint16_t (endSeq - beginSeq + 1) % 2 == 1) {
            start.WriteHtonU16 (0); //padding
        }
    }
}

uint32_t RfbHeader::Deserialize (Buffer::Iterator start)
{
    NS_ASSERT (m_length >= 3);

    (void) RtcpHeader::DeserializeCommon (start);
    NS_ASSERT (m_packetType == RTP_FB);
    NS_ASSERT (m_typeOrCnt == RTCP_RTPFB_RFB);


    uint32_t hdr_ssrc = start.ReadNtohU32();
    NS_ASSERT (hdr_ssrc == 0);

    m_reportTimeMs = start.ReadNtohU32();
    m_fbSeq = start.ReadNtohU16();
    m_monitoredMs = start.ReadNtohU16();


    NS_LOG_INFO("hdr_ssrc = " << hdr_ssrc 
            << ", reportTime=" << m_reportTimeMs 
            << ", seq=" << m_fbSeq 
            << ", monitredMs=" << m_monitoredMs);

    size_t len_left = (4*(m_length + 1)) - 20;

    while (len_left > 0) 
    {
        NS_ASSERT (len_left >= 8); // SSRC + count & end
        const auto ssrc = start.ReadNtohU32 ();
        const uint16_t count = start.ReadNtohU16 ();

        NS_ASSERT (count <= 0xffff);// length of 65536 not supported

        const uint32_t padding = count % 2;

        const uint16_t endSeq = start.ReadNtohU16 ();

        len_left -= 8;

        if(count == 0) {
            continue;
        }
        else {

            uint16_t beginSeq = endSeq - count + 1;

            for(auto i = 0; i < count; i++)
            {
                uint16_t seq = beginSeq + i;

                const auto octet1 = start.ReadU8 ();
                const auto octet2 = start.ReadU8 ();

                if (RtpHdrGetBit (octet1, 7)) {
                    const uint8_t ecn = (octet1 >> 5) & 0x03;
                    uint16_t ato = (uint16_t (octet1) << 8) & 0x1f00;
                    ato |= uint16_t (octet2);

                    AddFeedback(ssrc, seq, AtoToTs(ato), ecn);
                }

            }
            len_left -= count * 2;


            if(padding)
                len_left -= 2;
        }
    }

    return GetSerializedSize ();
}

void RfbHeader::Print (std::ostream& os) const
{
    NS_ASSERT (m_length >= 3);
    RtcpHeader::PrintN (os);
    os << "\nReport timestamp = " << m_reportTimeMs << std::endl;
    os << "Feedback seq = " << m_fbSeq << ", Monitored ms = " << m_monitoredMs << std::endl;

    size_t i = 0;
    for (const auto& rb : m_reportBlocks) {
        auto countEnd = GetCountEndSeq(rb.first);
        const uint16_t count = countEnd.first;
        const uint16_t endSeq = countEnd.second;
        os << "Report block #" << i << " = "
           << "{ SSRC = " << rb.first
           << " ,count=" << count << ", end_seq=" << endSeq << " --> \n";

        const uint16_t beginsSeq = endSeq - count + 1;

        for (uint16_t j = 0; j < count; ++j) 
        {
            uint16_t seq = beginsSeq + j;

            const auto& mbit = rb.second.find (seq);
            const bool received = (mbit != rb.second.end ());
            os << "<" << seq << ":L=" << int(received);
            if (received) {
                const auto& mb = mbit->second;
                os << ", ECN=" << int(mb.m_ecn)
                   << ", ATO=" << int(TsToAto (mb.m_timestampMs));
            }
            os << ">,";



            if(j % 2 == 1)
                os << "\n";
        }
        os << "}, \n";
        ++i;
    }
}

// first: count
// second: end_seq
void RfbHeader::CalcCountEndSeq(uint32_t ssrc, const ReportBlock_t& rb)
{
    if(rb.empty()) {
        m_countEndSeq[ssrc] = std::make_pair (0, 0);
        return;
    }
    if (rb.size () == 1) {
        m_countEndSeq[ssrc] = std::make_pair (1, rb.begin()->first);
        return;
    }

#define UPDATE_LO(_var_, _val_) if(_var_ > _val_) _var_ = _val_ ;
#define UPDATE_HI(_var_, _val_)  if(_var_ < _val_) _var_ = _val_ ;

    int32_t front_lo = 65536, front_hi = -1, front_count = 0,
            rear_lo = 65536, rear_hi = -1, rear_count = 0;

    auto mbit = rb.begin ();
    for (; mbit != rb.end (); ++mbit) {

        if(mbit->first <= 65536/2) 
        {
            UPDATE_LO (front_lo, mbit->first);
            UPDATE_HI (front_hi, mbit->first);
            front_count++;
        }
        else 
        {
            UPDATE_LO (rear_lo, mbit->first);
            UPDATE_HI (rear_hi, mbit->first);
            rear_count++;
        }

    }

    int32_t hi = 0, lo = 0;

    if(front_count == 0 && rear_count == 0) {
        NS_LOG_INFO("Error: Cannot calculate hi/lo, "
                "(front_lo, front_hi)=("
                << front_lo << ", " << front_hi 
                << "), (rear_lo, rear_hi)=" 
                << rear_lo << ", " << rear_hi
                << ")");

        m_countEndSeq[ssrc] = std::make_pair (0, 0);
        return;
    }
    else if(front_count == 0 && rear_count) {
        hi = rear_hi;
        lo = rear_lo;
    }
    else if(front_count && rear_count == 0) {
        hi = front_hi;
        lo = front_lo;
    }
    else {
        if(front_lo < 100) {
            hi = front_hi + 65536;
            lo = rear_lo;
            NS_LOG_INFO("Wrap around: front_count=" << front_count 
                    << ", (front_lo, front_hi)=("
                    << front_lo << ", " << front_hi 
                    << "), rear_count=" << rear_count
                    << ", (rear_lo, rear_hi)=(" << rear_lo << ", " << rear_hi 
                    << ")");
        }
        else {
            hi = rear_hi;
            lo = front_lo;
            NS_LOG_INFO("Passing harf: front_count=" << front_count 
                    << ", (front_lo, front_hi)=("
                    << front_lo << ", " << front_hi 
                    << "), rear_count=" << rear_count
                    << ", (rear_lo, rear_hi)=(" << rear_lo << ", " << rear_hi 
                    << ")");
        }
        
    }


    m_countEndSeq[ssrc] = std::make_pair (hi - lo + 1, uint16_t(hi));
}

std::pair<uint16_t, uint16_t> 
RfbHeader::GetCountEndSeq(uint32_t ssrc) const
{
    if(m_countEndSeq.size() == 0 || 
            m_countEndSeq.find(ssrc) == m_countEndSeq.end())
        return std::make_pair( 0, 0 );

    auto itr = m_countEndSeq.find(ssrc);
    return itr->second;
}
bool RfbHeader::Update()
{
    size_t len = 20;

    for (const auto& rb : m_reportBlocks) 
    {
        len += 8; // SSRC, count, end_seq

        CalcCountEndSeq(rb.first, rb.second);
        auto countEnd = GetCountEndSeq(rb.first);
        const uint16_t count = countEnd.first;
        const uint16_t padding = count % 2;

        len += (count + padding) * 2;
    }

    if (len > 0xffff) {
        return false;
    }
    m_length = (len-4)/4;

    return true;
}

uint16_t RfbHeader::TsToAto (uint64_t tsMs) const
{
    NS_ASSERT (tsMs <= m_reportTimeMs);
    return uint16_t (m_reportTimeMs - tsMs);
}

uint64_t RfbHeader::AtoToTs (uint16_t ato) const
{
    NS_ASSERT (ato <= m_reportTimeMs);
    return uint64_t (m_reportTimeMs - ato);
}

}
