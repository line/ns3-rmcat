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
 * Receiver application implementation for rmcat ns3 module.
 *
 * @author Jungnam Gwock
 *
 */

#include "rmcat-ccfs-receiver.h"
#include "rtp-header.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/packet.h"
#include "ns3/simulator.h"
#include "ns3/log.h"

NS_LOG_COMPONENT_DEFINE ("RmcatCcfsReceiver");

namespace ns3 {
RmcatCcfsReceiver::RmcatCcfsReceiver ()
  : m_fbPeriodMs(100)
  , m_fbEvent{}
  , m_fbHeader{}
  , m_refPointUs(0)
  , m_nqMonitor(NULL)
{}

RmcatCcfsReceiver::~RmcatCcfsReceiver () {}


void RmcatCcfsReceiver::StartApplication ()
{
    NS_LOG_INFO("Start Application. FB period(ms)=" 
            << m_fbPeriodMs );

    RmcatReceiver::StartApplication();

}

void RmcatCcfsReceiver::StopApplication ()
{
    NS_LOG_INFO("Stop Application");

    Simulator::Cancel (m_fbEvent);

    RmcatReceiver::StopApplication();
}

void RmcatCcfsReceiver::RecvPacket (Ptr<Socket> socket)
{
    Address remoteAddr{};
    auto packet = m_socket->RecvFrom(remoteAddr);
    NS_ASSERT(packet);

    if(m_refPointUs == 0) {
        m_refPointUs = Simulator::Now ().GetMicroSeconds ();
        m_fbEvent = Simulator::Schedule(MilliSeconds(m_fbPeriodMs), &RmcatCcfsReceiver::FbTimerHandler, this);
        NS_LOG_INFO("SyncTime for RX:localTimestampUs=" << m_refPointUs);
    }

    auto srcIp = InetSocketAddress::ConvertFrom (remoteAddr).GetIpv4 ();
    auto srcPort = InetSocketAddress::ConvertFrom (remoteAddr).GetPort ();

    RtpHeader header{};
    packet->RemoveHeader(header);

    if (m_waiting) {
        m_waiting = false;
        m_remoteSsrc = header.GetSsrc ();
        m_srcIp = srcIp;
        m_srcPort = srcPort;
        m_fbHeader.SetSendSsrc(m_ssrc);
    } 
    else {
        // Only one flow supported
        NS_ASSERT (m_remoteSsrc == header.GetSsrc ());
        NS_ASSERT (m_srcIp == srcIp);
        NS_ASSERT (m_srcPort == srcPort);
    }

    auto recvTimestampMs = GetCurrElapsedTimeMs(); 
    auto res = m_fbHeader.AddFeedback (m_remoteSsrc, 
                                        header.GetSequence(), 
                                        recvTimestampMs);

    auto countEndseq = m_fbHeader.GetCountEndSeq(m_remoteSsrc);

    if(m_nqMonitor) {
        m_nqMonitor->AddSnapshot(m_remoteSsrc, countEndseq.second);
    }

    NS_LOG_INFO("[" << m_remoteSsrc << "] AddFeedback (count,seq)=("
            << countEndseq.first << ", " << countEndseq.second
            << ") result=" << res
            << " Size=" << packet->GetSize()
            << " SyncedTime=" << recvTimestampMs);

}


void RmcatCcfsReceiver::FbTimerHandler()
{
    /// Send feedback 

    std::stringstream ss;

    m_fbHeader.IncreaseFbSeq();
    m_fbHeader.SetReportTime( GetCurrElapsedTimeMs() );
    m_fbHeader.SetMonitoredTime( uint16_t(m_fbPeriodMs) );

    m_fbHeader.Print(ss);

    NS_LOG_INFO("FB period(ms)="<< m_fbPeriodMs << "\n" << ss.rdbuf() );

    auto packet = Create<Packet> ();
    packet->AddHeader(m_fbHeader);
    m_socket->SendTo(packet, 0, InetSocketAddress{m_srcIp, m_srcPort});

    m_fbEvent = Simulator::Schedule(MilliSeconds(m_fbPeriodMs), &RmcatCcfsReceiver::FbTimerHandler, this);
    m_fbHeader.CleanReportBlocks();
}

uint64_t RmcatCcfsReceiver::GetCurrElapsedTimeMs()
{
    uint64_t currUs = Simulator::Now ().GetMicroSeconds ();
    
    return (currUs - m_refPointUs)/1000;
}
void RmcatCcfsReceiver::SetNQMonitor(ns3::Ptr<ns3::UtilNQMonitor> monitor)
{
    m_nqMonitor = monitor;
    NS_LOG_INFO("Set NQMonitor: " << monitor);
}

}

