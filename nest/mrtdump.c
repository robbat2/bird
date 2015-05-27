/*
 *	BIRD -- Multi-Threaded Routing Toolkit (MRT) Routing Information Export Format
 *
 *	(c) 2015 CZ.NIC z.s.p.o.
 *
 *	Can be freely distributed and used under the terms of the GNU GPL.
 */

#include "nest/mrtdump.h"

void
mrt_msg_init(struct mrt_buffer *msg, pool *mem_pool)
{
  msg->mem_pool = mem_pool;
  msg->msg_capacity = MRT_MSG_DEFAULT_CAPACITY;
  msg->msg_length = MRTDUMP_HDR_LENGTH;	/* Reserve for the main MRT Header */
  msg->msg = mb_allocz(msg->mem_pool, msg->msg_capacity);
}

void
mrt_msg_free(struct mrt_buffer *msg)
{
  mb_free(msg->msg);
}

static void
mrt_grow_msg_buffer(struct mrt_buffer * msg, size_t min_required_capacity)
{
  msg->msg_capacity *= 2;
  if (min_required_capacity > msg->msg_capacity)
    msg->msg_capacity = min_required_capacity;
  msg->msg = mb_realloc(msg->msg, msg->msg_capacity);
  debug("Grow buffer up to the %d Bytes. \n", msg->msg_capacity);
}

static void
mrt_write_to_msg(struct mrt_buffer * msg, const void *data, size_t data_size)
{
  if (data_size == 0)
    return;

  u32 i;
  for (i = 0; i < data_size; i++)
    debug("%02X ", ((byte*)data)[i]);
  debug("| ");

  size_t required_size = data_size + msg->msg_length;
  if (msg->msg_capacity < required_size)
    mrt_grow_msg_buffer(msg, required_size);

  memcpy(&msg->msg[msg->msg_length], data, data_size);
  msg->msg_length += data_size;
}
#define mrt_write_to_msg_(msg, data) mrt_write_to_msg(msg, &data, sizeof(data))

void
mrt_peer_index_table_init(struct mrt_peer_index_table *pit, u32 collector_bgp_id, const char *name)
{
  struct mrt_buffer * msg = &pit->msg;
  mrt_msg_init(msg, &root_pool);

  pit->peer_count = 0;
  if (name != NULL)
    pit->name_length = strlen(name);
  else
    pit->name_length = 0;

  mrt_write_to_msg_(msg, collector_bgp_id);
  mrt_write_to_msg_(msg, pit->name_length);
  mrt_write_to_msg(msg, name, pit->name_length);
  mrt_write_to_msg_(msg, pit->peer_count);
  debug("\n");
}

static byte *
mrt_peer_index_table_get_peer_count(struct mrt_peer_index_table *pit)
{
  struct mrt_buffer * msg = &pit->msg;
  uint collector_bgp_id_size = 4;
  uint name_length_size = 2;
  uint name_size = pit->name_length;
  uint peer_count_offset = MRTDUMP_HDR_LENGTH + collector_bgp_id_size + name_length_size + name_size;
  return &(msg->msg[peer_count_offset]);
}

static void
mrt_peer_index_table_inc_peer_count(struct mrt_peer_index_table *pit)
{
  pit->peer_count++;
  byte *peer_count = mrt_peer_index_table_get_peer_count(pit);
  put_u16(peer_count, pit->peer_count);
}

void
mrt_peer_index_table_add_peer(struct mrt_peer_index_table *pit, u32 peer_bgp_id, ip_addr *peer_ip_addr, u32 peer_as)
{
  struct mrt_buffer * msg = &pit->msg;

  u8 peer_type = PEER_TYPE_AS_32BIT;
  if (sizeof(*peer_ip_addr) > sizeof(ip4_addr))
    peer_type |= PEER_TYPE_IPV6;

  mrt_write_to_msg_(msg, peer_type);
  mrt_write_to_msg_(msg, peer_bgp_id);
  mrt_write_to_msg_(msg, *peer_ip_addr);
  mrt_write_to_msg_(msg, peer_as);

  mrt_peer_index_table_inc_peer_count(pit);
  debug("\n");
}

void
mrt_rib_table_init(struct mrt_rib_table *rt_msg, u32 sequence_number, u8 prefix_length, ip_addr *prefix)
{
  struct mrt_buffer *msg = &rt_msg->msg;
  mrt_msg_init(msg, &root_pool);

  rt_msg->entry_count = 0;
#ifdef IPV6
  rt_msg->type = RIB_IPV6_UNICAST;
#else
  rt_msg->type = RIB_IPV4_UNICAST;
#endif

  mrt_write_to_msg_(msg, sequence_number);
  mrt_write_to_msg_(msg, prefix_length);
  mrt_write_to_msg_(msg, *prefix);
  mrt_write_to_msg_(msg, rt_msg->entry_count);
  debug("\n");
}

static byte *
mrt_rib_table_get_entry_count(struct mrt_rib_table *rt_msg)
{
  struct mrt_buffer *msg = &rt_msg->msg;
  u32 sequence_number_size = 4;
  u32 prefix_length_size = 1;

  u32 prefix_size = 4;
  if (rt_msg->type == RIB_IPV4_UNICAST)
    prefix_size = 4;
  else if (rt_msg->type == RIB_IPV6_UNICAST)
    prefix_size = 16;
  else
    bug("mrt_rib_table_get_entry_count: unknown RIB type!");

  u32 offset = MRTDUMP_HDR_LENGTH + sequence_number_size + prefix_length_size + prefix_size;
  return &msg->msg[offset];
}

static void
mrt_rib_table_inc_entry_count(struct mrt_rib_table *rt_msg)
{
  rt_msg->entry_count++;
  byte *entry_count = mrt_rib_table_get_entry_count(rt_msg);
  put_u16(entry_count, rt_msg->entry_count);
}

void
mrt_rib_table_add_entry(struct mrt_rib_table *rt_msg, const struct mrt_rib_entry *rib)
{
  struct mrt_buffer *msg = &rt_msg->msg;

  mrt_write_to_msg_(msg, rib->peer_index);
  mrt_write_to_msg_(msg, rib->originated_time);
  mrt_write_to_msg_(msg, rib->attributes_length);
  mrt_write_to_msg(msg, rib->attributes, rib->attributes_length);

  mrt_rib_table_inc_entry_count(rt_msg);
  debug("\n");
}
