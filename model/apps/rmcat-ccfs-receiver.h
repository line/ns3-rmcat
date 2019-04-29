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
 * Receiver application interface for vcc receiver
 *
 * @author Jungnam gwock
 */

#ifndef RMCAT_CCFS_RECEIVER_H
#define RMCAT_CCFS_RECEIVER_H

#include "rmcat-receiver.h"
#include "rfb-header.h"
#include "ns3/rmcat-utils.h"

namespace ns3 {

class RmcatCcfsReceiver: public RmcatReceiver
{
public:
    RmcatCcfsReceiver ();
    virtual ~RmcatCcfsReceiver ();

    void SetNQMonitor(ns3::Ptr<ns3::UtilNQMonitor> monitor);

private:
    virtual void StartApplication ();
    virtual void StopApplication ();

    virtual void RecvPacket (Ptr<Socket> socket);

    void FbTimerHandler();

    uint64_t GetCurrElapsedTimeMs();

private:
    uint32_t    m_fbPeriodMs;
    EventId     m_fbEvent;
    RfbHeader   m_fbHeader;
    uint64_t    m_refPointUs;
    ns3::Ptr<ns3::UtilNQMonitor> m_nqMonitor;
};

}

#endif /* RMCAT_CCFS_RECEIVER_H */
