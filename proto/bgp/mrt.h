#ifndef _BIRD_BGP_MRT_H_
#define _BIRD_BGP_MRT_H_

#include "nest/route.h"
#include "nest/mrtdump.h"
#include "bgp.h"

struct mrt_fib {
  const struct fib *fib;
  struct fib_iterator fit;
  byte fit_state;
#define MRT_FIT_STATE_RUNNING	1
#define MRT_FIT_STATE_DONE 	2
};

/*
 * PEER INDEX TABLE
 */

struct mrt_fib_peer_index_table {
  struct mrt_fib fib;
  struct mrt_peer_index_table mrt_pit;
};

void bgp_mrt_peer_index_table_init(const struct bgp_proto *p, const struct rtable *table, struct mrt_fib_peer_index_table *state);
void bgp_mrt_peer_index_table_step(struct mrt_fib_peer_index_table *state);
void bgp_mrt_peer_index_table_dump(const struct bgp_proto *bgp, struct mrt_fib_peer_index_table *state);
void bgp_mrt_peer_index_table_free(struct mrt_fib_peer_index_table *state);


/*
 * RIB TABLE
 */

struct mrt_fib_rib_table {
  struct mrt_fib fib;
  struct mrt_rib_table mrt_rib_table;
  u16 peer_index;
};

void bgp_mrt_rib_table_init(const struct bgp_proto *p, const struct rtable *table, struct mrt_fib_rib_table *state, u32 sequence_number);
void bgp_mrt_rib_table_step(struct mrt_fib_rib_table *state);
void bgp_mrt_rib_table_dump(const struct bgp_proto *bgp, struct mrt_fib_rib_table *state);
void bgp_mrt_rib_table_free(struct mrt_fib_rib_table *state);

#endif /* _BIRD_BGP_MRT_H_ */
