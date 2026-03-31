#include "ns3/flow-monitor-module.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int main() {
    Time::SetResolution(Time::NS);

    // ======================
    // NÓS
    // ======================
    NodeContainer leftNodes;
    leftNodes.Create(2); // S1 (UDP), S2 (TCP)

    NodeContainer rightNodes;
    rightNodes.Create(2); // D1 (UDP), D2 (TCP)

    NodeContainer routers;
    routers.Create(2);

    // ======================
    // LINKS
    // ======================
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("2ms"));

    PointToPointHelper bottleneck;
    bottleneck.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    bottleneck.SetChannelAttribute("Delay", StringValue("10ms"));

    // S1 -> R1
    NetDeviceContainer d1 = p2p.Install(leftNodes.Get(0), routers.Get(0));

    // S2 -> R1
    NetDeviceContainer d2 = p2p.Install(leftNodes.Get(1), routers.Get(0));

    // R1 -> R2 (bottleneck)
    NetDeviceContainer d3 = bottleneck.Install(routers.Get(0), routers.Get(1));

    // R2 -> D1
    NetDeviceContainer d4 = p2p.Install(routers.Get(1), rightNodes.Get(0));

    // R2 -> D2
    NetDeviceContainer d5 = p2p.Install(routers.Get(1), rightNodes.Get(1));

    // ======================
    // INTERNET
    // ======================
    InternetStackHelper stack;
    stack.Install(leftNodes);
    stack.Install(rightNodes);
    stack.Install(routers);

    // ======================
    // IPs
    // ======================
    Ipv4AddressHelper address;

    address.SetBase("10.0.1.0", "255.255.255.0");
    Ipv4InterfaceContainer i1 = address.Assign(d1);

    address.SetBase("10.0.2.0", "255.255.255.0");
    Ipv4InterfaceContainer i2 = address.Assign(d2);

    address.SetBase("10.0.3.0", "255.255.255.0");
    Ipv4InterfaceContainer i3 = address.Assign(d3);

    address.SetBase("10.0.4.0", "255.255.255.0");
    Ipv4InterfaceContainer i4 = address.Assign(d4);

    address.SetBase("10.0.5.0", "255.255.255.0");
    Ipv4InterfaceContainer i5 = address.Assign(d5);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // ======================
    // 🔥 UDP BACKGROUND (S1 -> D1)
    // ======================
    uint16_t udpPort = 4000;

    OnOffHelper udpApp("ns3::UdpSocketFactory",
                       InetSocketAddress(i4.GetAddress(0), udpPort));

    udpApp.SetAttribute("DataRate", StringValue("8Mbps"));
    udpApp.SetAttribute("PacketSize", UintegerValue(1000));
    udpApp.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    udpApp.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

    ApplicationContainer udpSender = udpApp.Install(leftNodes.Get(0));
    udpSender.Start(Seconds(1.0));
    udpSender.Stop(Seconds(10.0));

    PacketSinkHelper udpSink("ns3::UdpSocketFactory",
                              InetSocketAddress(Ipv4Address::GetAny(), udpPort));

    ApplicationContainer udpReceiver = udpSink.Install(rightNodes.Get(0));
    udpReceiver.Start(Seconds(1.0));
    udpReceiver.Stop(Seconds(10.0));

    // ======================
    // 🔥 TCP RENO (S2 -> D2)
    // ======================
    uint16_t tcpPort = 5000;

    PacketSinkHelper tcpSink("ns3::TcpSocketFactory",
                             InetSocketAddress(Ipv4Address::GetAny(), tcpPort));

    ApplicationContainer sinkApp = tcpSink.Install(rightNodes.Get(1));
    sinkApp.Start(Seconds(1.0));
    sinkApp.Stop(Seconds(10.0));

    BulkSendHelper tcpApp("ns3::TcpSocketFactory",
                          InetSocketAddress(i5.GetAddress(1), tcpPort));

    tcpApp.SetAttribute("MaxBytes", UintegerValue(0)); // fluxo infinito

    ApplicationContainer tcpSender = tcpApp.Install(leftNodes.Get(1));
    tcpSender.Start(Seconds(1.0));
    tcpSender.Stop(Seconds(10.0));

    // ======================
    // SIMULAÇÃO
    // ======================
    Simulator::Stop(Seconds(10.0));
    FlowMonitorHelper flowHelper;
    Ptr<FlowMonitor> monitor = flowHelper.InstallAll();
    Simulator::Run();
    Simulator::Destroy();

    monitor->CheckForLostPackets();

    Ptr<Ipv4FlowClassifier> classifier =
        DynamicCast<Ipv4FlowClassifier>(flowHelper.GetClassifier());

    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    std::ofstream file("flow.txt");

    for (auto &flow : stats) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);

        double duration = flow.second.timeLastRxPacket.GetSeconds()
                        - flow.second.timeFirstTxPacket.GetSeconds();

        double throughput = 0;
        if (duration > 0)
            throughput = (flow.second.rxBytes * 8.0) / duration / 1e6; // Mbps

        file << flow.first << " "
            << t.sourceAddress << " -> " << t.destinationAddress << " "
            << throughput << "\n";
    }

    file.close();

    return 0;
}