
#ifndef __AODV_ROUTING_PROTOCOL_H__
#define __AODV_ROUTING_PROTOCOL_H__

#include "aodv-rtable.h"
#include "aodv-rqueue.h"
#include "aodv-packet.h"
#include "aodv-neighbor.h"
#include "aodv-dpd.h"
#include "ns3/node.h"
#include "ns3/traced-callback.h"
#include "ns3/random-variable-stream.h"
#include "ns3/output-stream-wrapper.h"
#include "ns3/ipv4-routing-protocol.h"
#include "ns3/ipv4-interface.h"
#include "ns3/ipv4-l3-protocol.h"
#include "ns3/log.h"
#include "ns3/callback.h"
#include "ns3/traced-value.h"
#include <map>

#define aodvaddr_t int


namespace ns3
{
namespace aodvmesh
{

class RoutingProtocol : public Ipv4RoutingProtocol
{
public:
  static TypeId GetTypeId (void);
  static const uint32_t AODV_PORT;

  /// c-tor
  RoutingProtocol ();
  virtual ~RoutingProtocol();
  virtual void DoDispose ();

  ///\name From Ipv4RoutingProtocol
  //\{
  Ptr<Ipv4Route> RouteOutput (Ptr<Packet> p, const Ipv4Header &header, Ptr<NetDevice> oif, Socket::SocketErrno &sockerr);
  bool RouteInput (Ptr<const Packet> p, const Ipv4Header &header, Ptr<const NetDevice> idev,
                   UnicastForwardCallback ucb, MulticastForwardCallback mcb,
                   LocalDeliverCallback lcb, ErrorCallback ecb);
  virtual void NotifyInterfaceUp (uint32_t interface);
  virtual void NotifyInterfaceDown (uint32_t interface);
  virtual void NotifyAddAddress (uint32_t interface, Ipv4InterfaceAddress address);
  virtual void NotifyRemoveAddress (uint32_t interface, Ipv4InterfaceAddress address);
  virtual void SetIpv4 (Ptr<Ipv4> ipv4);
  virtual void PrintRoutingTable (Ptr<OutputStreamWrapper> stream) const;
  //\}

  ///\name Handle protocol parameters
  //\{
  Time GetMaxQueueTime () const { return MaxQueueTime; }
  void SetMaxQueueTime (Time t);
  uint32_t GetMaxQueueLen () const { return MaxQueueLen; }
  void SetMaxQueueLen (uint32_t len);
  bool GetDesinationOnlyFlag () const { return DestinationOnly; }
  void SetDesinationOnlyFlag (bool f) { DestinationOnly = f; }
  bool GetGratuitousReplyFlag () const { return GratuitousReply; }
  void SetGratuitousReplyFlag (bool f) { GratuitousReply = f; }
  void SetHelloEnable (bool f) { EnableHello = f; }
  bool GetHelloEnable () const { return EnableHello; }
  void SetBroadcastEnable (bool f) { EnableBroadcast = f; }
  bool GetBroadcastEnable () const { return EnableBroadcast; }
 void SetMaliciousEnable (bool f) { IsMalicious = f; }                        
bool GetMaliciousEnable () const { return IsMalicious; }                     
   int64_t AssignStreams (int64_t stream);
  //}
  ///\name AODVMESH Functions
  //\{
  /// AODVMESH FUNCTIONS
  void SetLocalWeight(uint32_t _localWeight) {m_localWeight = _localWeight;}
  uint32_t GetLocalWeight() const {return m_localWeight;}
  void UpdateLocalWeight ();
  void SetLocalWeightFunction(WeightFunction _weightFunction) {m_localWeightFunction = _weightFunction;}
  WeightFunction GetLocalWeightFunction() const{return m_localWeightFunction;}
  void SetLocalNodeStatus(NodeStatus _localNodeStatus);
  NodeStatus GetLocalNodeStatus() const {return m_localNodeStatus;}
  void SetLocalAssociatedCORE(const Ipv4Address _localAssociatedCORE);
  Ipv4Address GetLocalAssociatedCORE() const {return m_localAssociatedCORE;}
  void SetLocalcore_noncoreIndicator(core_noncore_Indicator _localcore_noncoreIndicator);
  core_noncore_Indicator GetLocalcore_noncoreIndicator() const {return m_localcore_noncoreIndicator;}
  void SetLocalCurrentBnNeighbors(uint32_t _currentBnNodes) {m_localCurrentBnNeighbors= (uint16_t) _currentBnNodes;}
  uint32_t GetLocalCurrentBnNeighbors() const {return (uint32_t) m_localCurrentBnNeighbors;}
  void SetLocalLastBnNeighbors(uint32_t _lastBnNodes) {m_localLastBnNeighbors= (uint16_t) _lastBnNodes;}
  uint32_t GetLocalLastBnNeighbors() const {return (uint32_t) m_localLastBnNeighbors;}
  void UpdateLocalLastBnNeighbors(){SetLocalLastBnNeighbors(m_localCurrentBnNeighbors);SetLocalCurrentBnNeighbors(m_nb.GetNeighborhoodSize(NEIGH_NODE));}
  void SetRule1 (bool f){Rule1 = f;}
  bool GetRule1 () const {return Rule1;}
  void SetRule2 (bool f){Rule2 = f;}
  bool GetRule2 () const{return Rule2;}

  void handlePacketReceipt(Ptr<Packet> p); 

	
  void GetLocalState ();
  Groups IsDirectlyConnected(NeighborSet one, NeighborSet two);
  uint32_t GetOneHopNeighborsSize (aodvmesh::NodeStatus nodeStatus){return m_nb.GetOneHopNeighbors(nodeStatus).size();}
  Groups HandleJoinAmNotDuplex(NeighborSet one, NeighborSet two);
  bool HigherWeight(Ipv4Address neighbor);
  bool HigherWeight(NeighborTuple *node);
  bool HigherWeight(MulticastBnNeighborTuple *node2hop);
  bool Are1HopNeighbors(const Ipv4Address &BNnode_v, const Ipv4Address &BNnode_w);
  bool Are2HopNeighbors(const Ipv4Address&, const Ipv4Address&);
  bool HandlePushJoinNonDC(const Ipv4Address &BNnode_v, const Ipv4Address &BNnode_w);
  NeighborTriples GetCommonBN(NeighborPair gp);

  bool BCN2BNRule2();
  void AssociationAlgorithm();
  bool Joining_Quitting_Mechanism();
  bool HandleJoin();
  bool HandlePushJoin();
  bool HandleJoinAmDuplex();
  bool HeartBeat_Pushjoin_Anchors();
  bool HeartBeat_Pushjoin_Anchors_1();
  bool HeartBeat_Pushjoin_Anchors_2();
  Groups HeartBeat_Pushjoin_Anchors_2a(Groups gp);
  Groups HeartBeat_Pushjoin_Anchors_2b(Groups gp);
  Groups HeartBeat_Pushjoin_Anchors_2c(Groups gp);
  Groups HeartBeat_Pushjoin_Anchors_2d(Groups gp);
  bool HeartBeat_Pushjoin_Anchors_3();
  Groups HeartBeat_Pushjoin_Anchors_3a(Groups pairs);
  Groups HeartBeat_Pushjoin_Anchors_3b(Groups pairs);
  Groups HeartBeat_Pushjoin_Anchors_3c(Groups pairs);
  //\}
private:
  ///\name Protocol parameters.
  //\{
  uint32_t RreqRetries;             ///< Maximum number of retransmissions of RREQ with TTL = NetDiameter to discover a route
  uint16_t RreqRateLimit;           ///< Maximum number of RREQ per second.
  uint16_t RerrRateLimit;           ///< Maximum number of REER per second.
  Time ActiveRouteTimeout;          ///< Period of time during which the route is considered to be valid.
  uint32_t NetDiameter;             ///< Net diameter measures the maximum possible number of hops between two nodes in the network
  /**
   *  NodeTraversalTime is a conservative estimate of the average one hop traversal time for packets
   *  and should include queuing delays, interrupt processing times and transfer times.
   */
  Time NodeTraversalTime;
  Time NetTraversalTime;             ///< Estimate of the average net traversal time.
  Time PathDiscoveryTime;            ///< Estimate of maximum time needed to find route in network.
  Time MyRouteTimeout;               ///< Value of lifetime field in RREP generating by this node.
  /**
   * Every HelloInterval the node checks whether it has sent a broadcast  within the last HelloInterval.
   * If it has not, it MAY broadcast a  Hello message
   */
  Time HelloInterval;
  uint32_t AllowedHelloLoss;         ///< Number of hello messages which may be loss for valid link
  /**
   * DeletePeriod is intended to provide an upper bound on the time for which an upstream node A
   * can have a neighbor B as an active next hop for destination D, while B has invalidated the route to D.
   */
  Time DeletePeriod;
  Time NextHopWait;                  ///< Period of our waiting for the neighbour's RREP_ACK
  /**
   * The TimeoutBuffer is configurable.  Its purpose is to provide a buffer for the timeout so that if the RREP is delayed
   * due to congestion, a timeout is less likely to occur while the RREP is still en route back to the source.
   */
  uint16_t TimeoutBuffer;
  Time BlackListTimeout;             ///< Time for which the node is put into the blacklist
  uint32_t MaxQueueLen;              ///< The maximum number of packets that we allow a routing protocol to buffer.
  Time MaxQueueTime;                 ///< The maximum period of time that a routing protocol is allowed to buffer a packet for.
  bool DestinationOnly;              ///< Indicates only the destination may respond to this RREQ.
  bool GratuitousReply;              ///< Indicates whether a gratuitous RREP should be unicast to the node originated route discovery.
  bool EnableHello;                  ///< Indicates whether a hello messages enable
  bool EnableBroadcast;              ///< Indicates whether a a broadcast data packets forwarding enable
  //\}

  /// IP protocol
  Ptr<Ipv4> m_ipv4;
  /// Raw socket per each IP interface, map socket -> iface address (IP + mask)
  std::map< Ptr<Socket>, Ipv4InterfaceAddress > m_socketAddresses;
  /// Loopback device used to defer RREQ until packet will be fully formed
  Ptr<NetDevice> m_lo; 

  /// Routing table
  RoutingTable m_routingTable;
  /// A "drop-front" queue used by the routing layer to buffer packets to which it does not have a route.
  RequestQueue m_queue;
  /// Broadcast ID
  uint32_t m_requestId;
  /// Request sequence number
  uint32_t m_seqNo;
  /// Handle duplicated RREQ
  IdCache m_rreqIdCache;
  /// Handle duplicated broadcast/multicast packets
  DuplicatePacketDetection m_dpd;
  /// Handle neighbors
  Neighbors m_nb;
  /// Number of RREQs used for RREQ rate control
  uint16_t m_rreqCount;
  /// Number of RERRs used for RERR rate control
  uint16_t m_rerrCount;
bool IsMalicious; 
  /// Tracing node status
  TracedCallback<Ptr<const aodvmesh::RoutingProtocol> > m_localNodeStatusTrace;
  /// Tracing control traffic sent
  TracedCallback <Ptr<const Packet> > m_txPacketTrace;
  /// Tracing control traffic received
  TracedCallback <Ptr<const Packet> > m_rxPacketTrace;

  /// Node main address
  Ipv4Address m_mainAddress;

	///\name AODVMESH protocol parameters
	//\{
	IdCache m_helloIdCache;
	uint16_t m_messageSequenceNumber; ///< Message Sequence Number: incremented by one each time a new AODVMESH packet is transmitted.
	uint16_t m_weightSize; ///< Size of the weight field, counted in bytes and measured from the beginning of the "Weight field" field and until the end of such a field.
	uint32_t m_localWeight; ///< Local weight.
	WeightFunction m_localWeightFunction; ///< Weight function used.
	NodeStatus m_localNodeStatus; ///< Local node status.
	core_noncore_Indicator m_localcore_noncoreIndicator; ///< Local BN-to-BNC indicator.
	Ipv4Address m_localAssociatedCORE; ///< Local associated BN or next hop RN towards a BN.
	/// HelloInterval Time short_timer;					///< Timer for sending HelloMessages.
	Time ShortInterval; ///< Timer for updating the neighbor list.
	Time LongInterval; ///< Timer for updating the neighbor list.
	///m_nb std::list<NeighborTuple> localNeighborList;	///< Neighbor List.
	///MulticastBnNeighborSet localMulticastNeighborList;		///< Two Hop Neighbor List
	uint16_t m_localCurrentBnNeighbors; ///< Current number of NB neighbor nodes
	uint16_t m_localLastBnNeighbors; ///< Number of BN neighbor in the last short-time period
	bool Rule1; ///< Rule 1 on/off
	bool Rule2; ///< Rule 2 on/off
	//}

private:
  /// Start protocol operation
  void Start ();
  /// Queue packet and send route request
  void DeferredRouteOutput (Ptr<const Packet> p, const Ipv4Header & header, UnicastForwardCallback ucb, ErrorCallback ecb);
  /// If route exists and valid, forward packet.
  bool Forwarding (Ptr<const Packet> p, const Ipv4Header & header, UnicastForwardCallback ucb, ErrorCallback ecb);
  /**
  * To reduce congestion in a network, repeated attempts by a source node at route discovery
  * for a single destination MUST utilize a binary exponential backoff.
  */
  void ScheduleRreqRetry (Ipv4Address dst);
  /**
   * Set lifetime field in routing table entry to the maximum of existing lifetime and lt, if the entry exists
   * \param addr - destination address
   * \param lt - proposed time for lifetime field in routing table entry for destination with address addr.
   * \return true if route to destination address addr exist
   */
  bool UpdateRouteLifeTime (Ipv4Address addr, Time lt);
  /**
   * Update neighbor record.
   * \param receiver is supposed to be my interface
   * \param sender is supposed to be IP address of my neighbor.
   */
  void UpdateRouteToNeighbor (Ipv4Address sender, Ipv4Address receiver);
  /// Check that packet is send from own interface
  bool IsMyOwnAddress (Ipv4Address src);
  /// Get the node address that is connected to that neighbor.
  bool GetMyAddress(NeighborTuple *node,Ipv4Address &myself);
  /// Find socket with local interface address iface
  Ptr<Socket> FindSocketWithInterfaceAddress (Ipv4InterfaceAddress iface) const;
  /// Process hello message
  void ProcessHello (RrepHeader const & rrepHeader, Ipv4Address receiverIfaceAddr);
  /// Create loopback route for given header
  Ptr<Ipv4Route> LoopbackRoute (const Ipv4Header & header, Ptr<NetDevice> oif) const;

  void NotifyNodeStatusChanged (void) const;

  ///\name Receive control packets
  //\{
  /// Receive and process control packet
  void RecvAodv (Ptr<Socket> socket);
  /// Receive HELLO
  void RecvHello (Ptr<Packet> p, Ipv4Address receiver, Ipv4Address src);
  /// Receive RREQ
  void RecvRequest (Ptr<Packet> p, Ipv4Address receiver, Ipv4Address src);
  /// Receive RREP
  void RecvReply (Ptr<Packet> p, Ipv4Address my,Ipv4Address src);
  /// Receive RREP_ACK
  void RecvReplyAck (Ipv4Address neighbor);
  /// Receive RERR from node with address src
  void RecvError (Ptr<Packet> p, Ipv4Address src);
  //\}

	Ipv4Address mcast_base_address_;
	short rrep_pending_ack;
        int jq_seqno;

  ///\name Send
  //\{
  /// Forward packet from route request queue
  void SendPacketFromQueue (Ipv4Address dst, Ptr<Ipv4Route> route);
  /// Send hello
  void SendHello ();
  /// Send RREQ
  void SendRequest (Ipv4Address dst);
  /// Send RREP
  void SendReply (RreqHeader const & rreqHeader, RoutingTableEntry const & toOrigin);
  /** Send RREP by intermediate node
   * \param toDst routing table entry to destination
   * \param toOrigin routing table entry to originator
   * \param gratRep indicates whether a gratuitous RREP should be unicast to destination
   */
  void SendReplyByIntermediateNode (RoutingTableEntry & toDst, RoutingTableEntry & toOrigin, bool gratRep);
  /// Send RREP_ACK
  void SendReplyAck (Ipv4Address neighbor);
  /// Initiate RERR
  void SendRerrWhenBreaksLinkToNextHop (Ipv4Address nextHop);
  /// Forward RERR
  void SendRerrMessage (Ptr<Packet> packet,  std::vector<Ipv4Address> precursors);
  /**
   * Send RERR message when no route to forward input packet. Unicast if there is reverse route to originating node, broadcast otherwise.
   * \param dst - destination node IP address
   * \param dstSeqNo - destination node sequence number
   * \param origin - originating node IP address
   */
  void SendRerrWhenNoRouteToForward (Ipv4Address dst, uint32_t dstSeqNo, Ipv4Address origin);
  //\}

  /// Hello timer
  Timer m_htimer;
  /// Schedule next send of hello message
  void ShortTimerExpire ();
  void SetShortInterval (Time short_t){ShortInterval = short_t;}
  Time GetShortInterval () const {return ShortInterval;}
  /// Long timer
  Timer m_ltimer;
  /// Schedule next execution of BCN: Association+BCN-to-BN conversion, BN:BN-to-BCN conversion.
  void LongTimerExpire ();
  void SetLongInterval (Time long_t){LongInterval = long_t;}
  Time GetLongInterval () const {return LongInterval;}

  /// RREQ rate limit timer
  Timer m_rreqRateLimitTimer;
  /// Reset RREQ count and schedule RREQ rate limit timer with delay 1 sec.
  void RreqRateLimitTimerExpire ();
  /// RERR rate limit timer
  Timer m_rerrRateLimitTimer;
  /// Reset RERR count and schedule RERR rate limit timer with delay 1 sec.
  void RerrRateLimitTimerExpire ();
  /// Map IP address + RREQ timer.
  std::map<Ipv4Address, Timer> m_addressReqTimer;
  /// Handle route discovery process
  void RouteRequestTimerExpire (Ipv4Address dst);
  /// Mark link to neighbor node as unidirectional for blacklistTimeout
  void AckTimerExpire (Ipv4Address neighbor,  Time blacklistTimeout);
 Ptr<UniformRandomVariable> m_uniformRandomVariable; 

  /// Traced Callback: transmitted packets.
  TracedCallback<Ptr<const Packet> > m_txTrace; // trace
  /// Traced Callback: received packets.
  TracedCallback<Ptr<const Packet> > m_rxTrace; // trace 
};

}
}
#endif /* __AODV_ROUTING_PROTOCOL_H__ */
