/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*

 */

#include "ns3/command-line.h"
#include "ns3/config.h"
#include "ns3/uinteger.h"
#include "ns3/boolean.h"
#include "ns3/double.h"
#include "ns3/string.h"
#include "ns3/log.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/mobility-helper.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/udp-client-server-helper.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/on-off-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/packet-sink.h"
#include "ns3/yans-wifi-channel.h"
#include "ns3/ipv4-list-routing-helper.h"
#include "ns3/olsr-helper.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/olsr-routing-protocol.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("vht-wifi-network");


void PrintStats (UdpServerHelper *server)
{
  uint32_t cur = server->GetServer ()->GetReceived() ;
  std::fstream f;
  f.open ("testac_123123123123.txt", std::fstream::out | std::fstream::app);
  f << cur << std::endl;
  f.close();
  std::cout << "total packets = " << cur << std::endl;
}


int main (int argc, char *argv[])
{
  bool udp = true;
  bool useRts = true;
  double simulationTime = 600; //seconds
  double distance = 86.7; //meters
  double dbn = 5.0; //meters
  int mcs = -1; // -1 indicates an unset value
  double dbs = 5.0; //meters

  double minExpectedThroughput = 0;
  double maxExpectedThroughput = 0;
  int n = 3;
  std::size_t nStations {3};

  CommandLine cmd (__FILE__);
  cmd.AddValue ("distance", "Distance in meters between the station and the access point", distance);
  cmd.AddValue ("dbn", "Distance in meters between 2 node", dbn);
  cmd.AddValue ("n", "number of nodes", n);
  cmd.AddValue ("nStations", "Number of non-AP HE stations", nStations);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.AddValue ("dbs", "Distance in meters between source nodes", dbs);
  cmd.AddValue ("udp", "UDP if set to 1, TCP otherwise", udp);
  cmd.AddValue ("useRts", "Enable/disable RTS/CTS", useRts);
  cmd.AddValue ("mcs", "if set, limit testing to a specific MCS (0-9)", mcs);
  cmd.AddValue ("minExpectedThroughput", "if set, simulation fails if the lowest throughput is below this value", minExpectedThroughput);
  cmd.AddValue ("maxExpectedThroughput", "if set, simulation fails if the highest throughput is above this value", maxExpectedThroughput);
  cmd.Parse (argc,argv);

  if (useRts)
    {
      Config::SetDefault ("ns3::WifiRemoteStationManager::RtsCtsThreshold", StringValue ("0"));
      Config::SetDefault("ns3::WifiRemoteStationManager::MaxSsrc", UintegerValue (0));
      Config::SetDefault("ns3::WifiRemoteStationManager::MaxSlrc", UintegerValue (0));
    }
  //Time interPacketInterval = Seconds (0.001);
  Config::SetDefault ("ns3::WifiRemoteStationManager::NonUnicastMode",
                      StringValue ("VhtMcs7"));
  double prevThroughput [8];
  for (uint32_t l = 0; l < 8; l++)
    {
      prevThroughput[l] = 0;
    }
//  std::cout << "MCS value" << "\t\t" << "Channel width" << "\t\t" << "short GI" << "\t\t" << "Number of Packets" << '\n';
  int minMcs = 7;
  int maxMcs = 7;
  if (mcs >= 0 && mcs <= 9)
    {
      minMcs = mcs;
      maxMcs = mcs;
    }
  for (int mcs = minMcs; mcs <= maxMcs; mcs++)
    {
      uint8_t index = 0;
      double previous = 0;
      for (int channelWidth = 80; channelWidth <= 80; )
        {
          if (mcs == 9 && channelWidth == 20)
            {
              channelWidth *= 2;
              continue;
            }
          for (int sgi = 0; sgi <= 0; sgi++)
            {
              uint32_t payloadSize; //1500 byte IP packet
              if (udp)
                {
                  payloadSize = 1250; //bytes
                }
              else
                {
                  payloadSize = 1250; //bytes
                  Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (payloadSize));
                }

              NodeContainer wifiStaNode;
              wifiStaNode.Create (n);


//              YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
              YansWifiChannelHelper wifiChannelHelper;
              wifiChannelHelper.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
              wifiChannelHelper.AddPropagationLoss ("ns3::FriisPropagationLossModel",
                                                    "SystemLoss", DoubleValue(1),
                                                    "Frequency", DoubleValue(5.18e9));
//              phy.SetChannel (wifiChannelHelper.Create ());
              YansWifiPhyHelper phy;
              phy.SetChannel (wifiChannelHelper.Create ());
              phy.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO);
              phy.Set ("TxPowerStart", DoubleValue(16)); // 2,5 W or 33,98 dBm
              phy.Set ("TxPowerEnd", DoubleValue(16)); // 2,5 W or 33,98 dBm
              phy.Set ("RxNoiseFigure", DoubleValue (7));
              phy.Set ("Antennas", UintegerValue(1));

              phy.Set ("ChannelSettings", StringValue ("{0, " + std::to_string (channelWidth)
                                                       + ", BAND_5GHZ, 0}"));

              WifiHelper wifi;
              wifi.SetStandard (WIFI_STANDARD_80211ac);
              wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                            "DataMode",StringValue ("VhtMcs7"),
                                            "ControlMode",StringValue ("VhtMcs0"));
              WifiMacHelper mac;

              //mac.SetType ("ns3::AdhocWifiMac");
              //NetDeviceContainer devices = wifiHelper.Install (phy, wifi, wifiStaNode); //было (wifiPhyHelper, wifiHelper, nodes);

              std::ostringstream oss;
              oss << "VhtMcs" << mcs;
//              wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager","DataMode", StringValue (oss.str ()),
//                                            "ControlMode", StringValue ("VhtMcs0");

              Ssid ssid = Ssid ("ns3-80211ac");

              mac.SetType ("ns3::AdhocWifiMac",
                           "Ssid", SsidValue (ssid));

              NetDeviceContainer staDevice;
              staDevice = wifi.Install (phy, mac, wifiStaNode);

              mac.SetType ("ns3::ApWifiMac",
                           "EnableBeaconJitter", BooleanValue (false),
                           "Ssid", SsidValue (ssid));

             // NetDeviceContainer apDevice;
              //apDevice = wifi.Install (phy, mac, wifiApNode);

              // Set guard interval
      //        Config::Set ("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/HtConfiguration/ShortGuardIntervalSupported", BooleanValue (sgi));

              // mobility.
              MobilityHelper mobility;
              Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator> ();

              positionAlloc->Add (Vector (0.0, 0.0, 0.0));

              positionAlloc->Add (Vector (distance, 0.0, 0.0));
              positionAlloc->Add (Vector (58.0, 0.0, 0.0)); //relay 40.0 for 160MHz, 58.0 for 80MHz, 80.0 for 160MHz

              mobility.SetPositionAllocator (positionAlloc);

              mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");

              for (int f = 0; f<=n-1; f++)
                {
                  mobility.Install (wifiStaNode.Get(f));
                }

              OlsrHelper olsr;
              Ipv4StaticRoutingHelper staticRouting;
              olsr.Set("HelloInterval", TimeValue (Seconds(0.1)));
              //olsr.Set("TcInterval", TimeValue (Seconds(2)));
              //olsr.Set("MidInterval", TimeValue (Seconds(2)));
              //olsr.Set("HnaInterval", TimeValue (Seconds(2)));
              //olsr.Set("Willingness", EnumValue (Seconds(5.0)));

              Ipv4ListRoutingHelper list;
              list.Add (staticRouting, 0);
              list.Add (olsr, 1);

              /* Internet stack*/
              InternetStackHelper stack;
              //stack.Install (wifiApNode);
              stack.SetRoutingHelper (list);
              stack.SetRoutingHelper (list);

              for (int f = 0; f<=n-1; f++)
                {
                  stack.Install (wifiStaNode.Get(f));
                }

              Ipv4AddressHelper address;
              address.SetBase ("192.168.1.0", "255.255.255.0");
              Ipv4InterfaceContainer staNodeInterface;
              //Ipv4InterfaceContainer apNodeInterface;

              staNodeInterface = address.Assign (staDevice);
              //apNodeInterface = address.Assign (apDevice);

              /* Setting applications */
              ApplicationContainer serverApp;



                  //UDP flow
                  uint16_t port = 5005;
                  UdpServerHelper server (port);
                  serverApp = server.Install (wifiStaNode.Get (0));




                  for (int f = 1; f<=n-2; f++)  //(n-2) - for relay, (n-1) without relay
                    {
                      UdpClientHelper client (staNodeInterface.GetAddress (0), port);
                      client.SetAttribute ("MaxPackets", UintegerValue (10000));
                      client.SetAttribute ("Interval", TimeValue (Time ("0.001"))); //packets/s
                      client.SetAttribute ("PacketSize", UintegerValue (payloadSize));
                      ApplicationContainer clientApp = client.Install (wifiStaNode.Get (f));
                      clientApp.Start (Seconds (10.0));
                      clientApp.Stop (Seconds (simulationTime + 1));
                    }


                  serverApp.Start (Seconds (0.0));
                  serverApp.Stop (Seconds (simulationTime +10));

              //Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
              Simulator::Stop (Seconds (simulationTime + 10));
              Simulator::Run ();

              uint64_t rxBytes = 0;
              uint32_t rxPackets = 0;
              uint32_t rxPackets1 = 0;
              uint32_t rxPackets2 = 0;



              if (udp)
                {
                  rxBytes = payloadSize * DynamicCast<UdpServer> (serverApp.Get (0))->GetReceived ();
                  rxPackets = 1 * DynamicCast<UdpServer> (serverApp.Get (0))->GetReceived ();


                }
              else
                {
                  rxBytes = DynamicCast<PacketSink> (serverApp.Get (0))->GetTotalRx ();
                  rxPackets = 1 * DynamicCast<UdpServer> (serverApp.Get (0))->GetReceived ();
                }
              double throughput = (rxBytes * 8) / (simulationTime * 1000000.0); //Mbit/s
              double recpack = rxPackets;
              double recpack1 = rxPackets1;
              double recpack2 = rxPackets2;




              Simulator::Destroy ();

              std::cout << mcs << "\t\t" << channelWidth << " MHz\t\t"  << recpack << " Packets \t\t" << (n-2) << " sources\t  "<< recpack2 << " Packets\t  " << distance << "m \t\t"<< dbs << "m\t" << recpack/((n-2)*10000) << " " << std::endl; // (n-2) - for relay, (n-1) without relay

              //test first element
              if (mcs == 0 && channelWidth == 20 && sgi == 0)
                {
                  if (throughput < -1)
                    {
                      NS_LOG_ERROR ("Obtained throughput " << throughput << " is not expected!");
                      exit (1);
                    }
                }
              //test last element
              if (mcs == 9 && channelWidth == 160 && sgi == 1)
                {
                  if (maxExpectedThroughput > 0 && throughput > maxExpectedThroughput)
                    {
                      NS_LOG_ERROR ("Obtained throughput " << throughput << " is not expected!");
                      exit (1);
                    }
                }
              //test previous throughput is smaller (for the same mcs)
              if (throughput > -1)
                {
                  previous = throughput;
                }
              else
                {
                  NS_LOG_ERROR ("Obtained throughput " << throughput << " is not expected!");
                  exit (1);
                }
              //test previous throughput is smaller (for the same channel width and GI)
              if (throughput > -1)
                {
                  prevThroughput [index] = throughput;
                }
              else
                {
                  NS_LOG_ERROR ("Obtained throughput " << throughput << " is not expected!");
                  exit (1);
                }
              index++;
            }
          channelWidth *= 2;
        }
    }
  return 0;
}
