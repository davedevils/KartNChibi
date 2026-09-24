#include "capture.h"
#include <cassert>
#include <cstdint>

using namespace probe;

int main() {
    // fake stack element 0 is retaddr 1 is arg0 2 is a pointer to a struct
    uint32_t target_struct[3] = {0xAABBCCDD, 0x11223344, 0};
    uint32_t stack[3];
    stack[0] = 0xDEADBEEF;
    stack[1] = 0x00000063;
    stack[2] = reinterpret_cast<uint32_t>(target_struct);

    HookContext ctx{};
    ctx.eax = 7; ctx.ecx = 0; ctx.edx = 0;
    ctx.stack = stack;

    // val mode takes the low 2 bytes of arg0 which is 99
    CaptureDesc v; v.label = "opcode"; v.source = SourceKind::Arg; v.source_index = 0;
    v.mode = CaptureMode::Val; v.size = 2;
    CaptureBlob b{};
    assert(resolve_capture(v, ctx, 512, b));
    assert(b.size == 2);
    assert(b.data[0] == 0x63 && b.data[1] == 0x00);

    // ptr mode with no deref dumps 8 bytes at arg1
    CaptureDesc p; p.label = "struct"; p.source = SourceKind::Arg; p.source_index = 1;
    p.mode = CaptureMode::Ptr; p.size = 8;
    CaptureBlob pb{};
    assert(resolve_capture(p, ctx, 512, pb));
    assert(pb.size == 8);
    assert(*reinterpret_cast<uint32_t*>(pb.data) == 0xAABBCCDD);
    assert(*reinterpret_cast<uint32_t*>(pb.data + 4) == 0x11223344);

    // ptr mode with one deref reads 4 bytes at the address arg1 points to
    uint32_t inner = 0xCAFEF00D;
    uint32_t holder[1] = {reinterpret_cast<uint32_t>(&inner)};
    uint32_t stack2[2] = {0, reinterpret_cast<uint32_t>(holder)};
    HookContext ctx2{}; ctx2.stack = stack2;
    CaptureDesc d; d.source = SourceKind::Arg; d.source_index = 0; d.mode = CaptureMode::Ptr;
    d.deref = {0}; d.size = 4;
    CaptureBlob db{};
    assert(resolve_capture(d, ctx2, 512, db));
    assert(db.size == 4);
    assert(*reinterpret_cast<uint32_t*>(db.data) == 0xCAFEF00D);

    // faulting pointer empties the blob and returns false with no crash
    CaptureDesc f; f.source = SourceKind::Eax; f.mode = CaptureMode::Ptr; f.size = 16;
    HookContext bad{}; bad.eax = 0; bad.stack = stack;
    CaptureBlob fb{};
    assert(!resolve_capture(f, bad, 512, fb));
    assert(fb.size == 0);

    // a deref hop that faults on a bad address empties the blob
    CaptureDesc df; df.source = SourceKind::Eax; df.mode = CaptureMode::Ptr;
    df.deref = {0}; df.size = 8;
    HookContext bad_deref{}; bad_deref.eax = 0xDEAD0000; bad_deref.stack = stack;
    CaptureBlob dfb{};
    assert(!resolve_capture(df, bad_deref, 512, dfb));
    assert(dfb.size == 0);

    // size clamps to max bytes
    CaptureDesc c; c.source = SourceKind::Arg; c.source_index = 1; c.mode = CaptureMode::Ptr; c.size = 8;
    CaptureBlob cb{};
    assert(resolve_capture(c, ctx, 4, cb));
    assert(cb.size == 4);
    assert(*reinterpret_cast<uint32_t*>(cb.data) == 0xAABBCCDD);

    // abs source is the literal address ptr mode dumps memory there
    uint32_t abs_struct[2] = {0x1234ABCD, 0x77665544};
    CaptureDesc ad; ad.label = "abs"; ad.source = SourceKind::Abs;
    ad.source_index = reinterpret_cast<uint32_t>(abs_struct);
    ad.mode = CaptureMode::Ptr; ad.size = 8;
    HookContext nctx{};                               // abs ignores ctx
    CaptureBlob ab{};
    assert(resolve_capture(ad, nctx, 512, ab));
    assert(ab.size == 8);
    assert(*reinterpret_cast<uint32_t*>(ab.data) == 0x1234ABCD);
    assert(*reinterpret_cast<uint32_t*>(ab.data + 4) == 0x77665544);

    // abs plus one deref source index points at a pointer to inner
    uint32_t abs_inner = 0xFEEDFACE;
    uint32_t abs_holder = reinterpret_cast<uint32_t>(&abs_inner);
    CaptureDesc ad2; ad2.source = SourceKind::Abs;
    ad2.source_index = reinterpret_cast<uint32_t>(&abs_holder);
    ad2.mode = CaptureMode::Ptr; ad2.deref = {0}; ad2.size = 4;
    CaptureBlob ab2{};
    assert(resolve_capture(ad2, nctx, 512, ab2));
    assert(*reinterpret_cast<uint32_t*>(ab2.data) == 0xFEEDFACE);

    return 0;
}
