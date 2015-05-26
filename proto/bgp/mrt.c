#include "nest/route.h"
#include "lib/socket.h"

#include "bgp.h"
#include "mrt.h"

void
bgp_mrt_peer_index_table_init(const struct bgp_proto *p, const struct rtable *table, struct fib_mrt_peer_index_table *state)
{
  u32 collector_bgp_id = p->local_id;
  const char *collector_name = p->cf->c.name;
  state->fib = &table->fib;

  mrt_peer_index_table_init(&state->mrt_pit, collector_bgp_id, collector_name);

  FIB_ITERATE_INIT(&state->fit, state->fib);
}

void
bgp_mrt_peer_index_table_cont(struct fib_mrt_peer_index_table *state)
{
  uint max_work_size = 4;
  uint super_max_work_size = 16;

  debug("0");

  FIB_ITERATE_START(state->fib, &state->fit, f)
    {
      debug("6");
      net *n = (net *) f;

      debug("n->n.prefix: %I/%d \n", n->n.prefix, n->n.pxlen);
      debug("n->routes->attrs->from: %I \n", n->routes->attrs->from);
      debug("n->routes->attrs->gw: %I \n", n->routes->attrs->gw);
      debug("n->routes->attrs->source: %d \n", n->routes->attrs->source);
      if (n->routes->attrs->hostentry != NULL)
      {
	debug("n->routes->attrs->hostentry->addr: %I \n", n->routes->attrs->hostentry->addr);
	debug("n->routes->attrs->hostentry->gw: %I \n", n->routes->attrs->hostentry->gw);
      }

      if (!super_max_work_size--)
      {
	debug("g");
	FIB_ITERATE_PUT(&state->fit, f);
	return;
      }

      if (n->routes->attrs->source != RTS_BGP)
	continue;

      debug("a");

      if (!max_work_size--)
      {
	debug("d");
	FIB_ITERATE_PUT(&state->fit, f);
	return;
      }

      debug("b");

      struct bgp_proto *bgp = (struct bgp_proto *)n->routes->attrs->src->proto;

      u32 peer_bgp_id = bgp->remote_id;
      u32 peer_as = bgp->remote_as;
      ip_addr *peer_ip_addr = &bgp->conn->sk->daddr;
      mrt_peer_index_table_add_peer(&state->mrt_pit, peer_bgp_id, peer_ip_addr, peer_as);

      debug("c");

    } FIB_ITERATE_END(f);

}

void
bgp_mrt_peer_index_table_dump(const struct bgp_proto *bgp, const struct fib_mrt_peer_index_table *state)
{
  byte *msg = state->mrt_pit.msg.msg;
  u32 msg_length = state->mrt_pit.msg.msg_length;

  mrt_dump_message((const struct proto*)bgp, TABLE_DUMP_V2, PEER_INDEX_TABLE, msg, msg_length);
}

void
bgp_mrt_peer_index_table_free(struct fib_mrt_peer_index_table *state)
{
  mrt_msg_free(&state->mrt_pit.msg);
}
