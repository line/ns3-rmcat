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

#ifndef RMCAT_UTILS_H
#define RMCAT_UTILS_H

#include "ns3/object.h"
#include "ns3/queue.h"

#include <map>
#include <set>
#include <deque>

namespace ns3 {


std::string utilConvertKbps(uint32_t Bps);

double utilGetCorrelationCoef(std::vector<int64_t> &x, std::vector<int64_t> &y);


class UtilNQMonitor : public Object
{
public:
    UtilNQMonitor(ns3::Ptr<ns3::Queue> queue, uint32_t dataRate);
    virtual ~UtilNQMonitor();

    class Entry {
    public:
        uint64_t timeUs;
        uint16_t seq;
        uint32_t remainPkts;
        uint32_t remainBytes;
        uint64_t delayUs;

    };

    void AddSnapshot(uint32_t ssrc, uint16_t seq);

    void DelOlds(uint64_t timeUs);

    Entry GetEntry(uint32_t ssrc, uint16_t seq);

private:


    typedef std::deque<Entry> QEntries;
    std::map<uint32_t /* SSRC */, QEntries> m_mapSsrc;

    ns3::Ptr<ns3::Queue>       m_netQueue;
    uint32_t                   m_dataRate;

};

}

#endif /* RMCAT_CCFS_UTILS_H */
