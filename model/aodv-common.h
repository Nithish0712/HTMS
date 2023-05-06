/// \brief	This header file declares the Common data structures.

#ifndef __AODV_COMMON_H__
#define __AODV_COMMON_H__

#include <list>
#include "ns3/ipv4-address.h"
#include "ns3/timer.h"
#include <math.h>

namespace ns3
{
namespace aodvmesh
{

#define LONG_SHORT_RATIO 3
#define SHORT_INTERVAL 2 //seconds
#define LONG_INTERVAL (LONG_SHORT_RATIO*SHORT_INTERVAL) //seconds
#define MIN_HELLO rint(LONG_SHORT_RATIO*2/3) //TODO change according to long ratio

enum NodeStatus {
		RN_NODE = 1,  
		CORE = 2, 
		NEIGH_NODE = 3   
};

enum core_noncore_Indicator {
	CONVERT_BREAK = 0, 
	CONVERT_ALLOW = 1, 
	CONVERT_OTHER = 2  
};

enum WeightFunction {
	W_NODE_DEGREE = 1,	
	W_NODE_IP = 2,		
	W_NODE_RND = 3,
	W_NODE_BNDEGREE = 4	
};


struct MulticastBnNeighborTuple
{
    MulticastBnNeighborTuple(Ipv4Address addr, uint32_t weight, core_noncore_Indicator ind,  Time time):
		twoHopBnNeighborIfaceAddr (addr), twoHopBnNeighborWeight (weight), twoHopBnNeighborIndicator (ind), twoHopBnNeighborTimeout (time)
    {}

    MulticastBnNeighborTuple(Ipv4Address addr, uint32_t weight, core_noncore_Indicator ind):
    		twoHopBnNeighborIfaceAddr (addr), twoHopBnNeighborWeight (weight), twoHopBnNeighborIndicator (ind)
    {twoHopBnNeighborTimeout = Seconds(Simulator::Now().ToInteger(Time::S));}

	MulticastBnNeighborTuple(){}

  Ipv4Address twoHopBnNeighborIfaceAddr;
  uint32_t twoHopBnNeighborWeight;
  core_noncore_Indicator twoHopBnNeighborIndicator;
  Time twoHopBnNeighborTimeout;
};

typedef std::list<MulticastBnNeighborTuple> MulticastBnNeighborSet;

static inline bool
  operator > (const MulticastBnNeighborTuple &a, const MulticastBnNeighborTuple &b)
  {
  	return (a.twoHopBnNeighborIfaceAddr.Get() > b.twoHopBnNeighborIfaceAddr.Get());
  }

  static inline bool
  operator < (const MulticastBnNeighborTuple &a, const MulticastBnNeighborTuple &b)
  {
  	return (a.twoHopBnNeighborIfaceAddr.Get() < b.twoHopBnNeighborIfaceAddr.Get());
  }

  static inline bool
  operator == (const MulticastBnNeighborTuple &a, const MulticastBnNeighborTuple &b)
  {
    return (a.twoHopBnNeighborIfaceAddr == b.twoHopBnNeighborIfaceAddr);
  }

  static inline std::ostream&
  operator << (std::ostream &os, const MulticastBnNeighborTuple &tuple)
  {
//	os << "MulticastBnNeighborTuple(twoHopBnNeighborIfaceAddr="
//		<< tuple.twoHopBnNeighborIfaceAddr << ", twoHopBnNeighborIndicator="
//		<< tuple.twoHopBnNeighborIndicator << ", twoHopBnNeighborWeight="
//		<< tuple.twoHopBnNeighborWeight << ")";
    os << "[ IP: " << tuple.twoHopBnNeighborIfaceAddr
       << ", Ind " << (tuple.twoHopBnNeighborIndicator == CONVERT_BREAK?"BREAK":(tuple.twoHopBnNeighborIndicator==CONVERT_ALLOW?"ALLOW":"OTHER"))
       << ", W " << tuple.twoHopBnNeighborWeight
       << ", T " << tuple.twoHopBnNeighborTimeout.GetSeconds()
       << "]; ";
    return os;
  }
}
}
#endif
