#include "nest/route.h"
#include "lib/socket.h"
#include "nest/iface.h"

#include "bgp.h"
#include "mrt.h"

void
bgp_mrt_peer_index_table_init(const struct bgp_proto *p, const struct rtable *table, struct fib_mrt_peer_index_table *state)
{
  u32 collector_bgp_id = p->local_id;
  const char *collector_name = p->cf->c.name;
  state->fib = &table->fib;

  debug("bgp_mrt_peer_index_table_init: \n");
  debug("  table name: '%s' (%d) \n", collector_name, strlen(collector_name));
  debug("  collector_bgp_id: %I \n", collector_bgp_id);
  mrt_peer_index_table_init(&state->mrt_pit, collector_bgp_id, collector_name);

#ifdef DEBUGGING
  fib_check(state->fib);
#endif
  FIB_ITERATE_INIT(&state->fit, state->fib);
  state->fit_state = MRT_PIT_STATE_RUNNING;
}

void
bgp_mrt_peer_index_table_step(struct fib_mrt_peer_index_table *state)
{
  uint max_work_size = 64;
  uint super_max_work_size = 128;

  if (state->fit_state != MRT_PIT_STATE_RUNNING && state->fit_state != MRT_PIT_STATE_DONE)
    bug("You have to call the bgp_mrt_peer_index_table_init() before the bgp_mrt_peer_index_table_step().");

  if (state->fit_state != MRT_PIT_STATE_RUNNING)
    return;

  FIB_ITERATE_START(state->fib, &state->fit, f)
  {
    net *n = (net *) f;

    if (!super_max_work_size--)
    {
      debug("!!! stopping bgp_mrt_peer_index_table_step at super_max_work_size !!! \n ");
      FIB_ITERATE_PUT(&state->fit, f);
      return;
    }

    rte *e;
    for(e = n->routes; e; e = e->next)
    {
      if (e->attrs->source == RTS_BGP && e->net->n.flags == 0)
      {
	if (!max_work_size--)
	{
	  debug("!!! stopping bgp_mrt_peer_index_table_step at max_work_size !!! \n \m");
	  FIB_ITERATE_PUT(&state->fit, f);
	  return;
	}

	struct bgp_proto *bgp = (struct bgp_proto *)e->attrs->src->proto;

	u32 peer_bgp_id = bgp->remote_id;
	u32 peer_as = bgp->remote_as;

	debug("\n");
	rte_dump(e);

	debug("n->n.prefix:    %I \n", n->n.prefix);
	debug("e->attrs->from: %I \n", e->attrs->from);
	debug("e->attrs->gw:   %I \n", e->attrs->gw);
	debug("e->attrs->dest: %d \n", e->attrs->dest);

	debug("e->attrs->aflags: %u  \n", e->attrs->aflags);
	debug("e->attrs->cast: %u  \n", e->attrs->cast);
	debug("e->attrs->dest: %u  \n", e->attrs->dest);
	debug("e->attrs->flags: %u  \n", e->attrs->flags);
	debug("e->attrs->hash_key: %hu \n", e->attrs->hash_key);
	debug("e->attrs->iface->addr->ip: %I  \n", e->attrs->iface->addr->ip);
	debug("e->attrs->iface->addr->opposite: %hu \n", e->attrs->iface->addr->opposite);


	ip_addr *peer_ip_addr = &e->attrs->from;
	debug("Add MRT: BGP-ID: %I, PEER-IP: %I, PEER-AS: %d \n", peer_bgp_id, *peer_ip_addr, peer_as);
	mrt_peer_index_table_add_peer(&state->mrt_pit, peer_bgp_id, peer_ip_addr, peer_as);
      }
    }
  } FIB_ITERATE_END(f);
  state->fit_state = MRT_PIT_STATE_DONE;
}

void
bgp_mrt_peer_index_table_dump(const struct bgp_proto *bgp, struct fib_mrt_peer_index_table *state)
{
  byte *msg = state->mrt_pit.msg.msg;
  u32 msg_length = state->mrt_pit.msg.msg_length;

  mrt_dump_message((const struct proto*)bgp, TABLE_DUMP_V2, PEER_INDEX_TABLE, msg, msg_length);
}

void
bgp_mrt_peer_index_table_free(struct fib_mrt_peer_index_table *state)
{
  mrt_msg_free(&state->mrt_pit.msg);
  fit_get(state->fib, &state->fit);
}
