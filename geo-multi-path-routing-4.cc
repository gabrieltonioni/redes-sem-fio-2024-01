#include <fstream>
#include <iostream>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/aodv-module.h"
#include "ns3/olsr-module.h"
#include "ns3/dsdv-module.h"
#include "ns3/gpsr-module.h"
#include "ns3/dsr-module.h"
#include "ns3/applications-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/energy-module.h"
#include "ns3/wifi-radio-energy-model-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/gnuplot.h"
#include "ns3/flow-monitor-module.h"
#include <ns3/flow-monitor-helper.h>

using namespace ns3;
using namespace dsr;

NS_LOG_COMPONENT_DEFINE("wsn-routing-compare");

class RoutingExperiment
{
    public:
    RoutingExperiment();
    void Run(int nSinks, double txp, std::string CSVfileName);
    std::string CommandSetup(int argc, char **argv);

    private:
    Ptr<Socket> SetupPacketReceive(Ipv4Address addr, Ptr<Node> node);
    void ReceivePacket(Ptr<Socket> socket);
    void CheckThroughput();

    uint32_t port;
    uint32_t bytesTotal;
    uint32_t packetsReceived;

    std::string m_CSVfileName;
    int m_nSinks;
    std::string m_protocolName;
    double m_txp;
    uint32_t m_protocol;
};

RoutingExperiment::RoutingExperiment()
  : port(9),
    bytesTotal(0),
    packetsReceived(0),
    m_CSVfileName("wsn-routing.output.csv"),
    m_protocol(4)
{
}

static inline std::string
PrintReceivedPacket(Ptr<Socket> socket, Ptr<Packet> packet, Address senderAddress)
{
    std::ostringstream oss;

    oss << Simulator::Now().GetSeconds() << " " << socket->GetNode()->GetId();

    if(InetSocketAddress::IsMatchingType(senderAddress))
    {
        InetSocketAddress addr = InetSocketAddress::ConvertFrom(senderAddress);
        oss << " received one packet from " << addr.GetIpv4();
    }
    else
    {
        oss << " received one packet!";
    }
    return oss.str();
}

void
RoutingExperiment::ReceivePacket(Ptr<Socket> socket)
{
    Ptr<Packet> packet;
    Address senderAddress;
    while((packet = socket->RecvFrom(senderAddress)))
    {
        bytesTotal += packet->GetSize();
        packetsReceived += 1;
        NS_LOG_UNCOND(PrintReceivedPacket(socket, packet, senderAddress));
    }
}

Ptr<Socket>
RoutingExperiment::SetupPacketReceive(Ipv4Address addr, Ptr<Node> node)
{
    TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
    Ptr<Socket> sink = Socket::CreateSocket(node, tid);
    InetSocketAddress local = InetSocketAddress(addr, port);
    sink->Bind(local);
    sink->SetRecvCallback(MakeCallback (&RoutingExperiment::ReceivePacket, this));

    return sink;
}

void
RoutingExperiment::CheckThroughput()
{
    double kbs = (bytesTotal * 8.0) / 1000;
    bytesTotal = 0;

    std::ofstream out(m_CSVfileName.c_str(), std::ios::app);

    out << (Simulator::Now()).GetSeconds() << "\t\t\t\t\t"
        << kbs << "\t\t\t\t"
        << packetsReceived << "\t\t\t\t\t"
        << m_nSinks << "\t\t\t\t\t"
        << m_protocolName << "\t\t\t\t"
        << m_txp << ""
        << std::endl;

    out.close();
    packetsReceived = 0;
    Simulator::Schedule(Seconds(1.0), &RoutingExperiment::CheckThroughput, this);
}

void ThroughputMonitor(FlowMonitorHelper *fmhelper, Ptr<FlowMonitor> flowMon, Gnuplot2dDataset DataSet)
{
    double localThrou = 0;
    std::map<FlowId, FlowMonitor::FlowStats> flowStats = flowMon->GetFlowStats();
    Ptr<Ipv4FlowClassifier> classing = DynamicCast<Ipv4FlowClassifier>(fmhelper->GetClassifier());
    for(std::map<FlowId, FlowMonitor::FlowStats>::const_iterator stats = flowStats.begin(); stats != flowStats.end(); ++stats)
    {
        if(stats->first == 1)
        {
        //   Ipv4FlowClassifier::FiveTuple fiveTuple = classing->FindFlow (stats->first);
        //   std::cout<<"Flow ID     : " << stats->first <<" ; "<< fiveTuple.sourceAddress <<" -----> " << fiveTuple.destinationAddress << std::endl;
        //   std::cout<<"Tx Packets = " << stats->second.txPackets << std::endl;
        //   std::cout<<"Rx Packets = " << stats->second.rxPackets << std::endl;
        //   std::cout<<"Duration    : " << (stats->second.timeLastRxPacket.GetSeconds()-stats->second.timeFirstTxPacket.GetSeconds()) << std::endl;
        //   std::cout<<"Last Received Packet  : "<< stats->second.timeLastRxPacket.GetSeconds() << " Seconds" << std::endl;
        //   std::cout<<"Throughput: " << stats->second.rxBytes * 8.0 / (stats->second.timeLastRxPacket.GetSeconds()-stats->second.timeFirstTxPacket.GetSeconds())/1024  << " Kbps"<<std::endl;
          localThrou = (stats->second.rxBytes * 8.0 / (stats->second.timeLastRxPacket.GetSeconds()-stats->second.timeFirstTxPacket.GetSeconds())/1024);
          // Update gnuplot data
          DataSet.Add((double)Simulator::Now().GetSeconds(),(double) localThrou);
        //   std::cout << "---------------------------------------------------------------------------" << std::endl;
        }
    }
    Simulator::Schedule(Seconds(1), &ThroughputMonitor, fmhelper, flowMon, DataSet);
   //if(flowToXml)
    {
      flowMon->SerializeToXmlFile("ThroughputMonitor.xml", true, true);
    }
}

std::string
RoutingExperiment::CommandSetup(int argc, char **argv)
{
    LogComponentEnable("wsn-routing-compare", LOG_LEVEL_INFO);

    CommandLine cmd;
    cmd.AddValue("CSVfileName", "The name of the CSV output file name", m_CSVfileName);
    cmd.AddValue("protocol", "1=OLSR;2=AODV;3=DSDV;4=GPSR;5=DSR", m_protocol);
    cmd.Parse(argc, argv);
    return m_CSVfileName;
}

int
main(int argc, char *argv[])
{
    RoutingExperiment experiment;
    std::string CSVfileName = experiment.CommandSetup(argc,argv);

    // Blank out the last output file and write the column headers
    std::ofstream out(CSVfileName.c_str());
    out << "SimulationSecond\t" <<
    "ReceiveRate\t\t" <<
    "PacketsReceived\t\t" <<
    "NumberOfSinks\t\t" <<
    "RoutingProtocol\t\t" <<
    "TransmissionPower" <<
    std::endl;
    out.close();

    int nSinks = 1;
    double txp = 7.5;

    experiment.Run(nSinks, txp, CSVfileName);
}

void
RoutingExperiment::Run(int nSinks, double txp, std::string CSVfileName)
{
    //------------------------------------Initial Setup-----------------------------------//
    
    Packet::EnablePrinting();
    m_nSinks = nSinks;
    m_txp = txp;
    m_CSVfileName = CSVfileName;

    int nSensors = 50;

    double TotalTime = 200.0;
    std::string rate("2048bps");
    std::string phyMode("DsssRate1Mbps");
    std::string tr_name("wsn-routing-compare");
    m_protocolName = "protocol";

    Config::SetDefault("ns3::OnOffApplication::PacketSize", StringValue("64"));
    Config::SetDefault("ns3::OnOffApplication::DataRate", StringValue(rate));

    // Set Non-unicastMode rate to unicast mode
    Config::SetDefault("ns3::WifiRemoteStationManager::NonUnicastMode", StringValue(phyMode));

    NodeContainer sensorNodes;
    sensorNodes.Create(nSensors);
    NodeContainer gwNodes;
    gwNodes.Create(nSinks);
    NodeContainer allNodes;
    allNodes.Add(sensorNodes);
    allNodes.Add(gwNodes);

    //-----------------------------------Phy + Mac Layer----------------------------------//
    
    // Setting up wifi phy and channel using helpers
    WifiHelper wifi;
    wifi.SetStandard(WIFI_PHY_STANDARD_80211b);

    YansWifiPhyHelper wifiPhy =  YansWifiPhyHelper::Default();
    YansWifiChannelHelper wifiChannel;
    wifiChannel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
    wifiChannel.AddPropagationLoss("ns3::FriisPropagationLossModel");
    wifiPhy.SetChannel(wifiChannel.Create());

    // Add a mac and disable rate control
    WifiMacHelper wifiMac;
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue(phyMode),
                                 "ControlMode", StringValue(phyMode));

    wifiPhy.Set("TxPowerStart", DoubleValue(txp));
    wifiPhy.Set("TxPowerEnd", DoubleValue(txp));

    wifiMac.SetType("ns3::AdhocWifiMac");
    NetDeviceContainer sensorDevices = wifi.Install(wifiPhy, wifiMac, sensorNodes);
    NetDeviceContainer gwDevices = wifi.Install(wifiPhy, wifiMac, gwNodes);

    //-----------------------------------Mobility-----------------------------------------//
    
    MobilityHelper mobility;
    /* Sensor nodes positions */
    mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                  "X", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=300.0]"),
                                  "Y", StringValue("ns3::UniformRandomVariable[Min=0.0|Max=300.0]"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(sensorNodes);

    /* Gateway nodes positions */
    mobility.SetPositionAllocator("ns3::RandomRectanglePositionAllocator",
                                  "X", StringValue("ns3::UniformRandomVariable[Min=350.0|Max=400.0]"),
                                  "Y", StringValue("ns3::UniformRandomVariable[Min=350.0|Max=400.0]"));
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(gwNodes);

    //-------------------------------------Energy Model-----------------------------------//
    
    /* Energy source */
    BasicEnergySourceHelper basicSourceHelper;
    // Configure energy source
    basicSourceHelper.Set("BasicEnergySourceInitialEnergyJ", DoubleValue(0.1));
    // Install source
    EnergySourceContainer sources = basicSourceHelper.Install(sensorNodes);
    /* Device energy model */
    WifiRadioEnergyModelHelper radioEnergyHelper;
    // Configure radio energy model
    radioEnergyHelper.Set("TxCurrentA", DoubleValue(0.0174));
    // Install device model
    DeviceEnergyModelContainer deviceModels = radioEnergyHelper.Install(sensorDevices, sources);

    //------------------------------------Routing-----------------------------------------//
    
    AodvHelper aodv;
    OlsrHelper olsr;
    DsdvHelper dsdv;
    DsrHelper dsr;
    GpsrHelper gpsr;
    DsrMainHelper dsrMain;
    Ipv4ListRoutingHelper list;
    InternetStackHelper internet;

    switch(m_protocol)
    {
        case 1:
        list.Add(olsr, 100);
        m_protocolName = "OLSR";
        break;
        case 2:
        list.Add(aodv, 100);
        m_protocolName = "AODV";
        break;
        case 3:
        list.Add(dsdv, 100);
        m_protocolName = "DSDV";
        break;
        case 4:
        list.Add(gpsr, 100);
        m_protocolName = "GPSR";
        break;
        case 5:
        m_protocolName = "DSR";
        break;
        default:
        NS_FATAL_ERROR ("No such protocol:" << m_protocol);
    }

    if(m_protocol < 5)
    {
        internet.SetRoutingHelper(list);
        internet.Install(allNodes);
    }
    else if(m_protocol == 5)
    {
        internet.Install(allNodes);
        dsrMain.Install(dsr, allNodes);
    }

    //------------------------------------Addressing--------------------------------------//

    Ipv4AddressHelper address;
    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer sensorInterfaces, gwInterfaces;
    sensorInterfaces = address.Assign(sensorDevices);
    gwInterfaces = address.Assign(gwDevices);

    //------------------------------------Application-------------------------------------//
    
    OnOffHelper onoff1("ns3::UdpSocketFactory",Address());
    onoff1.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1.0]"));
    onoff1.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0.0]"));

    for(int i = 0; i < nSinks; i++)
    {
        Ptr<Socket> sink = SetupPacketReceive(gwInterfaces.GetAddress(i), gwNodes.Get(i));

        AddressValue remoteAddress(InetSocketAddress(gwInterfaces.GetAddress(i), port));
        onoff1.SetAttribute("Remote", remoteAddress);

        Ptr<UniformRandomVariable> var = CreateObject<UniformRandomVariable>();
        ApplicationContainer temp = onoff1.Install(sensorNodes.Get(i + nSinks));
        temp.Start(Seconds(var->GetValue(100.0,101.0)));
        temp.Stop(Seconds(TotalTime));
    }

    //------------------------------------Gnuplot Parameters------------------------------//
    //------------------------------------Flowmon-Throughput------------------------------//

    std::string fileNameWithNoExtension = "FlowVSThroughput_";
    std::string graphicsFileName        = fileNameWithNoExtension + ".png";
    std::string plotFileName            = fileNameWithNoExtension + ".plt";
    std::string plotTitle               = "Throughput x Time";
    std::string dataTitle               = "Throughput";

    // Instantiate the plot and set its title.
    Gnuplot gnuplot(graphicsFileName);
    gnuplot.SetTitle(plotTitle);

    // Make the graphics file, which the plot file will be when it
    // is used with Gnuplot, be a PNG file.
    gnuplot.SetTerminal("png");

    // Set the labels for each axis.
    gnuplot.SetLegend("Simulation Time", "Throughput (Kbps)");

    Gnuplot2dDataset dataset;
    dataset.SetTitle(dataTitle);
    dataset.SetStyle(Gnuplot2dDataset::LINES_POINTS);

    //flowMonitor declaration
    FlowMonitorHelper fmHelper;
    Ptr<FlowMonitor> allMon = fmHelper.InstallAll();
    // Call the flow monitor function
    ThroughputMonitor(&fmHelper, allMon, dataset);

    //------------------------------------Simulation Execution----------------------------//
    
    NS_LOG_INFO("Running simulation with " + m_protocolName + ".");

    // CheckThroughput();

    Simulator::Stop(Seconds(TotalTime));

    //------------------------------------Animation---------------------------------------//
    
    // NetAnim is not working with wifi
    
    // AnimationInterface anim("wsn-animation.xml"); // Mandatory
    // anim.SetMaxPktsPerTraceFile(500000);
    // for(uint32_t i = 0; i < sensorNodes.GetN(); ++i)
    // {
    //     anim.UpdateNodeDescription(sensorNodes.Get(i), "SENSOR"); // Optional
    //     anim.UpdateNodeColor(sensorNodes.Get(i), 255, 0, 0); // Coloração
    // }
    // for(uint32_t i = 0; i < gwNodes.GetN(); ++i)
    // {
    //     anim.UpdateNodeDescription(gwNodes.Get(i), "GATEWAY"); // Optional
    //     anim.UpdateNodeColor(gwNodes.Get(i), 0, 255, 0); // Coloração
    // }
    
    // anim.EnablePacketMetadata();

    Simulator::Run();

    //------------------------------------Gnuplot Continuation----------------------------//
    
    gnuplot.AddDataset(dataset);
    // Open the plot file.
    std::ofstream plotFile(plotFileName.c_str());
    // Write the plot file.
    gnuplot.GenerateOutput(plotFile);
    // Close the plot file.
    plotFile.close();

    //------------------------------------End of Simulation-------------------------------//

    Simulator::Destroy();
}
