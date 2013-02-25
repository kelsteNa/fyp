/*
 * wifi.cpp
 *
 *  Created on: 24 Oct 2012
 *      Author: thomas
 */

#include "ns3/core-module.h"
#include "ns3/simulator.h"
#include "ns3/node.h"
#include "ns3/global-value.h"
#include "ns3/wifi-module.h"
#include "ns3/wimax-helper.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/internet-module.h"
#include "ns3/udp-echo-helper.h"
#include "ns3/olsr-module.h"
#include "ns3/flow-monitor.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/inet-socket-address.h"
#include "ns3/csma-module.h"
#include "ns3/fypApp.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/packet-socket-address.h"

using namespace ns3;
NS_LOG_COMPONENT_DEFINE("Wifi");

void packetReceived(Ptr<const Packet>, const Address &);

static void SetPosition(Ptr<Node> node, double x, double y)
{
	Ptr<MobilityModel> mobility = node->GetObject<MobilityModel>();
	Vector pos = mobility->GetPosition();
	pos.x = x;
	pos.y = y;
	mobility->SetPosition(pos);

}

int main(int argc, char * argv[])
{

	CommandLine cmd;
	cmd.Parse(argc, argv);

	NodeContainer wifiNodes;
	wifiNodes.Create(2);
	NodeContainer gatewayNode;
	gatewayNode.Create(1);
	NodeContainer satelliteNode;
	satelliteNode.Create(1);
	NodeContainer wimaxNode;
	wimaxNode.Create(1);
	NodeContainer hqNode;
	hqNode.Create(1);

	NodeContainer csmaNodes; //hqNode 1 and wimaxNode 1;
	csmaNodes.Add(hqNode);
	csmaNodes.Add(wimaxNode);


	WifiHelper wifi;
	YansWifiPhyHelper wifiPhy = YansWifiPhyHelper::Default();
	wifiPhy.SetPcapDataLinkType(YansWifiPhyHelper::DLT_IEEE802_11);
	YansWifiChannelHelper channel;
	channel.SetPropagationDelay("ns3::ConstantSpeedPropagationDelayModel");
	channel.AddPropagationLoss("ns3::TwoRayGroundPropagationLossModel", "SystemLoss", DoubleValue(1), "HeightAboveZ", DoubleValue(1.5));

	wifiPhy.Set("TxPowerStart", DoubleValue(33));
	wifiPhy.Set("TxPowerEnd", DoubleValue(33));
	wifiPhy.Set ("TxPowerLevels", UintegerValue(1));
	wifiPhy.Set ("TxGain", DoubleValue(0));
	wifiPhy.Set ("RxGain", DoubleValue(0));
	wifiPhy.Set ("EnergyDetectionThreshold", DoubleValue(-61.8));
	wifiPhy.Set ("CcaMode1Threshold", DoubleValue(-64.8));

	wifiPhy.SetChannel(channel.Create());

	NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default();

	wifi.SetStandard(WIFI_PHY_STANDARD_80211b);
	wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager", "DataMode", StringValue("DsssRate1Mbps"), "ControlMode", StringValue("DsssRate1Mbps"));

	Ssid ssid = Ssid("rescue-net");
	wifiMac.SetType("ns3::StaWifiMac", "Ssid", SsidValue(ssid), "ActiveProbing", BooleanValue(false));

	//wifiNodes.Add(gatewayNode);
	NetDeviceContainer wifiStaDevs = wifi.Install(wifiPhy, wifiMac, wifiNodes);

	wifiMac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));

	NetDeviceContainer wifiApDev = wifi.Install(wifiPhy, wifiMac, gatewayNode);


	//P2P stuff
	PointToPointHelper pointToPoint;
	pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
	pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));
	NodeContainer p2pNodes;
	p2pNodes.Add(gatewayNode.Get(0));
	p2pNodes.Add(wimaxNode.Get(0));
	NetDeviceContainer p2pdevs = pointToPoint.Install(p2pNodes);


	////// p2p nodes 0 = gateway 1 = wimax

	PointToPointHelper satpointToPoint;
	satpointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
	satpointToPoint.SetChannelAttribute ("Delay", StringValue ("600ms"));
	NodeContainer satp2pNodes;
	satp2pNodes.Add(gatewayNode.Get(0));
	satp2pNodes.Add(satelliteNode.Get(0));
	NetDeviceContainer satp2pdevs = satpointToPoint.Install(satp2pNodes);
	////// sat p2p nodes 0 = gateway 1 = sat
	PointToPointHelper HQsatpointToPoint;
	HQsatpointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
	HQsatpointToPoint.SetChannelAttribute ("Delay", StringValue ("600ms"));
	NodeContainer HQsatp2pNodes;
	HQsatp2pNodes.Add(satelliteNode.Get(0));
	HQsatp2pNodes.Add(hqNode.Get(0));
	NetDeviceContainer HQsatp2pdevs = HQsatpointToPoint.Install(HQsatp2pNodes);

	//set up the csma properties
	CsmaHelper csma;
	csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
	csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6500)));
	//the csma devices
	NetDeviceContainer csmaDevs;
	csmaDevs = csma.Install(csmaNodes);

	InternetStackHelper stack;
	stack.Install(wimaxNode);
	stack.Install(gatewayNode);
	stack.Install(satelliteNode);
	stack.Install(hqNode);
	stack.Install(wifiNodes);



	Ipv4GlobalRoutingHelper::PopulateRoutingTables ();


	Ipv4AddressHelper address;
	address.SetBase("10.1.1.0", "255.255.255.0");
	Ipv4InterfaceContainer p2pinterface = address.Assign(p2pdevs);
	Ipv4InterfaceContainer satp2pinterface  = address.Assign(satp2pdevs);
	Ipv4InterfaceContainer HQsatp2pinterface  = address.Assign(HQsatp2pdevs);
	address.SetBase("10.1.2.0", "255.255.255.0");
	Ipv4InterfaceContainer csmaInterfaces;
	csmaInterfaces = address.Assign(csmaDevs);


	address.SetBase("10.1.6.0", "255.255.255.0");
	Ipv4InterfaceContainer wifiInterfaces = address.Assign(wifiStaDevs);
	Ipv4InterfaceContainer wifiApInterface = address.Assign(wifiApDev);

	address.SetBase("10.1.3.0", "255.255.255.0");



	Ptr<Node> t_node = wifiNodes.Get(0);
	Ipv4StaticRoutingHelper helper;
	Ptr<Ipv4> ipv4 = t_node->GetObject<Ipv4>();
	Ptr<Ipv4StaticRouting> Ipv4stat = helper.GetStaticRouting(ipv4);
	Ipv4stat->SetDefaultRoute("10.1.6.3",1,-10);

	Ptr<Node> t_node2 = wifiNodes.Get(1);
	Ipv4StaticRoutingHelper helper2;
	Ptr<Ipv4> ipv42 = t_node2->GetObject<Ipv4>();
	Ptr<Ipv4StaticRouting> Ipv4stat2 = helper2.GetStaticRouting(ipv42);
	Ipv4stat2->SetDefaultRoute("10.1.6.3",1,-10);




	// Install FlowMonitor on all nodes
	FlowMonitorHelper flowmon;
	Ptr<FlowMonitor> monitor = flowmon.InstallAll();


	UdpEchoClientHelper echoClient (wifiApInterface.GetAddress(0), 9);
	echoClient.SetAttribute ("MaxPackets", UintegerValue (10000));
	echoClient.SetAttribute ("Interval", TimeValue (Seconds (.1)));
	echoClient.SetAttribute ("PacketSize", UintegerValue (2048));

	ApplicationContainer clientApps = echoClient.Install (wifiNodes.Get (0));
	clientApps.Start (Seconds (2.0));
	clientApps.Stop (Seconds (9.0));
/*
	UdpEchoClientHelper echoClient1 (wifiApInterface.GetAddress (0), 9);
	echoClient1.SetAttribute ("MaxPackets", UintegerValue (10000));
	echoClient1.SetAttribute ("Interval", TimeValue (Seconds (.15)));
	echoClient1.SetAttribute ("PacketSize", UintegerValue (2048));

	UdpEchoClientHelper echoClient2 (wifiApInterface.GetAddress(0), 9);
	echoClient2.SetAttribute ("MaxPackets", UintegerValue (10000));
	echoClient2.SetAttribute ("Interval", TimeValue (Seconds (.15)));
	echoClient2.SetAttribute ("PacketSize", UintegerValue (2048));

	ApplicationContainer clientApps2 = echoClient2.Install (wifiNodes.Get (0));
	clientApps2.Start (Seconds (2.0));
	clientApps2.Stop (Seconds (9.0));

	UdpEchoServerHelper echoServer(9);
	ApplicationContainer serverApps = echoServer.Install(gatewayNode.Get(0));


	ApplicationContainer clientApps1 = echoClient1.Install (wifiNodes.Get (1));
	clientApps1.Start (Seconds (2.0));
	clientApps1.Stop (Seconds (9.0));





	 */

	UdpEchoServerHelper echoServer2(9);
	ApplicationContainer serverApps2 = echoServer2.Install(gatewayNode.Get(0));


	/*UdpEchoServerHelper echoServer(9);
	ApplicationContainer serverApps = echoServer.Install(gatewayNode.Get(0));
*/
	Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
	Ipv4GlobalRoutingHelper::RecomputeRoutingTables();


	MobilityHelper mobility;
	Ptr<ListPositionAllocator> positionAlloc = CreateObject <ListPositionAllocator>();
	positionAlloc->Add(Vector(20,20,0));
	//positionAlloc->Add(Vector(1000,0,0));
	//positionAlloc->Add(Vector(450,0,0));
	mobility.SetPositionAllocator(positionAlloc);
	mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
	mobility.Install(hqNode);
	mobility.Install(satelliteNode);
	mobility.Install(wifiNodes);

	MobilityHelper wimaxMobility;
	Ptr<ListPositionAllocator> wimaxPositionAlloc = CreateObject<ListPositionAllocator>();
	wimaxPositionAlloc->Add(Vector(20,20,0));
	wimaxMobility.SetPositionAllocator(wimaxPositionAlloc);
	wimaxMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
	wimaxMobility.Install(gatewayNode);
	wimaxMobility.Install(wimaxNode);

	SetPosition(gatewayNode.Get(0), -50, 0);
	SetPosition(satelliteNode.Get(0), 50, -50.0);
	SetPosition(wimaxNode.Get(0), -100, -50.0);
	SetPosition(hqNode.Get(0), -50.0, -100.0);
	SetPosition(wifiNodes.Get(0), -25, 25);
	SetPosition(wifiNodes.Get(1), -50, 50);
	Simulator::Stop(Seconds(10.0));

	Simulator::Run();

	// Print per flow statistics
	monitor->CheckForLostPackets ();
	Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier ());
	std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();

	for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator iter = stats.begin (); iter != stats.end (); ++iter)
	{
		Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (iter->first);
		std::cout << "Flow ID: " << iter->first << " Src Addr " << t.sourceAddress << " Dst Addr " << t.destinationAddress;
		std::cout << "Tx Packets = " << iter->second.txPackets;
		std::cout << "Rx Packets = " << iter->second.rxPackets;
		std::cout << "Throughput: " << iter->second.rxBytes * 8.0 / (iter->second.timeLastRxPacket.GetSeconds()-iter->second.timeFirstTxPacket.GetSeconds()) / 1024  << " Kbps\n";
	}

	Simulator::Destroy();

}
int packetsRx = 0;
void packetReceived(Ptr<const Packet> p, const Address & addr)
{

	if(packetsRx%2==0)
	{
		std::cout << "Got packet EVEN\n";

	}
	else
	{
		std::cout << "Got packet ODD\n";
	}
	packetsRx++;
}

FYPApp::FYPApp()
{
	std::cout << "Hello i am the FYP App.\n";
}

void FYPApp::Setup(Ptr<Node> node)
{
	m_node = node;
}

FYPApp::~FYPApp()
{
	std::cout << "Killing this instance.\n";
}

bool FYPApp::PacketIntercept(Ptr<Packet> p, const Ipv4Header &)
{

	std::cout << "GOT PACEKT!\n";
	return true;

}

void FYPApp::StartApplication()
{
	Ptr<Ipv4L3Protocol> ipv4Proto = m_node->GetObject<Ipv4L3Protocol> ();
	if (ipv4Proto != 0)
	{
		NS_LOG_INFO ("Ipv4 packet interceptor added");
		std::cout << "Added packet interceptor!\n";
		ipv4Proto->AddPacketInterceptor (MakeCallback (&FYPApp::PacketIntercept, this), UdpL4Protocol::PROT_NUMBER);
	}
	else
	{
		std::cout << "Did not add packet interceptor!\n";
		NS_LOG_INFO ("No Ipv4 with packet intercept facility");
	}
	std::cout << "Starting the application\n";
}

void FYPApp::StopApplication()
{
	std::cout << "Stopping the application\n";
}
