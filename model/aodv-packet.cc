#include "aodv-packet.h"
#include "ns3/log.h"
#include "ns3/address-utils.h"
#include "ns3/assert.h"
#include "string.h"

#define IPV4_ADDRESS_SIZE 4
#define AODVMESH_HEADER_SIZE 1
#define AODVMESH_HELLO_HEADER_SIZE 23
#define AODVMESH_HELLO_BNNEIGHBOR_ENTRY 24

namespace ns3 {
namespace aodvmesh {

NS_LOG_COMPONENT_DEFINE ("AodvPacket");

NS_OBJECT_ENSURE_REGISTERED (TypeHeader);

TypeHeader::TypeHeader (MessageType t = TYPE_HELLO) :
  m_type (t), m_valid (true)
{
}

TypeId
TypeHeader::GetTypeId ()
{
  static TypeId tid = TypeId ("ns3::aodvmesh::TypeHeader")
      .SetParent<Header> ()
      .SetGroupName("Aodv")
      .AddConstructor<TypeHeader> ()
      ;
  return tid;
}

TypeId
TypeHeader::GetInstanceTypeId () const
{
  return GetTypeId ();
}

uint32_t
TypeHeader::GetSerializedSize () const
{
  return AODVMESH_HEADER_SIZE;
}

void
TypeHeader::Serialize (Buffer::Iterator i) const
{
  i.WriteU8 ((uint8_t) m_type);
}

uint32_t
TypeHeader::Deserialize (Buffer::Iterator start)
{
  Buffer::Iterator i = start;
  uint8_t type = i.ReadU8 ();
  m_valid = true;
  switch (type)
    {
    case AODVTYPE_RREQ:
    	NS_LOG_DEBUG ("Deserializing packet RREQ");
    case AODVTYPE_RREP:
    	NS_LOG_DEBUG ("Deserializing packet RREP");
    case AODVTYPE_RERR:
    	NS_LOG_DEBUG ("Deserializing packet RERR");
    case AODVTYPE_RREP_ACK:
    	NS_LOG_DEBUG ("Deserializing packet HELLO");
    case TYPE_HELLO:
      {
        m_type = (MessageType) type;
        break;
      }
    default:
      m_valid = false;
    }
  uint32_t dist = i.GetDistanceFrom (start);
  NS_ASSERT (dist == GetSerializedSize ());
  return dist;
}

void
TypeHeader::Print (std::ostream &os) const
{
  switch (m_type)
    {
    case AODVTYPE_RREQ:
      {
        os << "RREQ";
        break;
      }
    case AODVTYPE_RREP:
      {
        os << "RREP";
        break;
      }
    case AODVTYPE_RERR:
      {
        os << "RERR";
        break;
      }
    case AODVTYPE_RREP_ACK:
      {
        os << "RREP_ACK";
        break;
      }
    case TYPE_HELLO:
      {
        os << "HELLO";
        break;
      }
    default:
      os << "UNKNOWN_TYPE";
    }
}

bool
TypeHeader::operator== (TypeHeader const & o) const
{
  return (m_type == o.m_type && m_valid == o.m_valid);
}

std::ostream &
operator<< (std::ostream & os, TypeHeader const & h)
{
  h.Print (os);
  return os;
}

// ---------------- AODVMESH Hello Message -------------------------------

NS_OBJECT_ENSURE_REGISTERED (HelloHeader);

HelloHeader::HelloHeader(//uint16_t messageLength,
		uint16_t sequenceNumber, NodeStatus nodeStatus, core_noncore_Indicator bnBcnIndicator,
//		uint8_t listSize,
		Ipv4Address originatorAddress, Ipv4Address associatedBnAddress,
		//uint16_t weightSize,
		WeightFunction weightFunction, uint32_t weightValue, MulticastBnNeighborSet tset):
		m_messageLength (AODVMESH_HELLO_HEADER_SIZE + AODVMESH_HELLO_BNNEIGHBOR_ENTRY*(nodeStatus==RN_NODE?0:tset.size())),
		m_messageSequenceNumber (sequenceNumber),
		m_nodeStatus (nodeStatus), m_bnBcnIndicator (bnBcnIndicator),
		m_listSize (nodeStatus==RN_NODE?0:tset.size()),
		m_nodeIdentifier (originatorAddress),
		m_associatedBnIdentifier (associatedBnAddress),
		m_weightSize (sizeof(weightValue)),
		m_weightFunction (weightFunction), m_weightValue (weightValue),
		m_MulticastBnNeighbors (tset)
{ if (m_listSize == 0) m_MulticastBnNeighbors.clear();}

HelloHeader::~HelloHeader() {}

TypeId HelloHeader::GetTypeId(void) {
	static TypeId
	tid = TypeId("ns3::aodvmesh::HelloHeader")
  	   .SetParent<Header> ()
           .SetGroupName("Aodv")
	   .AddConstructor<HelloHeader> ()
	   ;
	return tid;
}
TypeId HelloHeader::GetInstanceTypeId(void) const {
	return GetTypeId();
}

uint32_t HelloHeader::GetSerializedSize(void) const {
	uint32_t size = AODVMESH_HELLO_HEADER_SIZE;
	size += ((uint16_t) m_listSize) * AODVMESH_HELLO_BNNEIGHBOR_ENTRY;
	return size;
}

void HelloHeader::Print(std::ostream &os) const {
	os << "| " << m_nodeStatus << " | " << m_bnBcnIndicator << " | " << (uint16_t)m_listSize << " |\n"
	   << "| " << m_messageLength << " | " << m_messageSequenceNumber << " |\n|";
	m_nodeIdentifier.Print(os);
	os <<" |\n|";
	m_associatedBnIdentifier.Print(os);
	os <<" |\n| " << m_weightSize << " | " << m_weightFunction << " |\n"
	   << "| " << m_weightValue <<" |\n";
	for (MulticastBnNeighborSet::const_iterator iter = m_MulticastBnNeighbors.begin(); m_listSize>0 && iter != m_MulticastBnNeighbors.end(); iter++) {
		os << "\t| ";
		iter->twoHopBnNeighborIfaceAddr.Print(os);
		os << " |.| ";
		os << iter->twoHopBnNeighborWeight;
		os << " |.| ";
		os << iter->twoHopBnNeighborIndicator;
		os << " |\n";
	}
}

void HelloHeader::Serialize(Buffer::Iterator start) const {
	Buffer::Iterator i = start;
	i.WriteU8(m_nodeStatus);
	i.WriteU8(m_bnBcnIndicator);

	i.WriteU8(m_listSize);


	i.WriteHtonU16(AODVMESH_HELLO_HEADER_SIZE + m_listSize * AODVMESH_HELLO_BNNEIGHBOR_ENTRY);
	i.WriteHtonU16(m_messageSequenceNumber);

	i.WriteHtonU32(m_nodeIdentifier.Get());

	i.WriteHtonU32(m_associatedBnIdentifier.Get());

	i.WriteHtonU16(sizeof(m_weightValue));
	i.WriteHtonU16(m_weightFunction);

	i.WriteHtonU32(m_weightValue);
	for (MulticastBnNeighborSet::const_iterator iter = m_MulticastBnNeighbors.begin();
			m_listSize>0 && iter != m_MulticastBnNeighbors.end(); iter++) {
		Ipv4Address address = iter->twoHopBnNeighborIfaceAddr;
		uint32_t weight = iter->twoHopBnNeighborWeight;
		uint32_t ind = (uint32_t)iter->twoHopBnNeighborIndicator;
		i.WriteHtonU32(address.Get());
		i.WriteHtonU32(weight);
		i.WriteHtonU32(ind);
	}
	NS_LOG_DEBUG ("Serialize Hello packet");
}

uint32_t HelloHeader::Deserialize(Buffer::Iterator start) {
	uint32_t size;
	Buffer::Iterator i = start;

	m_nodeStatus = NodeStatus(i.ReadU8());
	m_bnBcnIndicator = core_noncore_Indicator(i.ReadU8());
	m_listSize = (uint16_t)i.ReadU8();

	m_messageLength = i.ReadNtohU16();
	m_messageSequenceNumber= i.ReadNtohU16();

	m_nodeIdentifier = Ipv4Address(i.ReadNtohU32());

	m_associatedBnIdentifier= Ipv4Address(i.ReadNtohU32());

	m_weightSize= i.ReadNtohU16();
	m_weightFunction= WeightFunction(i.ReadNtohU16());

	m_weightValue= i.ReadNtohU32();

	size = AODVMESH_HELLO_HEADER_SIZE;
	uint32_t messageSize = m_messageLength - AODVMESH_HELLO_HEADER_SIZE;
    NS_ASSERT (messageSize % AODVMESH_HELLO_BNNEIGHBOR_ENTRY == 0);

    int numBnNeighbors = messageSize / AODVMESH_HELLO_BNNEIGHBOR_ENTRY;
	m_MulticastBnNeighbors.clear();
	for (int n = 0; n < m_listSize; ++n){
		Ipv4Address address = Ipv4Address(i.ReadNtohU32());
		uint32_t weight = i.ReadNtohU32();
		core_noncore_Indicator ind = core_noncore_Indicator(i.ReadNtohU32());
		MulticastBnNeighborTuple thn (address, weight, ind);
		this->m_MulticastBnNeighbors.push_back(thn);
		size += AODVMESH_HELLO_BNNEIGHBOR_ENTRY;
	}
	NS_ASSERT(m_listSize==numBnNeighbors);
	NS_LOG_DEBUG ("Deserialize Hello packet");
	return size;
}

//-----------------------------------------------------------------------------
// RREQ
//-----------------------------------------------------------------------------
RreqHeader::RreqHeader (uint8_t flags, uint8_t reserved, uint8_t hopCount, uint32_t requestID, Ipv4Address dst,
                        uint32_t dstSeqNo, Ipv4Address origin, uint32_t originSeqNo) :
                       m_flags (flags), m_reserved (reserved), m_hopCount (hopCount), m_requestID (requestID), m_dst(dst),
                       m_dstSeqNo (dstSeqNo), m_origin(origin),  m_originSeqNo (originSeqNo)
{
}

NS_OBJECT_ENSURE_REGISTERED (RreqHeader);

TypeId
RreqHeader::GetTypeId ()
{
  static TypeId tid = TypeId ("ns3::aodvmesh::RreqHeader")
      .SetParent<Header> ()
       .SetGroupName("Aodv")
      .AddConstructor<RreqHeader> ()
      ;
  return tid;
}

TypeId
RreqHeader::GetInstanceTypeId () const
{
  return GetTypeId ();
}

uint32_t
RreqHeader::GetSerializedSize () const
{
  return 23;
}

void
RreqHeader::Serialize (Buffer::Iterator i) const
{
  i.WriteU8 (m_flags);
  i.WriteU8 (m_reserved);
  i.WriteU8 (m_hopCount);
  i.WriteHtonU32 (m_requestID);
  WriteTo (i, m_dst);
  i.WriteHtonU32 (m_dstSeqNo);
  WriteTo (i, m_origin);
  i.WriteHtonU32 (m_originSeqNo);
}

uint32_t
RreqHeader::Deserialize (Buffer::Iterator start)
{
  Buffer::Iterator i = start;
  m_flags = i.ReadU8 ();
  m_reserved = i.ReadU8 ();
  m_hopCount = i.ReadU8 ();
  m_requestID = i.ReadNtohU32 ();
  ReadFrom (i, m_dst);
  m_dstSeqNo = i.ReadNtohU32 ();
  ReadFrom (i, m_origin);
  m_originSeqNo = i.ReadNtohU32 ();

  uint32_t dist = i.GetDistanceFrom (start);
  NS_ASSERT (dist == GetSerializedSize ());
  return dist;
}

void
RreqHeader::Print (std::ostream &os) const
{
  os << "RREQ ID " << m_requestID << " destination: ipv4 " << m_dst
      << " sequence number " << m_dstSeqNo << " source: ipv4 "
      << m_origin << " sequence number " << m_originSeqNo
      << " flags:" << " Gratuitous RREP " << (*this).GetGratiousRrep ()
      << " Destination only " << (*this).GetDestinationOnly ()
      << " Unknown sequence number " << (*this).GetUnknownSeqno ();
}

std::ostream &
operator<< (std::ostream & os, RreqHeader const & h)
{
  h.Print (os);
  return os;
}

void
RreqHeader::SetGratiousRrep (bool f)
{
  if (f)
    m_flags |= (1 << 5);
  else
    m_flags &= ~(1 << 5);
}

bool
RreqHeader::GetGratiousRrep () const
{
  return (m_flags & (1 << 5));
}

void
RreqHeader::SetDestinationOnly (bool f)
{
  if (f)
    m_flags |= (1 << 4);
  else
    m_flags &= ~(1 << 4);
}

bool
RreqHeader::GetDestinationOnly () const
{
  return (m_flags & (1 << 4));
}

void
RreqHeader::SetUnknownSeqno (bool f)
{
  if (f)
    m_flags |= (1 << 3);
  else
    m_flags &= ~(1 << 3);
}

bool
RreqHeader::GetUnknownSeqno () const
{
  return (m_flags & (1 << 3));
}

bool
RreqHeader::operator== (RreqHeader const & o) const
{
  return (m_flags == o.m_flags && m_reserved == o.m_reserved &&
          m_hopCount == o.m_hopCount && m_requestID == o.m_requestID &&
          m_dst == o.m_dst && m_dstSeqNo == o.m_dstSeqNo &&
          m_origin == o.m_origin && m_originSeqNo == o.m_originSeqNo);
}

//-----------------------------------------------------------------------------
// RREP
//-----------------------------------------------------------------------------

RrepHeader::RrepHeader (uint8_t prefixSize, uint8_t hopCount, Ipv4Address dst,
                        uint32_t dstSeqNo, Ipv4Address origin, Time lifeTime) :
                        m_flags (0), m_prefixSize (prefixSize), m_hopCount (hopCount),
                        m_dst (dst), m_dstSeqNo (dstSeqNo), m_origin (origin)
{
  m_lifeTime = uint32_t (lifeTime.GetMilliSeconds ());
}

NS_OBJECT_ENSURE_REGISTERED (RrepHeader);

TypeId
RrepHeader::GetTypeId ()
{
  static TypeId tid = TypeId ("ns3::aodvmesh::RrepHeader")
      .SetParent<Header> ()
      .SetGroupName("Aodv")
      .AddConstructor<RrepHeader> ()
      ;
  return tid;
}

TypeId
RrepHeader::GetInstanceTypeId () const
{
  return GetTypeId ();
}

uint32_t
RrepHeader::GetSerializedSize () const
{
  return 19;
}

void
RrepHeader::Serialize (Buffer::Iterator i) const
{
  i.WriteU8 (m_flags);
  i.WriteU8 (m_prefixSize);
  i.WriteU8 (m_hopCount);
  WriteTo (i, m_dst);
  i.WriteHtonU32 (m_dstSeqNo);
  WriteTo (i, m_origin);
  i.WriteHtonU32 (m_lifeTime);
}

uint32_t
RrepHeader::Deserialize (Buffer::Iterator start)
{
  Buffer::Iterator i = start;

  m_flags = i.ReadU8 ();
  m_prefixSize = i.ReadU8 ();
  m_hopCount = i.ReadU8 ();
  ReadFrom (i, m_dst);
  m_dstSeqNo = i.ReadNtohU32 ();
  ReadFrom (i, m_origin);
  m_lifeTime = i.ReadNtohU32 ();

  uint32_t dist = i.GetDistanceFrom (start);
  NS_ASSERT (dist == GetSerializedSize ());
  return dist;
}

void
RrepHeader::Print (std::ostream &os) const
{
  os << "destination: ipv4 " << m_dst << " sequence number " << m_dstSeqNo;
  if (m_prefixSize != 0)
    {
      os << " prefix size " << m_prefixSize;
    }
  os << " source ipv4 " << m_origin << " lifetime " << m_lifeTime
      << " acknowledgment required flag " << (*this).GetAckRequired ();
}

void
RrepHeader::SetLifeTime (Time t)
{
  m_lifeTime = t.GetMilliSeconds ();
}

Time
RrepHeader::GetLifeTime () const
{
  Time t (MilliSeconds (m_lifeTime));
  return t;
}

void
RrepHeader::SetAckRequired (bool f)
{
  if (f)
    m_flags |= (1 << 6);
  else
    m_flags &= ~(1 << 6);
}

bool
RrepHeader::GetAckRequired () const
{
  return (m_flags & (1 << 6));
}

void
RrepHeader::SetPrefixSize (uint8_t sz)
{
  m_prefixSize = sz;
}

uint8_t
RrepHeader::GetPrefixSize () const
{
  return m_prefixSize;
}

bool
RrepHeader::operator== (RrepHeader const & o) const
{
  return (m_flags == o.m_flags && m_prefixSize == o.m_prefixSize &&
          m_hopCount == o.m_hopCount && m_dst == o.m_dst && m_dstSeqNo == o.m_dstSeqNo &&
          m_origin == o.m_origin && m_lifeTime == o.m_lifeTime);
}

void
RrepHeader::SetHello (Ipv4Address origin, uint32_t srcSeqNo, Time lifetime)
{
  m_flags = 0;
  m_prefixSize = 0;
  m_hopCount = 0;
  m_dst = origin;
  m_dstSeqNo = srcSeqNo;
  m_origin = origin;
  m_lifeTime = lifetime.GetMilliSeconds ();
}

std::ostream &
operator<< (std::ostream & os, RrepHeader const & h)
{
  h.Print (os);
  return os;
}

//-----------------------------------------------------------------------------
// RREP-ACK
//-----------------------------------------------------------------------------

RrepAckHeader::RrepAckHeader () :
  m_reserved (0)
{
}

NS_OBJECT_ENSURE_REGISTERED (RrepAckHeader);
TypeId
RrepAckHeader::GetTypeId ()
{
  static TypeId tid = TypeId ("ns3::aodvmesh::RrepAckHeader")
      .SetParent<Header> ()
      .SetGroupName("Aodv")
      .AddConstructor<RrepAckHeader> ()
      ;
  return tid;
}

TypeId
RrepAckHeader::GetInstanceTypeId () const
{
  return GetTypeId ();
}

uint32_t
RrepAckHeader::GetSerializedSize () const
{
  return 1;
}

void
RrepAckHeader::Serialize (Buffer::Iterator i ) const
{
  i.WriteU8 (m_reserved);
}

uint32_t
RrepAckHeader::Deserialize (Buffer::Iterator start )
{
  Buffer::Iterator i = start;
  m_reserved = i.ReadU8 ();
  uint32_t dist = i.GetDistanceFrom (start);
  NS_ASSERT (dist == GetSerializedSize ());
  return dist;
}

void
RrepAckHeader::Print (std::ostream &os ) const
{
}

bool
RrepAckHeader::operator== (RrepAckHeader const & o ) const
{
  return m_reserved == o.m_reserved;
}

std::ostream &
operator<< (std::ostream & os, RrepAckHeader const & h )
{
  h.Print (os);
  return os;
}

//-----------------------------------------------------------------------------
// RERR
//-----------------------------------------------------------------------------
RerrHeader::RerrHeader () :
  m_flag (0), m_reserved (0)
{
}

NS_OBJECT_ENSURE_REGISTERED (RerrHeader);

TypeId
RerrHeader::GetTypeId ()
{
  static TypeId tid = TypeId ("ns3::aodvmesh::RerrHeader")
      .SetParent<Header> ()
      .SetGroupName("Aodv")
      .AddConstructor<RerrHeader> ()
      ;
  return tid;
}

TypeId
RerrHeader::GetInstanceTypeId () const
{
  return GetTypeId ();
}

uint32_t
RerrHeader::GetSerializedSize () const
{
  return (3 + 8 * GetDestCount ());
}

void
RerrHeader::Serialize (Buffer::Iterator i ) const
{
  i.WriteU8 (m_flag);
  i.WriteU8 (m_reserved);
  i.WriteU8 (GetDestCount ());
  std::map<Ipv4Address, uint32_t>::const_iterator j;
  for (j = m_unreachableDstSeqNo.begin (); j != m_unreachableDstSeqNo.end (); ++j)
    {
      WriteTo (i, (*j).first);
      i.WriteHtonU32 ((*j).second);
    }
}

uint32_t
RerrHeader::Deserialize (Buffer::Iterator start )
{
  Buffer::Iterator i = start;
  m_flag = i.ReadU8 ();
  m_reserved = i.ReadU8 ();
  uint8_t dest = i.ReadU8 ();
  m_unreachableDstSeqNo.clear ();
  Ipv4Address address;
  uint32_t seqNo;
  for (uint8_t k = 0; k < dest; ++k)
    {
      ReadFrom (i, address);
      seqNo = i.ReadNtohU32 ();
      m_unreachableDstSeqNo.insert (std::make_pair (address, seqNo));
    }

  uint32_t dist = i.GetDistanceFrom (start);
  NS_ASSERT (dist == GetSerializedSize ());
  return dist;
}

void
RerrHeader::Print (std::ostream &os ) const
{
  os << "Unreachable destination (ipv4 address, seq. number):";
  std::map<Ipv4Address, uint32_t>::const_iterator j;
  for (j = m_unreachableDstSeqNo.begin (); j != m_unreachableDstSeqNo.end (); ++j)
    {
      os << (*j).first << ", " << (*j).second;
    }
  os << "No delete flag " << (*this).GetNoDelete ();
}

void
RerrHeader::SetNoDelete (bool f )
{
  if (f)
    m_flag |= (1 << 0);
  else
    m_flag &= ~(1 << 0);
}

bool
RerrHeader::GetNoDelete () const
{
  return (m_flag & (1 << 0));
}

bool
RerrHeader::AddUnDestination (Ipv4Address dst, uint32_t seqNo )
{
  if (m_unreachableDstSeqNo.find (dst) != m_unreachableDstSeqNo.end ())
    return true;

  NS_ASSERT (GetDestCount() < 255); // can't support more than 255 destinations in single RERR
  m_unreachableDstSeqNo.insert (std::make_pair (dst, seqNo));
  return true;
}

bool
RerrHeader::RemoveUnDestination (std::pair<Ipv4Address, uint32_t> & un )
{
  if (m_unreachableDstSeqNo.empty ())
    return false;
  std::map<Ipv4Address, uint32_t>::iterator i = m_unreachableDstSeqNo.begin ();
  un = *i;
  m_unreachableDstSeqNo.erase (i);
  return true;
}

void
RerrHeader::Clear ()
{
  m_unreachableDstSeqNo.clear ();
  m_flag = 0;
  m_reserved = 0;
}

bool
RerrHeader::operator== (RerrHeader const & o ) const
{
  if (m_flag != o.m_flag || m_reserved != o.m_reserved || GetDestCount () != o.GetDestCount ())
    return false;

  std::map<Ipv4Address, uint32_t>::const_iterator j = m_unreachableDstSeqNo.begin ();
  std::map<Ipv4Address, uint32_t>::const_iterator k = o.m_unreachableDstSeqNo.begin ();
  for (uint8_t i = 0; i < GetDestCount (); ++i)
    {
      if ((j->first != k->first) || (j->second != k->second))
        return false;

      j++;
      k++;
    }
  return true;
}

std::ostream &
operator<< (std::ostream & os, RerrHeader const & h )
{
  h.Print (os);
  return os;
}

}} // namespace aodvmesh, ns3

#include "ns3/test.h"
#include "ns3/packet.h"

namespace ns3 {

class MbnHelloTest : public TestCase {
public:
	MbnHelloTest ();
	virtual void DoRun (void);
};

MbnHelloTest::MbnHelloTest ()
  :TestCase ("Check AODVMESH Hello message header")
{}

void
MbnHelloTest::DoRun (void)
{
	Packet packet;
	uint16_t listSize = 0;
	uint16_t messageLength = 0;
	uint32_t weightValue = 5, messageSequenceNumber = 101;
	uint16_t weightSize = sizeof(weightValue);
	{
		aodvmesh::MulticastBnNeighborSet thn;
		struct aodvmesh::MulticastBnNeighborTuple t1 (Ipv4Address("192.168.1.1"), 4, aodvmesh::CONVERT_ALLOW);
		thn.push_back(t1);
		struct aodvmesh::MulticastBnNeighborTuple t2 (Ipv4Address("192.168.1.2"), 8, aodvmesh::CONVERT_OTHER);
		thn.push_back(t2);
		struct aodvmesh::MulticastBnNeighborTuple t3 (Ipv4Address("192.168.1.5"), 9, aodvmesh::CONVERT_BREAK);
		thn.push_back(t3);

		listSize = thn.size();
		messageLength = AODVMESH_HELLO_HEADER_SIZE+AODVMESH_HELLO_BNNEIGHBOR_ENTRY*listSize;

		aodvmesh::HelloHeader msgIn(messageSequenceNumber,aodvmesh::RN_NODE,aodvmesh::CONVERT_ALLOW,Ipv4Address("192.168.1.4"),
						Ipv4Address("192.168.1.2"),aodvmesh::W_NODE_DEGREE,weightValue,thn);

		packet.AddHeader(msgIn);
		aodvmesh::TypeHeader msgOne (aodvmesh::TYPE_HELLO);
		packet.AddHeader(msgOne);
	}
	{
		aodvmesh::TypeHeader msg0;
		packet.RemoveHeader(msg0);
		aodvmesh::HelloHeader msg1;
		packet.RemoveHeader(msg1);
		msg1.Print(std::cout);
		uint32_t sizeLeft = msg1.GetSerializedSize();

		NS_TEST_ASSERT_MSG_EQ(msg1.GetMessageLength(),messageLength,"Message size");
		NS_TEST_ASSERT_MSG_EQ(msg1.GetMessageSequenceNumber(),messageSequenceNumber,"Message sequence number");

		NS_TEST_ASSERT_MSG_EQ(msg1.GetNodeStatus(),aodvmesh::RN_NODE,"Node status");
		NS_TEST_ASSERT_MSG_EQ(msg1.Getcore_noncoreIndicator(),aodvmesh::CONVERT_ALLOW,"BN-BCN indicator");
		NS_TEST_ASSERT_MSG_EQ(msg1.GetListSize(),listSize,"List size");

		NS_TEST_ASSERT_MSG_EQ(msg1.GetOriginatorAddress(),Ipv4Address ("192.168.1.4"),"Originator Address");

		NS_TEST_ASSERT_MSG_EQ(msg1.GetAssociatedBnAddress(),Ipv4Address ("192.168.1.2"),"Associated BN");

		NS_TEST_ASSERT_MSG_EQ(msg1.GetWeightSize(),weightSize,"Weight size");
		NS_TEST_ASSERT_MSG_EQ(msg1.GetWeightFunction(),aodvmesh::W_NODE_DEGREE,"Weight function");
		NS_TEST_ASSERT_MSG_EQ(msg1.GetWeightValue(),weightValue,"Weight value");

		aodvmesh::MulticastBnNeighborSet bnNeighborList = msg1.GetMulticastNeighborSet();

		NS_TEST_ASSERT_MSG_EQ (bnNeighborList.size(), listSize, "List size");
		NS_TEST_ASSERT_MSG_EQ (bnNeighborList.begin()->twoHopBnNeighborIfaceAddr, Ipv4Address ("192.168.1.1"), "First element IP");
		NS_TEST_ASSERT_MSG_EQ (bnNeighborList.begin()->twoHopBnNeighborWeight, 4, "First element Weight");
		NS_TEST_ASSERT_MSG_EQ (bnNeighborList.begin()->twoHopBnNeighborIndicator, aodvmesh::CONVERT_ALLOW, "First element Indicator");

		sizeLeft -= msg1.GetSerializedSize();

		NS_TEST_ASSERT_MSG_EQ (sizeLeft, 0, "There are some bytes not matched!");
 	}
}


} // namespace ns3
