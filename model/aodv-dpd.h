#ifndef __AODV_DUPLICATEPACKETDETECTION_H__
#define __AODV_DUPLICATEPACKETDETECTION_H__

#include "aodv-id-cache.h"
#include "ns3/nstime.h"
#include "ns3/packet.h"
#include "ns3/ipv4-header.h"

namespace ns3
{
namespace aodvmesh
{
/**
 * \ingroup aodv
 * 
 * \brief Helper class used to remember already seen packets and detect duplicates.
 *
 * Currently duplicate detection is based on unique packet ID given by Packet::GetUid ()
 * This approach is known to be weak and should be changed.
 */
class DuplicatePacketDetection
{
public:
  /// C-tor
  DuplicatePacketDetection (Time lifetime) : m_idCache (lifetime) {}
  /// Check that the packet is duplicated. If not, save information about this packet.
  bool IsDuplicate (Ptr<const Packet> p, const Ipv4Header & header);
  /// Set duplicate records lifetimes
  void SetLifetime (Time lifetime);
  /// Get duplicate records lifetimes
  Time GetLifetime () const;
private:
  /// Impl
  IdCache m_idCache;
};

}
}
#endif  /* __AODV_DUPLICATEPACKETDETECTION_H__ */
