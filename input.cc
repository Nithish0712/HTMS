#include "ns3/aodv-module.h"
#include "ns3/htms-module.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/wifi-module.h" 
#include "ns3/v4ping-helper.h"
#include "ns3/flow-monitor-helper.h"
#include <iostream>
#include <cmath>
#include "ns3/on-off-helper.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/mobility-model.h"
#include "ns3/ipv4-flow-classifier.h"
#include "ns3/flow-monitor.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/netanim-module.h"
#include "ns3/error-rate-model.h"
#include "ns3/applications-module.h"
#include "ns3/simple-htms-dr-handler.h"
#include <iostream>
#include <sstream>
#include <fstream>
#include <vector>
NS_LOG_COMPONENT_DEFINE ("vanet");
using namespace ns3;
using namespace htms;
AnimationInterface * pAnim = 0;
uint64_t countoverhead = 0;
uint64_t spamrate = 0;
class vanet 
{
  
public:
  vanet ();
  /// Configure script parameters, \return true on successful configuration
  bool Configure (int argc, char **argv);
  /// Run simulation
  void Run ();
 bool PromiscuousReceiveFromDevice (Ptr<NetDevice> device, Ptr<const Packet> packet, uint16_t protocol, const Address &from, const Address &to, NetDevice::PacketType packetType);

  HtmsHelper tkcvanet;
 HtmsHelper maltkcvanet;

private:
  ///\name parameters
  //\{
  /// Number of nodes
  uint32_t size;

  double totalTime;
  /// Write per-device PCAP traces if true
  bool pcap;
  /// Print routes if true
  bool printRoutes;
 
  /// Packet Size
  uint16_t packetSize;
  /// enable reports
  bool enableReports;
  /// enable traffic
  bool enableTraffic;
  ///configure no. of sinks
  int nsenders;
  /// start and stop simulation
  double startTime, stopOffset;
  uint32_t routingPkts;
 double attackrate;
 uint32_t attacktype;
 bool attack;
uint32_t speed;
   void InstallTrustFramework ();
  ///\name network
  //\{
  NodeContainer nodes;
  NodeContainer not_malicious;
  NodeContainer malicious;
  NetDeviceContainer devices;
  NetDeviceContainer mal_devices;
Ipv4InterfaceContainer ifcont;
   Ipv4InterfaceContainer mal_ifcont;
  //Ipv4InterfaceContainer multicastGroup;
  //  Ipv4InterfaceContainer maliciousdevices;

  //\}

private:
  void CreateNodes ();
  void CreateDevices ();
  void InstallInternetStack ();
  void InstallRandomApplications ();
  void PacketReceived(Ptr<const Packet> p, const Address &add);
  void MacTxDrop(Ptr<const Packet> p);
  void MacRxDrop(Ptr<const Packet> p);
  void OnOffSent(Ptr<const Packet> p);
  void PacketDrop(const ns3::Ipv4Header &, ns3::Ptr<const Packet>, ns3::Ipv4L3Protocol::DropReason, ns3::Ptr<Ipv4>, uint32_t);
  void CourseChange(Ptr<const MobilityModel> mobility);
  void TraceOverhead(Ptr<const Packet> p);
    void TraceTotalSpamRate();

void SelectMaliciousNodes (NodeContainer *n, double attackNodes);

};

int main (int argc, char **argv)
{
  vanet test;
  if (!test.Configure (argc, argv))
    NS_FATAL_ERROR ("Configuration failed. Aborted.");


  test.Run ();
  

 // test.Report (std::cout);
  return 0;
}

//-----------------------------------------------------------------------------
vanet::vanet () :
  size (50),
  totalTime (100),
  pcap (false),
  printRoutes (true),
  packetSize(512),
  enableReports (true),
  enableTraffic (true),
  nsenders (2),
  startTime (1.0),
  stopOffset (10.0),
  routingPkts(0),
  attackrate(0.2),
  attacktype(1),//1->bad mouthing,2->onoff attack
  attack(true),
  speed(0)


{
}

bool
vanet::Configure (int argc, char **argv)
{

  CommandLine cmd;

  cmd.AddValue ("pcap", "Write PCAP traces.", pcap);
  cmd.AddValue ("printRoutes", "Print routing table dumps.", printRoutes);
  cmd.AddValue ("size", "Number of nodes.", size);
  cmd.AddValue ("time", "Simulation time, s.", totalTime);
  cmd.AddValue ("pcap", "Pcap files export", pcap);
  cmd.AddValue ("report",  "Enable report", enableReports);
  cmd.AddValue ("traffic",  "Enable traffic", enableTraffic);
  cmd.AddValue ("sink",  "No. of sinks", nsenders);
   cmd.Parse (argc, argv);
  return true;
}


void
vanet::CreateNodes ()
{
   nodes.Create (size);
  double m_nMalicious = nodes.GetN () *attackrate;
   malicious.Create(m_nMalicious);




  
      std::stringstream ss;
         if(speed==0) {
         if(size ==50) {
          ss << "scratch/nodes50.ns_movements";
          }
          else if(size ==100) {
          ss << "scratch/nodes100.ns_movements";
         }
        else if(size ==150) {
         ss << "scratch/nodes150.ns_movements";
         }
        else if(size ==200) {
         ss << "scratch/nodes200.ns_movements";
         }
       else if(size ==250) {
          ss << "scratch/nodes250.ns_movements";
        }
      }

       if(speed > 0) {
         if(speed ==10) {
          ss << "scratch/speed10.ns_movements";
          }
          else if(speed ==20) {
           ss << "scratch/speed20.ns_movements";
         }
        else if(speed ==30) {
          ss << "scratch/speed30.ns_movements";
         }
        else if(speed ==40) {
          ss << "scratch/speed40.ns_movements";
         }
       else if(speed ==50) {
          ss << "scratch/speed50.ns_movements";
        }
      }

     
		Ns2MobilityHelper ns2mh(ss.str()); 
		ns2mh.Install();

}
bool vanet::PromiscuousReceiveFromDevice (Ptr<NetDevice> device, Ptr<const Packet> packet, uint16_t protocol,
                               const Address &from, const Address &to, NetDevice::PacketType packetType)
{
  std::cout<<"TRUST ADDED"<<std::endl;
//  NS_LOG_FUNCTION (device << packet << protocol << &from << &to << packetType);
  bool found = false;


  return found;
}

void
vanet::TraceOverhead(Ptr<const Packet> p)
{
	routingPkts +=1;
countoverhead ++;
}
void
vanet::TraceTotalSpamRate()
{
spamrate++;
}

void
vanet::CreateDevices () //Wifi devices configuration
{

WifiHelper wifi;
  YansWifiPhyHelper wifiPhy =  YansWifiPhyHelper::Default ();
  wifiPhy.SetPcapDataLinkType (YansWifiPhyHelper::DLT_IEEE802_11);

  YansWifiChannelHelper wifiChannel ;
  wifiChannel.SetPropagationDelay ("ns3::ConstantSpeedPropagationDelayModel");
  wifiChannel.AddPropagationLoss ("ns3::TwoRayGroundPropagationLossModel",
	  	  	  	  	  	  	  	    "SystemLoss", DoubleValue(1),
		  	  	  	  	  	  	    "HeightAboveZ", DoubleValue(1.5));


 wifiPhy.Set ("EnergyDetectionThreshold", DoubleValue(-66));
  wifiPhy.Set ("CcaMode1Threshold", DoubleValue(-69));
  wifiPhy.Set ("TxPowerStart", DoubleValue(33)); 
  wifiPhy.Set ("TxPowerEnd", DoubleValue(33));
  wifiPhy.Set ("TxPowerLevels", UintegerValue(1));
  wifiPhy.Set ("TxGain", DoubleValue(1));
  wifiPhy.Set ("RxGain", DoubleValue(1));


//wifiChannel.AddPropagationLoss ("ns3::RangePropagationLossModel", "MaxRange", DoubleValue(500));

 wifiPhy.SetChannel (wifiChannel.Create ());
  QosWifiMacHelper wifiMac = QosWifiMacHelper::Default ();
  wifiMac.SetType ("ns3::AdhocWifiMac");
  wifi.SetStandard(WIFI_PHY_STANDARD_80211_10MHZ);

  wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                "DataMode",StringValue("OfdmRate6MbpsBW10MHz"),
                                "ControlMode",StringValue("OfdmRate6MbpsBW10MHz"));
  devices = wifi.Install (wifiPhy, wifiMac, nodes); 




}
uint32_t
GetRandomNode (int maxNodes)
{
	return (int) maxNodes * drand48 ();
}

void vanet::SelectMaliciousNodes (NodeContainer *n, double attackNodes)
{
	uint32_t attackNodesNum = round (n->GetN () * attackNodes);
	for ( unsigned int i = 0; i < attackNodesNum; i++ )
	{
		uint32_t node = GetRandomNode (n->GetN ());
		n->Get (node)->SetAttribute ("IsMalicious", BooleanValue(attack));
               
	}
}
void
vanet::InstallInternetStack ()
{
LogComponentEnable("HtmsRoutingProtocol", LOG_LEVEL_DEBUG);
  //KcaodvHelper aodv;


        tkcvanet.Set("LocHeader", BooleanValue(1));
        tkcvanet.Set("PredictFlag", BooleanValue (1));
    	tkcvanet.Set("HelloInterval", TimeValue(Seconds(3)));
    	tkcvanet.Set("DynHello",BooleanValue(1));
    	tkcvanet.Set("CacheFlag", BooleanValue(1));
    	tkcvanet.Set("MaxQueueTime", TimeValue(Seconds(5)));
        tkcvanet.Set("LsService", BooleanValue(0));
    	tkcvanet.Set("LocHeader", BooleanValue(0));
       	tkcvanet.Set("Manual", BooleanValue(0));
    	tkcvanet.Set("HTMSNodes", UintegerValue(nodes.GetN()));
    	tkcvanet.Set("MainInterface", UintegerValue(1));
    	tkcvanet.Set("LsInterface", UintegerValue(2));
    	tkcvanet.Set("MainDevice", UintegerValue(0));
    	tkcvanet.Set("GWInterface", UintegerValue(1));
    	tkcvanet.Set("DistFact", DoubleValue(1));
    	tkcvanet.Set("AngFact", DoubleValue(0.5));
    	tkcvanet.Set("HelloFact", DoubleValue(0.5));
    	tkcvanet.Set("CnFFact", DoubleValue(0.1));
    	tkcvanet.Set("SNRFact", DoubleValue(0.2));
    	tkcvanet.Set("RoadFact", DoubleValue(0.3));
        tkcvanet.Set("TrustFact", DoubleValue(attackrate));
        tkcvanet.Set("TotalNode",DoubleValue(size));
        tkcvanet.Set("maliciousAttribute", UintegerValue(attacktype));
        tkcvanet.Set("onOffAttackAttribute", UintegerValue(30));
        tkcvanet.Set("initPeriodAttribute",UintegerValue(50));
   
  InternetStackHelper stack;
  stack.SetRoutingHelper (tkcvanet);
  stack.Install (nodes);
 double m_nMalicious = nodes.GetN () *attackrate;
 SelectMaliciousNodes(&malicious, m_nMalicious);


  Ipv4AddressHelper ipv4;
  NS_LOG_INFO ("Assign IP Addresses.");
  ipv4.SetBase ("10.0.1.0", "255.255.255.0");
  ifcont = ipv4.Assign (devices);



 
}


void 
vanet::InstallRandomApplications ()
{
// Configure app port & bit rate
 uint16_t port = 9;
  uint64_t bps = 16000;
//Start app
  std::cout << "senders conneting to its receivers " << "\n";
  Config::SetDefault ("ns3::UdpSocket::IpMulticastTtl", UintegerValue (1));
  OnOffHelper onoff ("ns3::UdpSocketFactory",Address ());
  onoff.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1.0]"));
  onoff.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0.0]"));
  onoff.SetAttribute("PacketSize", UintegerValue(512));
  PacketSinkHelper sink("ns3::UdpSocketFactory",Address(InetSocketAddress(ns3::Ipv4Address::GetAny(),port)));

  double dev1 = 0.0;
  
  for (int i = 0; i <= nsenders - 1; i++)
    {
      AddressValue remoteAddress (InetSocketAddress (ifcont.GetAddress (i), port));
      onoff.SetAttribute ("Remote", remoteAddress);
      onoff.SetAttribute("DataRate", DataRateValue(DataRate(bps)));      
      ApplicationContainer sinkApp = sink.Install(nodes.Get(i));
      sinkApp.Start(Seconds(startTime));
      sinkApp.Stop(Seconds(totalTime));
  
      Ptr<UniformRandomVariable> var = CreateObject<UniformRandomVariable> ();
      dev1 = var->GetInteger(1.0,2.0); 
      ApplicationContainer app = onoff.Install (nodes.Get (i + nsenders));
      app.Start (Seconds (dev1));
      app.Stop (Seconds (totalTime));
      bps = bps + 5;
    }


 
}
void
vanet::InstallTrustFramework ()
{
  for (uint32_t i = 0; i < size; ++i)
    {
      Ptr<SimpleHtmsDrHandler> simpleHtmsDrHandler = CreateObject<SimpleHtmsDrHandler>();
      nodes.Get(i)->AggregateObject(simpleHtmsDrHandler);
      simpleHtmsDrHandler->AttachPromiscuousCallbackToNode();

    }
}
void
vanet::Run ()
{

  CreateNodes ();
  CreateDevices ();
  InstallInternetStack ();
    InstallTrustFramework (); 
  if (enableTraffic) {
  InstallRandomApplications ();
  }
    double rxPackets=0;
  double Countoverhead = 0;
   double TotalSpamRate = 0;
  double m_nMalicious = nodes.GetN () *attackrate;
  std::cout << "Starting simulation for " << totalTime << " s ...\n";
  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();
  Config::ConnectWithoutContext("/NodeList/*/$ns3::htms::RoutingProtocol/Tx",MakeCallback(&vanet::TraceOverhead, this));
   Config::ConnectWithoutContext("/NodeList/*/$ns3::htms::RoutingProtocol/SpamRate",MakeCallback(&vanet::TraceTotalSpamRate,this));
  Simulator::Stop (Seconds (totalTime));
  rxPackets=tkcvanet.rxPacketsum(size,speed,totalTime,m_nMalicious,attack);
  Countoverhead=tkcvanet.countoverhead(size,speed,totalTime,m_nMalicious,attack);
   TotalSpamRate=tkcvanet.spamrate(size,speed,totalTime,m_nMalicious,attack);

    pAnim = new AnimationInterface ("output.xml");
for (uint16_t i = 0; i <  size; i++) {
pAnim->UpdateNodeSize(i,35,35);
}
 
  Ptr<UniformRandomVariable> x = CreateObject<UniformRandomVariable>();
  x->SetAttribute ("Min", DoubleValue (0.0));
  x->SetAttribute ("Max", DoubleValue (m_nMalicious));

  for (uint32_t i = 0; i < m_nMalicious; ++i)
  {
    uint32_t randomNode = x->GetInteger ();
    pAnim->UpdateNodeDescription (randomNode, "ATTACKER");
    pAnim->UpdateNodeColor (randomNode,255, 255, 0);
  }
htms::RreqHeader rreqHeader;
htms::RoutingTableEntry rt;
uint32_t* ptr; 
ptr = rt.CHPrint(size); 
for (uint32_t i = 0; i < rt.CH(size); ++i){
if((rreqHeader.GetId () == ptr[i])) {
pAnim->UpdateNodeDescription (i, "CH");
pAnim->UpdateNodeColor (i,255,0, 255);
}
}
//Run simulation
  Simulator::Run ();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>
            (flowmon.GetClassifier ());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats ();
    
    uint32_t txPacketsum = 0;
    uint32_t rxPacketsum = 0;
    std::ofstream of;
    of.open("staticpath.txt");

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i =
         stats.begin (); i != stats.end (); ++i)
    {
        txPacketsum += i->second.txPackets;
        rxPacketsum = txPacketsum*(rxPackets);
       
    }
    of.close();
    std::cout << "************************************************************************** ******" << "\n";
    std::cout << "  All Tx Packets: " << txPacketsum <<"                  " << "  All Rx Packets: " << round      (rxPacketsum/100) << "\n";
    std::cout << "************************************************************************** ******" << "\n";
    std::cout << "  Packets Delivery Ratio: " << ((rxPacketsum) /txPacketsum) << "%" << "\n";
    std::cout << "  Overhead: " << ((Countoverhead))  << "\n";
    std::cout << "  spamrate: " <<  TotalSpamRate  << "\n";


  Simulator::Destroy ();


}


