#ifndef __AODV_HEADER_H__
#define __AODV_HEADER_H__

#include <stdint.h>
#include <iostream>
#include <vector>
#include <map>
#include "ns3/header.h"
#include "ns3/ipv4-address.h"
#include "ns3/nstime.h"
#include "ns3/enum.h"
#include "aodv-common.h"

namespace ns3 {
namespace aodvmesh {

enum MessageType
{
  AODVTYPE_RREQ  = 1,   //!< AODVTYPE_RREQ
  AODVTYPE_RREP  = 2,   //!< AODVTYPE_RREP
  AODVTYPE_RERR  = 3,   //!< AODVTYPE_RERR
  AODVTYPE_RREP_ACK = 4, //!< AODVTYPE_RREP_ACK
  TYPE_HELLO = 5 // !< TYPE_HELLO
};

/**
* \ingroup aodvmesh
* \brief AODVMESH types
*/
class TypeHeader : public Header
{
public:
  /// c-tor
  TypeHeader (MessageType t);

  ///\name Header serialization/deserialization
  //\{
  static TypeId GetTypeId ();
  TypeId GetInstanceTypeId () const;
  uint32_t GetSerializedSize () const;
  void Serialize (Buffer::Iterator start) const;
  uint32_t Deserialize (Buffer::Iterator start);
  void Print (std::ostream &os) const;
  //\}

  /// Return type
  MessageType Get () const { return m_type; }
  /// Check that type if valid
  bool IsValid () const { return m_valid; }
  bool operator== (TypeHeader const & o) const;
private:
  MessageType m_type;
  bool m_valid;
};

std::ostream & operator<< (std::ostream & os, TypeHeader const & h);

//    The basic layout of any packet in AODVMESH (omitting IP and UDP headers):
//
//        0               1               2               3
//        0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7 0 1 2 3 4 5 6 7
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//1      |      Type     |  Node Status  |  BN Bcn Ind.  |   List Size   |
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//2      |         Message Length        |    Message Sequence Number    |
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//3      |             Originator Address or Node Identifier             |
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//4      |                       Associated BN Address                   |
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//5      |         Weight Size           |         Weight Function       |
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//6      |                            Weight   wt(u)	                 |
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//7      |                      BN Neighbor Address	                     | BCN+BN ONLY
//8      :                      BN Neighbor Weight 						 : BCN+BN ONLY
//9      |                   BN Neighbor BN-Bcn Indicator                | BCN+BN ONLY
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//


class HelloHeader: public Header {
public:

private:
	/// Message Length: The length (in bytes) of the message.
	uint16_t m_messageLength;
	/// Message Sequence Number: incremented by one each time a new AODVMESH packet is transmitted.
	uint16_t m_messageSequenceNumber;
	/// Status of the node: Regular Node, Backbone Capable Node or Backbone Node.
	NodeStatus m_nodeStatus;
	/// Conversion indicator.
	core_noncore_Indicator m_bnBcnIndicator;
	/// Numbers of address in BN neighbor list
	uint8_t m_listSize;
	/// Address of the current node.
	Ipv4Address m_nodeIdentifier;
	/// Address of the associated backbone node.
	Ipv4Address m_associatedBnIdentifier;
	/// Size of the weight field, counted in bytes and measured from the beginning of the "Weight field" field and until the end of such a field.
	uint16_t m_weightSize;
	/// Node weight function used.
	WeightFunction m_weightFunction;
	/// Node weight represented by one single 4-bytes word
	uint32_t m_weightValue;


public:
	MulticastBnNeighborSet m_MulticastBnNeighbors;
	HelloHeader(//uint16_t messageLength,
			uint16_t sequenceNumber = 0 ,NodeStatus nodeStatus=RN_NODE, core_noncore_Indicator bnBcnIndicator=CONVERT_OTHER,
//			uint8_t listSize = 0,
			Ipv4Address m_nodeIdentifier = Ipv4Address() , Ipv4Address m_associatedBnIdentifier=Ipv4Address(),
			//uint16_t weightSize = 0,
			WeightFunction weightFunction = W_NODE_DEGREE,uint32_t weightValue = 0, MulticastBnNeighborSet tset = MulticastBnNeighborSet());
	virtual ~HelloHeader();
	void SetMessageLength(uint16_t length) {m_messageLength = length;}
	uint16_t GetMessageLength() const {return m_messageLength;}
	void SetMessageSequenceNumber(uint16_t seqnum) {m_messageSequenceNumber = seqnum;}
	uint16_t GetMessageSequenceNumber() const {return m_messageSequenceNumber;}
	void SetNodeStatus(NodeStatus nodeStatus) {m_nodeStatus = nodeStatus;}
	NodeStatus GetNodeStatus() const {return m_nodeStatus;}
	void SetListSize(uint8_t listSize) {m_listSize = listSize;}
	uint8_t GetListSize() const {return m_listSize;}
	void Setcore_noncoreIndicator(core_noncore_Indicator bnBcnIndicator) {m_bnBcnIndicator = bnBcnIndicator;}
	core_noncore_Indicator Getcore_noncoreIndicator() const {return m_bnBcnIndicator;}
	void SetOriginatorAddress(Ipv4Address originatorAddress) {m_nodeIdentifier = originatorAddress;}
	Ipv4Address GetOriginatorAddress() const {return m_nodeIdentifier;}
	void SetAssociatedBnAddress(Ipv4Address associatedBnAddress) {m_associatedBnIdentifier = associatedBnAddress;}
	Ipv4Address GetAssociatedBnAddress() const {return m_associatedBnIdentifier;}
	void SetWeightSize(uint16_t weightSize) {m_weightSize = weightSize;}
	uint16_t GetWeightSize() const {return m_weightSize;}
	void SetWeightFunction(WeightFunction weightFunction) {m_weightFunction = weightFunction;}
	uint32_t GetWeightFunction() const {return m_weightFunction;}
	void SetWeightValue(uint32_t weightValue) {m_weightValue = weightValue;}
	uint32_t GetWeightValue() const {return m_weightValue;}
	MulticastBnNeighborSet GetMulticastNeighborSet() const {return m_MulticastBnNeighbors;}

	static TypeId GetTypeId();
	TypeId GetInstanceTypeId() const;
	void Print(std::ostream &os) const;
	uint32_t GetSerializedSize(void) const;
	void Serialize(Buffer::Iterator start) const;
	uint32_t Deserialize(Buffer::Iterator start);
};

std::ostream & operator<< (std::ostream & os, HelloHeader const &);

/**
* \ingroup aodvmesh
* \brief   Route Request (RREQ) Message Format
  \verbatim
  0                   1                   2                   3
  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |     Type      |J|R|G|D|U|   Reserved          |   Hop Count   |
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |                            RREQ ID                            |
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |                    Destination IP Address                     |
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |                  Destination Sequence Number                  |
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |                    Originator IP Address                      |
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |                  Originator Sequence Number                   |
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  \endverbatim
*/
class RreqHeader : public Header
{
public:
  /// c-tor
  RreqHeader (uint8_t flags = 0, uint8_t reserved = 0, uint8_t hopCount = 0,
      uint32_t requestID = 0, Ipv4Address dst = Ipv4Address (),
      uint32_t dstSeqNo = 0, Ipv4Address origin = Ipv4Address (),
      uint32_t originSeqNo = 0);

  ///\name Header serialization/deserialization
  //\{
  static TypeId GetTypeId ();
  TypeId GetInstanceTypeId () const;
  uint32_t GetSerializedSize () const;
  void Serialize (Buffer::Iterator start) const;
  uint32_t Deserialize (Buffer::Iterator start);
  void Print (std::ostream &os) const;
  //\}

  ///\name Fields
  //\{
  void SetHopCount (uint8_t count) { m_hopCount = count; }
  uint8_t GetHopCount () const { return m_hopCount; }
  void SetId (uint32_t id) { m_requestID = id; }
  uint32_t GetId () const { return m_requestID; }
  void SetDst (Ipv4Address a) { m_dst = a; }
  Ipv4Address GetDst () const { return m_dst; }
  void SetDstSeqno (uint32_t s) { m_dstSeqNo = s; }
  uint32_t GetDstSeqno () const { return m_dstSeqNo; }
  void SetOrigin (Ipv4Address a) { m_origin = a; }
  Ipv4Address GetOrigin () const { return m_origin; }
  void SetOriginSeqno (uint32_t s) { m_originSeqNo = s; }
  uint32_t GetOriginSeqno () const { return m_originSeqNo; }
  //\}

  ///\name Flags
  //\{
  void SetGratiousRrep (bool f);
  bool GetGratiousRrep () const;
  void SetDestinationOnly (bool f);
  bool GetDestinationOnly () const;
  void SetUnknownSeqno (bool f);
  bool GetUnknownSeqno () const;
  //\}

  bool operator== (RreqHeader const & o) const;
private:
  uint8_t        m_flags;          ///< |J|R|G|D|U| bit flags, see RFC
  uint8_t        m_reserved;       ///< Not used
  uint8_t        m_hopCount;       ///< Hop Count
  uint32_t       m_requestID;      ///< RREQ ID
  Ipv4Address    m_dst;            ///< Destination IP Address
  uint32_t       m_dstSeqNo;       ///< Destination Sequence Number
  Ipv4Address    m_origin;         ///< Originator IP Address
  uint32_t       m_originSeqNo;    ///< Source Sequence Number
};

std::ostream & operator<< (std::ostream & os, RreqHeader const &);

/**
* \ingroup aodvmesh
* \brief Route Reply (RREP) Message Format
  \verbatim
  0                   1                   2                   3
  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |     Type      |R|A|    Reserved     |Prefix Sz|   Hop Count   |
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |                     Destination IP address                    |
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |                  Destination Sequence Number                  |
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |                    Originator IP address                      |
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |                           Lifetime                            |
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  \endverbatim
*/
class RrepHeader : public Header
{
public:
  /// c-tor
  RrepHeader (uint8_t prefixSize = 0, uint8_t hopCount = 0, Ipv4Address dst =
      Ipv4Address (), uint32_t dstSeqNo = 0, Ipv4Address origin =
      Ipv4Address (), Time lifetime = MilliSeconds (0));
  ///\name Header serialization/deserialization
  //\{
  static TypeId GetTypeId ();
  TypeId GetInstanceTypeId () const;
  uint32_t GetSerializedSize () const;
  void Serialize (Buffer::Iterator start) const;
  uint32_t Deserialize (Buffer::Iterator start);
  void Print (std::ostream &os) const;
  //\}

  ///\name Fields
  //\{
  void SetHopCount (uint8_t count) { m_hopCount = count; }
  uint8_t GetHopCount () const { return m_hopCount; }
  void SetDst (Ipv4Address a) { m_dst = a; }
  Ipv4Address GetDst () const { return m_dst; }
  void SetDstSeqno (uint32_t s) { m_dstSeqNo = s; }
  uint32_t GetDstSeqno () const { return m_dstSeqNo; }
  void SetOrigin (Ipv4Address a) { m_origin = a; }
  Ipv4Address GetOrigin () const { return m_origin; }
  void SetLifeTime (Time t);
  Time GetLifeTime () const;
  //\}

  ///\name Flags
  //\{
  void SetAckRequired (bool f);
  bool GetAckRequired () const;
  void SetPrefixSize (uint8_t sz);
  uint8_t GetPrefixSize () const;
  //\}

  /// Configure RREP to be a Hello message
  void SetHello (Ipv4Address src, uint32_t srcSeqNo, Time lifetime);

  bool operator== (RrepHeader const & o) const;
private:
  uint8_t       m_flags;	        ///< A - acknowledgment required flag
  uint8_t       m_prefixSize;	    ///< Prefix Size
  uint8_t	    m_hopCount;         ///< Hop Count
  Ipv4Address   m_dst;              ///< Destination IP Address
  uint32_t      m_dstSeqNo;         ///< Destination Sequence Number
  Ipv4Address	m_origin;           ///< Source IP Address
  uint32_t      m_lifeTime;         ///< Lifetime (in milliseconds)
};

std::ostream & operator<< (std::ostream & os, RrepHeader const &);

/**
* \ingroup aodvmesh
* \brief Route Reply Acknowledgment (RREP-ACK) Message Format
  \verbatim
  0                   1
  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |     Type      |   Reserved    |
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  \endverbatim
*/
class RrepAckHeader : public Header
{
public:
  /// c-tor
  RrepAckHeader ();

  ///\name Header serialization/deserialization
  //\{
  static TypeId GetTypeId ();
  TypeId GetInstanceTypeId () const;
  uint32_t GetSerializedSize () const;
  void Serialize (Buffer::Iterator start) const;
  uint32_t Deserialize (Buffer::Iterator start);
  void Print (std::ostream &os) const;
  //\}

  bool operator== (RrepAckHeader const & o) const;
private:
  uint8_t       m_reserved;
};
std::ostream & operator<< (std::ostream & os, RrepAckHeader const &);


/**
* \ingroup aodvmesh
* \brief Route Error (RERR) Message Format
  \verbatim
  0                   1                   2                   3
  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |     Type      |N|          Reserved           |   DestCount   |
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |            Unreachable Destination IP Address (1)             |
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |         Unreachable Destination Sequence Number (1)           |
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-|
  |  Additional Unreachable Destination IP Addresses (if needed)  |
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  |Additional Unreachable Destination Sequence Numbers (if needed)|
  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  \endverbatim
*/
class RerrHeader : public Header
{
public:
  /// c-tor
  RerrHeader ();

  ///\name Header serialization/deserialization
  //\{
  static TypeId GetTypeId ();
  TypeId GetInstanceTypeId () const;
  uint32_t GetSerializedSize () const;
  void Serialize (Buffer::Iterator i) const;
  uint32_t Deserialize (Buffer::Iterator start);
  void Print (std::ostream &os) const;
  //\}

  ///\name No delete flag
  //\{
  void SetNoDelete (bool f);
  bool GetNoDelete () const;
  //\}

  /**
   * Add unreachable node address and its sequence number in RERR header
   *\return false if we already added maximum possible number of unreachable destinations
   */
  bool AddUnDestination (Ipv4Address dst, uint32_t seqNo);
  /** Delete pair (address + sequence number) from REER header, if the number of unreachable destinations > 0
   * \return true on success
   */
  bool RemoveUnDestination (std::pair<Ipv4Address, uint32_t> & un);
  /// Clear header
  void Clear();
  /// Return number of unreachable destinations in RERR message
  uint8_t GetDestCount () const { return (uint8_t)m_unreachableDstSeqNo.size(); }
  bool operator== (RerrHeader const & o) const;
private:
  uint8_t m_flag;            ///< No delete flag
  uint8_t m_reserved;        ///< Not used

  /// List of Unreachable destination: IP addresses and sequence numbers
  std::map<Ipv4Address, uint32_t> m_unreachableDstSeqNo;
};

std::ostream & operator<< (std::ostream & os, RerrHeader const &);

}//end namespace aodvmesh
}//end namespace ns3

#endif /* __AODV_HEADER_H__ */
