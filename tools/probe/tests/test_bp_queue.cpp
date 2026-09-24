#include "bp_queue.h"
#include <cassert>
using namespace probe;
static BpHit mk(uint64_t seq){ BpHit h{}; h.seq=seq; return h; }
int main() {
    BpQueue q(2);
    assert(q.try_push(mk(1)));
    assert(q.try_push(mk(2)));
    assert(!q.try_push(mk(3)));        // full so it drops
    assert(q.dropped() == 1);
    BpHit out{};
    assert(q.try_pop(out) && out.seq == 1);
    assert(q.try_pop(out) && out.seq == 2);
    assert(!q.try_pop(out));           // empty
    return 0;
}
