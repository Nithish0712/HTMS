#define NS_LOG_APPEND_CONTEXT                                   \
  if (GetObject<Node> ()) { std::clog << "[node " << GetObject<Node> ()->GetId () << "] "; }

#include "aodv-routing-protocol.h"
#include "ns3/log.h"
#include "ns3/boolean.h"
#include "ns3/random-variable-stream.h"
#include "ns3/inet-socket-address.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/wifi-net-device.h"
#include "ns3/adhoc-wifi-mac.h"
#include "ns3/string.h"
#include "ns3/pointer.h"
#include <algorithm>
#include <limits>

NS_LOG_COMPONENT_DEFINE ("AodvRoutingProtocol");

namespace ns3
{
namespace aodvmesh
{
NS_OBJECT_ENSURE_REGISTERED (RoutingProtocol);

/// UDP Port for AODV control traffic
const uint32_t RoutingProtocol::AODV_PORT = 654;


//-----------------------------------------------------------------------------
/// Tag used by AODV implementation
struct DeferredRouteOutputTag : public Tag
{
  /// Positive if output device is fixed in RouteOutput
  int32_t oif;

  DeferredRouteOutputTag (int32_t o = -1) : Tag (), oif (o) {}

  static TypeId GetTypeId ()
  {
    static TypeId tid = TypeId ("ns3::aodvmesh::DeferredRouteOutputTag").SetParent<Tag> ();
    return tid;
  }

  TypeId  GetInstanceTypeId () const 
  {
    return GetTypeId ();
  }

  uint32_t GetSerializedSize () const
  {
    return sizeof(int32_t);
  }
  
  void  Serialize (TagBuffer i) const
  {
    i.WriteU32 (oif);
  }

  void  Deserialize (TagBuffer i)
  {
    oif = i.ReadU32 ();
  }

  void  Print (std::ostream &os) const
  {
    os << "DeferredRouteOutputTag: output interface = " << oif;
  }
};

//-----------------------------------------------------------------------------
RoutingProtocol::RoutingProtocol () :
  RreqRetries (2),
  RreqRateLimit (10),
  RerrRateLimit (10),
  ActiveRouteTimeout (Seconds (3)),
  NetDiameter (35),
  NodeTraversalTime (MilliSeconds (40)),
  NetTraversalTime (Time ((2 * NetDiameter) * NodeTraversalTime)),
  PathDiscoveryTime ( Time (2 * NetTraversalTime)),
  MyRouteTimeout (Time (2 * std::max (PathDiscoveryTime, ActiveRouteTimeout))),
  HelloInterval (Seconds (2)),
  AllowedHelloLoss (1),
  DeletePeriod (Time (5 * std::max (ActiveRouteTimeout, HelloInterval))),
  NextHopWait (NodeTraversalTime + MilliSeconds (10)),
  TimeoutBuffer (2),
  BlackListTimeout (Time (RreqRetries * NetTraversalTime)),
  MaxQueueLen (64),
  MaxQueueTime (Seconds (30)),
  DestinationOnly (false),
  GratuitousReply (true),
  EnableHello (true),
  m_routingTable (DeletePeriod),
  m_queue (MaxQueueLen, MaxQueueTime),
  m_requestId (0),
  m_seqNo (0),
  m_rreqIdCache (PathDiscoveryTime),
  m_dpd (PathDiscoveryTime),
  m_nb (Seconds(LONG_INTERVAL)),
  m_rreqCount (0),
  m_rerrCount (0),
  m_mainAddress(Ipv4Address::GetAny()),
  m_helloIdCache(Seconds(SHORT_INTERVAL)),
  m_messageSequenceNumber(0),
  m_weightSize(1),
  m_localWeight(0),
  m_localWeightFunction(W_NODE_DEGREE),
  m_localNodeStatus(CORE),
  m_localcore_noncoreIndicator(CONVERT_OTHER),
  m_localAssociatedCORE(Ipv4Address::GetAny()),
  m_localCurrentBnNeighbors(0),
  m_localLastBnNeighbors(0),
  Rule1(true),
  Rule2(true),
  m_htimer (Timer::CANCEL_ON_DESTROY),
  m_ltimer (Timer::CANCEL_ON_DESTROY),
  m_rreqRateLimitTimer (Timer::CANCEL_ON_DESTROY)
  {
	if (EnableHello)
	{
	  m_nb.SetCallback (MakeCallback (&RoutingProtocol::SendRerrWhenBreaksLinkToNextHop, this));
	}
  }


TypeId
RoutingProtocol::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::aodvmesh::RoutingProtocol")
    .SetParent<Ipv4RoutingProtocol> ()
    .AddConstructor<RoutingProtocol> ()
    .AddAttribute ("HelloInterval", "HELLO messages emission interval.",
                   TimeValue (Seconds (1)),
                   MakeTimeAccessor (&RoutingProtocol::HelloInterval),
                   MakeTimeChecker ())
    .AddAttribute ("RreqRetries", "Maximum number of retransmissions of RREQ to discover a route",
                   UintegerValue (2),
                   MakeUintegerAccessor (&RoutingProtocol::RreqRetries),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("RreqRateLimit", "Maximum number of RREQ per second.",
                   UintegerValue (10),
                   MakeUintegerAccessor (&RoutingProtocol::RreqRateLimit),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("RerrRateLimit", "Maximum number of RERR per second.",
                   UintegerValue (10),
                   MakeUintegerAccessor (&RoutingProtocol::RerrRateLimit),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("NodeTraversalTime", "Conservative estimate of the average one hop traversal time for packets and should include "
                   "queuing delays, interrupt processing times and transfer times.",
                   TimeValue (MilliSeconds (40)),
                   MakeTimeAccessor (&RoutingProtocol::NodeTraversalTime),
                   MakeTimeChecker ())
    .AddAttribute ("NextHopWait", "Period of our waiting for the neighbour's RREP_ACK = 10 ms + NodeTraversalTime",
                   TimeValue (MilliSeconds (50)),
                   MakeTimeAccessor (&RoutingProtocol::NextHopWait),
                   MakeTimeChecker ())
    .AddAttribute ("ActiveRouteTimeout", "Period of time during which the route is considered to be valid",
                   TimeValue (Seconds (3)),
                   MakeTimeAccessor (&RoutingProtocol::ActiveRouteTimeout),
                   MakeTimeChecker ())
    .AddAttribute ("MyRouteTimeout", "Value of lifetime field in RREP generating by this node = 2 * max(ActiveRouteTimeout, PathDiscoveryTime)",
                   TimeValue (Seconds (11.2)),
                   MakeTimeAccessor (&RoutingProtocol::MyRouteTimeout),
                   MakeTimeChecker ())
    .AddAttribute ("BlackListTimeout", "Time for which the node is put into the blacklist = RreqRetries * NetTraversalTime",
                   TimeValue (Seconds (5.6)),
                   MakeTimeAccessor (&RoutingProtocol::BlackListTimeout),
                   MakeTimeChecker ())
    .AddAttribute ("DeletePeriod", "DeletePeriod is intended to provide an upper bound on the time for which an upstream node A "
                   "can have a neighbor B as an active next hop for destination D, while B has invalidated the route to D."
                   " = 5 * max (HelloInterval, ActiveRouteTimeout)",
                   TimeValue (Seconds (15)),
                   MakeTimeAccessor (&RoutingProtocol::DeletePeriod),
                   MakeTimeChecker ())
    .AddAttribute ("TimeoutBuffer", "Its purpose is to provide a buffer for the timeout so that if the RREP is delayed"
                   " due to congestion, a timeout is less likely to occur while the RREP is still en route back to the source.",
                   UintegerValue (2),
                   MakeUintegerAccessor (&RoutingProtocol::TimeoutBuffer),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("NetDiameter", "Net diameter measures the maximum possible number of hops between two nodes in the network",
                   UintegerValue (35),
                   MakeUintegerAccessor (&RoutingProtocol::NetDiameter),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("NetTraversalTime", "Estimate of the average net traversal time = 2 * NodeTraversalTime * NetDiameter",
                   TimeValue (Seconds (2.8)),
                   MakeTimeAccessor (&RoutingProtocol::NetTraversalTime),
                   MakeTimeChecker ())
    .AddAttribute ("PathDiscoveryTime", "Estimate of maximum time needed to find route in network = 2 * NetTraversalTime",
                   TimeValue (Seconds (5.6)),
                   MakeTimeAccessor (&RoutingProtocol::PathDiscoveryTime),
                   MakeTimeChecker ())
    .AddAttribute ("MaxQueueLen", "Maximum number of packets that we allow a routing protocol to buffer.",
                   UintegerValue (64),
                   MakeUintegerAccessor (&RoutingProtocol::SetMaxQueueLen,
                                         &RoutingProtocol::GetMaxQueueLen),
                   MakeUintegerChecker<uint32_t> ())
    .AddAttribute ("MaxQueueTime", "Maximum time packets can be queued (in seconds)",
                   TimeValue (Seconds (30)),
                   MakeTimeAccessor (&RoutingProtocol::SetMaxQueueTime,
                                     &RoutingProtocol::GetMaxQueueTime),
                   MakeTimeChecker ())
    .AddAttribute ("AllowedHelloLoss", "Number of hello messages which may be loss for valid link.",
                   UintegerValue (1),
                   MakeUintegerAccessor (&RoutingProtocol::AllowedHelloLoss),
                   MakeUintegerChecker<uint16_t> ())
    .AddAttribute ("GratuitousReply", "Indicates whether a gratuitous RREP should be unicast to the node originated route discovery.",
                   BooleanValue (true),
                   MakeBooleanAccessor (&RoutingProtocol::SetGratuitousReplyFlag,
                                        &RoutingProtocol::GetGratuitousReplyFlag),
                   MakeBooleanChecker ())
    .AddAttribute ("DestinationOnly", "Indicates only the destination may respond to this RREQ.",
                   BooleanValue (false),
                   MakeBooleanAccessor (&RoutingProtocol::SetDesinationOnlyFlag,
                                        &RoutingProtocol::GetDesinationOnlyFlag),
                   MakeBooleanChecker ())
    .AddAttribute ("EnableHello", "Indicates whether a hello messages enable.",
                   BooleanValue (true),
                   MakeBooleanAccessor (&RoutingProtocol::SetHelloEnable,
                                        &RoutingProtocol::GetHelloEnable),
                   MakeBooleanChecker ())
    .AddAttribute ("EnableBroadcast", "Indicates whether a broadcast data packets forwarding enable.",
                   BooleanValue (true),
                   MakeBooleanAccessor (&RoutingProtocol::SetBroadcastEnable,
                                        &RoutingProtocol::GetBroadcastEnable),
                   MakeBooleanChecker ())
	  .AddAttribute ("Rule1", "Indicates whether the BCN-to-BN conversion rule 1 is applied or not.",
					 BooleanValue (true),
					 MakeBooleanAccessor (&RoutingProtocol::SetRule1,
										  &RoutingProtocol::GetRule1),
					 MakeBooleanChecker ())
	  .AddAttribute ("Rule2", "Indicates whether the BCN-to-BN conversion rule 2 is applied or not.",
					 BooleanValue (true),
					 MakeBooleanAccessor (&RoutingProtocol::SetRule2,
										  &RoutingProtocol::GetRule2),
					 MakeBooleanChecker ())
	 .AddAttribute ("localNodeStatus", "Indicates the status of the node.",
					 EnumValue(CORE),
					 MakeEnumAccessor(&RoutingProtocol::SetLocalNodeStatus,
									  &RoutingProtocol::GetLocalNodeStatus),
					 MakeEnumChecker(RN_NODE,"Regular Node",
							 CORE,"Backbone Capable Node",
							 NEIGH_NODE,"Backbone Node") )
	 .AddAttribute ("localWeightFunction", "Indicates the weight function to use.",
					 EnumValue (W_NODE_DEGREE),
					 MakeEnumAccessor(&RoutingProtocol::SetLocalWeightFunction,
									  &RoutingProtocol::GetLocalWeightFunction),
					 MakeEnumChecker(W_NODE_DEGREE,"Node Degree",
							 W_NODE_IP,"Node IP",
							 W_NODE_RND,"Node RND Value",
							 W_NODE_BNDEGREE,"Node BN size"))
	  .AddAttribute ("localWeight", "Node's weight",
					 UintegerValue (0),
					 MakeUintegerAccessor (&RoutingProtocol::SetLocalWeight,
							 	 	 	   &RoutingProtocol::GetLocalWeight),
					 MakeUintegerChecker<uint32_t> ())
	 .AddAttribute ("ShortInterval", "Short Interval of Time for Hellos.",
					 TimeValue (Seconds (SHORT_INTERVAL)),
					 MakeTimeAccessor (&RoutingProtocol::SetShortInterval,
							 	 	   &RoutingProtocol::GetShortInterval),
					 MakeTimeChecker ())
	 .AddAttribute ("LongInterval", "Long Interval of Time for BCN2BN algorithm and viceversa.",
					 TimeValue (Seconds (LONG_INTERVAL)),
					 MakeTimeAccessor (&RoutingProtocol::SetLongInterval,
							 	 	   &RoutingProtocol::GetLongInterval),
					 MakeTimeChecker ())
	 .AddTraceSource ("NodeStatusChanged", "The AODVMESH node's status has changed.",
			 	 	 MakeTraceSourceAccessor(&RoutingProtocol::m_localNodeStatusTrace))
    .AddTraceSource ("ControlMessageTrafficSent", "Control message traffic sent.",
				   MakeTraceSourceAccessor(&RoutingProtocol::m_txPacketTrace))
    .AddTraceSource ("ControlMessageTrafficReceived", "Control message traffic received.",
	  			   MakeTraceSourceAccessor(&RoutingProtocol::m_rxPacketTrace))
    .AddAttribute ("UniformRv",
                   "Access to the underlying UniformRandomVariable",
                   StringValue ("ns3::UniformRandomVariable"),
                   MakePointerAccessor (&RoutingProtocol::m_uniformRandomVariable),
                   MakePointerChecker<UniformRandomVariable> ())
   .AddTraceSource ("Tx", "A new routing protocol packet is created and is sent or retransmitted", // trace
                     MakeTraceSourceAccessor (&RoutingProtocol::m_txTrace),
                     "ns3::Packet::TracedCallback")    
    .AddTraceSource ("Rx", "A new routing protocol packet is received", // trace
                     MakeTraceSourceAccessor (&RoutingProtocol::m_rxTrace),
                     "ns3::Packet::TracedCallback")
.AddAttribute ("IsMalicious", "Is the node malicious",
                   BooleanValue (false),
                   MakeBooleanAccessor (&RoutingProtocol::SetMaliciousEnable,
                                        &RoutingProtocol::GetMaliciousEnable),
                   MakeBooleanChecker ())


  ;
  return tid;
}

void
RoutingProtocol::NotifyNodeStatusChanged (void) const
{
	m_localNodeStatusTrace(this);
}

void
RoutingProtocol::SetMaxQueueLen (uint32_t len)
{
  MaxQueueLen = len;
  m_queue.SetMaxQueueLen (len);
}
void
RoutingProtocol::SetMaxQueueTime (Time t)
{
  MaxQueueTime = t;
  m_queue.SetQueueTimeout (t);
}

RoutingProtocol::~RoutingProtocol ()
{
}

void
RoutingProtocol::DoDispose ()
{
  m_ipv4 = 0;
  for (std::map<Ptr<Socket>, Ipv4InterfaceAddress>::iterator iter =
         m_socketAddresses.begin (); iter != m_socketAddresses.end (); iter++)
    {
      iter->first->Close ();
    }
  GetLocalState();
  m_nb.Purge();
  m_socketAddresses.clear ();
  Ipv4RoutingProtocol::DoDispose ();
}

void
RoutingProtocol::PrintRoutingTable (Ptr<OutputStreamWrapper> stream) const
{
  *stream->GetStream () << "Node: " << m_ipv4->GetObject<Node> ()->GetId () << " Time: " << Simulator::Now ().GetSeconds () << "s ";
  m_routingTable.Print (stream);
}

void
RoutingProtocol::GetLocalState ()
{
  NS_LOG_INFO ("Node " << m_mainAddress << ", Status " << (GetLocalNodeStatus()==NEIGH_NODE?"BN":(GetLocalNodeStatus()==CORE?"BCN":"RN"))
		  << ", W=" << GetLocalWeight() << ", F=" << GetLocalWeightFunction() << ", I=" << (m_localcore_noncoreIndicator==CONVERT_BREAK?"BREAK":(m_localcore_noncoreIndicator==CONVERT_OTHER?"OTHER":"ALLOW"))
		  << ", AssBN="<< GetLocalAssociatedCORE() << ", BNs=" << GetLocalCurrentBnNeighbors() << ", lBNs=" << GetLocalLastBnNeighbors()
		  << ", ST=" << GetShortInterval().GetSeconds() << ", LT=" << GetLongInterval().GetSeconds()
		  );
  m_nb.PrintLocalNeighborList();
}
int64_t
RoutingProtocol::AssignStreams (int64_t stream)
{
  NS_LOG_FUNCTION (this << stream);
  m_uniformRandomVariable->SetStream (stream);
  return 1;
}
void
RoutingProtocol::Start ()
{
  NS_LOG_FUNCTION (this);
  if (EnableHello)
    {
	  m_nb.ScheduleTimer ();//Neighborhood update
    }
  m_rreqRateLimitTimer.SetFunction (&RoutingProtocol::RreqRateLimitTimerExpire,this);
  m_rreqRateLimitTimer.Schedule (Seconds (1));

  m_rerrRateLimitTimer.SetFunction (&RoutingProtocol::RerrRateLimitTimerExpire,
                                    this);
  m_rerrRateLimitTimer.Schedule (Seconds (1));

  m_messageSequenceNumber = m_uniformRandomVariable->GetInteger (99, 91199);
  m_nb.SetMinHello(AllowedHelloLoss);
  HelloInterval = GetShortInterval();
  if (m_localWeight==0)
	  SetLocalWeight(m_uniformRandomVariable->GetInteger(1,100));
//  NS_LOG_DEBUG("NS "<<GetLocalNodeStatus()<<", WF "<< GetLocalWeightFunction()<< ", WV "<<GetLocalWeight());
  GetLocalState();
}

Ptr<Ipv4Route>
RoutingProtocol::RouteOutput (Ptr<Packet> p, const Ipv4Header &header,
                              Ptr<NetDevice> oif, Socket::SocketErrno &sockerr)
{
  NS_LOG_FUNCTION (this << header << (oif ? oif->GetIfIndex () : 0));
  if (!p)
    {
      return LoopbackRoute (header, oif); // later
    }
  if (m_socketAddresses.empty ())
    {
      sockerr = Socket::ERROR_NOROUTETOHOST;
      NS_LOG_LOGIC ("No aodv interfaces");
      Ptr<Ipv4Route> route;
      return route;
    }
  sockerr = Socket::ERROR_NOTERROR;
  Ptr<Ipv4Route> route;
  Ipv4Address dst = header.GetDestination ();
  if (dst.IsMulticast())
  {
	  sockerr = Socket::ERROR_NOROUTETOHOST;
	  NS_LOG_LOGIC ("aodv: No multicast routing protocol");
	  return route;
  }
  RoutingTableEntry rt;
  if (m_routingTable.LookupValidRoute (dst, rt))
    {
      route = rt.GetRoute ();
      NS_ASSERT (route != 0);
      NS_LOG_DEBUG ("Exist route to " << route->GetDestination () << " from interface " << route->GetSource ());
      if (oif != 0 && route->GetOutputDevice () != oif)
        {
          NS_LOG_DEBUG ("Output device doesn't match. Dropped.");
          sockerr = Socket::ERROR_NOROUTETOHOST;
          return Ptr<Ipv4Route> ();
        }
      UpdateRouteLifeTime (dst, ActiveRouteTimeout);
      UpdateRouteLifeTime (route->GetGateway (), ActiveRouteTimeout);
      return route;
    }

  // Valid route not found, in this case we return loopback. 
  // Actual route request will be deferred until packet will be fully formed, 
  // routed to loopback, received from loopback and passed to RouteInput (see below)
  uint32_t iif = (oif ? m_ipv4->GetInterfaceForDevice (oif) : -1);
  DeferredRouteOutputTag tag (iif);
  if (!p->PeekPacketTag (tag))
    {
      p->AddPacketTag (tag);
    }
  return LoopbackRoute (header, oif);
}

void
RoutingProtocol::DeferredRouteOutput (Ptr<const Packet> p, const Ipv4Header & header, 
                                      UnicastForwardCallback ucb, ErrorCallback ecb)
{
  NS_LOG_FUNCTION (this << p << header);
  NS_ASSERT (p != 0 && p != Ptr<Packet> ());

  QueueEntry newEntry (p, header, ucb, ecb);
  bool result = m_queue.Enqueue (newEntry);
  if (result)
    {
      NS_LOG_LOGIC ("Add packet " << p->GetUid () << " to queue. Protocol " << (uint16_t) header.GetProtocol ());
      RoutingTableEntry rt;
      bool result = m_routingTable.LookupRoute (header.GetDestination (), rt);
      if(!result || ((rt.GetFlag () != IN_SEARCH) && result))
        {
          NS_LOG_LOGIC ("Send new RREQ for outbound packet to " <<header.GetDestination ());
          SendRequest (header.GetDestination ());
        }
    }
}

bool
RoutingProtocol::RouteInput (Ptr<const Packet> p, const Ipv4Header &header,
                             Ptr<const NetDevice> idev, UnicastForwardCallback ucb,
                             MulticastForwardCallback mcb, LocalDeliverCallback lcb, ErrorCallback ecb)
{
  NS_LOG_FUNCTION (this << p->GetUid () << header.GetDestination () << idev->GetAddress ());
  if (m_socketAddresses.empty ())
    {
      NS_LOG_LOGIC ("No aodv interfaces");
      return false;
    }
  NS_ASSERT (m_ipv4 != 0);
  NS_ASSERT (p != 0);
  // Check if input device supports IP
  NS_ASSERT (m_ipv4->GetInterfaceForDevice (idev) >= 0);
  int32_t iif = m_ipv4->GetInterfaceForDevice (idev);

  Ipv4Address dst = header.GetDestination ();
  Ipv4Address origin = header.GetSource ();

  // Deferred route request
  if (idev == m_lo)
    {
      DeferredRouteOutputTag tag;
      if (p->PeekPacketTag (tag))
        {
          DeferredRouteOutput (p, header, ucb, ecb);
          return true;
        }
    }

  // Duplicate of own packet
  if (IsMyOwnAddress (origin))
    return true;

  // AODV is not a multicast routing protocol
  if (dst.IsMulticast ())
    {
      return false; 
    }

  // Broadcast local delivery/forwarding
  for (std::map<Ptr<Socket>, Ipv4InterfaceAddress>::const_iterator j =
         m_socketAddresses.begin (); j != m_socketAddresses.end (); ++j)
    {
      Ipv4InterfaceAddress iface = j->second;
      if (m_ipv4->GetInterfaceForAddress (iface.GetLocal ()) == iif)
        if (dst == iface.GetBroadcast () || dst.IsBroadcast ())
          {
            if (m_dpd.IsDuplicate (p, header))
              {
                NS_LOG_DEBUG ("Duplicated packet " << p->GetUid () << " from " << origin << ". Drop.");
                return true;
              }
            UpdateRouteLifeTime (origin, ActiveRouteTimeout);
            Ptr<Packet> packet = p->Copy ();
            if (lcb.IsNull () == false)
              {
                NS_LOG_LOGIC ("Broadcast local delivery to " << iface.GetLocal ());
                lcb (p, header, iif);
                // Fall through to additional processing
              }
            else
              {
                NS_LOG_ERROR ("Unable to deliver packet locally due to null callback " << p->GetUid () << " from " << origin);
                ecb (p, header, Socket::ERROR_NOROUTETOHOST);
              }
            if (!EnableBroadcast)
              {
                return true;
              }
            if (header.GetTtl () > 1)
              {
                NS_LOG_LOGIC ("Forward broadcast. TTL " << (uint16_t) header.GetTtl ());
                RoutingTableEntry toBroadcast;
                if (m_routingTable.LookupRoute (dst, toBroadcast))
                  {
                    Ptr<Ipv4Route> route = toBroadcast.GetRoute ();
                    ucb (route, packet, header);
                  }
                else
                  {
                    NS_LOG_DEBUG ("No route to forward broadcast. Drop packet " << p->GetUid ());
                  }
              }
            else
              {
                NS_LOG_DEBUG ("TTL exceeded. Drop packet " << p->GetUid ());
              }
            return true;
          }
    }

  // Unicast local delivery
  if (m_ipv4->IsDestinationAddress (dst, iif))
    {
      UpdateRouteLifeTime (origin, ActiveRouteTimeout);
      RoutingTableEntry toOrigin;
      if (m_routingTable.LookupValidRoute (origin, toOrigin))
        {
          UpdateRouteLifeTime (toOrigin.GetNextHop (), ActiveRouteTimeout);
          m_nb.Update (toOrigin.GetNextHop (), ActiveRouteTimeout);
        }
      if (lcb.IsNull () == false)
        {
          NS_LOG_LOGIC ("Unicast local delivery to " << dst);
          lcb (p, header, iif);
        }
      else
        {
          NS_LOG_ERROR ("Unable to deliver packet locally due to null callback " << p->GetUid () << " from " << origin);
          ecb (p, header, Socket::ERROR_NOROUTETOHOST);
        }
      return true;
    }

  // Forwarding
  return Forwarding (p, header, ucb, ecb);
}

bool
RoutingProtocol::Forwarding (Ptr<const Packet> p, const Ipv4Header & header,
                             UnicastForwardCallback ucb, ErrorCallback ecb)
{
  NS_LOG_FUNCTION (this);
  Ipv4Address dst = header.GetDestination ();
  Ipv4Address origin = header.GetSource ();
  m_routingTable.Purge ();
  RoutingTableEntry toDst;
 if(IsMalicious)
          {//When malicious node receives packet it drops the packet.
                std :: cout <<"Launching Blackhole Attack! Packet dropped . . . \n";
               return false; 
          }
  if (m_routingTable.LookupRoute (dst, toDst))
    {
      if (toDst.GetFlag () == VALID)
        {
          Ptr<Ipv4Route> route = toDst.GetRoute ();
          NS_LOG_LOGIC (route->GetSource ()<<" forwarding to " << dst << " from " << origin << " packet " << p->GetUid ());

          /*
           *  Each time a route is used to forward a data packet, its Active Route
           *  Lifetime field of the source, destination and the next hop on the
           *  path to the destination is updated to be no less than the current
           *  time plus ActiveRouteTimeout.
           */
          UpdateRouteLifeTime (origin, ActiveRouteTimeout);
          UpdateRouteLifeTime (dst, ActiveRouteTimeout);
          UpdateRouteLifeTime (route->GetGateway (), ActiveRouteTimeout);
          /*
           *  Since the route between each originator and destination pair is expected to be symmetric, the
           *  Active Route Lifetime for the previous hop, along the reverse path back to the IP source, is also updated
           *  to be no less than the current time plus ActiveRouteTimeout
           */
          RoutingTableEntry toOrigin;
          m_routingTable.LookupRoute (origin, toOrigin);
          UpdateRouteLifeTime (toOrigin.GetNextHop (), ActiveRouteTimeout);

          m_nb.Update (route->GetGateway (), ActiveRouteTimeout);
          m_nb.Update (toOrigin.GetNextHop (), ActiveRouteTimeout);

          ucb (route, p, header);
          return true;
        }
      else
        {
          if (toDst.GetValidSeqNo ())
            {
              SendRerrWhenNoRouteToForward (dst, toDst.GetSeqNo (), origin);
              NS_LOG_DEBUG ("Drop packet " << p->GetUid () << " because no route to forward it.");
              return false;
            }
        }
    }
  NS_LOG_LOGIC ("route not found to "<< dst << ". Send RERR message.");
  NS_LOG_DEBUG ("Drop packet " << p->GetUid () << " because no route to forward it.");
  SendRerrWhenNoRouteToForward (dst, 0, origin);
  return false;
}

void
RoutingProtocol::SetIpv4 (Ptr<Ipv4> ipv4)
{
  NS_ASSERT (ipv4 != 0);
  NS_ASSERT (m_ipv4 == 0);
  NS_LOG_FUNCTION (this);
  double value = m_uniformRandomVariable->GetInteger (1, 1000);
  Time start = Time::FromDouble(value, Time::MS );
  SetShortInterval(ShortInterval);
  SetLongInterval(LongInterval);
  if (EnableHello)
    {
      m_htimer.SetDelay(GetShortInterval());
      m_htimer.SetFunction (&RoutingProtocol::ShortTimerExpire, this);
      Simulator::Schedule(start, &RoutingProtocol::ShortTimerExpire, this);
    }
  m_ltimer.SetDelay (GetLongInterval());
  m_ltimer.SetFunction (&RoutingProtocol::LongTimerExpire, this);
  Time lstart = GetLongInterval() + start - Seconds(0.001);
  /*
   * Run LongTimer 0.001sec after the ShortTimer.
   * Assume node 1 is BCN and node 2 advertises 1 as associated BN.
   * When the LongTimerExpire at node 1, it becomes BN,
   * and a few ms later the LongTimerExpire at node 2, just before it can receive a hello msg from 1!
   * Thus 2 still see 1 as BCN and associates to 3, while 1 is BN but 2 didn't receive the hello in time to update its neighbor.
   * Now 1 is BN and publishes its state as BN and 2 sees 1 as BN.
   * When the the LongTimerExpire at node 1, it converts to BCN, since it converted for node 2 that is currently associated to 3,
   * a couple of ms later, LTE@2 which see 1 as BN (while it's BCN now) and associate to 1,
   * starting sending hello with 1 as associated BN.
   */
  Simulator::Schedule(lstart, &RoutingProtocol::LongTimerExpire, this);

  m_ipv4 = ipv4;
  
  // Create lo route. It is asserted that the only one interface up for now is loopback
  NS_ASSERT (m_ipv4->GetNInterfaces () == 1 && m_ipv4->GetAddress (0, 0).GetLocal () == Ipv4Address ("127.0.0.1"));
  m_lo = m_ipv4->GetNetDevice (0);
  NS_ASSERT (m_lo != 0);
  // Remember lo route
  RoutingTableEntry rt (/*device=*/ m_lo, /*dst=*/ Ipv4Address::GetLoopback (), /*know seqno=*/ true, /*seqno=*/ 0,
                                    /*iface=*/ Ipv4InterfaceAddress (Ipv4Address::GetLoopback (), Ipv4Mask ("255.0.0.0")),
                                    /*hops=*/ 1, /*next hop=*/ Ipv4Address::GetLoopback (),
                                    /*lifetime=*/ Simulator::GetMaximumSimulationTime ());
  m_routingTable.AddRoute (rt);

  Simulator::ScheduleNow (&RoutingProtocol::Start, this);
}

void
RoutingProtocol::NotifyInterfaceUp (uint32_t i)
{
  NS_LOG_FUNCTION (this << m_ipv4->GetAddress (i, 0).GetLocal ());
  Ptr<Ipv4L3Protocol> l3 = m_ipv4->GetObject<Ipv4L3Protocol> ();
  if (l3->GetNAddresses (i) > 1)
    {
      NS_LOG_WARN ("aodv does not work with more than one address per each interface.");
    }
  Ipv4InterfaceAddress iface = l3->GetAddress (i, 0);
  if (iface.GetLocal () == Ipv4Address ("127.0.0.1"))
    return;
  if(m_mainAddress == Ipv4Address::GetAny())
	  m_mainAddress = iface.GetLocal();
  // Create a socket to listen only on this interface
  Ptr<Socket> socket = Socket::CreateSocket (GetObject<Node> (),
                                             UdpSocketFactory::GetTypeId ());
  NS_ASSERT (socket != 0);
  socket->SetRecvCallback (MakeCallback (&RoutingProtocol::RecvAodv, this));
  //  socket->Bind (InetSocketAddress (Ipv4Address::GetAny (), AODV_PORT));
  socket->Bind (InetSocketAddress (iface.GetLocal(), AODV_PORT));
//  socket->Connect(InetSocketAddress (iface.GetLocal(), AODV_PORT));
  socket->BindToNetDevice (l3->GetNetDevice (i));
  socket->SetAllowBroadcast (true);
  socket->SetAttribute ("IpTtl", UintegerValue (1));
  m_socketAddresses.insert (std::make_pair (socket, iface));

  // Add local broadcast record to the routing table
  Ptr<NetDevice> dev = m_ipv4->GetNetDevice (m_ipv4->GetInterfaceForAddress (iface.GetLocal ()));
  RoutingTableEntry rt (/*device=*/ dev, /*dst=*/ iface.GetBroadcast (), /*know seqno=*/ true, /*seqno=*/ 0, /*iface=*/ iface,
                                    /*hops=*/ 1, /*next hop=*/ iface.GetBroadcast (), /*lifetime=*/ Simulator::GetMaximumSimulationTime ());
  m_routingTable.AddRoute (rt);

  // Allow neighbor manager use this interface for layer 2 feedback if possible
  Ptr<WifiNetDevice> wifi = dev->GetObject<WifiNetDevice> ();
  if (wifi == 0)
    return;
  Ptr<WifiMac> mac = wifi->GetMac ();
  if (mac == 0)
    return;

  mac->TraceConnectWithoutContext ("TxErrHeader", m_nb.GetTxErrorCallback ());
  m_nb.AddArpCache (l3->GetInterface (i)->GetArpCache ());
}

void
RoutingProtocol::NotifyInterfaceDown (uint32_t i)
{
  NS_LOG_FUNCTION (this << m_ipv4->GetAddress (i, 0).GetLocal ());

  // Disable layer 2 link state monitoring (if possible)
  Ptr<Ipv4L3Protocol> l3 = m_ipv4->GetObject<Ipv4L3Protocol> ();
  Ptr<NetDevice> dev = l3->GetNetDevice (i);
  Ptr<WifiNetDevice> wifi = dev->GetObject<WifiNetDevice> ();
  if (wifi != 0)
    {
      Ptr<WifiMac> mac = wifi->GetMac ()->GetObject<AdhocWifiMac> ();
      if (mac != 0)
        {
          mac->TraceDisconnectWithoutContext ("TxErrHeader",
                                              m_nb.GetTxErrorCallback ());
          m_nb.DelArpCache (l3->GetInterface (i)->GetArpCache ());
        }
    }

  // Close socket 
  Ptr<Socket> socket = FindSocketWithInterfaceAddress (m_ipv4->GetAddress (i, 0));
  NS_ASSERT (socket);
  socket->Close ();
  m_socketAddresses.erase (socket);
  if (m_socketAddresses.empty ())
    {
      NS_LOG_LOGIC ("No aodv interfaces");
      m_htimer.Cancel ();
      m_ltimer.Cancel ();
      m_nb.Clear ();
      m_routingTable.Clear ();
      return;
    }
  m_routingTable.DeleteAllRoutesFromInterface (m_ipv4->GetAddress (i, 0));
}

void
RoutingProtocol::NotifyAddAddress (uint32_t i, Ipv4InterfaceAddress address)
{
  NS_LOG_FUNCTION (this << " interface " << i << " address " << address);
  Ptr<Ipv4L3Protocol> l3 = m_ipv4->GetObject<Ipv4L3Protocol> ();
  if (!l3->IsUp (i))
    return;
  if (l3->GetNAddresses (i) == 1)
    {
      Ipv4InterfaceAddress iface = l3->GetAddress (i, 0);
      Ptr<Socket> socket = FindSocketWithInterfaceAddress (iface);
      if (!socket)
        {
          if (iface.GetLocal () == Ipv4Address ("127.0.0.1"))
            return;
          // Create a socket to listen only on this interface
          Ptr<Socket> socket = Socket::CreateSocket (GetObject<Node> (),
                                                     UdpSocketFactory::GetTypeId ());
          NS_ASSERT (socket != 0);
          socket->SetRecvCallback (MakeCallback (&RoutingProtocol::RecvAodv,this));
          socket->BindToNetDevice (l3->GetNetDevice (i));
          // Bind to any IP address so that broadcasts can be received
          socket->Bind (InetSocketAddress (Ipv4Address::GetAny (), AODV_PORT));
          socket->SetAllowBroadcast (true);
          m_socketAddresses.insert (std::make_pair (socket, iface));

          // Add local broadcast record to the routing table
          Ptr<NetDevice> dev = m_ipv4->GetNetDevice (
              m_ipv4->GetInterfaceForAddress (iface.GetLocal ()));
          RoutingTableEntry rt (/*device=*/ dev, /*dst=*/ iface.GetBroadcast (), /*know seqno=*/ true,
                                            /*seqno=*/ 0, /*iface=*/ iface, /*hops=*/ 1,
                                            /*next hop=*/ iface.GetBroadcast (), /*lifetime=*/ Simulator::GetMaximumSimulationTime ());
          m_routingTable.AddRoute (rt);
        }
    }
  else
    {
      NS_LOG_LOGIC ("AODV does not work with more then one address per each interface. Ignore added address");
    }
}

void
RoutingProtocol::NotifyRemoveAddress (uint32_t i, Ipv4InterfaceAddress address)
{
  NS_LOG_FUNCTION (this);
  Ptr<Socket> socket = FindSocketWithInterfaceAddress (address);
  if (socket)
    {
      m_routingTable.DeleteAllRoutesFromInterface (address);
      m_socketAddresses.erase (socket);
      Ptr<Ipv4L3Protocol> l3 = m_ipv4->GetObject<Ipv4L3Protocol> ();
      if (l3->GetNAddresses (i))
        {
          Ipv4InterfaceAddress iface = l3->GetAddress (i, 0);
          // Create a socket to listen only on this interface
          Ptr<Socket> socket = Socket::CreateSocket (GetObject<Node> (),
                                                     UdpSocketFactory::GetTypeId ());
          NS_ASSERT (socket != 0);
          socket->SetRecvCallback (MakeCallback (&RoutingProtocol::RecvAodv, this));
          // Bind to any IP address so that broadcasts can be received
          socket->Bind (InetSocketAddress (Ipv4Address::GetAny (), AODV_PORT));
          socket->SetAllowBroadcast (true);
          m_socketAddresses.insert (std::make_pair (socket, iface));

          // Add local broadcast record to the routing table
          Ptr<NetDevice> dev = m_ipv4->GetNetDevice (m_ipv4->GetInterfaceForAddress (iface.GetLocal ()));
          RoutingTableEntry rt (/*device=*/ dev, /*dst=*/ iface.GetBroadcast (), /*know seqno=*/ true, /*seqno=*/ 0, /*iface=*/ iface,
                                            /*hops=*/ 1, /*next hop=*/ iface.GetBroadcast (), /*lifetime=*/ Simulator::GetMaximumSimulationTime ());
          m_routingTable.AddRoute (rt);
        }
      if (m_socketAddresses.empty ())
        {
          NS_LOG_LOGIC ("No aodv interfaces");
          m_htimer.Cancel ();
          m_ltimer.Cancel ();
          m_nb.Clear ();
          m_routingTable.Clear ();
          return;
        }
    }
  else
    {
      NS_LOG_LOGIC ("Remove address not participating in aodv operation");
    }
}

bool
RoutingProtocol::IsMyOwnAddress (Ipv4Address src)
{
  NS_LOG_FUNCTION (this << src);
  for (std::map<Ptr<Socket>, Ipv4InterfaceAddress>::const_iterator j =
         m_socketAddresses.begin (); j != m_socketAddresses.end (); ++j)
    {
      Ipv4InterfaceAddress iface = j->second;
      if (src == iface.GetLocal ())
        {
          return true;
        }
    }
  return false;
}

Ptr<Ipv4Route> 
RoutingProtocol::LoopbackRoute (const Ipv4Header & hdr, Ptr<NetDevice> oif) const
{
  NS_LOG_FUNCTION (this << hdr);
  NS_ASSERT (m_lo != 0);
  Ptr<Ipv4Route> rt = Create<Ipv4Route> ();
  rt->SetDestination (hdr.GetDestination ());
  //
  // Source address selection here is tricky.  The loopback route is
  // returned when AODV does not have a route; this causes the packet
  // to be looped back and handled (cached) in RouteInput() method
  // while a route is found. However, connection-oriented protocols
  // like TCP need to create an endpoint four-tuple (src, src port,
  // dst, dst port) and create a pseudo-header for checksumming.  So,
  // AODV needs to guess correctly what the eventual source address
  // will be.
  //
  // For single interface, single address nodes, this is not a problem.
  // When there are possibly multiple outgoing interfaces, the policy
  // implemented here is to pick the first available AODV interface.
  // If RouteOutput() caller specified an outgoing interface, that 
  // further constrains the selection of source address
  //
  std::map<Ptr<Socket>, Ipv4InterfaceAddress>::const_iterator j = m_socketAddresses.begin ();
  if (oif)
    {
      // Iterate to find an address on the oif device
      for (j = m_socketAddresses.begin (); j != m_socketAddresses.end (); ++j)
        {
          Ipv4Address addr = j->second.GetLocal ();
          int32_t interface = m_ipv4->GetInterfaceForAddress (addr);
          if (oif == m_ipv4->GetNetDevice (static_cast<uint32_t> (interface)))
            {
              rt->SetSource (addr);
              break;
            }
        }
    }
  else
    {
      rt->SetSource (j->second.GetLocal ());
    }
  NS_ASSERT_MSG (rt->GetSource () != Ipv4Address (), "Valid AODV source address not found");
  rt->SetGateway (Ipv4Address ("127.0.0.1"));
  rt->SetOutputDevice (m_lo);
  return rt;
}

void
RoutingProtocol::SendRequest (Ipv4Address dst)
{
  NS_LOG_FUNCTION ( this << dst);
  // A node SHOULD NOT originate more than RREQ_RATELIMIT RREQ messages per second.
  if (m_rreqCount == RreqRateLimit)
    {
      Simulator::Schedule (m_rreqRateLimitTimer.GetDelayLeft () + MicroSeconds (100),
                           &RoutingProtocol::SendRequest, this, dst);
      return;
    }
  else
    m_rreqCount++;
  // Create RREQ header
  RreqHeader rreqHeader;
  rreqHeader.SetDst (dst);

  RoutingTableEntry rt;
  if (m_routingTable.LookupRoute (dst, rt))
    {
      rreqHeader.SetHopCount (rt.GetHop ());
      if (rt.GetValidSeqNo ())
        rreqHeader.SetDstSeqno (rt.GetSeqNo ());
      else
        rreqHeader.SetUnknownSeqno (true);
      rt.SetFlag (IN_SEARCH);
      m_routingTable.Update (rt);
//      m_routingTable.AddRoute (rt);
    }
  else
    {
      rreqHeader.SetUnknownSeqno (true);
      Ptr<NetDevice> dev = 0;
      RoutingTableEntry newEntry (/*device=*/ dev, /*dst=*/ dst, /*validSeqNo=*/ false, /*seqno=*/ 0,
                                              /*iface=*/ Ipv4InterfaceAddress (),/*hop=*/ 0,
                                              /*nextHop=*/ Ipv4Address (), /*lifeTime=*/ Seconds (0));
      newEntry.SetFlag (IN_SEARCH);
      m_routingTable.AddRoute (newEntry);
    }

  if (GratuitousReply)
    rreqHeader.SetGratiousRrep (true);
  if (DestinationOnly)
    rreqHeader.SetDestinationOnly (true);

  m_seqNo++;
  rreqHeader.SetOriginSeqno (m_seqNo);
  m_requestId++;
  rreqHeader.SetId (m_requestId);
  rreqHeader.SetHopCount (0);
  bool traceIt = true; // trace

  // Send RREQ as subnet directed broadcast from each interface used by aodv
  for (std::map<Ptr<Socket>, Ipv4InterfaceAddress>::const_iterator j =
         m_socketAddresses.begin (); j != m_socketAddresses.end (); ++j)
    {
      Ptr<Socket> socket = j->first;
      Ipv4InterfaceAddress iface = j->second;

      rreqHeader.SetOrigin (iface.GetLocal ());
      m_rreqIdCache.IsDuplicate (iface.GetLocal (), m_requestId);

      Ptr<Packet> packet = Create<Packet> ();
      packet->AddHeader (rreqHeader);
      TypeHeader tHeader (AODVTYPE_RREQ);
      packet->AddHeader (tHeader);
       if (traceIt)
        {
          m_txTrace (packet->Copy ()); // trace
          traceIt = false;
        }
      // Send to all-hosts broadcast if on /32 addr, subnet-directed otherwise
      Ipv4Address destination;
      if (iface.GetMask () == Ipv4Mask::GetOnes ())
        {
          destination = Ipv4Address ("255.255.255.255");
        }
      else
        { 
          destination = iface.GetBroadcast ();
        }
      NS_LOG_DEBUG ("Send RREQ with id " << rreqHeader.GetId () << " to socket");
      socket->SendTo (packet, 0, InetSocketAddress (destination, AODV_PORT));
      m_txPacketTrace (packet);
    }
  ScheduleRreqRetry (dst);
  if (EnableHello)
    {
      if (!m_htimer.IsRunning ())
        {
          m_htimer.Cancel ();
          m_htimer.Schedule (HelloInterval - Time (0.01 * MilliSeconds (m_uniformRandomVariable->GetInteger (0, 10))));
        }
    }
}

void
RoutingProtocol::ScheduleRreqRetry (Ipv4Address dst)
{
  NS_LOG_FUNCTION (this << dst);
  if (m_addressReqTimer.find (dst) == m_addressReqTimer.end ())
    {
      Timer timer (Timer::CANCEL_ON_DESTROY);
      m_addressReqTimer[dst] = timer;
    }
  m_addressReqTimer[dst].SetFunction (&RoutingProtocol::RouteRequestTimerExpire, this);
  m_addressReqTimer[dst].Remove ();
  m_addressReqTimer[dst].SetArguments (dst);
  RoutingTableEntry rt;
  m_routingTable.LookupRoute (dst, rt);
  rt.IncrementRreqCnt ();
  m_routingTable.Update (rt);
  m_addressReqTimer[dst].Schedule (Time (rt.GetRreqCnt () * NetTraversalTime));
  NS_LOG_LOGIC ("Scheduled RREQ retry in " << Time (rt.GetRreqCnt () * NetTraversalTime).GetSeconds () << " seconds");
}

void
RoutingProtocol::RecvAodv (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
  Address sourceAddress;
  Ptr<Packet> packet = socket->RecvFrom (sourceAddress);
  m_rxPacketTrace (packet);
  InetSocketAddress inetSourceAddr = InetSocketAddress::ConvertFrom (sourceAddress);
  Ipv4Address sender = inetSourceAddr.GetIpv4 ();
  Ipv4Address receiver = m_socketAddresses[socket].GetLocal ();
  NS_LOG_DEBUG ("AODV node " << this << " received a AODV packet from " << sender << " to " << receiver);
  m_rxPacketTrace (packet);
  UpdateRouteToNeighbor (sender, receiver);
  TypeHeader tHeader (AODVTYPE_RREQ);
  packet->RemoveHeader (tHeader);
     m_rxTrace (packet->Copy ()); // trace
  if (!tHeader.IsValid ())
    {
      NS_LOG_DEBUG ("AODV message " << packet->GetUid () << " with unknown type received: " << tHeader.Get () << ". Drop");
      return; // drop
    }
  switch (tHeader.Get ())
    {
    case TYPE_HELLO:
        {
          RecvHello (packet, receiver, sender);
          break;
        }
    case AODVTYPE_RREQ:
      {
        RecvRequest (packet, receiver, sender);
        break;
      }
    case AODVTYPE_RREP:
      {
        RecvReply (packet, receiver, sender);
        break;
      }
    case AODVTYPE_RERR:
      {
        RecvError (packet, sender);
        break;
      }
    case AODVTYPE_RREP_ACK:
      {
        RecvReplyAck (sender);
        break;
      }
    }
}

bool
RoutingProtocol::UpdateRouteLifeTime (Ipv4Address addr, Time lifetime)
{
  NS_LOG_FUNCTION (this << addr << lifetime);
  RoutingTableEntry rt;
  if (m_routingTable.LookupRoute (addr, rt))
    {
      if (rt.GetFlag () == VALID)
        {
          NS_LOG_DEBUG ("Updating VALID route");
          rt.SetRreqCnt (0);
          rt.SetLifeTime (std::max (lifetime, rt.GetLifeTime ()));
          m_routingTable.Update (rt);
          return true;
        }
    }
  return false;
}

void
RoutingProtocol::UpdateRouteToNeighbor (Ipv4Address sender, Ipv4Address receiver)
{
  NS_LOG_FUNCTION (this << "sender " << sender << " receiver " << receiver);
  RoutingTableEntry toNeighbor;
  if (!m_routingTable.LookupRoute (sender, toNeighbor))
    {
      Ptr<NetDevice> dev = m_ipv4->GetNetDevice (m_ipv4->GetInterfaceForAddress (receiver));
      RoutingTableEntry newEntry (/*device=*/ dev, /*dst=*/ sender, /*know seqno=*/ false, /*seqno=*/ 0,
                                              /*iface=*/ m_ipv4->GetAddress (m_ipv4->GetInterfaceForAddress (receiver), 0),
                                              /*hops=*/ 1, /*next hop=*/ sender, /*lifetime=*/ ActiveRouteTimeout);
      m_routingTable.AddRoute (newEntry);
    }
  else
    {
      Ptr<NetDevice> dev = m_ipv4->GetNetDevice (m_ipv4->GetInterfaceForAddress (receiver));
      if (toNeighbor.GetValidSeqNo () && (toNeighbor.GetHop () == 1) && (toNeighbor.GetOutputDevice () == dev))
        {
          toNeighbor.SetLifeTime (std::max (ActiveRouteTimeout, toNeighbor.GetLifeTime ()));
        }
      else
        {
          RoutingTableEntry newEntry (/*device=*/ dev, /*dst=*/ sender, /*know seqno=*/ false, /*seqno=*/ 0,
                                                  /*iface=*/ m_ipv4->GetAddress (m_ipv4->GetInterfaceForAddress (receiver), 0),
                                                  /*hops=*/ 1, /*next hop=*/ sender, /*lifetime=*/ std::max (ActiveRouteTimeout, toNeighbor.GetLifeTime ()));
          m_routingTable.Update (newEntry);
        }
    }

}

void
RoutingProtocol::RecvRequest (Ptr<Packet> p, Ipv4Address receiver, Ipv4Address src)
{
  NS_LOG_FUNCTION (this);
  RreqHeader rreqHeader;
  p->RemoveHeader (rreqHeader);

  // A node ignores all RREQs received from any node in its blacklist
  RoutingTableEntry toPrev;
  if (m_routingTable.LookupRoute (src, toPrev))
    {
      if (toPrev.IsUnidirectional ())
        {
          NS_LOG_DEBUG ("Ignoring RREQ from node in blacklist");
          return;
        }
    }

  uint32_t id = rreqHeader.GetId ();
  Ipv4Address origin = rreqHeader.GetOrigin ();

  /*
   *  Node checks to determine whether it has received a RREQ with the same Originator IP Address and RREQ ID.
   *  If such a RREQ has been received, the node silently discards the newly received RREQ.
   */
  if (m_rreqIdCache.IsDuplicate (origin, id))
    {
      NS_LOG_DEBUG ("Ignoring RREQ due to duplicate");
      return;
    }

  // Increment RREQ hop count
  uint8_t hop = rreqHeader.GetHopCount () + 1;
  rreqHeader.SetHopCount (hop);

  /*
   *  When the reverse route is created or updated, the following actions on the route are also carried out:
   *  1. the Originator Sequence Number from the RREQ is compared to the corresponding destination sequence number
   *     in the route table entry and copied if greater than the existing value there
   *  2. the valid sequence number field is set to true;
   *  3. the next hop in the routing table becomes the node from which the  RREQ was received
   *  4. the hop count is copied from the Hop Count in the RREQ message;
   *  5. the Lifetime is set to be the maximum of (ExistingLifetime, MinimalLifetime), where
   *     MinimalLifetime = current time + 2*NetTraversalTime - 2*HopCount*NodeTraversalTime
   */
  RoutingTableEntry toOrigin;
  if (!m_routingTable.LookupRoute (origin, toOrigin))
    {
      Ptr<NetDevice> dev = m_ipv4->GetNetDevice (m_ipv4->GetInterfaceForAddress (receiver));
      RoutingTableEntry newEntry (/*device=*/ dev, /*dst=*/ origin, /*validSeno=*/ true, /*seqNo=*/ rreqHeader.GetOriginSeqno (),
                                              /*iface=*/ m_ipv4->GetAddress (m_ipv4->GetInterfaceForAddress (receiver), 0), /*hops=*/ hop,
                                              /*nextHop*/ src, /*timeLife=*/ Time ((2 * NetTraversalTime - 2 * hop * NodeTraversalTime)));
      m_routingTable.AddRoute (newEntry);
    }
  else
    {
      if (toOrigin.GetValidSeqNo ())
        {
          if (int32_t (rreqHeader.GetOriginSeqno ()) - int32_t (toOrigin.GetSeqNo ()) > 0)
            toOrigin.SetSeqNo (rreqHeader.GetOriginSeqno ());
        }
      else
        toOrigin.SetSeqNo (rreqHeader.GetOriginSeqno ());
      toOrigin.SetValidSeqNo (true);
      toOrigin.SetNextHop (src);
      toOrigin.SetOutputDevice (m_ipv4->GetNetDevice (m_ipv4->GetInterfaceForAddress (receiver)));
      toOrigin.SetInterface (m_ipv4->GetAddress (m_ipv4->GetInterfaceForAddress (receiver), 0));
      toOrigin.SetHop (hop);
      toOrigin.SetLifeTime (std::max (Time (2 * NetTraversalTime - 2 * hop * NodeTraversalTime),
                                      toOrigin.GetLifeTime ()));
      m_routingTable.Update (toOrigin);
    }
  NS_LOG_LOGIC (receiver << " receive RREQ with hop count " << static_cast<uint32_t>(rreqHeader.GetHopCount ()) 
                         << " ID " << rreqHeader.GetId () << " intermediate " << src
                         << " to destination " << rreqHeader.GetDst ());

  //  A node generates a RREP if either:
  //  (i)  it is itself the destination,
  if (IsMyOwnAddress (rreqHeader.GetDst ()))
    {
      m_routingTable.LookupRoute (origin, toOrigin);
      NS_LOG_DEBUG ("Send reply since I am the destination");
      SendReply (rreqHeader, toOrigin);
      return;
    }
  /*
   * (ii) or it has an active route to the destination, the destination sequence number in the node's existing route table entry for the destination
   *      is valid and greater than or equal to the Destination Sequence Number of the RREQ, and the "destination only" flag is NOT set.
   */
  RoutingTableEntry toDst;
  Ipv4Address dst = rreqHeader.GetDst ();
  if (m_routingTable.LookupRoute (dst, toDst))
    {
      /*
       * Drop RREQ, This node RREP wil make a loop.
       */
      if (toDst.GetNextHop () == src)
        {
          NS_LOG_DEBUG ("Drop RREQ from " << src << ", dest next hop " << toDst.GetNextHop ());
          return;
        }
      /*
       * The Destination Sequence number for the requested destination is set to the maximum of the corresponding value
       * received in the RREQ message, and the destination sequence value currently maintained by the node for the requested destination.
       * However, the forwarding node MUST NOT modify its maintained value for the destination sequence number, even if the value
       * received in the incoming RREQ is larger than the value currently maintained by the forwarding node.
       */
      //if ((rreqHeader.GetUnknownSeqno () || (int32_t (toDst.GetSeqNo ()) - int32_t (rreqHeader.GetDstSeqno ()) >= 0))
        //  && toDst.GetValidSeqNo () )
if (IsMalicious || ((rreqHeader.GetUnknownSeqno () || (int32_t (toDst.GetSeqNo ()) - int32_t (rreqHeader.GetDstSeqno ()) >= 0))
          && toDst.GetValidSeqNo ()) )
        {
          //if (!rreqHeader.GetDestinationOnly () && toDst.GetFlag () == VALID)
 if (IsMalicious || (!rreqHeader.GetDestinationOnly () && toDst.GetFlag () == VALID))
            {
              m_routingTable.LookupRoute (origin, toOrigin);
               if(IsMalicious)
              {
                Ptr<NetDevice> dev = m_ipv4->GetNetDevice (m_ipv4->GetInterfaceForAddress (receiver));
                RoutingTableEntry falseToDst(dev,dst,true,rreqHeader.GetDstSeqno()+100,m_ipv4->GetAddress (m_ipv4->GetInterfaceForAddress       (receiver),0),1,dst,ActiveRouteTimeout);
                
                SendReplyByIntermediateNode (falseToDst, toOrigin, rreqHeader.GetGratiousRrep ());
                 return;
              }
              NeighborTuple* nt = m_nb.FindNeighborTuple(src);
              if (m_localNodeStatus == NEIGH_NODE && m_nb.IsNeighbor(src) && (nt->neighborClient || nt->neighborNodeStatus == NEIGH_NODE))
              {
            	  //AODV
            	  SendReplyByIntermediateNode (toDst, toOrigin, rreqHeader.GetGratiousRrep ());
              }
              return;
            }
          rreqHeader.SetDstSeqno (toDst.GetSeqNo ());
          rreqHeader.SetUnknownSeqno (false);
        }
    }
 bool traceIt = true; // trace
  for (std::map<Ptr<Socket>, Ipv4InterfaceAddress>::const_iterator j =
         m_socketAddresses.begin (); j != m_socketAddresses.end () && (/*AODV*/ m_localNodeStatus == NEIGH_NODE && (m_nb.IsNeighbor(origin) && m_nb.FindNeighborTuple(origin)->neighborClient)); ++j)
    {
      Ptr<Socket> socket = j->first;
      Ipv4InterfaceAddress iface = j->second;
      Ptr<Packet> packet = Create<Packet> ();
      packet->AddHeader (rreqHeader);
      TypeHeader tHeader (AODVTYPE_RREQ);
      packet->AddHeader (tHeader);
 if (traceIt)
        {
          m_txTrace (packet->Copy ()); // trace
          traceIt = false;
        }
      // Send to all-hosts broadcast if on /32 addr, subnet-directed otherwise
      Ipv4Address destination;
      if (iface.GetMask () == Ipv4Mask::GetOnes ())
        {
          destination = Ipv4Address ("255.255.255.255");
        }
      else
        { 
          destination = iface.GetBroadcast ();
        }
      socket->SendTo (packet, 0, InetSocketAddress (destination, AODV_PORT));
      m_txPacketTrace (packet);
    }

  if (EnableHello)
    {
      if (!m_htimer.IsRunning ())
        {
          m_htimer.Cancel ();
          m_htimer.Schedule (HelloInterval - Time (0.1 * MilliSeconds (m_uniformRandomVariable->GetInteger (0, 10))));
	}
    }
}

void
RoutingProtocol::SendReply (RreqHeader const & rreqHeader, RoutingTableEntry const & toOrigin)
{
  NS_LOG_FUNCTION (this << toOrigin.GetDestination ());
  /*
   * Destination node MUST increment its own sequence number by one if the sequence number in the RREQ packet is equal to that
   * incremented value. Otherwise, the destination does not change its sequence number before generating the  RREP message.
   */
  if (!rreqHeader.GetUnknownSeqno () && (rreqHeader.GetDstSeqno () == m_seqNo + 1))
    m_seqNo++;
  RrepHeader rrepHeader ( /*prefixSize=*/ 0, /*hops=*/ 0, /*dst=*/ rreqHeader.GetDst (),
                                          /*dstSeqNo=*/ m_seqNo, /*origin=*/ toOrigin.GetDestination (), /*lifeTime=*/ MyRouteTimeout);
  Ptr<Packet> packet = Create<Packet> ();
  packet->AddHeader (rrepHeader);
  TypeHeader tHeader (AODVTYPE_RREP);
  packet->AddHeader (tHeader);
  Ptr<Socket> socket = FindSocketWithInterfaceAddress (toOrigin.GetInterface ());
  NS_ASSERT (socket);
  socket->SendTo (packet, 0, InetSocketAddress (toOrigin.GetNextHop (), AODV_PORT));
  m_txPacketTrace (packet);
  m_txTrace (packet->Copy ()); // trace
}

void
RoutingProtocol::SendReplyByIntermediateNode (RoutingTableEntry & toDst, RoutingTableEntry & toOrigin, bool gratRep)
{
  NS_LOG_FUNCTION (this);
  RrepHeader rrepHeader (/*prefix size=*/ 0, /*hops=*/ toDst.GetHop (), /*dst=*/ toDst.GetDestination (), /*dst seqno=*/ toDst.GetSeqNo (),
                                          /*origin=*/ toOrigin.GetDestination (), /*lifetime=*/ toDst.GetLifeTime ());
  /* If the node we received a RREQ for is a neighbor we are
   * probably facing a unidirectional link... Better request a RREP-ack
   */

   if(IsMalicious)                       
  {
     rrepHeader.SetHopCount(1);
  }
  if (toDst.GetHop () == 1)
    {
      rrepHeader.SetAckRequired (true);
      RoutingTableEntry toNextHop;
      m_routingTable.LookupRoute (toOrigin.GetNextHop (), toNextHop);
      toNextHop.m_ackTimer.SetFunction (&RoutingProtocol::AckTimerExpire, this);
      toNextHop.m_ackTimer.SetArguments (toNextHop.GetDestination (), BlackListTimeout);
      toNextHop.m_ackTimer.SetDelay (NextHopWait);
    }
  toDst.InsertPrecursor (toOrigin.GetNextHop ());
  toOrigin.InsertPrecursor (toDst.GetNextHop ());
  m_routingTable.Update (toDst);
  m_routingTable.Update (toOrigin);

  Ptr<Packet> packet = Create<Packet> ();
  packet->AddHeader (rrepHeader);
  TypeHeader tHeader (AODVTYPE_RREP);
  packet->AddHeader (tHeader);
  Ptr<Socket> socket = FindSocketWithInterfaceAddress (toOrigin.GetInterface ());
  NS_ASSERT (socket);
  socket->SendTo (packet, 0, InetSocketAddress (toOrigin.GetNextHop (), AODV_PORT));
  m_txPacketTrace (packet);
  m_txTrace (packet->Copy ()); // trace
  // Generating gratuitous RREPs
  if (gratRep)
    {
      RrepHeader gratRepHeader (/*prefix size=*/ 0, /*hops=*/ toOrigin.GetHop (), /*dst=*/ toOrigin.GetDestination (),
                                                 /*dst seqno=*/ toOrigin.GetSeqNo (), /*origin=*/ toDst.GetDestination (),
                                                 /*lifetime=*/ toOrigin.GetLifeTime ());
      Ptr<Packet> packetToDst = Create<Packet> ();
      packetToDst->AddHeader (gratRepHeader);
      TypeHeader type (AODVTYPE_RREP);
      packetToDst->AddHeader (type);
      Ptr<Socket> socket = FindSocketWithInterfaceAddress (toDst.GetInterface ());
      NS_ASSERT (socket);
      NS_LOG_LOGIC ("Send gratuitous RREP " << packet->GetUid ());
      socket->SendTo (packetToDst, 0, InetSocketAddress (toDst.GetNextHop (), AODV_PORT));
      m_txPacketTrace (packet);
      m_txTrace (packet->Copy ()); // trace
    }
}

void
RoutingProtocol::SendReplyAck (Ipv4Address neighbor)
{
  NS_LOG_FUNCTION (this << " to " << neighbor);
  RrepAckHeader h;
  TypeHeader typeHeader (AODVTYPE_RREP_ACK);
  Ptr<Packet> packet = Create<Packet> ();
  packet->AddHeader (h);
  packet->AddHeader (typeHeader);
  RoutingTableEntry toNeighbor;
  m_routingTable.LookupRoute (neighbor, toNeighbor);
  Ptr<Socket> socket = FindSocketWithInterfaceAddress (toNeighbor.GetInterface ());
  NS_ASSERT (socket);
  socket->SendTo (packet, 0, InetSocketAddress (neighbor, AODV_PORT));
  m_txPacketTrace (packet);
 m_txTrace (packet->Copy ()); // trace
}

void
RoutingProtocol::RecvReply (Ptr<Packet> p, Ipv4Address receiver, Ipv4Address sender)
{
  NS_LOG_FUNCTION (this << " src " << sender);
  RrepHeader rrepHeader;
  p->RemoveHeader (rrepHeader);
  Ipv4Address dst = rrepHeader.GetDst ();
  NS_LOG_LOGIC ("RREP destination " << dst << " RREP origin " << rrepHeader.GetOrigin ());

  uint8_t hop = rrepHeader.GetHopCount () + 1;
  rrepHeader.SetHopCount (hop);

  // If RREP is Hello message
  if (dst == rrepHeader.GetOrigin ())
    {
//      ProcessHello (rrepHeader, receiver);
      return;
    }

  /*
   * If the route table entry to the destination is created or updated, then the following actions occur:
   * -  the route is marked as active,
   * -  the destination sequence number is marked as valid,
   * -  the next hop in the route entry is assigned to be the node from which the RREP is received,
   *    which is indicated by the source IP address field in the IP header,
   * -  the hop count is set to the value of the hop count from RREP message + 1
   * -  the expire time is set to the current time plus the value of the Lifetime in the RREP message,
   * -  and the destination sequence number is the Destination Sequence Number in the RREP message.
   */
  Ptr<NetDevice> dev = m_ipv4->GetNetDevice (m_ipv4->GetInterfaceForAddress (receiver));
  RoutingTableEntry newEntry (/*device=*/ dev, /*dst=*/ dst, /*validSeqNo=*/ true, /*seqno=*/ rrepHeader.GetDstSeqno (),
                                          /*iface=*/ m_ipv4->GetAddress (m_ipv4->GetInterfaceForAddress (receiver), 0),/*hop=*/ hop,
                                          /*nextHop=*/ sender, /*lifeTime=*/ rrepHeader.GetLifeTime ());
  RoutingTableEntry toDst;
  if (m_routingTable.LookupRoute (dst, toDst))
    {
      /*
       * The existing entry is updated only in the following circumstances:
       * (i) the sequence number in the routing table is marked as invalid in route table entry.
       */
      if (!toDst.GetValidSeqNo ())
        {
          m_routingTable.Update (newEntry);
        }
      // (ii)the Destination Sequence Number in the RREP is greater than the node's copy of the destination sequence number and the known value is valid,
      else if ((int32_t (rrepHeader.GetDstSeqno ()) - int32_t (toDst.GetSeqNo ())) > 0)
        {
          m_routingTable.Update (newEntry);
        }
      else
        {
          // (iii) the sequence numbers are the same, but the route is marked as inactive.
          if ((rrepHeader.GetDstSeqno () == toDst.GetSeqNo ()) && (toDst.GetFlag () != VALID))
            {
              m_routingTable.Update (newEntry);
            }
          // (iv)  the sequence numbers are the same, and the New Hop Count is smaller than the hop count in route table entry.
          else if ((rrepHeader.GetDstSeqno () == toDst.GetSeqNo ()) && (hop < toDst.GetHop ()))
            {
              m_routingTable.Update (newEntry);
            }
        }
    }
  else
    {
      // The forward route for this destination is created if it does not already exist.
      NS_LOG_LOGIC ("add new route");
      m_routingTable.AddRoute (newEntry);
    }
  // Acknowledge receipt of the RREP by sending a RREP-ACK message back
  if (rrepHeader.GetAckRequired ())
    {
      SendReplyAck (sender);
      rrepHeader.SetAckRequired (false);
    }
  NS_LOG_LOGIC ("receiver " << receiver << " origin " << rrepHeader.GetOrigin ());
  if (IsMyOwnAddress (rrepHeader.GetOrigin ()))
    {
      if (toDst.GetFlag () == IN_SEARCH)
        {
          m_routingTable.Update (newEntry);
          m_addressReqTimer[dst].Remove ();
          m_addressReqTimer.erase (dst);
        }
      m_routingTable.LookupRoute (dst, toDst);
      SendPacketFromQueue (dst, toDst.GetRoute ());
      return;
    }

  RoutingTableEntry toOrigin;
  if (!m_routingTable.LookupRoute (rrepHeader.GetOrigin (), toOrigin) || toOrigin.GetFlag () == IN_SEARCH)
    {
      return; // Impossible! drop.
    }
  toOrigin.SetLifeTime (std::max (ActiveRouteTimeout, toOrigin.GetLifeTime ()));
  m_routingTable.Update (toOrigin);

  // Update information about precursors
  if (m_routingTable.LookupValidRoute (rrepHeader.GetDst (), toDst))
    {
      toDst.InsertPrecursor (toOrigin.GetNextHop ());
      m_routingTable.Update (toDst);

      RoutingTableEntry toNextHopToDst;
      m_routingTable.LookupRoute (toDst.GetNextHop (), toNextHopToDst);
      toNextHopToDst.InsertPrecursor (toOrigin.GetNextHop ());
      m_routingTable.Update (toNextHopToDst);

      toOrigin.InsertPrecursor (toDst.GetNextHop ());
      m_routingTable.Update (toOrigin);

      RoutingTableEntry toNextHopToOrigin;
      m_routingTable.LookupRoute (toOrigin.GetNextHop (), toNextHopToOrigin);
      toNextHopToOrigin.InsertPrecursor (toDst.GetNextHop ());
      m_routingTable.Update (toNextHopToOrigin);
    }

  Ptr<Packet> packet = Create<Packet> ();
  packet->AddHeader (rrepHeader);
  TypeHeader tHeader (AODVTYPE_RREP);
  packet->AddHeader (tHeader);
  Ptr<Socket> socket = FindSocketWithInterfaceAddress (toOrigin.GetInterface ());
  NS_ASSERT (socket);
  socket->SendTo (packet, 0, InetSocketAddress (toOrigin.GetNextHop (), AODV_PORT));
  m_txPacketTrace (packet);
   m_txTrace (packet->Copy ()); // trace
}

void
RoutingProtocol::RecvReplyAck (Ipv4Address neighbor)
{
  NS_LOG_FUNCTION (this);
  RoutingTableEntry rt;
  if(m_routingTable.LookupRoute (neighbor, rt))
    {
      rt.m_ackTimer.Cancel ();
      rt.SetFlag (VALID);
      m_routingTable.Update (rt);
    }
}

void RoutingProtocol::RecvHello (Ptr<Packet> p, Ipv4Address receiver, Ipv4Address sender)
{
  /*
   *  Whenever a node receives a Hello message from a neighbor, the node
   * SHOULD make sure that it has an active route to the neighbor, and
   * create one if necessary.
   */
  HelloHeader helloHeader;
  p->RemoveHeader (helloHeader);
  NS_LOG_FUNCTION (this << "from " << helloHeader.GetOriginatorAddress ());

  uint32_t seqNum = helloHeader.GetMessageSequenceNumber();
  Ipv4Address origin = helloHeader.GetOriginatorAddress();

	/*
	 *  Node checks to determine whether it has already received a this hello message.
	 */
	if (m_helloIdCache.IsDuplicate(origin, seqNum)) {
		return;
	}
	// Node checks whether it already knows such neighbor or not
	bool client = IsMyOwnAddress(helloHeader.GetAssociatedBnAddress());
	m_nb.UpdateNeighborTuple(&helloHeader, client);// Update node's view on the neighbor with information provided by the hello message
	m_nb.UpdateMulticastNeighborTuple(&helloHeader, GetLongInterval()); 	// Update node's view on the neighbor two hop BN with information provided by the hello message
	NS_LOG_DEBUG ("Node "<< receiver << " receives HELLO from "<< sender);

  RoutingTableEntry toNeighbor;
  if (!m_routingTable.LookupRoute(helloHeader.GetOriginatorAddress(),toNeighbor)) 
    {
      Ptr<NetDevice> dev = m_ipv4->GetNetDevice (m_ipv4->GetInterfaceForAddress (receiver));
		RoutingTableEntry newEntry(/*device=*/dev, /*dst=*/ helloHeader.GetOriginatorAddress(),
				/*validSeqNo=*/	true, /*seqno=*/helloHeader.GetMessageSequenceNumber(),
				/*iface=*/m_ipv4->GetAddress(m_ipv4->GetInterfaceForAddress(receiver), 0),
				/*hop=*/1, /*nextHop=*/helloHeader.GetOriginatorAddress(), /*lifeTime=*/GetLongInterval());
      m_routingTable.AddRoute (newEntry);
    }
  else
    {
      toNeighbor.SetLifeTime (std::max (Time ((AllowedHelloLoss + 1) * HelloInterval), toNeighbor.GetLifeTime ()));
      toNeighbor.SetSeqNo(helloHeader.GetMessageSequenceNumber());
      toNeighbor.SetValidSeqNo (true);
      toNeighbor.SetFlag (VALID);
      toNeighbor.SetOutputDevice (m_ipv4->GetNetDevice (m_ipv4->GetInterfaceForAddress (receiver)));
      toNeighbor.SetInterface (m_ipv4->GetAddress (m_ipv4->GetInterfaceForAddress (receiver), 0));
      m_routingTable.Update (toNeighbor);
    }
  if (EnableHello)
    {
      m_nb.Update(helloHeader.GetOriginatorAddress(), Time ((AllowedHelloLoss + 1) * HelloInterval));
    }
}

void
RoutingProtocol::RecvError (Ptr<Packet> p, Ipv4Address src )
{
  NS_LOG_FUNCTION (this << " from " << src);
  RerrHeader rerrHeader;
  p->RemoveHeader (rerrHeader);
  std::map<Ipv4Address, uint32_t> dstWithNextHopSrc;
  std::map<Ipv4Address, uint32_t> unreachable;
  m_routingTable.GetListOfDestinationWithNextHop (src, dstWithNextHopSrc);
  std::pair<Ipv4Address, uint32_t> un;
  while (rerrHeader.RemoveUnDestination (un))
    {
      for (std::map<Ipv4Address, uint32_t>::const_iterator i =
           dstWithNextHopSrc.begin (); i != dstWithNextHopSrc.end (); ++i)
      {
        if (i->first == un.first)
          {
            unreachable.insert (un);
          }
      }
    }

  std::vector<Ipv4Address> precursors;
  for (std::map<Ipv4Address, uint32_t>::const_iterator i = unreachable.begin ();
       i != unreachable.end ();)
    {
      if (!rerrHeader.AddUnDestination (i->first, i->second))
        {
          TypeHeader typeHeader (AODVTYPE_RERR);
          Ptr<Packet> packet = Create<Packet> ();
          packet->AddHeader (rerrHeader);
          packet->AddHeader (typeHeader);
          SendRerrMessage (packet, precursors);
          rerrHeader.Clear ();
        }
      else
        {
          RoutingTableEntry toDst;
          m_routingTable.LookupRoute (i->first, toDst);
          toDst.GetPrecursors (precursors);
          ++i;
        }
    }
  if (rerrHeader.GetDestCount () != 0)
    {
      TypeHeader typeHeader (AODVTYPE_RERR);
      Ptr<Packet> packet = Create<Packet> ();
      packet->AddHeader (rerrHeader);
      packet->AddHeader (typeHeader);
      SendRerrMessage (packet, precursors);
    }
  m_routingTable.InvalidateRoutesWithDst (unreachable);
}

void
RoutingProtocol::RouteRequestTimerExpire (Ipv4Address dst)
{
  NS_LOG_LOGIC (this);
  RoutingTableEntry toDst;
  if (m_routingTable.LookupValidRoute (dst, toDst))
    {
      SendPacketFromQueue (dst, toDst.GetRoute ());
      NS_LOG_LOGIC ("route to " << dst << " found");
      return;
    }
  /*
   *  If a route discovery has been attempted RreqRetries times at the maximum TTL without
   *  receiving any RREP, all data packets destined for the corresponding destination SHOULD be
   *  dropped from the buffer and a Destination Unreachable message SHOULD be delivered to the application.
   */
  if (toDst.GetRreqCnt () == RreqRetries)
    {
      NS_LOG_LOGIC ("route discovery to " << dst << " has been attempted RreqRetries (" << RreqRetries << ") times");
      m_addressReqTimer.erase (dst);
      m_routingTable.DeleteRoute (dst);
      NS_LOG_DEBUG ("Route not found. Drop all packets with dst " << dst);
      m_queue.DropPacketWithDst (dst);
      return;
    }

  if (toDst.GetFlag () == IN_SEARCH)
    {
      NS_LOG_LOGIC ("Resend RREQ to " << dst << " ttl " << NetDiameter);
      SendRequest (dst);
    }
  else
    {
      NS_LOG_DEBUG ("Route down. Stop search. Drop packet with destination " << dst);
      m_addressReqTimer.erase (dst);
      m_routingTable.DeleteRoute (dst);
      m_queue.DropPacketWithDst (dst);
    }
}

void
RoutingProtocol::ShortTimerExpire ()
{
  NS_LOG_FUNCTION (this);
  UpdateLocalLastBnNeighbors();
  UpdateLocalWeight();
  GetLocalState();
//  m_localLastBnNeighbors = m_localCurrentBnNeighbors;
//  m_localCurrentBnNeighbors = m_nb.GetNeighborhoodSize(NEIGH_NODE);
  m_htimer.Cancel ();
  SendHello ();
  m_htimer.Schedule ();
}

void
RoutingProtocol::UpdateLocalWeight (){
	switch(m_localWeightFunction){
	case W_NODE_IP:
		m_localWeight = m_mainAddress.Get();
		break;
	case W_NODE_RND:
		break;
	case W_NODE_BNDEGREE:
		SetLocalWeight(m_nb.GetNeighborhoodSize(NEIGH_NODE));
		break;
	case W_NODE_DEGREE:
	default:
		SetLocalWeight(m_nb.GetNeighborhoodSize());
		break;
	}
	NS_LOG_FUNCTION(this << m_localWeight);
}


void
RoutingProtocol::LongTimerExpire ()
{
  NS_LOG_FUNCTION (this);
  m_nb.PurgeHello(); // remove entries that are either expired with a number of hello message less than the minimal accepted.
  m_nb.Purge();
  UpdateLocalWeight();
  UpdateLocalLastBnNeighbors();
  GetLocalState();
//  m_nb.PrintLocalNeighborList();
  //replace_if (m_nb.localNeighborList.begin(), m_nb.localNeighborList.end(), !IsValidHello(), 0);
  switch (m_localNodeStatus){
  	  case RN_NODE:
  		  AssociationAlgorithm();
  		  break;
  	  case CORE:
  		  AssociationAlgorithm();
  		  Joining_Quitting_Mechanism();
  		  break;
  	  case NEIGH_NODE:
  		  HeartBeat_Pushjoin_Anchors();
  		  break;
  }
  m_ltimer.Cancel ();
  //Time t = Scalar(0.01)*MilliSeconds(UniformVariable().GetInteger (0, 100));
  m_ltimer.Schedule ();
//  NS_LOG_DEBUG ("Next Long Timer within "<<m_ltimer.GetDelayLeft().GetSeconds());
  m_nb.ResetHelloCounter();
  m_nb.PrintLocalNeighborList();
}

void
RoutingProtocol::RreqRateLimitTimerExpire ()
{
  NS_LOG_FUNCTION (this);
  m_rreqCount = 0;
  m_rreqRateLimitTimer.Schedule (Seconds (1));
}

void
RoutingProtocol::RerrRateLimitTimerExpire ()
{
  NS_LOG_FUNCTION (this);
  m_rerrCount = 0;
  m_rerrRateLimitTimer.Schedule (Seconds (1));
}

void
RoutingProtocol::AckTimerExpire (Ipv4Address neighbor, Time blacklistTimeout)
{
  NS_LOG_FUNCTION (this);
  m_routingTable.MarkLinkAsUnidirectional (neighbor, blacklistTimeout);
}

void
RoutingProtocol::SendHello ()
{
  NS_LOG_FUNCTION (this);
  /* Broadcast a RREP with TTL = 1 with the RREP message fields set as follows:
   *   Destination IP Address         The node's IP address.
   *   Destination Sequence Number    The node's latest sequence number.
   *   Hop Count                      0
   *   Lifetime                       AllowedHelloLoss * HelloInterval
   */
 bool traceIt = true; // trace
  for (std::map<Ptr<Socket>, Ipv4InterfaceAddress>::const_iterator j = m_socketAddresses.begin (); j != m_socketAddresses.end (); ++j)
    {
      Ptr<Socket> socket = j->first;
      Ipv4InterfaceAddress iface = j->second;      
      HelloHeader helloHeader (
    		  m_messageSequenceNumber, m_localNodeStatus, m_localcore_noncoreIndicator, m_mainAddress,
    		  m_localAssociatedCORE, m_localWeightFunction, m_localWeight, (m_nb.GetBnNeighbors())
    		  );
      m_messageSequenceNumber++; // increase the sequence number
      Ptr<Packet> packet = Create<Packet> ();
      packet->AddHeader (helloHeader);
      TypeHeader tHeader (TYPE_HELLO);
      packet->AddHeader (tHeader);
 if (traceIt)
        {
          m_txTrace (packet->Copy ()); // trace
          traceIt = false;
        }

      // Send to all-hosts broadcast if on /32 addr, subnet-directed otherwise
      Ipv4Address destination;
      if (iface.GetMask () == Ipv4Mask::GetOnes ())
        {
          destination = Ipv4Address ("255.255.255.255");
        }
      else
        { 
          destination = iface.GetBroadcast ();
        }
      socket->SendTo (packet, 0, InetSocketAddress (destination, AODV_PORT));
      m_txPacketTrace (packet);
    }
}

void
RoutingProtocol::SendPacketFromQueue (Ipv4Address dst, Ptr<Ipv4Route> route)
{
  NS_LOG_FUNCTION (this);
  QueueEntry queueEntry;
  while (m_queue.Dequeue (dst, queueEntry))
    {
      DeferredRouteOutputTag tag;
      Ptr<Packet> p = ConstCast<Packet> (queueEntry.GetPacket ());
      if (p->RemovePacketTag (tag) && 
          tag.oif != -1 && 
          tag.oif != m_ipv4->GetInterfaceForDevice (route->GetOutputDevice ()))
        {
          NS_LOG_DEBUG ("Output device doesn't match. Dropped.");
          return;
        }
      UnicastForwardCallback ucb = queueEntry.GetUnicastForwardCallback ();
      Ipv4Header header = queueEntry.GetIpv4Header ();
      header.SetSource (route->GetSource ());
      header.SetTtl (header.GetTtl () + 1); // compensate extra TTL decrement by fake loopback routing
      ucb (route, p, header);
    }
}

void
RoutingProtocol::SendRerrWhenBreaksLinkToNextHop (Ipv4Address nextHop)
{
  NS_LOG_FUNCTION (this << nextHop);
  RerrHeader rerrHeader;
  std::vector<Ipv4Address> precursors;
  std::map<Ipv4Address, uint32_t> unreachable;

  RoutingTableEntry toNextHop;
  if (!m_routingTable.LookupRoute (nextHop, toNextHop))
    return;
  toNextHop.GetPrecursors (precursors);
  rerrHeader.AddUnDestination (nextHop, toNextHop.GetSeqNo ());
  m_routingTable.GetListOfDestinationWithNextHop (nextHop, unreachable);
  for (std::map<Ipv4Address, uint32_t>::const_iterator i = unreachable.begin (); i
      != unreachable.end ();)
    {
      if (!rerrHeader.AddUnDestination (i->first, i->second))
        {
          NS_LOG_INFO ("Send RERR message with maximum size.");
          TypeHeader typeHeader (AODVTYPE_RERR);
          Ptr<Packet> packet = Create<Packet> ();
          packet->AddHeader (rerrHeader);
          packet->AddHeader (typeHeader);
          SendRerrMessage (packet, precursors);
          rerrHeader.Clear ();
        }
      else
        {
          RoutingTableEntry toDst;
          m_routingTable.LookupRoute (i->first, toDst);
          toDst.GetPrecursors (precursors);
          ++i;
        }
    }
  if (rerrHeader.GetDestCount () != 0)
    {
      TypeHeader typeHeader (AODVTYPE_RERR);
      Ptr<Packet> packet = Create<Packet> ();
      packet->AddHeader (rerrHeader);
      packet->AddHeader (typeHeader);
      SendRerrMessage (packet, precursors);
    }
  unreachable.insert (std::make_pair (nextHop, toNextHop.GetSeqNo ()));
  m_routingTable.InvalidateRoutesWithDst (unreachable);
}

void
RoutingProtocol::SendRerrWhenNoRouteToForward (Ipv4Address dst,
                                               uint32_t dstSeqNo, Ipv4Address origin)
{
  NS_LOG_FUNCTION (this);
  // A node SHOULD NOT originate more than RERR_RATELIMIT RERR messages per second.
  if (m_rerrCount == RerrRateLimit)
    {
      // Just make sure that the RerrRateLimit timer is running and will expire
      NS_ASSERT (m_rerrRateLimitTimer.IsRunning ());
      // discard the packet and return
      NS_LOG_LOGIC ("RerrRateLimit reached at " << Simulator::Now ().GetSeconds () << " with timer delay left " 
                                                << m_rerrRateLimitTimer.GetDelayLeft ().GetSeconds ()
                                                << "; suppressing RERR");
      return;
    }
  RerrHeader rerrHeader;
  rerrHeader.AddUnDestination (dst, dstSeqNo);
  RoutingTableEntry toOrigin;
  Ptr<Packet> packet = Create<Packet> ();
  packet->AddHeader (rerrHeader);
  packet->AddHeader (TypeHeader (AODVTYPE_RERR));
        m_txTrace (packet->Copy ()); // trace
  if (m_routingTable.LookupValidRoute (origin, toOrigin))
    {
      Ptr<Socket> socket = FindSocketWithInterfaceAddress (
          toOrigin.GetInterface ());
      NS_ASSERT (socket);
      NS_LOG_LOGIC ("Unicast RERR to the source of the data transmission");
      socket->SendTo (packet, 0, InetSocketAddress (toOrigin.GetNextHop (), AODV_PORT));
      m_txPacketTrace (packet);

    }
  else
    {
      for (std::map<Ptr<Socket>, Ipv4InterfaceAddress>::const_iterator i =
             m_socketAddresses.begin (); i != m_socketAddresses.end (); ++i)
        {
          Ptr<Socket> socket = i->first;
          Ipv4InterfaceAddress iface = i->second;
          NS_ASSERT (socket);
          NS_LOG_LOGIC ("Broadcast RERR message from interface " << iface.GetLocal ());
          // Send to all-hosts broadcast if on /32 addr, subnet-directed otherwise
          Ipv4Address destination;
          if (iface.GetMask () == Ipv4Mask::GetOnes ())
            {
              destination = Ipv4Address ("255.255.255.255");
            }
          else
            { 
              destination = iface.GetBroadcast ();
            }
          socket->SendTo (packet, 0, InetSocketAddress (destination, AODV_PORT));
          m_txPacketTrace (packet);
        }
    }
}

void
RoutingProtocol::SendRerrMessage (Ptr<Packet> packet, std::vector<Ipv4Address> precursors)
{
  NS_LOG_FUNCTION (this);
  m_txTrace (packet->Copy ()); // trace
  if (precursors.empty ())
    {
      NS_LOG_LOGIC ("No precursors");
      return;
    }
  // A node SHOULD NOT originate more than RERR_RATELIMIT RERR messages per second.
  if (m_rerrCount == RerrRateLimit)
    {
      // Just make sure that the RerrRateLimit timer is running and will expire
      NS_ASSERT (m_rerrRateLimitTimer.IsRunning ());
      // discard the packet and return
      NS_LOG_LOGIC ("RerrRateLimit reached at " << Simulator::Now ().GetSeconds () << " with timer delay left " 
                                                << m_rerrRateLimitTimer.GetDelayLeft ().GetSeconds ()
                                                << "; suppressing RERR");
      return;
    }
  // If there is only one precursor, RERR SHOULD be unicast toward that precursor
  if (precursors.size () == 1)
    {
      RoutingTableEntry toPrecursor;
      if (m_routingTable.LookupValidRoute (precursors.front (), toPrecursor))
        {
          Ptr<Socket> socket = FindSocketWithInterfaceAddress (toPrecursor.GetInterface ());
          NS_ASSERT (socket);
          NS_LOG_LOGIC ("one precursor => unicast RERR to " << toPrecursor.GetDestination () << " from " << toPrecursor.GetInterface ().GetLocal ());
          socket->SendTo (packet, 0, InetSocketAddress (precursors.front (), AODV_PORT));
          m_txPacketTrace (packet);
          m_rerrCount++;
        }
      return;
    }

  //  Should only transmit RERR on those interfaces which have precursor nodes for the broken route
  std::vector<Ipv4InterfaceAddress> ifaces;
  RoutingTableEntry toPrecursor;
  for (std::vector<Ipv4Address>::const_iterator i = precursors.begin (); i != precursors.end (); ++i)
    {
      if (m_routingTable.LookupValidRoute (*i, toPrecursor) && 
          std::find (ifaces.begin (), ifaces.end (), toPrecursor.GetInterface ()) == ifaces.end ())
        {
          ifaces.push_back (toPrecursor.GetInterface ());
        }
    }

  Ptr<Packet> copy;
  for (std::vector<Ipv4InterfaceAddress>::const_iterator i = ifaces.begin (); i != ifaces.end (); ++i)
    {
      Ptr<Socket> socket = FindSocketWithInterfaceAddress (*i);
      NS_ASSERT (socket);
      NS_LOG_LOGIC ("Broadcast RERR message from interface " << i->GetLocal ());
      // Send to all-hosts broadcast if on /32 addr, subnet-directed otherwise
      Ipv4Address destination;
      if (i->GetMask () == Ipv4Mask::GetOnes ())
        {
          destination = Ipv4Address ("255.255.255.255");
        }
      else
        { 
          destination = i->GetBroadcast ();
        }
      copy = packet->Copy();
      socket->SendTo (copy, 0, InetSocketAddress (destination, AODV_PORT));
      m_txPacketTrace (copy);
      m_rerrCount++;
    }
}

Ptr<Socket>
RoutingProtocol::FindSocketWithInterfaceAddress (Ipv4InterfaceAddress addr ) const
{
  NS_LOG_FUNCTION (this << addr);
  for (std::map<Ptr<Socket>, Ipv4InterfaceAddress>::const_iterator j =
         m_socketAddresses.begin (); j != m_socketAddresses.end (); ++j)
    {
      Ptr<Socket> socket = j->first;
      Ipv4InterfaceAddress iface = j->second;
      if (iface == addr)
        return socket;
    }
  Ptr<Socket> socket;
  return socket;
}

void
RoutingProtocol::SetLocalNodeStatus(NodeStatus _localNodeStatus) {
	  NS_LOG_FUNCTION(this << _localNodeStatus);
	  m_localNodeStatus = _localNodeStatus;
	  NotifyNodeStatusChanged();
}

void
RoutingProtocol::SetLocalAssociatedCORE(const Ipv4Address _localAssociatedCORE) {
	  NS_LOG_FUNCTION(this << _localAssociatedCORE);
	  if (m_localAssociatedCORE != _localAssociatedCORE){
		 
	  }
	  m_localAssociatedCORE = _localAssociatedCORE;
}

void
RoutingProtocol::SetLocalcore_noncoreIndicator(core_noncore_Indicator _localcore_noncoreIndicator) {
	  NS_LOG_FUNCTION(this << _localcore_noncoreIndicator);
	  m_localcore_noncoreIndicator = _localcore_noncoreIndicator;
}

void
RoutingProtocol::AssociationAlgorithm() {
	NS_LOG_FUNCTION (this);
	NS_ASSERT(m_localNodeStatus != NEIGH_NODE);
	NeighborTuple *best = m_nb.GetBestNeighbor (NEIGH_NODE);
	if (best == NULL)
		best = m_nb.GetBestNeighbor(CORE);
	if (best == NULL)
		best = m_nb.GetBestNeighbor(RN_NODE);
	if (best == NULL) {
		
	} else
		SetLocalAssociatedCORE(best->neighborIfaceAddr);
}

bool
RoutingProtocol::Joining_Quitting_Mechanism() {
	NS_LOG_FUNCTION (this);
	NS_ASSERT(m_localNodeStatus == CORE);
	bool rule2 = BCN2BNRule2();
	if (!rule2) return false;
	bool conv1 = HandleJoin(); 
	bool conv2 = HandlePushJoin(); 
	bool conv3 = HandleJoinAmDuplex(); 
	bool BCN2BN = rule2 && (conv1 || conv2 || conv3);
	if(BCN2BN){
		SetLocalNodeStatus(NEIGH_NODE); 
	}
	return BCN2BN;
}


bool
RoutingProtocol::HandleJoin() {
	NS_LOG_FUNCTION (this);
	bool convert = false;
	bool conversion1a = m_nb.GetOneHopNeighbors(NEIGH_NODE).empty();
	NeighborTuple *best_bcn = m_nb.GetBestNeighbor(CORE);// get the best Bcn
	bool conversion1b = (best_bcn != NULL && HigherWeight(best_bcn));// || best_bcn == NULL;
	bool conversion2 = !(m_nb.GetClients(CORE).empty() && m_nb.GetClients(RN_NODE).empty());
	convert = ((conversion1a && conversion1b) || conversion2);
	return convert;
}

bool
RoutingProtocol::Are1HopNeighbors(const Ipv4Address &anode_v, const Ipv4Address &anode_w) {
	NS_LOG_FUNCTION (this);
	bool neighbors = ! (m_nb.Intersection(m_nb.GetMulticastNeighbors(anode_v),anode_w).empty() && m_nb.Intersection(m_nb.GetMulticastNeighbors(anode_w),anode_v).empty());
	return neighbors;
}

bool
RoutingProtocol::Are2HopNeighbors(const Ipv4Address &BNnode_v, const Ipv4Address &BNnode_w) {
	NS_LOG_FUNCTION (this << BNnode_v << BNnode_w);
	AddressSet vset= m_nb.GetMulticastNeighbors(BNnode_v);
	m_nb.PrintAddressSet(vset);
	AddressSet wset= m_nb.GetMulticastNeighbors(BNnode_w);
	m_nb.PrintAddressSet(wset);
	bool are2hop = !m_nb.Intersection(vset,wset).empty();
	return are2hop;
}


bool
RoutingProtocol::HandlePushJoinNonDC(const Ipv4Address &BNnode_v, const Ipv4Address &BNnode_w) {
	NS_LOG_FUNCTION (this);
	NS_ASSERT(m_nb.FindNeighborTuple(BNnode_v)->neighborNodeStatus == NEIGH_NODE);
	NS_ASSERT(m_nb.FindNeighborTuple(BNnode_w)->neighborNodeStatus == NEIGH_NODE || m_nb.FindNeighborTuple(BNnode_w)->neighborNodeStatus == CORE);
	Groups pairs = IsDirectlyConnected(m_nb.GetOneHopNeighbors(NEIGH_NODE), m_nb.GetOneHopNeighbors(NEIGH_NODE));
	Ipv4Address ipx, ipy;
	bool xy_exist = false;
	
	for (std::list<NeighborPair>::iterator xy = pairs.begin(); xy != pairs.end() && !xy_exist; xy++) {
		ipx = xy->neighborFirstIfaceAddr;
		ipy = xy->neighborSecondIfaceAddr;
		if (!Are1HopNeighbors (ipx, ipy)) continue; 
		if( ipx == BNnode_v || ipx == BNnode_w || ipy == BNnode_v || ipy == BNnode_w )
			continue;
		AddressSet NBNw = m_nb.GetMulticastNeighbors(BNnode_v);
		AddressSet NBNv = m_nb.GetMulticastNeighbors(BNnode_w);
		bool vBNx = !m_nb.Intersection(NBNv,ipx).empty(); 
		bool wBNy = !m_nb.Intersection(NBNw,ipy).empty(); 
		bool vBNy = !m_nb.Intersection(NBNv,ipy).empty();
		bool wBNx = !m_nb.Intersection(NBNw,ipx).empty();
		xy_exist |= ((vBNx && wBNy) || (vBNy && wBNx ));
		
	}
	return xy_exist;
}

Groups
RoutingProtocol::HandleJoinAmNotDuplex(NeighborSet set_1, NeighborSet set_2){
	NS_LOG_FUNCTION (this);
	Groups no_connected_pair; 
	Groups pairs = IsDirectlyConnected(set_1,set_2);
	for (Groups::iterator pair = pairs.begin(); pair != pairs.end(); pair++) {
	
		Ipv4Address ipv,ipw;
		AddressSet NBNv,NBNw;
		ipv = pair->neighborFirstIfaceAddr;
		ipw = pair->neighborSecondIfaceAddr;
		NS_ASSERT (ipv!=ipw);
		NS_ASSERT(m_nb.IsNeighbor(ipv) && m_nb.IsNeighbor(ipw));
		NBNv = m_nb.GetMulticastNeighbors(ipv);
		NBNw = m_nb.GetMulticastNeighbors(ipw);
		bool are1hop = !m_nb.Intersection(NBNv, ipw).empty() || !m_nb.Intersection(NBNw, ipv).empty();

		bool are2hop = !m_nb.Intersection(NBNv, NBNw).empty();
		if ( !(are1hop || are2hop) ) { 
			bool RULE1 = GetRule1() && HandlePushJoinNonDC(ipv, ipw); 
			if (!RULE1) {
				NeighborPair nnpp = {ipv, ipw}; 
				no_connected_pair.push_back(nnpp);
				
			}
			else
NS_LOG_DEBUG(" is directly Connected through: Node v,w <" << ipv << "," << ipw
						<<"> are NOT 1 hop ("<<are1hop<<") neither 2-Hop ("<<are2hop<<") neighbors"<<RULE1);
				
		}
		else
NS_LOG_DEBUG("not directly Connected: Node v,w <" << ipv << "," << ipw <<"> are 1 hop ("<<are1hop<<") or  2-Hop ("<<are2hop<<") neighbors ");
			
	}
	return no_connected_pair;
}

bool
RoutingProtocol::BCN2BNRule2(){
	NS_LOG_FUNCTION (this);
	if(GetRule2())
	{	
		return (m_localCurrentBnNeighbors > m_localLastBnNeighbors?false:true);
	}
	else
		return true;
}

bool ex (int i) {
  return ((i%2)==1);
}
//
Groups
RoutingProtocol::IsDirectlyConnected(NeighborSet one, NeighborSet two){
	NS_LOG_FUNCTION (this);
	Groups all_pairs;
	for(NeighborSet::iterator iter1 = one.begin(); iter1 != one.end(); iter1++){
		for(NeighborSet::iterator iter2 = two.begin(); iter2!= two.end(); iter2++){
			if(iter1->neighborIfaceAddr==iter2->neighborIfaceAddr)continue;
			NeighborPair np = {iter1->neighborIfaceAddr,iter2->neighborIfaceAddr};
				all_pairs.push_back(np);
		}
	}
	return all_pairs;
}

bool
RoutingProtocol::HandlePushJoin() {
	NS_LOG_FUNCTION(this);
	bool pair2connect = false;
	bool neighbor_xv = false;
	bool neighbor_xw = false;
	Ipv4Address ipv, ipw, ipx;

	Groups BNPairs = IsDirectlyConnected(m_nb.GetOneHopNeighbors(NEIGH_NODE),m_nb.GetOneHopNeighbors(NEIGH_NODE));
	NeighborSet BCNnodes = m_nb.GetOneHopNeighbors(CORE);
	for(Groups::iterator pair = BNPairs.begin(); pair != BNPairs.end() && !pair2connect; pair++){
		ipv = pair->neighborFirstIfaceAddr;
		ipw = pair->neighborSecondIfaceAddr;
		bool onehop = Are1HopNeighbors(ipv,ipw);
		bool twohop = Are2HopNeighbors(ipv,ipw);
		{
			AddressSet BNv = m_nb.GetMulticastNeighbors(ipv);
			BNv.push_back(ipv);
			AddressSet BNw = m_nb.GetMulticastNeighbors(ipw);
			BNw.push_back(ipw);
			bool onetwo = !(m_nb.Intersection (BNv,BNw).empty());
			NS_ASSERT (onetwo == (onehop||twohop));
		}
		bool ruleone = (GetRule1() && HandlePushJoinNonDC(ipv,ipw));
		if ( onehop || twohop || ruleone ) {
			
			continue;
		}
		bool x_exist = false, higher = false;
		for (NeighborSet::iterator bcn_x = BCNnodes.begin(); bcn_x!= BCNnodes.end() && !x_exist ; bcn_x++) {// for each x in Nbcn(u)
			ipx = bcn_x->neighborIfaceAddr;
			neighbor_xv = Are1HopNeighbors(ipx, ipv);
			neighbor_xw = Are1HopNeighbors(ipx, ipw);
			higher = HigherWeight(ipx);
			
			x_exist |= (neighbor_xw && neighbor_xv && !higher);
		}
		pair2connect |= !x_exist;
	}
	
	return pair2connect;
}


bool
RoutingProtocol::HandleJoinAmDuplex() {
	NS_LOG_FUNCTION (this);

	Groups no_connected_pair = HandleJoinAmNotDuplex(m_nb.GetOneHopNeighbors(NEIGH_NODE),m_nb.GetOneHopNeighbors(CORE));
	if (no_connected_pair.empty()) {
		return false;
	}
	bool pair2connect = false;
	NeighborSet bcn_nodes = m_nb.GetOneHopNeighbors(CORE);
	Groups::iterator pair_vw;
	Ipv4Address ipv,ipw,ipx;
	NeighborSet::iterator bcn_x;
	for (pair_vw = no_connected_pair.begin() ; pair_vw != no_connected_pair.end() && !pair2connect ; pair_vw++) {
		ipv = pair_vw->neighborFirstIfaceAddr;
		ipw = pair_vw->neighborSecondIfaceAddr;
		AddressSet NBNw = m_nb.GetMulticastNeighbors(ipw); 

		AddressSet NBNv = m_nb.GetMulticastNeighbors(ipv);
		NBNv.push_front (pair_vw->neighborFirstIfaceAddr); 

		NS_ASSERT(m_nb.Intersection(NBNw,NBNv).empty()); 
		if (NBNw.empty()) continue; 
		bool x_exist = false;

		for (bcn_x = bcn_nodes.begin(); bcn_x != bcn_nodes.end() && !x_exist; bcn_x++) {
			ipx = bcn_x->neighborIfaceAddr;
			if ( ipx == ipw ) continue; 
			AddressSet NBNx = m_nb.GetMulticastNeighbors(ipx); 

			bool neighbor_xv = !(m_nb.Intersection(NBNx,ipv).empty());
			AddressSet intersection = m_nb.Intersection(NBNx, NBNw); 
			bool neighbor_xz = (!intersection.empty()); 
			x_exist |= ((neighbor_xv && neighbor_xz) && !HigherWeight(ipx)); 
			
		}
		pair2connect |= !x_exist;
	}

	return pair2connect;
}

bool
RoutingProtocol::HeartBeat_Pushjoin_Anchors() {
	NS_LOG_FUNCTION (this);
	NS_ASSERT(m_localNodeStatus == NEIGH_NODE);
	bool cond_1 = HeartBeat_Pushjoin_Anchors_1();
	bool cond_2 = HeartBeat_Pushjoin_Anchors_2();
	bool cond_3 = HeartBeat_Pushjoin_Anchors_3();
	SetLocalcore_noncoreIndicator(CONVERT_OTHER);
	if(!cond_1 || !(cond_2 && cond_3))
		SetLocalcore_noncoreIndicator(CONVERT_BREAK);
	if(cond_1 && !(cond_2 && cond_3))
		SetLocalcore_noncoreIndicator(CONVERT_ALLOW);
	bool BN2BCN = cond_1 && cond_2 && cond_3;
	if(BN2BCN){
		SetLocalNodeStatus(CORE);
	}
	return BN2BCN;
}


bool
RoutingProtocol::HeartBeat_Pushjoin_Anchors_1() {
	NS_LOG_FUNCTION (this);
	bool cond1 = (m_nb.GetClients(RN_NODE).empty());//no RN clients
	bool cond2 = true;
	NeighborSet bcn_nodes = m_nb.GetOneHopNeighbors (CORE);
	for(NeighborSet::iterator bcn_cli = bcn_nodes.begin(); bcn_cli != bcn_nodes.end() && cond1 && cond2; bcn_cli++){
		if (!bcn_cli->neighborClient) continue;
		uint32_t bcn_size = bcn_cli->neighborBnNeighbors.size();
		cond2 &= bcn_size>1; 
	}
	
	return cond1 && cond2;
}


Groups
RoutingProtocol::HeartBeat_Pushjoin_Anchors_2a(Groups gp) {
	NS_LOG_FUNCTION (this);
	NeighborTuple *nodev,*nodew;
	Ipv4Address ipv, ipw;
	Groups no_direct;
	for(Groups::iterator pair = gp.begin(); pair != gp.end(); pair++){
		ipv = pair->neighborFirstIfaceAddr;
		ipw = pair->neighborSecondIfaceAddr;
		nodev = m_nb.FindNeighborTuple(ipv);
		nodew = m_nb.FindNeighborTuple(ipw);
		bool neighbors = Are1HopNeighbors(ipv,ipw);
		bool higher = !HigherWeight(ipv) || !HigherWeight(ipw);
		bool breaker = ((nodev->neighborcore_noncoreIndicator == CONVERT_BREAK) || (nodew->neighborcore_noncoreIndicator == CONVERT_BREAK));
		if (!(neighbors && (higher||breaker))){ 
			NeighborPair vw = {ipv,ipw};
			no_direct.push_back(vw);
			}
		
	}

	return no_direct;
}



Groups
RoutingProtocol::HeartBeat_Pushjoin_Anchors_2b(Groups gp) {
	NS_LOG_FUNCTION (this);
	Groups no_indirect;
	Ipv4Address ipv, ipw, ipx;
	for(Groups::iterator pair = gp.begin(); pair != gp.end(); pair++){
		ipv = pair->neighborFirstIfaceAddr;
		ipw = pair->neighborSecondIfaceAddr;
		bool convert = false;
		AddressSet nodesX = m_nb.GetCommonBN(*pair);
		
		for(AddressSet::iterator commonbn = nodesX.begin(); commonbn != nodesX.end() && !convert; commonbn++){
			ipx = *commonbn;
			if (IsMyOwnAddress(ipx)) continue;
			MulticastBnNeighborTuple *common_bn = m_nb.FindMulticastBnNeighborTuple (ipv,ipx);
			NS_ASSERT (common_bn);
			bool higher = !HigherWeight(common_bn);
			bool breaker = common_bn->twoHopBnNeighborIndicator == CONVERT_BREAK;
			convert |= (higher||breaker);
			
		}
		if(!convert) no_indirect.push_back(*pair);
	}
	return no_indirect;
}

Groups
RoutingProtocol::HeartBeat_Pushjoin_Anchors_2c(Groups gp) {
	NS_LOG_FUNCTION (this);
	Groups no3hop;
	Ipv4Address ipv, ipw;
	if (!GetRule1()) return gp;
	for(Groups::iterator pair = gp.begin(); pair != gp.end(); pair++){
		ipv = pair->neighborFirstIfaceAddr;
		ipw = pair->neighborSecondIfaceAddr;
		
		bool RULE1 = HandlePushJoinNonDC(ipv,ipw);
		if (!RULE1)
			no3hop.push_back(*pair);
		
	}
	return no3hop;
}

bool
RoutingProtocol::HeartBeat_Pushjoin_Anchors_2() {
	NS_LOG_FUNCTION (this);
	Groups gp = IsDirectlyConnected(m_nb.GetOneHopNeighbors(NEIGH_NODE),m_nb.GetOneHopNeighbors(NEIGH_NODE));
	Groups gpA = HeartBeat_Pushjoin_Anchors_2a(gp);
	Groups gpB = HeartBeat_Pushjoin_Anchors_2b(gpA);
	Groups gpC = HeartBeat_Pushjoin_Anchors_2c(gpB);
	return gpC.empty();
}

bool
RoutingProtocol::HigherWeight(Ipv4Address neighbor){
	NS_LOG_FUNCTION (this);
	NeighborTuple *node = m_nb.FindNeighborTuple(neighbor);
	NS_ASSERT (node) ;
	return HigherWeight(node);
}

bool
RoutingProtocol::HigherWeight(NeighborTuple *node){
	NS_LOG_FUNCTION (this<<node->neighborIfaceAddr);
	return (m_localWeight > node->neighborWeight ||
			(m_localWeight == node->neighborWeight && m_mainAddress.Get() > node->neighborIfaceAddr.Get()));
}

bool
RoutingProtocol::HigherWeight(MulticastBnNeighborTuple *node2hop){
       NS_LOG_FUNCTION (this<<node2hop->twoHopBnNeighborIfaceAddr);
       return (m_localWeight > node2hop->twoHopBnNeighborWeight ||
                       (m_localWeight == node2hop->twoHopBnNeighborWeight && m_mainAddress.Get() > node2hop->twoHopBnNeighborIfaceAddr.Get()));
}

bool
RoutingProtocol::GetMyAddress(NeighborTuple *node,Ipv4Address &myself){
	NS_LOG_FUNCTION (this);
	RoutingTableEntry rt;
		Ipv4InterfaceAddress tmp;
		if(m_routingTable.LookupRoute(node->neighborIfaceAddr,rt))
			tmp = rt.GetInterface();
		else
			return false;
		myself = tmp.GetLocal();
	return true;
}

Groups
RoutingProtocol::HeartBeat_Pushjoin_Anchors_3a(Groups pairs) {
	NS_LOG_FUNCTION (this);
	Groups no_direct;
	Ipv4Address ipv, ipw;
	for(Groups::iterator pair = pairs.begin(); pair != pairs.end(); pair++){
		ipv = pair->neighborFirstIfaceAddr;
		ipw = pair->neighborSecondIfaceAddr;
		AddressSet BNw = m_nb.GetMulticastNeighbors(ipw);
		bool connected = Are1HopNeighbors(ipv,ipw);
		NeighborTuple* bn = m_nb.FindNeighborTuple(ipv);
		bool higher = !HigherWeight(bn);
		bool breaker = (bn->neighborcore_noncoreIndicator == CONVERT_BREAK);
		
		if (!(connected && (higher||breaker)))
			no_direct.push_back(*pair);
	}
	return no_direct;
}

Groups
RoutingProtocol::HeartBeat_Pushjoin_Anchors_3b(Groups pairs) {
	NS_LOG_FUNCTION (this);
	Groups no_indirect;
	Ipv4Address ipv, ipw;
	for(Groups::iterator pair = pairs.begin(); pair != pairs.end(); pair++){
		ipv = pair->neighborFirstIfaceAddr;
		ipw = pair->neighborSecondIfaceAddr;
		bool convert = false;
		
		AddressSet nodesx = m_nb.GetCommonBN(*pair);
		for(AddressSet::iterator bn_x = nodesx.begin(); bn_x != nodesx.end() && !convert; bn_x++){
			if (IsMyOwnAddress(*bn_x)) continue;
			MulticastBnNeighborTuple *common_bn = m_nb.FindMulticastBnNeighborTuple(ipv,*bn_x);
			MulticastBnNeighborTuple *common_bnw = m_nb.FindMulticastBnNeighborTuple(ipw,*bn_x);
			NS_ASSERT(common_bn && common_bnw && common_bn->twoHopBnNeighborIfaceAddr == common_bnw->twoHopBnNeighborIfaceAddr);
			bool higher = !HigherWeight(common_bn);
			bool breaker = common_bn->twoHopBnNeighborIndicator == CONVERT_BREAK;
			convert |= (higher||breaker);
			
		}
		if (!convert){
			
			no_indirect.push_back(*pair);
		}
	}
	return no_indirect;
}

Groups
RoutingProtocol::HeartBeat_Pushjoin_Anchors_3c(Groups gp) {
	NS_LOG_FUNCTION (this);
	Groups no3hop;
	Ipv4Address ipv, ipw;
	if (!GetRule1()) return gp;
	for(Groups::iterator pair = gp.begin(); pair != gp.end(); pair++){
		ipv = pair->neighborFirstIfaceAddr;
		ipw = pair->neighborSecondIfaceAddr;
		
		bool RULE1 = HandlePushJoinNonDC(ipv,ipw);
		if (!RULE1)
			no3hop.push_back(*pair);
		
	}
	return no3hop;
}

bool
RoutingProtocol::HeartBeat_Pushjoin_Anchors_3() {
	NS_LOG_FUNCTION (this);
	Groups pairs = IsDirectlyConnected(m_nb.GetOneHopNeighbors(NEIGH_NODE), m_nb.GetOneHopNeighbors(CORE));
	Groups pairsA = HeartBeat_Pushjoin_Anchors_3a(pairs);
	Groups pairsB = HeartBeat_Pushjoin_Anchors_3b(pairsA);
	Groups pairsC = HeartBeat_Pushjoin_Anchors_3c(pairsB);

	return pairsC.empty();
}

}
}

