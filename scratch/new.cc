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
NodeContainer gatewayNode;
int main(int argc, char * argv[])
{

	CommandLine cmd;
	cmd.Parse(argc, argv);

	//al of the nodes in the simulation
	//NodeContainer gatewayNode;
	gatewayNode.Create(1);
	NodeContainer satelliteNode;
	satelliteNode.Create(1);
	NodeContainer HQNode;
	HQNode.Create(1);
	NodeContainer wimaxBSNode;
	wimaxBSNode.Create(1);

	NodeContainer wifiNodes;
	wifiNodes.Create(2);

	//Add the wimax base and HQ to csma nodes
	NodeContainer csmaNodes;
	csmaNodes.Add(wimaxBSNode.Get(0));
	csmaNodes.Add(HQNode.Get(0));

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
	p2pNodes.Add(wimaxBSNode.Get(0));
	NetDeviceContainer p2pdevs = pointToPoint.Install(p2pNodes);

	PointToPointHelper satpointToPoint;
	satpointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
	satpointToPoint.SetChannelAttribute ("Delay", StringValue ("600ms"));
	NodeContainer satp2pNodes;
	satp2pNodes.Add(gatewayNode.Get(0));
	satp2pNodes.Add(satelliteNode.Get(0));
	NetDeviceContainer satp2pdevs = satpointToPoint.Install(satp2pNodes);

	PointToPointHelper HQsatpointToPoint;
	HQsatpointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
	HQsatpointToPoint.SetChannelAttribute ("Delay", StringValue ("600ms"));
	NodeContainer HQsatp2pNodes;
	HQsatp2pNodes.Add(satelliteNode.Get(0));
	HQsatp2pNodes.Add(HQNode.Get(0));
	NetDeviceContainer HQsatp2pdevs = HQsatpointToPoint.Install(HQsatp2pNodes);

	//set up the csma properties
	CsmaHelper csma;
	csma.SetChannelAttribute("DataRate", StringValue("100Mbps"));
	csma.SetChannelAttribute("Delay", TimeValue(NanoSeconds(6500)));
	//the csma devices
	NetDeviceContainer csmaDevs;
	csmaDevs = csma.Install(csmaNodes);


	InternetStackHelper stack;
	stack.Install(wimaxBSNode);
	stack.Install(gatewayNode);
	stack.Install(satelliteNode);
	stack.Install(HQNode);
	//stack.Install(wifiNodes);


	Ipv4AddressHelper address;
	address.SetBase("10.1.1.0", "255.255.255.0");
	Ipv4InterfaceContainer p2pinterface = address.Assign(p2pdevs);
	Ipv4InterfaceContainer satp2pinterface  = address.Assign(satp2pdevs);
	Ipv4InterfaceContainer HQsatp2pinterface  = address.Assign(HQsatp2pdevs);
	address.SetBase("10.1.2.0", "255.255.255.0");
	Ipv4InterfaceContainer csmaInterfaces;
	csmaInterfaces = address.Assign(csmaDevs);


	Ipv4StaticRoutingHelper staticRouting;
	Ipv4ListRoutingHelper list;
	NS_LOG_INFO ("Enabling OLSR Routing.");
	OlsrHelper olsr;
	list.Add (staticRouting, 0);
	list.Add (olsr, 10);
	InternetStackHelper internet;
	internet.SetRoutingHelper (list);
	internet.Install (wifiNodes);

	Ipv4InterfaceContainer wifiInterfaces = address.Assign(wifiStaDevs);
	Ipv4InterfaceContainer wifiApInterface = address.Assign(wifiApDev);

	address.SetBase("10.1.3.0", "255.255.255.0");



	Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
	Ipv4GlobalRoutingHelper::RecomputeRoutingTables();

	Ptr<Node> t_node = wifiNodes.Get(0);
	Ipv4StaticRoutingHelper helper;
	Ptr<Ipv4> ipv4 = t_node->GetObject<Ipv4>();
	Ptr<Ipv4StaticRouting> Ipv4stat = helper.GetStaticRouting(ipv4);
	Ipv4stat->SetDefaultRoute("10.1.2.5",1,-10);
	Ptr<Node> t_node2 = wifiNodes.Get(1);
	Ipv4StaticRoutingHelper helper2;
	Ptr<Ipv4> ipv42 = t_node2->GetObject<Ipv4>();
	Ptr<Ipv4StaticRouting> Ipv4stat2 = helper2.GetStaticRouting(ipv42);
	Ipv4stat2->SetDefaultRoute("10.1.2.5",1,-10);

	/*PacketSinkHelper psink ("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny() ,9));
	ApplicationContainer sinkApp = psink.Install(gatewayNode.Get(0));
	sinkApp.Start(Seconds(0.6));
	sinkApp.Stop(Seconds(100.0));
	 */

	/*Ptr<FYPApp> myApp = CreateObject<FYPApp>();
	myApp->Setup(gatewayNode.Get(0));
	gatewayNode.Get(0)->AddApplication(myApp);
	myApp->SetStartTime(Seconds(0.1));
	myApp->SetStopTime(Seconds(100.0));*/

	UdpEchoServerHelper echoServer (9);
	ApplicationContainer serverApps = echoServer.Install (gatewayNode.Get (0));
	serverApps.Start (Seconds (1.0));
	serverApps.Stop (Seconds (90.0));

	UdpEchoServerHelper echoServer1 (9);
	ApplicationContainer serverApps1 = echoServer1.Install (satelliteNode.Get (0));
	serverApps1.Start (Seconds (1.0));
	serverApps1.Stop (Seconds (90.0));

	/*UdpEchoClientHelper echoClient (HQsatp2pinterface.GetAddress (0), 9);
	echoClient.SetAttribute ("MaxPackets", UintegerValue (100));
	echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.)));
	echoClient.SetAttribute ("PacketSize", UintegerValue (1024));
	ApplicationContainer clientApps = echoClient.Install (wifiNodes.Get (0));
	clientApps.Start (Seconds (2.0));
	clientApps.Stop (Seconds (80.0));*/

	UdpEchoClientHelper echoClient2 (Ipv4Address("10.1.1.4"), 9);
	echoClient2.SetAttribute ("MaxPackets", UintegerValue (100));
	echoClient2.SetAttribute ("Interval", TimeValue (Seconds (1.)));
	echoClient2.SetAttribute ("PacketSize", UintegerValue (1024));
	ApplicationContainer clientApps2 = echoClient2.Install (wifiNodes.Get (1));
	clientApps2.Start (Seconds (2.0));
	clientApps2.Stop (Seconds (80.0));


	//Config::ConnectWithoutContext ("/NodeList/*/ApplicationList/*/$ns3::PacketSink/Rx",MakeCallback (&packetReceived));


	MobilityHelper mobility;
	Ptr<ListPositionAllocator> positionAlloc = CreateObject <ListPositionAllocator>();
	positionAlloc->Add(Vector(20,20,0));
	//positionAlloc->Add(Vector(1000,0,0));
	//positionAlloc->Add(Vector(450,0,0));
	mobility.SetPositionAllocator(positionAlloc);
	mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
	mobility.Install(HQNode);
	mobility.Install(satelliteNode);
	mobility.Install(wifiNodes);

	MobilityHelper wimaxMobility;
	Ptr<ListPositionAllocator> wimaxPositionAlloc = CreateObject<ListPositionAllocator>();
	wimaxPositionAlloc->Add(Vector(20,20,0));
	wimaxMobility.SetPositionAllocator(wimaxPositionAlloc);
	wimaxMobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
	wimaxMobility.Install(gatewayNode);
	wimaxMobility.Install(wimaxBSNode);

	SetPosition(gatewayNode.Get(0), -50, 0);
	SetPosition(satelliteNode.Get(0), 50, -50.0);
	SetPosition(wimaxBSNode.Get(0), -100, -50.0);
	SetPosition(HQNode.Get(0), -50.0, -100.0);
	SetPosition(wifiNodes.Get(0), -25, 25);
	SetPosition(wifiNodes.Get(1), -50, 50);

	Simulator::Run();

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
