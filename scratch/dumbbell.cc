#include "ns3/flow-monitor-module.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include <fstream>

using namespace ns3;

// ==========================================
// VARIÁVEIS GLOBAIS E FUNÇÕES PARA RASTREIO (TRACING)
// ==========================================
std::ofstream cwndStream;
std::ofstream throughputStream;
uint32_t prevRxBytes = 0;

// Função invocada sempre que a Janela de Congestionamento (Cwnd) é alterada
void CwndChange(uint32_t oldCwnd, uint32_t newCwnd) {
    cwndStream << Simulator::Now().GetSeconds() << "\t" << newCwnd << std::endl;
}

// Liga o rastreio (Trace) ao socket TCP do nó transmissor S2
void TraceCwnd() {
    // /NodeList/1/ refere-se ao nó S2 (índice 1 em leftNodes, e ID global 1)
    Config::ConnectWithoutContext("/NodeList/1/$ns3::TcpL4Protocol/SocketList/0/CongestionWindow", MakeCallback(&CwndChange));
}

// Calcula e regista o débito (Throughput) periodicamente (a cada 0.5 segundos)
void CalculateThroughput(Ptr<PacketSink> sink) {
    Time now = Simulator::Now();
    uint32_t currentRxBytes = sink->GetTotalRx();
    uint32_t bytesDeltas = currentRxBytes - prevRxBytes;
    prevRxBytes = currentRxBytes;

    // Calcula o Throughput em Mbps: (Bytes * 8) / (0.5 segundos * 1.000.000)
    double throughput = (bytesDeltas * 8.0) / (0.5 * 1e6);

    throughputStream << now.GetSeconds() << "\t" << throughput << std::endl;

    // Reagenda esta mesma função para ser executada novamente daqui a 0.5 segundos
    Simulator::Schedule(Seconds(0.5), &CalculateThroughput, sink);
}

// ==========================================
// FUNÇÃO PRINCIPAL
// ==========================================
int main() {
    Time::SetResolution(Time::NS);

   // 🔥 DEFINIR A VARIANTE DO TCP AQUI 🔥
    // Para a primeira parte do exercício, usamos o TcpNewReno (que é o Reno do ns-3):
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TypeId::LookupByName("ns3::TcpNewReno")));

    // Para a segunda parte, comente a linha acima e descomente a linha abaixo:
    // Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TypeId::LookupByName("ns3::TcpCubic")));
    // ======================
    // NÓS
    // ======================
    NodeContainer leftNodes;
    leftNodes.Create(2); // S1 (UDP), S2 (TCP)

    NodeContainer rightNodes;
    rightNodes.Create(2); // D1 (UDP), D2 (TCP)

    NodeContainer routers;
    routers.Create(2); // R1, R2

    // ======================
    // LIGAÇÕES (LINKS)
    // ======================
    // Ligações de Acesso: 10Mbps, 40ms de atraso
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    p2p.SetChannelAttribute("Delay", StringValue("40ms")); 

    // Ligação Central (Gargalo/Bottleneck): 10Mbps, 20ms de atraso
    PointToPointHelper bottleneck;
    bottleneck.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
    bottleneck.SetChannelAttribute("Delay", StringValue("20ms")); 

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
    // ENDEREÇOS IP
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
    // 🔥 TRÁFEGO UDP DE FUNDO (S1 -> D1)
    // ======================
    uint16_t udpPort = 4000;

    OnOffHelper udpApp("ns3::UdpSocketFactory", InetSocketAddress(i4.GetAddress(1), udpPort));
    udpApp.SetAttribute("DataRate", StringValue("8Mbps"));
    udpApp.SetAttribute("PacketSize", UintegerValue(1000));
    udpApp.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    udpApp.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));

    ApplicationContainer udpSender = udpApp.Install(leftNodes.Get(0));
    udpSender.Start(Seconds(1.0));
    udpSender.Stop(Seconds(10.0));

    PacketSinkHelper udpSink("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), udpPort));
    ApplicationContainer udpReceiver = udpSink.Install(rightNodes.Get(0));
    udpReceiver.Start(Seconds(1.0));
    udpReceiver.Stop(Seconds(10.0));

    // ======================
    // 🔥 TRÁFEGO TCP RENO (S2 -> D2)
    // ======================
    uint16_t tcpPort = 5000;

    PacketSinkHelper tcpSink("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), tcpPort));
    ApplicationContainer sinkApp = tcpSink.Install(rightNodes.Get(1));
    sinkApp.Start(Seconds(1.0));
    sinkApp.Stop(Seconds(10.0));

    // Utilização do OnOffHelper para limitar estritamente o envio a 5 Mbps
    OnOffHelper tcpApp("ns3::TcpSocketFactory", InetSocketAddress(i5.GetAddress(1), tcpPort));
    tcpApp.SetAttribute("DataRate", StringValue("5Mbps")); // Limite de 5Mbps exigido
    tcpApp.SetAttribute("PacketSize", UintegerValue(1000));
    tcpApp.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    tcpApp.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    
    ApplicationContainer tcpSender = tcpApp.Install(leftNodes.Get(1));
    tcpSender.Start(Seconds(1.0));
    tcpSender.Stop(Seconds(10.0));

    // ======================
    // PREPARAÇÃO DOS FICHEIROS DE SAÍDA E EVENTOS DE TRACING
    // ======================
    // Nota: Quando for simular o TCP Cubic, altere os nomes dos ficheiros abaixo
    cwndStream.open("cwnd_reno.dat");
    throughputStream.open("throughput_reno.dat");

    // Agenda o início da recolha da janela de congestionamento (logo após a aplicação iniciar)
    Simulator::Schedule(Seconds(1.00001), &TraceCwnd);
    
    // Obtém o ponteiro do destino TCP para medir os pacotes que chegam
    Ptr<PacketSink> tcpSinkPtr = DynamicCast<PacketSink>(sinkApp.Get(0));
    // Agenda a primeira medição de débito para o segundo 1.5
    Simulator::Schedule(Seconds(1.5), &CalculateThroughput, tcpSinkPtr);

    // ======================
    // SIMULAÇÃO E FLOWMONITOR
    // ======================
    Simulator::Stop(Seconds(10.0));
    FlowMonitorHelper flowHelper;
    Ptr<FlowMonitor> monitor = flowHelper.InstallAll();
    
    Simulator::Run();

    // Fechar os ficheiros após a execução
    cwndStream.close();
    throughputStream.close();

    // ======================
    // ESTATÍSTICAS FINAIS
    // ======================
    monitor->CheckForLostPackets();

    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowHelper.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();

    std::ofstream file("flow.txt");

    for (auto &flow : stats) {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flow.first);

        double duration = flow.second.timeLastRxPacket.GetSeconds() - flow.second.timeFirstTxPacket.GetSeconds();
        double throughput = 0;
        
        if (duration > 0)
            throughput = (flow.second.rxBytes * 8.0) / duration / 1e6; // Mbps

        file << flow.first << " "
             << t.sourceAddress << " -> " << t.destinationAddress << " "
             << throughput << "\n";
    }

    file.close();

    Simulator::Destroy();
    return 0;
}
