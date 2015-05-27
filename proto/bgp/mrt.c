#include "nest/route.h"
#include "lib/socket.h"
#include "nest/iface.h"

#include "bgp.h"
#include "mrt.h"

void bgp_mrt_table_step(struct mrt_fib *state, void (*yeild)(void *, rte *))
{
  uint max_work_size = 64;
  uint super_max_work_size = 128;

  if (state->fit_state != MRT_FIT_STATE_RUNNING && state->fit_state != MRT_FIT_STATE_DONE)
    bug("You have to call the bgp_mrt_table_step() before the bgp_mrt_rib_table_step().");

  if (state->fit_state != MRT_FIT_STATE_RUNNING)
    return;

  FIB_ITERATE_START(state->fib, &state->fit, f)
  {
    net *n = (net *) f;

    if (!super_max_work_size--)
    {
      debug("!!! stopping bgp_mrt_table_step at super_max_work_size !!! \n ");
      FIB_ITERATE_PUT(&state->fit, f);
      return;
    }

    rte *e;
    for (e = n->routes; e; e = e->next)
    {
      if (e->attrs->source == RTS_BGP && e->net->n.flags == 0)
      {
        if (!max_work_size--)
        {
          debug("!!! stopping bgp_mrt_table_step at max_work_size !!! \n");
          FIB_ITERATE_PUT(&state->fit, f);
          return;
        }

        yeild((void *)state, e);
      }
    }
  } FIB_ITERATE_END(f);
  state->fit_state = MRT_FIT_STATE_DONE;
}

/*
 * PEER INDEX TABLE
 */

void
bgp_mrt_peer_index_table_init(const struct bgp_proto *p, const struct rtable *table, struct mrt_fib_peer_index_table *state)
{
  u32 collector_bgp_id = p->local_id;
  const char *collector_name = p->cf->c.name;
  state->fib.fib = &table->fib;

  debug("bgp_mrt_peer_index_table_init: \n");
  debug("  table name: '%s' (%d) \n", collector_name, strlen(collector_name));
  debug("  collector_bgp_id: %I \n", collector_bgp_id);
  mrt_peer_index_table_init(&state->mrt_pit, collector_bgp_id, collector_name);

#ifdef DEBUGGING
  fib_check(state->fib.fib);
#endif
  FIB_ITERATE_INIT(&state->fib.fit, state->fib.fib);
  state->fib.fit_state = MRT_FIT_STATE_RUNNING;
}

static void
bgp_mrt_peer_index_table_inner_step(void *state_void, rte *e)
{
  struct mrt_fib_peer_index_table *state = (struct mrt_fib_peer_index_table *) state_void;
  struct bgp_proto *bgp = (struct bgp_proto *)e->attrs->src->proto;

  u32 peer_bgp_id = bgp->remote_id;
  u32 peer_as = bgp->remote_as;

  debug("\n");
  rte_dump(e);

  ip_addr *peer_ip_addr = &e->attrs->from;
  debug("Add MRT: BGP-ID: %I, PEER-IP: %I, PEER-AS: %d \n", peer_bgp_id, *peer_ip_addr, peer_as);
  mrt_peer_index_table_add_peer(&state->mrt_pit, peer_bgp_id, peer_ip_addr, peer_as);
}

void
bgp_mrt_peer_index_table_step(struct mrt_fib_peer_index_table *state)
{
  bgp_mrt_table_step((struct mrt_fib*) state, bgp_mrt_peer_index_table_inner_step);
}

void
bgp_mrt_peer_index_table_dump(const struct bgp_proto *bgp, struct mrt_fib_peer_index_table *state)
{
  byte *msg = state->mrt_pit.msg.msg;
  u32 msg_length = state->mrt_pit.msg.msg_length;

  mrt_dump_message((const struct proto*)bgp, TABLE_DUMP_V2, PEER_INDEX_TABLE, msg, msg_length);
}

void
bgp_mrt_peer_index_table_free(struct mrt_fib_peer_index_table *state)
{
  mrt_msg_free(&state->mrt_pit.msg);
  fit_get(state->fib.fib, &state->fib.fit);
}

/*
 * RIB TABLE
 */

void
bgp_mrt_rib_table_init(const struct bgp_proto *p, const struct rtable *table, struct mrt_fib_rib_table *state, u32 sequence_number)
{
  debug("bgp_mrt_rib_table_init: \n");
  debug("  sequence_number: %u \n", sequence_number);

  u8 prefix_length = 0;
  ip_addr prefix = IPA_NONE;

  if (p->prefix_hash.data == NULL)
    debug("bgp_mrt_rib_table_init: p->prefix_hash.data == NULL \n");
  else
  {
    HASH_WALK(p->prefix_hash, next, n)
      {
      prefix_length = n->n.pxlen;
      prefix = n->n.prefix;

      debug("bgp_mrt_rib_table_init: %I / %u \n", prefix, prefix_length);
      }
    HASH_WALK_END;
  }
  debug("FINAL bgp_mrt_rib_table_init: %I / %u \n", prefix, prefix_length);

  mrt_rib_table_init(&state->mrt_rib_table, sequence_number, prefix_length, &prefix);

  state->peer_index = 0;
  state->fib.fib = &table->fib;
#ifdef DEBUGGING
  fib_check(state->fib.fib);
#endif
  FIB_ITERATE_INIT(&state->fib.fit, state->fib.fib);
  state->fib.fit_state = MRT_FIT_STATE_RUNNING;
}

static void
bgp_mrt_rib_table_inner_step(void *state_void, rte *e)
{
  struct mrt_fib_rib_table *state = (struct mrt_fib_rib_table *) state_void;
  struct bgp_proto *bgp = (struct bgp_proto *)e->attrs->src->proto;

  const u32 attributes_buffer_size = 2048;
  byte attributes_buffer[attributes_buffer_size];

  uint attributes_length = bgp_encode_attrs(bgp, attributes_buffer, e->attrs->eattrs, attributes_buffer_size);
  if (attributes_length == -1)
    bug("bgp_mrt_rib_table_inner_step: attributes_buffer_size %d is too small.");

  u32 time = now;
  if (e->lastmod != 0)
    time = e->lastmod;

  struct mrt_rib_entry mrt_re = {
      .peer_index = state->peer_index++,
      .originated_time = time,
      .attributes_length = attributes_length,
      .attributes = attributes_buffer
  };

  debug("\n");
  rte_dump(e);

  debug("Add MRT: PeerIndex: %hu, time: %u, attr_len: %hu, attr: ", mrt_re.peer_index, mrt_re.originated_time, mrt_re.attributes_length);
  u32 i;
  for (i = 0; i < mrt_re.attributes_length; i++)
    debug("%02X ", mrt_re.attributes[i]);
  debug("\n");
  mrt_rib_table_add_entry(&state->mrt_rib_table, &mrt_re);
}

void
bgp_mrt_rib_table_step(struct mrt_fib_rib_table *state)
{
  bgp_mrt_table_step((struct mrt_fib*) state, bgp_mrt_rib_table_inner_step);
}

void
bgp_mrt_rib_table_dump(const struct bgp_proto *bgp, struct mrt_fib_rib_table *state)
{
  byte *msg = state->mrt_rib_table.msg.msg;
  u32 msg_length = state->mrt_rib_table.msg.msg_length;
#ifdef IPV6
  mrt_dump_message((const struct proto*)bgp, TABLE_DUMP_V2, RIB_IPV6_UNICAST, msg, msg_length);
#else
  mrt_dump_message((const struct proto*)bgp, TABLE_DUMP_V2, RIB_IPV4_UNICAST, msg, msg_length);
#endif
}

void
bgp_mrt_rib_table_free(struct mrt_fib_rib_table *state)
{
  mrt_msg_free(&state->mrt_rib_table.msg);
  fit_get(state->fib.fib, &state->fib.fit);
}
