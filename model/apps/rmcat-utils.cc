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
 *
 * @author Jungnam Gwock
 */

#include <iostream>
#include "rmcat-utils.h"
#include "ns3/simulator.h"
#include "ns3/queue.h"
#include "ns3/log.h"

#include <list>

NS_LOG_COMPONENT_DEFINE ("UtilNQMonitor");

namespace ns3 {

/// NS_OBJECT_ENSURE_REGISTERED (UtilNQMonitor);

std::string utilConvertKbps(uint32_t Bps)
{
    char buff[245];
    snprintf(buff, sizeof(buff), "%dkbps", Bps * 8 /1000);
    return std::string(buff, strlen(buff));
}

/*
 *                                  NxSUM(XY) - SUM(X)SUM(Y)
 * coef= ---------------------------------------------------------------------
 *        ROOT([NxSUM(X^2) - (SUM(X)SUM(X))^2][NxSUM(Y^2) - (SUM(Y)SUM(Y))^2])
 */
double utilGetCorrelationCoef(std::vector<int64_t> &x, std::vector<int64_t> &y)
{
    int64_t sumX = 0, sumY = 0, sumXY = 0;
    int64_t squareSumX = 0, squareSumY = 0;
    int64_t sz = x.size();

    for (int i = 0; i < sz; ++i)
    {
        sumX += x[i];
        sumY += y[i];
        sumXY += x[i] * y[i];
        squareSumX += x[i] * x[i];
        squareSumY += y[i] * y[i];
    }



    double coef = (double)(sz * sumXY - sumX * sumY)
                 / sqrt((sz * squareSumX - sumX * sumX)
                        * (sz * squareSumY - sumY * sumY));

    return coef;
}



UtilNQMonitor::UtilNQMonitor(ns3::Ptr<ns3::Queue> queue, uint32_t dataRate) :
    m_netQueue(queue),
    m_dataRate(dataRate/8)
{
    NS_LOG_INFO("INIT: NQ=" << m_netQueue << " DataRate=" << utilConvertKbps(m_dataRate));

}

UtilNQMonitor::~UtilNQMonitor() { }

void UtilNQMonitor::AddSnapshot(uint32_t ssrc, uint16_t seq)
{
    Entry entry;
    entry.timeUs = Simulator::Now ().GetMicroSeconds ();
    entry.seq = seq;
    entry.remainPkts = m_netQueue->GetNPackets();
    entry.remainBytes = m_netQueue->GetNBytes();
    entry.delayUs = uint64_t(entry.remainBytes) * 1000 * 1000 / uint64_t(m_dataRate);


    NS_LOG_INFO("delayUs=" << entry.delayUs << " ms=" << entry.delayUs/1000 << " bytes=" << entry.remainBytes << " dataRate=" << m_dataRate);


    if(m_mapSsrc.find(ssrc) == m_mapSsrc.end())
    {
        QEntries qentr;

        qentr.push_back(entry);
        m_mapSsrc[ssrc] = qentr;
    }
    else
    {
        auto& qentr = m_mapSsrc[ssrc];
        qentr.push_back(entry);
    }

}

void UtilNQMonitor::DelOlds(uint64_t timeUs)
{
    for(auto& map_entrs : m_mapSsrc)
    {
        while(map_entrs.second.size() > 0) {
            if(map_entrs.second.front().timeUs <= timeUs) {
                map_entrs.second.pop_front();
                    break;
            }
            else {
                break;
            }

        }
    }

}

UtilNQMonitor::Entry UtilNQMonitor::GetEntry(uint32_t ssrc, uint16_t seq)
{
    Entry ret = { 0 };
    if(m_mapSsrc.find(ssrc) == m_mapSsrc.end())
    {
       return ret;
    }


    auto &qentr = m_mapSsrc[ssrc];

    QEntries::iterator itr;
    for(itr = qentr.begin(); itr != qentr.end(); ++itr) {
        if(itr->seq == seq) {
            ret = (*itr);
            break;
        }

    }

    return ret;
}

}
