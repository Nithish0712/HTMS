#ifndef __AODV_NEIGHBOR_H__
#define __AODV_NEIGHBOR_H__

#include "ns3/simulator.h"
#include "ns3/timer.h"
#include "ns3/ipv4-address.h"
#include "ns3/callback.h"
#include "ns3/wifi-mac-header.h"
#include "ns3/arp-cache.h"
#include "aodv-packet.h"
#include <vector>
#include <list>

namespace ns3
{
namespace aodvmesh
{

class RoutingProtocol;

  /// Neighbor description
  struct NeighborTuple
  {
	/// Interface address of the neighbor node.
	Ipv4Address neighborIfaceAddr;
    ///Ipv4Address m_neighborAddress;
    Mac48Address m_hardwareAddress;
    /// Time in which the tuple will expire
    Time m_expireTime;
    /// Link towards such neighbor is closed
    bool close;

    /// number of hello messages received in the last period
    uint16_t helloCounter;
    /// last hello sequence number
    uint16_t sequenceNumber;
    /// Latest weight advertised by the neighbor node.
    uint32_t neighborWeight;
    /// Latest node status advertised by the neighbor node.
    NodeStatus neighborNodeStatus;
    /// Latest associated BN advertised by the neighbor node.
    Ipv4Address neighborAssociatedCORE;
    /// Latest BN-to-BCN indicator advertised by the neighbor node.
    core_noncore_Indicator neighborcore_noncoreIndicator;
    /// Client Set is represented by this flag.
    bool neighborClient;
    /// List of two hop BN neighbors
    std::list<MulticastBnNeighborTuple> neighborBnNeighbors;

    NeighborTuple (Ipv4Address ip, Mac48Address mac, Time expire) :
		neighborIfaceAddr (ip), m_hardwareAddress (mac), m_expireTime (expire),
		close (false), helloCounter(0), sequenceNumber(-1), neighborWeight(0), neighborNodeStatus(CORE),
		neighborcore_noncoreIndicator(CONVERT_OTHER),neighborClient(false)
    { }

    NeighborTuple (Ipv4Address ip, Time expire) :
		neighborIfaceAddr (ip), m_expireTime (expire),
		close (false), helloCounter(0), sequenceNumber(-1), neighborWeight(0), neighborNodeStatus(CORE),
		neighborcore_noncoreIndicator(CONVERT_OTHER),neighborClient(false)
	{ }
  };

  /// Neighbor Set type.
  typedef std::list<NeighborTuple> NeighborSet;

  static inline bool
  operator == (const NeighborTuple &a, const NeighborTuple &b)
  {
    return (a.neighborIfaceAddr == b.neighborIfaceAddr
            && a.neighborNodeStatus == b.neighborNodeStatus
            && a.neighborcore_noncoreIndicator == b.neighborcore_noncoreIndicator);
  }

  static inline bool
  operator > (const NeighborTuple &a, const NeighborTuple &b)
  {
  	if(a.neighborWeight > b.neighborWeight)
  		return true;
  	else if(a.neighborWeight == b.neighborWeight && a.neighborcore_noncoreIndicator == CONVERT_BREAK
  			&& b.neighborcore_noncoreIndicator != CONVERT_BREAK )
  		return true;
  	else if(a.neighborWeight == b.neighborWeight &&
  			a.neighborcore_noncoreIndicator == b.neighborcore_noncoreIndicator &&
  			a.neighborIfaceAddr.Get()>b.neighborIfaceAddr.Get())
  		return true;
  	else return false;
  }

  static inline bool
  operator < (const NeighborTuple &a, const NeighborTuple &b)
  {
    if(a.neighborWeight < b.neighborWeight)
			return true;
	else if(a.neighborWeight == b.neighborWeight && a.neighborcore_noncoreIndicator == CONVERT_BREAK
			&& b.neighborcore_noncoreIndicator != CONVERT_BREAK )
		return false;
	else if(a.neighborWeight == b.neighborWeight &&
			a.neighborcore_noncoreIndicator == b.neighborcore_noncoreIndicator &&
			a.neighborIfaceAddr.Get() > b.neighborIfaceAddr.Get())
		return false;
	else return true;
  }

  static inline std::ostream&
  operator << (std::ostream &os, const NeighborTuple &tuple)
  {
	os << "Neighbor(Addr=" << tuple.neighborIfaceAddr
//		 << ", m_hardwareAddress=" << tuple.m_hardwareAddress
		 << ", close="<< tuple.close
		 << ", expire="<<tuple.m_expireTime.GetSeconds()
		 << ", Weight="<<tuple.neighborWeight
		 << ", Status="<<(tuple.neighborNodeStatus == RN_NODE? "RN" : (tuple.neighborNodeStatus == NEIGH_NODE? "BN" : "BCN"))
		 << ", AssBN="<< tuple.neighborAssociatedCORE
		 << ", core_noncoreInd="<<(tuple.neighborcore_noncoreIndicator == CONVERT_BREAK?"BREAK":(tuple.neighborcore_noncoreIndicator==CONVERT_ALLOW?"ALLOW":"OTHER"))
		 << ")";
//    os << "NeighborTuple(neighborIfaceAddr=" << tuple.neighborIfaceAddr
//       << ", m_hardwareAddress=" << tuple.m_hardwareAddress
//       << ", close="<< tuple.close
//       << ", m_expireTime="<<tuple.m_expireTime
//       << ", neighborWeight="<<tuple.neighborWeight
//       << ", neighborStatus="<<(tuple.neighborNodeStatus == RN_NODE? "RN" : (tuple.neighborNodeStatus == NEIGH_NODE? "BN" : "BCN"))
//       << ", neighborAssociatedCORE="<< tuple.neighborAssociatedCORE
//       << ", neighborcore_noncoreIndicator="<<tuple.neighborcore_noncoreIndicator
//       << ")";
    return os;
  }

  /// A neighbor pair  ///TODO std::pair

  struct NeighborPair
  {
    /// First neighbor address.
    Ipv4Address neighborFirstIfaceAddr;
    /// Second neighbor address.
    Ipv4Address neighborSecondIfaceAddr;
  };

  static inline std::ostream&
  operator << (std::ostream &os, const NeighborPair &tuple)
  {
    os << "NeighborPair(neighborMainIfaceAddr=" << tuple.neighborFirstIfaceAddr
       << ", neighborOneIfaceAddr=" << tuple.neighborSecondIfaceAddr
       << ")";
    return os;
  }


  static inline bool
  operator < (const NeighborPair &a, const NeighborPair &b)
  {
  	return (a.neighborFirstIfaceAddr.Get() < b.neighborFirstIfaceAddr.Get());
  }

  static inline bool
  operator > (const NeighborPair &a, const NeighborPair &b)
  {
  	return (a.neighborFirstIfaceAddr.Get() > b.neighborFirstIfaceAddr.Get());
  }

  static inline bool
   operator == (const NeighborPair &a, const NeighborPair &b)
   {
   	return (a.neighborFirstIfaceAddr.Get() == b.neighborFirstIfaceAddr.Get() &&
   			a.neighborSecondIfaceAddr.Get() == b.neighborSecondIfaceAddr.Get());
   }

  /// A neighbor pair
  struct NeighborTriple
  {
    /// First neighbor address.
    Ipv4Address neighborCommonIfaceAddr;
    /// Second neighbor address.
    Ipv4Address neighborOneIfaceAddr;
    /// Second neighbor address.
    Ipv4Address neighborTwoIfaceAddr;
  };

  static inline std::ostream&
  operator << (std::ostream &os, const NeighborTriple &tuple)
  {
    os << "NeighborTriple(neighborMainIfaceAddr=" << tuple.neighborCommonIfaceAddr
       << ", neighborOneIfaceAddr=" << tuple.neighborOneIfaceAddr
       << ", neighborTwoIfaceAddr=" << tuple.neighborTwoIfaceAddr
       << ")";
    return os;
  }

  static inline bool
  operator < (const NeighborTriple &a, const NeighborTriple &b)
  {
  	return (a.neighborCommonIfaceAddr.Get() < b.neighborCommonIfaceAddr.Get());
  }

  static inline bool
  operator > (const NeighborTriple &a, const NeighborTriple &b)
  {
  	return (a.neighborCommonIfaceAddr.Get() > b.neighborCommonIfaceAddr.Get());
  }


  typedef std::list<NeighborPair>	Groups;
  typedef std::list<NeighborTriple> NeighborTriples;
  typedef std::list<Ipv4Address> AddressSet;
/**
 * \ingroup aodv
 * \brief maintain list of active neighbors
 */
class Neighbors
{
public:
	/// Constructor
	Neighbors (Time delay);
	/// Return expire time for neighbor node with address addr, if exists, else return 0.
	Time GetExpireTime (Ipv4Address addr);
	/// Check that node with address addr  is neighbor
	bool IsNeighbor (Ipv4Address addr);
	/// Update expire time for entry with address addr, if it exists, else add new entry
	void Update (Ipv4Address addr, Time expire);
	/// Remove all expired entries
	void Purge ();
	/// Remove all the entries with a few hello messages
	void PurgeHello ();
	/// Schedule m_ntimer.
	void ScheduleTimer ();
	/// Remove all entries
	void Clear () { localNeighborList.clear (); }
	/// Add ARP cache to be used to allow layer 2 notifications processing
	void AddArpCache (Ptr<ArpCache>);
	/// Don't use given ARP cache any more (interface is down)
	void DelArpCache (Ptr<ArpCache>);

	Callback<void, WifiMacHeader const &> GetTxErrorCallback () const { return m_txErrorCallback; }

	NeighborTuple* FindNeighborTuple (const Ipv4Address &mainAddr);
	NeighborTuple* FindNeighborTuple (const Ipv4Address &mainAddr, NodeStatus nodeStatus);
	void EraseNeighborTuple (const NeighborTuple &neighborTuple);
	void EraseNeighborTuple (const Ipv4Address &mainAddr);
	void InsertNeighborTuple (const NeighborTuple &tuple);
	void PrintLocalNeighborList();
	void PrintMulticastNeighborList();
	void PrintAddressSet(AddressSet set);

	MulticastBnNeighborTuple* FindMulticastBnNeighborTuple (const Ipv4Address &neighbor, const Ipv4Address &twoHopNeighbor);
	MulticastBnNeighborTuple* FindMulticastBnNeighborTuple (const Ipv4Address &twoHopNeighbor);
	void EraseMulticastBnNeighborTuple (const Ipv4Address &neighbor, const Ipv4Address &twoHopNeighbor);
	void EraseAllMulticastNeighborTuples(const Ipv4Address &twoHopNeighborAddress);
	void InsertMulticastNeighborTuple(const Ipv4Address oneHopNeighbor, const MulticastBnNeighborTuple &twoHopNeighbor);
	void UpdateNeighborTuple(HelloHeader *helloHeader, bool client);
	void UpdateMulticastNeighborTuple(HelloHeader *helloHeader, Time newtTime);
	void AddHelloCounter(NeighborTuple *nt) {nt->helloCounter++;}
	void ResetHelloCounter(NeighborTuple *nt) {nt->helloCounter=0;}
	void ResetHelloCounter();
	void SetMinHello (uint32_t minHello) { m_minHello = minHello;}
	uint32_t GetMinHello () { return m_minHello;}

	MulticastBnNeighborSet GetBnNeighbors();

	uint32_t GetNeighborhoodSize ();

	uint32_t GetNeighborhoodSize (NodeStatus nodeStatus);
	uint32_t GetBNNeighborhoodSize ();
	uint32_t GetBCNNeighborhoodSize ();
	uint32_t GetRNNeighborhoodSize ();

	
	bool Are1HopNeighbors(const Ipv4Address &BNnode_v, const Ipv4Address &BNnode_w);
	bool Are2HopNeighbors(const Ipv4Address &BNnode_v, const Ipv4Address &BNnode_w);
	
	bool AreNeighbors(NeighborPair hop_bn, Ipv4Address neighbor);
	
	NeighborTuple* GetBestNeighbor(NodeStatus nodeStatus);

	NeighborSet GetOneHopNeighbors(NodeStatus nodeStatus);

	Groups GetOneHopPairs(NodeStatus set1, NodeStatus set2);
	AddressSet Intersection(AddressSet set1, AddressSet set2);
	AddressSet Intersection(AddressSet set, Ipv4Address address);
	AddressSet GetMulticastNeighbors(Ipv4Address neighbor);
	NeighborSet GetClients(NodeStatus nodeStatus);
	NeighborSet GetClients();
		AddressSet GetCommonBN(NeighborPair gp);

	bool HigherWeight(Ipv4Address neighbor);

	bool HigherWeight(NeighborTuple *node);
	
	NeighborSet &GetNeighborList(){return localNeighborList;}

	bool BCN2BNRule1();
	bool BCN2BNRule2();

  ///\name Handle link failure callback
  //\{
  void SetCallback (Callback<void, Ipv4Address> cb) { m_handleLinkFailure = cb; }
  Callback<void, Ipv4Address> GetCallback () const { return m_handleLinkFailure; }
  //\}
  static bool compare0IP (const Ipv4Address &a, const Ipv4Address &b)
  {
  return a.Get() < b.Get();
  }
  static bool compare1IP (const NeighborTuple &a, const NeighborTuple &b)
  {
  return a.neighborIfaceAddr.Get() < b.neighborIfaceAddr.Get();
  }
  static bool compare2IP (const MulticastBnNeighborTuple &a, const MulticastBnNeighborTuple &b)
  {
  return a.twoHopBnNeighborIfaceAddr.Get() < b.twoHopBnNeighborIfaceAddr.Get();
  }
  /// BN Neighbor
  std::list<NeighborTuple> localNeighborList;
private:
  /// link failure callback
  Callback<void, Ipv4Address> m_handleLinkFailure;
  /// TX error callback
  Callback<void, WifiMacHeader const &> m_txErrorCallback;
  /// Timer for neighbor's list. Schedule Purge().
  Timer m_ntimer;
  int32_t m_minHello;
  Time m_shortTimer;
  Time m_longTimer;
  /// list of ARP cached to be used for layer 2 notifications processing
  std::vector<Ptr<ArpCache> > m_arp;

  /// Find MAC address by IP using list of ARP caches
  Mac48Address LookupMacAddress (Ipv4Address);
  /// Process layer 2 TX error notification
  void ProcessTxError (WifiMacHeader const &);
};

}
}
#endif /* __AODV_NEIGHBOR_H__ */
