#ifndef _BIRD_BGP_MRT_H_
#define _BIRD_BGP_MRT_H_

#include "nest/route.h"
#include "nest/mrtdump.h"
#include "bgp.h"

struct fib_mrt_peer_index_table {
  struct mrt_peer_index_table mrt_pit;
  const struct fib *fib;
  struct fib_iterator fit;
};

void bgp_mrt_peer_index_table_init(const struct bgp_proto *p, const struct rtable *table, struct fib_mrt_peer_index_table *state);
void bgp_mrt_peer_index_table_cont(struct fib_mrt_peer_index_table *state);
void bgp_mrt_peer_index_table_dump(const struct bgp_proto *bgp, const struct fib_mrt_peer_index_table *state);
void bgp_mrt_peer_index_table_free(struct fib_mrt_peer_index_table *state);

#endif /* _BIRD_BGP_MRT_H_ */
