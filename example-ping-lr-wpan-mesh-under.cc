#include <fstream>
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/internet-apps-module.h"
#include "ns3/mobility-module.h"
#include "ns3/spectrum-module.h"
#include "ns3/propagation-module.h"
#include "ns3/sixlowpan-module.h"
#include "ns3/lr-wpan-module.h"
#include "ns3/csma-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/applications-module.h"

#define TCP_SINK_PORT 9000
#define UDP_SINK_PORT 9001

//experimental parameters
#define MAX_BULK_BYTES 100000
#define DDOS_RATE "20480kb/s"
#define MAX_SIMULATION_TIME 10.0

//Number of Bots for DDoS
#define NUMBER_OF_BOTS 10

using namespace ns3;
NS_LOG_COMPONENT_DEFINE("DDoSAttack");

int main (int argc, char** argv)
{
  bool verbose = false;

  Packet::EnablePrinting ();

  CommandLine cmd (__FILE__);
  cmd.AddValue ("verbose", "turn on log components", verbose);
  cmd.Parse (argc, argv);

  if (verbose)
    {
      LogComponentEnable ("Ping6Application", LOG_LEVEL_ALL);
      LogComponentEnable ("LrWpanMac", LOG_LEVEL_ALL);
      LogComponentEnable ("LrWpanPhy", LOG_LEVEL_ALL);
      LogComponentEnable ("LrWpanNetDevice", LOG_LEVEL_ALL);
      LogComponentEnable ("SixLowPanNetDevice", LOG_LEVEL_ALL);
    }

  uint32_t nWsnNodes = 4;
  NodeContainer wsnNodes;
  wsnNodes.Create (nWsnNodes);

  NodeContainer wiredNodes;
  wiredNodes.Create (1);
  wiredNodes.Add (wsnNodes.Get (0));

    //Nodes for attack bots
    NodeContainer botNodes;
    botNodes.Create(NUMBER_OF_BOTS);
    botNodes.Add (wsnNodes.Get (0));

  MobilityHelper mobility;
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (80),
                                 "DeltaY", DoubleValue (80),
                                 "GridWidth", UintegerValue (10),
                                 "LayoutType", StringValue ("RowFirst"));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (botNodes);

  LrWpanHelper lrWpanHelperbot;
  // Add and install the LrWpanNetDevice for each node
  NetDeviceContainer lrwpanDevicesbot = lrWpanHelperbot.Install (botNodes);

  // Fake PAN association and short address assignment.
  // This is needed because the lr-wpan module does not provide (yet)
  // a full PAN association procedure.
  lrWpanHelperbot.AssociateToPan (lrwpanDevicesbot, 0);

  mobility.Install (wsnNodes);

  LrWpanHelper lrWpanHelper;
  // Add and install the LrWpanNetDevice for each node
  NetDeviceContainer lrwpanDevices = lrWpanHelper.Install (wsnNodes);

  // Fake PAN association and short address assignment.
  // This is needed because the lr-wpan module does not provide (yet)
  // a full PAN association procedure.
  lrWpanHelper.AssociateToPan (lrwpanDevices, 0);

  InternetStackHelper internetv6;
  //internetv6.Install (wsnNodes);
  //internetv6.Install (wiredNodes.Get (0));
  //internetv6.Install (botNodes);
  internetv6.InstallAll();

  SixLowPanHelper sixLowPanHelper;
  NetDeviceContainer sixLowPanDevices = sixLowPanHelper.Install (lrwpanDevices);

  SixLowPanHelper sixLowPanHelperbot;
  NetDeviceContainer sixLowPanDevicesbot = sixLowPanHelperbot.Install (lrwpanDevicesbot);

  CsmaHelper csmaHelper;
  NetDeviceContainer csmaDevices = csmaHelper.Install (wiredNodes);

  Ipv6AddressHelper ipv6;
  ipv6.SetBase (Ipv6Address ("2001:1::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer wiredDeviceInterfaces;
  wiredDeviceInterfaces = ipv6.Assign (csmaDevices);
  wiredDeviceInterfaces.SetForwarding (1, true);
  wiredDeviceInterfaces.SetDefaultRouteInAllNodes (1);

  ipv6.SetBase (Ipv6Address ("2001:2::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer wsnDeviceInterfaces;
  wsnDeviceInterfaces = ipv6.Assign (sixLowPanDevices);
  wsnDeviceInterfaces.SetForwarding (0, true);
  wsnDeviceInterfaces.SetDefaultRouteInAllNodes (0);

  for (uint32_t i = 0; i < sixLowPanDevices.GetN (); i++)
    {
      Ptr<NetDevice> dev = sixLowPanDevices.Get (i);
      dev->SetAttribute ("UseMeshUnder", BooleanValue (true));
      dev->SetAttribute ("MeshUnderRadius", UintegerValue (10));
      NS_LOG_UNCOND("Added to mesh= " << i);
    }

  ipv6.SetBase (Ipv6Address ("2001:3::"), Ipv6Prefix (64));
  Ipv6InterfaceContainer wsnDeviceInterfacesbot;
  wsnDeviceInterfacesbot = ipv6.Assign (sixLowPanDevicesbot);
  wsnDeviceInterfacesbot.SetForwarding (1, true);
  wsnDeviceInterfacesbot.SetDefaultRouteInAllNodes (1);

  for (uint32_t i = 0; i < sixLowPanDevicesbot.GetN (); i++)
    {
      Ptr<NetDevice> devbot = sixLowPanDevicesbot.Get (i);
      devbot->SetAttribute ("UseMeshUnder", BooleanValue (true));
      devbot->SetAttribute ("MeshUnderRadius", UintegerValue (20));
      NS_LOG_UNCOND("Added bot to mesh= " << i);
    }

  uint32_t packetSize = 10;
  uint32_t maxPacketCount = 5;
  Time interPacketInterval = Seconds (1.);
  Ping6Helper ping6;

  ping6.SetLocal (wsnDeviceInterfaces.GetAddress (nWsnNodes - 1, 1));
  ping6.SetRemote (wiredDeviceInterfaces.GetAddress (0, 1));

  ping6.SetAttribute ("MaxPackets", UintegerValue (maxPacketCount));
  ping6.SetAttribute ("Interval", TimeValue (interPacketInterval));
  ping6.SetAttribute ("PacketSize", UintegerValue (packetSize));
  ApplicationContainer apps = ping6.Install (wsnNodes.Get (nWsnNodes - 1));

  apps.Start (Seconds (1.0));
  apps.Stop (Seconds (10.0));


    // Sender Application (Packets generated by this application are throttled)
    BulkSendHelper bulkSend("ns3::TcpSocketFactory", Inet6SocketAddress(wiredDeviceInterfaces.GetAddress (0, 1), TCP_SINK_PORT));
    bulkSend.SetAttribute("MaxBytes", UintegerValue(MAX_BULK_BYTES));
    ApplicationContainer bulkSendApp = bulkSend.Install(wsnNodes.Get (nWsnNodes - 1));
    bulkSendApp.Start(Seconds(0.0));
    bulkSendApp.Stop(Seconds(MAX_SIMULATION_TIME - 10));

    // TCP Sink Application on server side
    PacketSinkHelper TCPsink("ns3::TcpSocketFactory",
                             Inet6SocketAddress(Ipv6Address::GetAny(), TCP_SINK_PORT));
    ApplicationContainer TCPSinkApp = TCPsink.Install(wiredNodes.Get(0));
    TCPSinkApp.Start(Seconds(0.0));
    TCPSinkApp.Stop(Seconds(MAX_SIMULATION_TIME));

// DDoS Application Behaviour
    OnOffHelper onoff("ns3::UdpSocketFactory", Address(Inet6SocketAddress(wiredDeviceInterfaces.GetAddress (0, 1), UDP_SINK_PORT)));
    onoff.SetConstantRate(DataRate(DDOS_RATE));
    onoff.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=30]"));
    onoff.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    ApplicationContainer onOffApp[NUMBER_OF_BOTS];

    //Install application in all bots
    for (int k = 0; k < NUMBER_OF_BOTS; ++k)
    {
        onOffApp[k] = onoff.Install(botNodes.Get(k));
        onOffApp[k].Start(Seconds(0.0));
        onOffApp[k].Stop(Seconds(MAX_SIMULATION_TIME));
    }
 // UDPSink on receiver side
    PacketSinkHelper UDPsink("ns3::UdpSocketFactory",
                             Address(Inet6SocketAddress(Ipv6Address::GetAny(), UDP_SINK_PORT)));
    ApplicationContainer UDPSinkApp = UDPsink.Install(wiredNodes.Get(1));
    UDPSinkApp.Start(Seconds(0.0));
    UDPSinkApp.Stop(Seconds(MAX_SIMULATION_TIME));

// 8. Install FlowMonitor on all nodes
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll ();
  Simulator::Stop (Seconds (10));

 //lrWpanHelper.EnablePcapAll ("third");
//wsnNodes.Get (0)


  Simulator::Run ();
 // Print per flow statistics
  monitor->CheckForLostPackets (); 
      Ptr<Ipv6FlowClassifier> classifier = DynamicCast<Ipv6FlowClassifier> (flowmon.GetClassifier6 ());
      std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();

      for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator iter = stats.begin (); iter != stats.end (); ++iter)
        {
          Ipv6FlowClassifier::FiveTuple t = classifier->FindFlow (iter->first);

          NS_LOG_UNCOND("Flow ID: " << iter->first << " Src Addr " << t.sourceAddress << " Dst Addr " << t.destinationAddress);
          NS_LOG_UNCOND("Tx Packets = " << iter->second.txPackets);
          NS_LOG_UNCOND("Rx Packets = " << iter->second.rxPackets);
          NS_LOG_UNCOND("Throughput: " << iter->second.rxBytes * 8.0 / (iter->second.timeLastRxPacket.GetSeconds()-iter->second.timeFirstTxPacket.GetSeconds()) / 1024  << " Kbps");

        }
  Simulator::Destroy ();

}

