#include "aodv-neighbor.h"
#include "ns3/log.h"
#include <algorithm>

NS_LOG_COMPONENT_DEFINE ("AodvNeighbors");

namespace ns3
{
namespace aodvmesh
{
Neighbors::Neighbors (Time delay) : 
  m_ntimer (Timer::CANCEL_ON_DESTROY)
{
  m_ntimer.SetDelay (delay);
  m_ntimer.SetFunction (&Neighbors::Purge, this);
  m_txErrorCallback = MakeCallback (&Neighbors::ProcessTxError, this);
}

bool
Neighbors::IsNeighbor (Ipv4Address addr)
{
  Purge ();
  for (NeighborSet::const_iterator i = localNeighborList.begin (); i != localNeighborList.end (); ++i)
    {
      if (i->neighborIfaceAddr == addr)
        return true;
    }
  return false;
}


NeighborTuple*
Neighbors::FindNeighborTuple(const Ipv4Address &neighborAddress) {
	NS_LOG_FUNCTION(this);
	for (NeighborSet::iterator nt = localNeighborList.begin(); nt != localNeighborList.end(); nt++){
		if (nt->neighborIfaceAddr == neighborAddress )
			return &(*nt);
	}
	return NULL;
}

Time
Neighbors::GetExpireTime (Ipv4Address addr)
{
  NS_LOG_FUNCTION(this);
  Purge ();
  for (NeighborSet::const_iterator i = localNeighborList.begin (); i
      != localNeighborList.end (); ++i)
    {
      if (i->neighborIfaceAddr == addr)
        return (i->m_expireTime - Simulator::Now ());
    }
  return Seconds (0);
}

void
Neighbors::Update (Ipv4Address addr, Time expire)
{
  NS_LOG_FUNCTION(this);
  for (NeighborSet::iterator i = localNeighborList.begin (); i != localNeighborList.end (); ++i)
    if (i->neighborIfaceAddr == addr)
      {
        i->m_expireTime = std::max (expire + Simulator::Now (), i->m_expireTime);
        if (i->m_hardwareAddress == Mac48Address ())
          i->m_hardwareAddress = LookupMacAddress (i->neighborIfaceAddr);
        return;
      }
  NS_LOG_INFO ("Open link to " << addr);
  NeighborTuple neighbor (addr, LookupMacAddress (addr), expire + Simulator::Now ());
  localNeighborList.push_back (neighbor);
  Purge ();
}


void Neighbors::UpdateNeighborTuple(HelloHeader *helloHeader, bool helloClient){
	NS_LOG_FUNCTION(this);
	NeighborTuple *tp = FindNeighborTuple(helloHeader->GetOriginatorAddress());
	if (!tp) {// Unknown neighbor -> create entry
		NeighborTuple inew = NeighborTuple(helloHeader->GetOriginatorAddress(), Simulator::Now()); //create a NeighborTuple
		inew.neighborIfaceAddr = helloHeader->GetOriginatorAddress();		// set the IP address
		inew.sequenceNumber = helloHeader->GetMessageSequenceNumber();		// get neighbor hello sequence number from the hello packet.
		InsertNeighborTuple(inew);											// add the tuple in the list
		tp = FindNeighborTuple(helloHeader->GetOriginatorAddress()); 		// retrieve a pointer to the neighbor tuple
	}
	AddHelloCounter(tp);
	tp->sequenceNumber = helloHeader->GetMessageSequenceNumber();
	tp->neighborAssociatedCORE = helloHeader->GetAssociatedBnAddress();
	tp->neighborWeight = helloHeader->GetWeightValue();
	tp->neighborNodeStatus = helloHeader->GetNodeStatus();
	tp->neighborcore_noncoreIndicator = helloHeader->Getcore_noncoreIndicator();
	tp->neighborClient = helloClient;
}

void
Neighbors::UpdateMulticastNeighborTuple(HelloHeader *helloMessage, Time nextTime){
	NS_LOG_FUNCTION(this);
	NeighborTuple *neighbor = FindNeighborTuple(helloMessage->GetOriginatorAddress());
	MulticastBnNeighborSet updates = helloMessage->GetMulticastNeighborSet();
	neighbor->neighborBnNeighbors.clear();
	nextTime += Simulator::Now();
	for(MulticastBnNeighborSet::const_iterator iter = updates.begin(); iter != updates.end();iter++){
		MulticastBnNeighborTuple twohop = *iter;
		twohop.twoHopBnNeighborTimeout = nextTime;
		neighbor->neighborBnNeighbors.push_back(twohop);
	}
	neighbor->neighborBnNeighbors.sort(compare2IP);
}

void
Neighbors::EraseNeighborTuple(const Ipv4Address &neighborAddress) {
	NS_LOG_FUNCTION(this);
	uint16_t size = localNeighborList.size();
	for (NeighborSet::iterator nt = localNeighborList.begin(); nt != localNeighborList.end(); nt++){ //&& nt->neighborIfaceAddr.Get() <= neighborAddress.Get(); nt++) {
		if (nt->neighborIfaceAddr == neighborAddress){
			nt = localNeighborList.erase (nt);
			NS_ASSERT (size == 1 + localNeighborList.size());
			break;
		}
	}
}

void
Neighbors::EraseNeighborTuple(const NeighborTuple &neighborTuple) {
	NS_LOG_FUNCTION(this);
	EraseNeighborTuple(neighborTuple.neighborIfaceAddr);
}

void
Neighbors::InsertNeighborTuple(const NeighborTuple &neighborTuple) {
	NS_LOG_FUNCTION(this);
	EraseNeighborTuple(neighborTuple);// remove old entry
	localNeighborList.push_back(neighborTuple);// add new entry
	localNeighborList.sort(compare1IP);//and sort the list
}

void
Neighbors::PrintLocalNeighborList(){
	NS_LOG_FUNCTION(this);
	for (NeighborSet::iterator nt = localNeighborList.begin(); nt!= localNeighborList.end(); nt++) {
		NS_LOG_INFO(*nt);
		MulticastBnNeighborSet nset = nt->neighborBnNeighbors;
		for (MulticastBnNeighborSet::iterator twohop = nset.begin() ; twohop != nset.end(); twohop++) {
			NS_LOG_INFO("\t"<<*twohop);
		}
	}
}

MulticastBnNeighborTuple*
Neighbors::FindMulticastBnNeighborTuple(const Ipv4Address &oneHopNeighbor,
		const Ipv4Address &twoHopNeighbor) {
	NS_LOG_FUNCTION(this);
	NeighborTuple *neighbor = FindNeighborTuple(oneHopNeighbor);
	if(neighbor == NULL)
		return NULL;
	for (MulticastBnNeighborSet::iterator nt = neighbor->neighborBnNeighbors.begin();
			nt != neighbor->neighborBnNeighbors.end(); nt++){// && nt->twoHopBnNeighborIfaceAddr.Get() <= twoHopNeighbor.Get(); nt++) {
		if (nt->twoHopBnNeighborIfaceAddr.Get() == twoHopNeighbor.Get()){
			return &(*nt);
		}
	}
	return NULL;
}

void
Neighbors::EraseMulticastBnNeighborTuple (const Ipv4Address &oneHopNeighbor, const Ipv4Address &twoHopNeighbor){
	NS_LOG_FUNCTION(this);
	NeighborTuple * neighbor = FindNeighborTuple(oneHopNeighbor);
	NS_ASSERT (neighbor != NULL);
	size_t size = neighbor->neighborBnNeighbors.size();
	for (MulticastBnNeighborSet::iterator nt = neighbor->neighborBnNeighbors.begin(); nt != neighbor->neighborBnNeighbors.end(); nt++) {
		if (nt->twoHopBnNeighborIfaceAddr == twoHopNeighbor ){
			nt = neighbor->neighborBnNeighbors.erase (nt);
			NS_ASSERT (size == 1+ neighbor->neighborBnNeighbors.size());
		}
	}
}

MulticastBnNeighborTuple*
Neighbors::FindMulticastBnNeighborTuple(const Ipv4Address &twoHopNeighbor) {
	NS_LOG_FUNCTION(this);
	for (NeighborSet::iterator nt = localNeighborList.begin(); nt != localNeighborList.end(); nt++){ // && nt->neighborIfaceAddr.Get() <= neighborAddress.Get(); nt++) {
		for (MulticastBnNeighborSet::iterator nt2 = nt->neighborBnNeighbors.begin(); nt2 != nt->neighborBnNeighbors.end(); nt2++){// && nt->twoHopBnNeighborIfaceAddr.Get() <= twoHopNeighbor.Get(); nt++) {
			if (nt2->twoHopBnNeighborIfaceAddr == twoHopNeighbor){
				return &(*nt2);
			}
		}
	}
	return NULL;
}

MulticastBnNeighborSet
Neighbors::GetBnNeighbors(){
	NS_LOG_FUNCTION(this);
	MulticastBnNeighborSet tset;
	for(NeighborSet::iterator ns = localNeighborList.begin();  ns != localNeighborList.end(); ns++){
		if(ns->neighborNodeStatus != NEIGH_NODE) continue;
		MulticastBnNeighborTuple tp (ns->neighborIfaceAddr, ns->neighborWeight, ns->neighborcore_noncoreIndicator, ns->m_expireTime);
		tset.push_back(tp);
	}
	tset.sort(compare2IP);
	return tset;
}

uint32_t
Neighbors::GetNeighborhoodSize(NodeStatus nodeStatus) {
	NS_LOG_FUNCTION(this);
	int size = 0;
	for (NeighborSet::iterator nt = localNeighborList.begin(); nt != localNeighborList.end(); nt++) {
		if (nt->neighborNodeStatus == nodeStatus)
			size++;
	}
	return size;
}

uint32_t
Neighbors::GetNeighborhoodSize() {
	NS_LOG_FUNCTION(this);
	return localNeighborList.size();
}


NeighborTuple*
Neighbors::GetBestNeighbor(aodvmesh::NodeStatus nodeStatus) {
	NS_LOG_FUNCTION(this);
	NeighborTuple *best = NULL;
	NeighborSet::iterator ntend = localNeighborList.end();
	for (NeighborSet::iterator nt = localNeighborList.begin(); nt != ntend; nt++) {
		if (nt->neighborNodeStatus == nodeStatus && (best == NULL ||//initialize best
				*nt > *best )) {//compare with best
			best = &(*nt);
		}
	}
	std::stringstream ss;
	ss << (nodeStatus==NEIGH_NODE?"BN":(nodeStatus==CORE?"BCN":"RN")) << " set: Best neighbor = ";
	if (best==NULL)
		ss<< "NULL";
	else
		ss << *best;
	NS_LOG_INFO(ss.str());
	return best;
}

NeighborSet
Neighbors::GetOneHopNeighbors(aodvmesh::NodeStatus nodeStatus) {
	NS_LOG_FUNCTION(this);
	NeighborSet ns;
	for (NeighborSet::iterator nt = localNeighborList.begin(); nt != localNeighborList.end(); nt++) {
		if (nt->neighborNodeStatus == nodeStatus) {
			ns.push_back(*nt);
		}
	}
	return ns;
}

NeighborSet
Neighbors::GetClients(aodvmesh::NodeStatus nodeStatus) {
	NS_LOG_FUNCTION(this);
	NeighborSet ns;
	for (NeighborSet::iterator nt = localNeighborList.begin(); nt != localNeighborList.end(); nt++) {
		if (nt->neighborClient && nt->neighborNodeStatus == nodeStatus) {
			ns.push_back(*nt);
		}
	}
	return ns;
}

NeighborSet
Neighbors::GetClients() {
	NS_LOG_FUNCTION(this);
	NeighborSet ns;
	for (NeighborSet::iterator nt = localNeighborList.begin(); nt != localNeighborList.end(); nt++) {
		if (nt->neighborClient) {
			ns.push_back(*nt);
		}
	}
	return ns;
}

bool
Neighbors::Are1HopNeighbors(const Ipv4Address &BNnode_v, const Ipv4Address &BNnode_w) {
	NS_LOG_FUNCTION(this);
	bool directly_connected = false;
	AddressSet bnv = GetMulticastNeighbors(BNnode_v);
	AddressSet bnw = GetMulticastNeighbors(BNnode_w);
	directly_connected = !(Intersection(bnv, BNnode_w).empty() && Intersection(bnw, BNnode_v).empty());
//	NeighborTuple *node_v = FindNeighborTuple(BNnode_v);
//	if(node_v == NULL) return false; // don't influence the result
//	for (MulticastBnNeighborSet::iterator two_hops = node_v->neighborBnNeighbors.begin();
//			two_hops != node_v->neighborBnNeighbors.end() && !directly_connected; two_hops++) {
//		directly_connected = (two_hops -> twoHopBnNeighborIfaceAddr == BNnode_w);
//	}
	return directly_connected;
}

AddressSet
Neighbors::GetMulticastNeighbors(Ipv4Address neighborAddr) {
	NS_LOG_FUNCTION(this);
	AddressSet two_hop_bn;
	NeighborTuple *neighbor = FindNeighborTuple(neighborAddr);
	NS_ASSERT (neighbor != NULL);
	for (MulticastBnNeighborSet::iterator nt = neighbor->neighborBnNeighbors.begin();
			nt != neighbor->neighborBnNeighbors.end(); nt++)
		two_hop_bn.push_back(nt->twoHopBnNeighborIfaceAddr);
	two_hop_bn.sort(compare0IP);
	return two_hop_bn;
}

AddressSet
Neighbors::Intersection(AddressSet set1, AddressSet set2){
	NS_LOG_FUNCTION(this);
	AddressSet result;
//	PrintAddressSet(set1);
//	PrintAddressSet(set2);
//	std::stringstream ss;
	for (AddressSet::iterator one = set1.begin(); one != set1.end(); one++) {
		for (AddressSet::iterator two = set2.begin(); two!= set2.end(); two++) {
			if(one->Get() == two->Get()){
				result.push_back(*one);
//				ss<<*one<<", ";
			}
		}
	}
//	NS_LOG_DEBUG("Intersection "<<ss.str());
	return result;
}

AddressSet
Neighbors::Intersection(AddressSet set1, Ipv4Address address){
	NS_LOG_FUNCTION (this);
	AddressSet result;
//	std::stringstream ss;
	for (AddressSet::iterator one = set1.begin(); one != set1.end(); one++) {
			if(one->Get() == address.Get()){
				result.push_back(*one);
//				ss<<address<<", ";
			}
	}
//	NS_LOG_DEBUG("Intersection "<<ss.str());
	return result;
}

bool
Neighbors::Are2HopNeighbors(const Ipv4Address &BNnode_v, const Ipv4Address &BNnode_w) {
	NS_LOG_FUNCTION(this);
	AddressSet vset = GetMulticastNeighbors(BNnode_v);
	AddressSet wset = GetMulticastNeighbors(BNnode_w);
	return (!Intersection(vset,wset).empty());
}

Groups
Neighbors::GetOneHopPairs(NodeStatus set1, NodeStatus set2){
	NS_LOG_FUNCTION(this);
	NeighborSet nodeset1 = GetOneHopNeighbors(set1);
	NeighborSet nodeset2 = GetOneHopNeighbors(set2);
	Groups npset;
	for(NeighborSet::iterator bn1 = nodeset1.begin(); bn1 != nodeset1.end(); bn1++){
		for(NeighborSet::iterator bn2 = (set1==set2?bn1++:nodeset2.begin()); bn2 != nodeset2.end(); bn2++){
			if (bn1->neighborIfaceAddr == bn2->neighborIfaceAddr) continue;//skip same node
			NeighborPair np = {bn1->neighborIfaceAddr , bn2->neighborIfaceAddr};
			npset.push_back(np);
		}
	}
	return npset;
}

void
Neighbors::PrintAddressSet(AddressSet set){
	NS_LOG_FUNCTION(this);
	std::stringstream ss;
	for (AddressSet::iterator one = set.begin(); one != set.end(); one++) {
			ss<<*one<<",";
	}
	NS_LOG_DEBUG("AddressSet: "<< ss.str());
}

AddressSet
Neighbors::GetCommonBN(NeighborPair bn_pair) {
	NS_LOG_FUNCTION(this);
	AddressSet node_v = GetMulticastNeighbors(bn_pair.neighborFirstIfaceAddr);
	AddressSet node_w = GetMulticastNeighbors(bn_pair.neighborSecondIfaceAddr);
	AddressSet commonBN = Intersection(node_v,node_w);
	//COMMENT down
//	for(AddressSet::iterator inter = commonBN.begin(); inter != commonBN.end(); inter++){
//		NS_ASSERT (FindMulticastBnNeighborTuple(bn_pair.neighborFirstIfaceAddr, *inter) &&
//				FindMulticastBnNeighborTuple(bn_pair.neighborSecondIfaceAddr, *inter));
//	}
	return commonBN;
}

void
Neighbors::ResetHelloCounter (){
for (NeighborSet::iterator iter = localNeighborList.begin(); iter != localNeighborList.end(); iter++)
			ResetHelloCounter(&(*iter));
}

void
Neighbors::PurgeHello ()
{
  NS_LOG_FUNCTION(this << GetMinHello());
  if (localNeighborList.empty ())
    return;

  NeighborSet newset;
  if (!m_handleLinkFailure.IsNull ())
    {
      for (NeighborSet::iterator j = localNeighborList.begin (); j != localNeighborList.end (); ++j)
        {
          if (j->helloCounter < GetMinHello())
            {
              NS_LOG_INFO ("Removing "<< *j<< ":" << j->helloCounter<< "/"<< GetMinHello());
              m_handleLinkFailure (j->neighborIfaceAddr);
            }
          else
        	  newset.push_back(*j);
        }
    }
  localNeighborList = newset;
}

struct CloseOneHopNeighbor
{
  bool operator() (const NeighborTuple & nb) const
  {
    return ((nb.m_expireTime < Simulator::Now ()) || nb.close);
  }
};


struct CloseMulticastNeighbor
{
  bool operator() (const MulticastBnNeighborTuple & nb) const
  {
    return (nb.twoHopBnNeighborTimeout < Simulator::Now ());
  }
};

void
Neighbors::Purge ()
{
  NS_LOG_FUNCTION(this);
  if (localNeighborList.empty ())
    return;
  NeighborSet newset;
  CloseOneHopNeighbor pred;
  CloseMulticastNeighbor pred2;
  if (!m_handleLinkFailure.IsNull ())
    {
      for (NeighborSet::iterator j = localNeighborList.begin (); j != localNeighborList.end (); ++j)
        {
          if (pred (*j))
            {
              NS_LOG_INFO ("Link expired towards: " << *j);
              m_handleLinkFailure (j->neighborIfaceAddr);
            }
          else {
        	  newset.push_back(*j);
        	  if(!(j->neighborBnNeighbors.empty())){
        		  j->neighborBnNeighbors.erase (std::remove_if (j->neighborBnNeighbors.begin (), j->neighborBnNeighbors.end (), pred2), j->neighborBnNeighbors.end ());
        	  }
          }
        }
    }
  localNeighborList.clear();
  localNeighborList = newset;
  ScheduleTimer();
}

void
Neighbors::ScheduleTimer ()
{
  m_ntimer.Cancel ();
  m_ntimer.Schedule ();
}

void
Neighbors::AddArpCache (Ptr<ArpCache> a)
{
  m_arp.push_back (a);
}

void
Neighbors::DelArpCache (Ptr<ArpCache> a)
{
  m_arp.erase (std::remove (m_arp.begin (), m_arp.end (), a), m_arp.end ());
}

Mac48Address
Neighbors::LookupMacAddress (Ipv4Address addr)
{
  Mac48Address hwaddr;
  for (std::vector<Ptr<ArpCache> >::const_iterator i = m_arp.begin ();
       i != m_arp.end (); ++i)
    {
      ArpCache::Entry * entry = (*i)->Lookup (addr);
      if (entry != 0 && entry->IsAlive () && !entry->IsExpired ())
        {
          hwaddr = Mac48Address::ConvertFrom (entry->GetMacAddress ());
          break;
        }
    }
  return hwaddr;
}

void
Neighbors::ProcessTxError (WifiMacHeader const & hdr)
{
	NS_LOG_FUNCTION(this);
  Mac48Address addr = hdr.GetAddr1 ();

  for (NeighborSet::iterator i = localNeighborList.begin (); i != localNeighborList.end (); ++i)
    {
      if (i->m_hardwareAddress == addr)
        i->close = true;
    }
  Purge ();
}
}
}

