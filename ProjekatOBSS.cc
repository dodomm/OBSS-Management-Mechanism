/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

// Mrezna topologija:
//
//
//         ------ n2 --- n3----
//      n0|                     |n1
//
// Topologija u odredjenom vremenskom trenutku prelazi u
//
//       -------- n2 --- n3----
//       |         |
//      n0--------n1
//
// - Svi linkovi su wireless IEEE 802.11b sa OSLR routing protokolom
// - n0 i n1 su u prvoj iteraciji izvan opsega
// - n1 se kreće desno i lijevo

// Za vizualizaciju preko netanim je ukljucujemo ovu biblioteku:
#include "ns3/netanim-module.h"

// Ukljucivanje potrebnih biblioteka:
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/olsr-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/mobility-module.h"
#include "ns3/myapp.h"


NS_LOG_COMPONENT_DEFINE ("ProjekatOBSS");

using namespace ns3;



static void
SetPosition (Ptr<Node> node, double x)
{
  Ptr<MobilityModel> mobility = node->GetObject<MobilityModel> ();
  Vector pos = mobility->GetPosition();
  pos.x = x;
  mobility->SetPosition(pos);
}

void
ReceivePacket(Ptr<const Packet> p, const Address & addr)
{
	std::cout << Simulator::Now ().GetSeconds () << "\t" << p->GetSize() <<"\n";
}


int main (int argc, char *argv[])
{
  bool enableFlowMonitor = false;
  std::string phyMode ("DsssRate1Mbps");

  CommandLine cmd;
  cmd.AddValue ("EnableMonitor", "Enable Flow Monitor", enableFlowMonitor);
  cmd.AddValue ("phyMode", "Wifi Phy mode", phyMode);
  cmd.Parse (argc, argv);

//
// Eksplicitno kreiramo 4 cvora koja su prikazana u topologiji
//
  NS_LOG_INFO ("Create nodes.");
  NodeContainer c;
  c.Create(4);

  // Konfigurisemo wifi konekciju
  WifiHelper wifi;

  YansWifiPhyHelper wifiPhy =  YansWifiPhyHelper::Default ();
  wifiPhy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11);

  YansWifiChannelHelper wifiChannel ;
  wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel.AddPropagationLoss ("ns3::TwoRayGroundPropagationLossModel",
	  	  	  	  	  	  	  	    "SystemLoss", DoubleValue(1),
		  	  	  	  	  	  	    "HeightAboveZ", DoubleValue(1.5));

  // Postavljamo vrijednosti parametara propagacije (opseg oko 250 metara)
  wifiPhy.Set ("TxPowerStart", DoubleValue(33));
  wifiPhy.Set ("TxPowerEnd", DoubleValue(33));
  wifiPhy.Set ("TxPowerLevels", UintegerValue(1));
  wifiPhy.Set ("TxGain", DoubleValue(0));
  wifiPhy.Set ("RxGain", DoubleValue(0));
  wifiPhy.Set ("EnergyDetectionThreshold", DoubleValue(-61.8));
  wifiPhy.Set ("CcaMode1Threshold", DoubleValue(-64.8));
 
// Kreiramo wifi kanal sa gore navedenim parametrima
  wifiPhy.SetChannel (wifiChannel.Create ());

  // Postavljamo Ad-Hoc WIFI MAC
  NqosWifiMacHelper wifiMac = NqosWifiMacHelper::Default ();
  wifiMac.SetType ("ns3::AdhocWifiMac");

  // Konfigurisemo 802.11b standard (Kao zamjena za 802.11e standard koji jos uvijek nije implementiran) 
  wifi.SetStandard (WIFI_PHY_STANDARD_80211b);

  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                "DataMode",StringValue(phyMode),
                                "ControlMode",StringValue(phyMode));


  NetDeviceContainer devices;
  devices = wifi.Install (wifiPhy, wifiMac, c);


//  Omogucavamo koristenje OLSR protokola
  OlsrHelper olsr;

  // Instaliramo routing protocol
  Ipv4ListRoutingHelper list;
  list.Add (olsr, 10);

  // Konfigurisemo internet stack
  InternetStackHelper internet;
  internet.SetRoutingHelper (list);
  internet.Install (c);

  // Pridruzujemo IP adrese
  Ipv4AddressHelper ipv4;
  NS_LOG_INFO ("Assign IP Addresses.");
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer ifcont = ipv4.Assign (devices);


  NS_LOG_INFO ("Create Applications.");

  // UDP konekcija od N0 do N1

  uint16_t sinkPort = 6;
  Address sinkAddress (InetSocketAddress (ifcont.GetAddress (1), sinkPort)); // interfejs n1
  PacketSinkHelper packetSinkHelper ("ns3::UdpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), sinkPort));
  ApplicationContainer sinkApps = packetSinkHelper.Install (c.Get (1)); //n1 sink
  sinkApps.Start (Seconds (0.));
  sinkApps.Stop (Seconds (25.));
  

  Ptr<Socket> ns3UdpSocket = Socket::CreateSocket (c.Get (0), UdpSocketFactory::GetTypeId ()); //Izvorisni cvor postavljen na n0

  // Kreiramo UDP na n0
  Ptr<MyApp> app = CreateObject<MyApp> ();
  app->Setup (ns3UdpSocket, sinkAddress, 1040, 100000, DataRate ("250Kbps"));
  c.Get (0)->AddApplication (app);
  app->SetStartTime (Seconds (1.));
  app->SetStopTime (Seconds (15.));




// Postavljamo Mobility za sve cvorove

  MobilityHelper mobility;
  Ptr<ListPositionAllocator> positionAlloc = CreateObject <ListPositionAllocator>();
  positionAlloc ->Add(Vector(100, 200, 0)); // cvor0
  positionAlloc ->Add(Vector(500,200, 0)); // cvor1 -- pocinje na udaljenoj lokaciji
  positionAlloc ->Add(Vector(200, 400, 0)); // cvor2
  positionAlloc ->Add(Vector(400, 400, 0)); // cvor3
  mobility.SetPositionAllocator(positionAlloc);
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(c);

  // trenutak u kojem cvor1 ulazi u opseg signala cvora 0
  Simulator::Schedule (Seconds (5.0), &SetPosition, c.Get (1), 200.0);

  // trenutak u kojem prestaje komunikacija izmedju cvora 0 i cvora 1
  Simulator::Schedule (Seconds (15.0), &SetPosition, c.Get (1), 500.0);

  // Pratimo primljene pakete
  Config::ConnectWithoutContext("/NodeList/*/ApplicationList/*/$ns3::PacketSink/Rx", MakeCallback (&ReceivePacket));

// Trace devices (pcap)
  wifiPhy.EnablePcap ("ProjekatOBSS-dev", devices);


  // Flow Monitor
  Ptr<FlowMonitor> flowmon;
  if (enableFlowMonitor)
    {
      FlowMonitorHelper flowmonHelper;
      flowmon = flowmonHelper.InstallAll ();
    }

//
// 
//
  
// Kreiranje datoteke za spremanje rezultata za animaciju:
  AnimationInterface anim ("ProjekatOBSS.xml");
  
// Postavljanje pozicija cvorova za NetAnim:
  //anim.SetConstantPosition (c.Get(0) ,100.0, 200.0);
  //anim.SetConstantPosition (c.Get(1), 500.0, 200.0);
  //anim.SetConstantPosition (c.Get(2), 200.0, 400.0);
  //anim.SetConstantPosition (c.Get(3), 400.0, 400.0);
NS_LOG_INFO ("Run Simulation.");
  Simulator::Stop (Seconds(18.0));
  Simulator::Run ();
  if (enableFlowMonitor)
    {
	  flowmon->CheckForLostPackets ();
	  flowmon->SerializeToXmlFile("ProjekatOBSS.flowmon", true, true);
    }
  Simulator::Destroy ();
  NS_LOG_INFO ("Done.");
}
