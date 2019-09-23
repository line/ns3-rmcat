*This repository is extended from* `https://github.com/cisco/ns3-rmcat <https://github.com/cisco/ns3-rmcat>`_ *to evaluate CCFS.*


.. contents::

ns3-rmcat Documentation
----------------------------

.. heading hierarchy:
   ------------- Chapter
   ************* Section (#.#)
   ============= Subsection (#.#.#)
   ############# Paragraph (no number)

ns3-rmcat is an `ns3 <https://www.nsnam.org/release/ns-allinone-3.26.tar.bz2>`_ module (Currently ns-3.26 is supported) used for `IETF RMCAT <https://datatracker.ietf.org/wg/rmcat/charter/>`_ candidate algorithm testing.

ns3-rmcat can be used in one of the following ways:

1. As a complement to unit testing and integration testing in the emulator environment

2. When tuning algorithms. ns3 offers easier support for different topologies as it provides sophisticated LTE and WiFi modules. This allows testers to understand internal workings in detail.

3. For comparing algorithms. Simulation testbed is a more reproducible environment than real world test and emulator testbed. It can be easily switched between multiple candidate algorithms, thus making side-by-side comparison of different algorithms an easy and fair task.


Model
*****************

The model for real-time media congestion is documented at `"Framework for Real-time Media Congestion Avoidance Techniques" <https://tools.ietf.org/html/draft-zhu-rmcat-framework-00>`_, and ns3-rmcat is a simplified implementation, focusing on the evaluation of real-time media congestion control algorithms in a simulation environment.

ns3-rmcat defines ns3 applications (see `model/apps <model/apps>`_) running on different network topologies. These ns3 applications send fake video codec data (`model/syncodecs <model/syncodecs>`_, more on `syncodecs <https://github.com/cisco/syncodecs>`_) as instructed by the congestion control algorithm under test.

The sender application, ``RmcatSender``, sends fake video codec data in media packets to the receiver application, ``RmcatReceiver``. ``RmcatReceiver`` receives a series of packets in order, generates timestamps on reception, and sends them back to ``RmcatSender`` in feedback packets. The (sender-based) congestion control algorithm running on ``RmcatSender`` processes the feedback information (see `model/congestion-control <model/congestion-control>`_), to get bandwidth estimation. The sender application then uses this bandwidth estimation to control the fake video encoder by adjusting its target video bitrate.

You can find the topologies currently supported here (see `model/topo <model/topo>`_). For now only point-to-point wired topology and WIFI topology are provided. We will add LTE support later.

Test cases
*****************

The test cases are in `test/rmcat-wired-test-suite <test/rmcat-wired-test-suite.cc>`_ and `test/rmcat-wifi-test-suite <test/rmcat-wifi-test-suite.cc>`_, currently organized in the three test scenarios below. The test case name is a combination of the test scenario and the congestion control algorithm name. 

  - `rmcat-wifi <https://datatracker.ietf.org/doc/draft-ietf-rmcat-eval-test/?include_text=1>`_

  - `rmcat-wired <https://datatracker.ietf.org/doc/draft-fu-rmcat-wifi-test-case/?include_text=1>`_

  - rmcat-wired-vparam, which is based on some of the wired test cases, but with different parameters such as bottleneck bandwidth, propagation delay, etc.

`LTE <https://datatracker.ietf.org/doc/draft-ietf-rmcat-wireless-tests/?include_text=1>`_ test cases are not implemented yet.

The list of available congestion control algorithm names is provided as follows:

  - `nada <https://datatracker.ietf.org/doc/draft-gwock-rmcat-ccfs/>`_

  - `ccfs <https://datatracker.ietf.org/doc/draft-ietf-rmcat-nada/>`_

Therefore, the number of available test suites is 6, ``3 test suites`` x ``2 congestion control algorithms``.

Naming convention for the test suite:

::

    # $(test-scenario-name)-$(algorithm-name)

For example, ``rmcat-wired-ccfs`` will test the rmcat-wired scenario with the ccfs algorithm.

Examples
*****************

`Examples <examples>`_ are provided as an application template for experimenting new test cases and algorithm changes.

Write your own congestion control algorithm
***************************************************

You can create your own congestion control algorithm by inheriting from  `SenderBasedController <model/congestion-control/sender-based-controller.h#L85>`_. `DummyController <model/congestion-control/dummy-controller.h#L39>`_ is an example which just prints packet loss, queuing delay and receiving rate without doing any congestion control: the bandwidth estimation is hard-coded.

To reuse the plotting tool, the following logs are expected (see `NadaController <model/congestion-control/nada-controller.cc>`_, `process_test_logs.py <tools/process_test_logs.py>`_):

::

    # rmcat flow 0, this is the flow id, SenderBasedController::m_id
    # ts, current timestamp when receving the rmcat feedback in millionseconds
    # loglen, packet history size, SenderBasedController::m_packetHistory.size()
    # qdel, queuing delay, SenderBasedController::getCurrentQdelay()
    # rtt, round trip time, SenderBasedController::getCurrentRTT()
    # ploss, packet loss count in last 500 ms, SenderBasedController::getPktLossInfo()
    # plr, packet loss ratio, SenderBasedController::getPktLossInfo()
    # xcurr, aggregated congestion signal that accounts for queuing delay, ECN
    # rrate, current receive rate in bps, SenderBasedController::getCurrentRecvRate()
    # srate, current estimated available bandwidth in bps
    # avgint, average inter-loss interval in packets, SenderBasedController::getLossIntervalInfo()
    # curint, most recent (currently growing) inter-loss interval in packets, SenderBasedController::getLossIntervalInfo()
    
    rmcat_0 ts: 158114 loglen: 60 qdel: 286 rtt: 386 ploss: 0 plr: 0.00 xcurr: 4.72 rrate: 863655.56 srate: 916165.81 avgint: 437.10 curint: 997


Usage
*****************


Build NS3 with ns3-rmcat
===========================

1. Download ns3 (Currently ns-3.26 is supported. Other versions may also work but untested)

2. Git clone ns3-rmcat into ``ns-3.xx/src``. Initialize syncodecs submodule (``git submodule update --init --recursive``)

3. Configure the workspace, ``CXXFLAGS="-std=c++11 -Wall -Werror -Wno-potentially-evaluated-expression -Wno-unused-local-typedefs" ./waf configure --enable-examples --enable-tests``

4. Run ``./waf build``

Run test suites
===================
1. Run test suites by running the ``./test.py`` script. When a test suite is completed, some log files are generated, which will be used to generate plots.

``./test.py -s rmcat-wired-ccfs -w rmcat.html -r``, where ``rmcat.html`` is the test report.

2. Create a new directory and move all temporary files created by test suites into the new directory.

3. Create plots. System requires the installation of the python module `matplotlib <https://matplotlib.org/>`_.

``python src/ns3-rmcat/tools/process_test_logs.py $(algorithm-name) $(specific-directory-name)``

``python src/ns3-rmcat/tools/plot_tests.py $(algorithm-name) $(specific-directory-name)``


Use test.chs
=============
You can also use `test.csh <tools/test.csh>`_ to run test suites and plot scripts in one go. 
When you run `test.csh`, the log files with testcase names will be created in the "testpy-output/[CURRENT UTC TIME]" directory unless otherwise specified.

``./src/ns3-rmcat/tools/test.csh $(algorithm-name) $(test-scenario-name-without-rmcat)``

::

    # Example: png files will be located in the "testpy-output/[CURRENT UTC TIME]"
    # ./src/ns3-rmcat/tools/test.csh ccfs wired
    # ./src/ns3-rmcat/tools/test.csh nada vparam 
    # ./src/ns3-rmcat/tools/test.csh some wifi 

You can run `test.csh` with a specific output directory.

``./src/ns3-rmcat/tools/test.csh $(algorithm-name) $(access-network-name-without-rmcat) $(specific-directory-name)``

::

    # Example: png files will be located in a specific directory
    # ./src/ns3-rmcat/tools/test.csh ccfs wired testout-rmcat-wired
    # ./src/ns3-rmcat/tools/test.csh nada vparam testout-rmcat-wired-vparam
    # ./src/ns3-rmcat/tools/test.csh some wifi testout-rmcat-wifi


Run rmcat-example
=====================
For a simple logic check, you can also use the rmcat-example.

``./waf --run "rmcat-example --log"``, ``--log`` will turn on RmcatSender/RmcatReceiver logs for debugging.

rmcat-example for CCFS is documented here : `RMCAT-EXMAPLE-CCFS.md <RMCAT-EXAMPLE-CCFS.md>`_


Troubleshooting
*****************

To debug "rmcat-wired-nada" test suite:

::

    ./waf --command-template="gdb %s" --run "test-runner"
    r --assert-on-failure --suite=rmcat-wired-nada

To debug rmcat example, enter ns3 source directory:

::

    ./waf --command-template="gdb %s" --run src/ns3-rmcat/examples/rmcat-example

Future work
**********************************

Add LTE topology and test cases

Add support for ECN marking

