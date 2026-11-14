// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

/* This file is part of the dynarmic project.
 * Copyright (c) 2022 MerryMage
 * SPDX-License-Identifier: 0BSD
 */

#include <array>
#include <initializer_list>
#include <tuple>
#include <utility>

#include <fmt/format.h>
#include <fmt/ostream.h>
#include "dynarmic/mcl/integer_of_size.hpp"
#include "dynarmic/backend/x64/xbyak.h"

#include "dynarmic/backend/x64/a64_emit_x64.h"
#include "dynarmic/backend/x64/abi.h"
#include "dynarmic/backend/x64/devirtualize.h"
#include "dynarmic/backend/x64/emit_x64_memory.h"
#include "dynarmic/backend/x64/exclusive_monitor_friend.h"
#include "dynarmic/backend/x64/perf_map.h"
#include "dynarmic/common/spin_lock_x64.h"
#include "dynarmic/interface/exclusive_monitor.h"

namespace Dynarmic::Backend::X64 {

using namespace Xbyak::util;

void A64EmitX64::GenMemory128Accessors() {
    code.align();
    memory_read_128 = code.getCurr<void (*)()>();
#ifdef _WIN32
    Devirtualize<&A64::UserCallbacks::MemoryRead128>(conf.callbacks).EmitCallWithReturnPointer(code, [&](Xbyak::Reg64 return_value_ptr, [[maybe_unused]] RegList args) {
        code.mov(code.ABI_PARAM3, code.ABI_PARAM2);
        code.lea(rsp, ptr[rsp - (8 + 16 + ABI_SHADOW_SPACE)]);
        code.lea(return_value_ptr, ptr[rsp + ABI_SHADOW_SPACE]);
    });
    code.movups(xmm1, xword[code.ABI_RETURN]);
    code.add(rsp, 8 + 16 + ABI_SHADOW_SPACE);
#else
    code.lea(rsp, ptr[rsp - 8]);
    Devirtualize<&A64::UserCallbacks::MemoryRead128>(conf.callbacks).EmitCall(code);
    if (code.HasHostFeature(HostFeature::SSE41)) {
        code.movq(xmm1, code.ABI_RETURN);
        code.pinsrq(xmm1, code.ABI_RETURN2, 1);
    } else {
        code.movq(xmm1, code.ABI_RETURN);
        code.movq(xmm2, code.ABI_RETURN2);
        code.punpcklqdq(xmm1, xmm2);
    }
    code.add(rsp, 8);
#endif
    code.ret();
    PerfMapRegister(memory_read_128, code.getCurr(), "a64_memory_read_128");

    code.align();
    memory_write_128 = code.getCurr<void (*)()>();
#ifdef _WIN32
    code.lea(rsp, ptr[rsp - (8 + 16 + ABI_SHADOW_SPACE)]);
    code.lea(code.ABI_PARAM3, ptr[rsp + ABI_SHADOW_SPACE]);
    code.movaps(xword[code.ABI_PARAM3], xmm1);
    Devirtualize<&A64::UserCallbacks::MemoryWrite128>(conf.callbacks).EmitCall(code);
    code.add(rsp, 8 + 16 + ABI_SHADOW_SPACE);
#else
    code.lea(rsp, ptr[rsp - 8]);
    if (code.HasHostFeature(HostFeature::SSE41)) {
        code.movq(code.ABI_PARAM3, xmm1);
        code.pextrq(code.ABI_PARAM4, xmm1, 1);
    } else {
        code.movq(code.ABI_PARAM3, xmm1);
        code.punpckhqdq(xmm1, xmm1);
        code.movq(code.ABI_PARAM4, xmm1);
    }
    Devirtualize<&A64::UserCallbacks::MemoryWrite128>(conf.callbacks).EmitCall(code);
    code.add(rsp, 8);
#endif
    code.ret();
    PerfMapRegister(memory_write_128, code.getCurr(), "a64_memory_write_128");

    code.align();
    memory_exclusive_write_128 = code.getCurr<void (*)()>();
#ifdef _WIN32
    code.lea(rsp, ptr[rsp - (8 + 32 + ABI_SHADOW_SPACE)]);
    code.lea(code.ABI_PARAM3, ptr[rsp + ABI_SHADOW_SPACE]);
    code.lea(code.ABI_PARAM4, ptr[rsp + ABI_SHADOW_SPACE + 16]);
    code.movaps(xword[code.ABI_PARAM3], xmm1);
    code.movaps(xword[code.ABI_PARAM4], xmm2);
    Devirtualize<&A64::UserCallbacks::MemoryWriteExclusive128>(conf.callbacks).EmitCall(code);
    code.add(rsp, 8 + 32 + ABI_SHADOW_SPACE);
#else
    code.lea(rsp, ptr[rsp - 8]);
    if (code.HasHostFeature(HostFeature::SSE41)) {
        code.movq(code.ABI_PARAM3, xmm1);
        code.pextrq(code.ABI_PARAM4, xmm1, 1);
        code.movq(code.ABI_PARAM5, xmm2);
        code.pextrq(code.ABI_PARAM6, xmm2, 1);
    } else {
        code.movq(code.ABI_PARAM3, xmm1);
        code.punpckhqdq(xmm1, xmm1);
        code.movq(code.ABI_PARAM4, xmm1);
        code.movq(code.ABI_PARAM5, xmm2);
        code.punpckhqdq(xmm2, xmm2);
        code.movq(code.ABI_PARAM6, xmm2);
    }
    Devirtualize<&A64::UserCallbacks::MemoryWriteExclusive128>(conf.callbacks).EmitCall(code);
    code.add(rsp, 8);
#endif
    code.ret();
    PerfMapRegister(memory_exclusive_write_128, code.getCurr(), "a64_memory_exclusive_write_128");
}

void A64EmitX64::GenFastmemFallbacks() {
    const std::initializer_list<int> idxes{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
    const std::array<std::pair<size_t, ArgCallback>, 4> read_callbacks{{
        {8, Devirtualize<&A64::UserCallbacks::MemoryRead>(conf.callbacks)},
        {16, Devirtualize<&A64::UserCallbacks::MemoryRead>(conf.callbacks)},
        {32, Devirtualize<&A64::UserCallbacks::MemoryRead>(conf.callbacks)},
        {64, Devirtualize<&A64::UserCallbacks::MemoryRead>(conf.callbacks)},
    }};
    const std::array<std::pair<size_t, ArgCallback>, 4> write_callbacks{{
        {8, Devirtualize<&A64::UserCallbacks::MemoryWrite>(conf.callbacks)},
        {16, Devirtualize<&A64::UserCallbacks::MemoryWrite>(conf.callbacks)},
        {32, Devirtualize<&A64::UserCallbacks::MemoryWrite>(conf.callbacks)},
        {64, Devirtualize<&A64::UserCallbacks::MemoryWrite>(conf.callbacks)},
    }};
    const std::array<std::pair<size_t, ArgCallback>, 4> exclusive_write_callbacks{{
        {8, Devirtualize<&A64::UserCallbacks::MemoryWriteExclusive8>(conf.callbacks)},
        {16, Devirtualize<&A64::UserCallbacks::MemoryWriteExclusive16>(conf.callbacks)},
        {32, Devirtualize<&A64::UserCallbacks::MemoryWriteExclusive32>(conf.callbacks)},
        {64, Devirtualize<&A64::UserCallbacks::MemoryWriteExclusive64>(conf.callbacks)},
    }};

    for (auto const ordered : {false, true}) {
        for (auto const vaddr_idx : idxes) {
            if (vaddr_idx == 4 || vaddr_idx == 15) {
                continue;
            }

            for (int value_idx : idxes) {
                code.align();
                read_fallbacks[std::make_tuple(ordered, 128, vaddr_idx, value_idx)] = code.getCurr<void (*)()>();
                ABI_PushCallerSaveRegistersAndAdjustStackExcept(code, HostLocXmmIdx(value_idx));
                if (vaddr_idx != code.ABI_PARAM2.getIdx()) {
                    code.mov(code.ABI_PARAM2, Xbyak::Reg64{vaddr_idx});
                }
                if (ordered) {
                    code.mfence();
                }
                code.call(memory_read_128);
                if (value_idx != 1) {
                    code.movaps(Xbyak::Xmm{value_idx}, xmm1);
                }
                ABI_PopCallerSaveRegistersAndAdjustStackExcept(code, HostLocXmmIdx(value_idx));
                code.ret();
                PerfMapRegister(read_fallbacks[std::make_tuple(ordered, 128, vaddr_idx, value_idx)], code.getCurr(), "a64_read_fallback_128");

                code.align();
                write_fallbacks[std::make_tuple(ordered, 128, vaddr_idx, value_idx)] = code.getCurr<void (*)()>();
                ABI_PushCallerSaveRegistersAndAdjustStack(code);
                if (vaddr_idx != code.ABI_PARAM2.getIdx()) {
                    code.mov(code.ABI_PARAM2, Xbyak::Reg64{vaddr_idx});
                }
                if (value_idx != 1) {
                    code.movaps(xmm1, Xbyak::Xmm{value_idx});
                }
                code.call(memory_write_128);
                if (ordered) {
                    code.mfence();
                }
                ABI_PopCallerSaveRegistersAndAdjustStack(code);
                code.ret();
                PerfMapRegister(write_fallbacks[std::make_tuple(ordered, 128, vaddr_idx, value_idx)], code.getCurr(), "a64_write_fallback_128");

                code.align();
                exclusive_write_fallbacks[std::make_tuple(ordered, 128, vaddr_idx, value_idx)] = code.getCurr<void (*)()>();
                ABI_PushCallerSaveRegistersAndAdjustStackExcept(code, HostLoc::RAX);
                if (value_idx != 1) {
                    code.movaps(xmm1, Xbyak::Xmm{value_idx});
                }
                if (code.HasHostFeature(HostFeature::SSE41)) {
                    code.movq(xmm2, rax);
                    code.pinsrq(xmm2, rdx, 1);
                } else {
                    code.movq(xmm2, rax);
                    code.movq(xmm0, rdx);
                    code.punpcklqdq(xmm2, xmm0);
                }
                if (vaddr_idx != code.ABI_PARAM2.getIdx()) {
                    code.mov(code.ABI_PARAM2, Xbyak::Reg64{vaddr_idx});
                }
                code.call(memory_exclusive_write_128);
                ABI_PopCallerSaveRegistersAndAdjustStackExcept(code, HostLoc::RAX);
                code.ret();
                PerfMapRegister(exclusive_write_fallbacks[std::make_tuple(ordered, 128, vaddr_idx, value_idx)], code.getCurr(), "a64_exclusive_write_fallback_128");

                if (value_idx == 4 || value_idx == 15) {
                    continue;
                }

                for (const auto& [bitsize, callback] : read_callbacks) {
                    code.align();
                    read_fallbacks[std::make_tuple(ordered, bitsize, vaddr_idx, value_idx)] = code.getCurr<void (*)()>();
                    ABI_PushCallerSaveRegistersAndAdjustStackExcept(code, HostLocRegIdx(value_idx));
                    // params = { this, vaddr, size }
                    if (vaddr_idx != code.ABI_PARAM2.getIdx()) {
                        code.mov(code.ABI_PARAM2, Xbyak::Reg64{vaddr_idx});
                    }
                    code.mov(code.ABI_PARAM3, bitsize / CHAR_BIT);
                    if (ordered) code.mfence();
                    callback.EmitCall(code);
                    if (value_idx != code.ABI_RETURN.getIdx()) {
                        code.mov(Xbyak::Reg64{value_idx}, code.ABI_RETURN);
                    }
                    ABI_PopCallerSaveRegistersAndAdjustStackExcept(code, HostLocRegIdx(value_idx));
                    code.ZeroExtendFrom(bitsize, Xbyak::Reg64{value_idx});
                    code.ret();
                    PerfMapRegister(read_fallbacks[std::make_tuple(ordered, bitsize, vaddr_idx, value_idx)], code.getCurr(), fmt::format("a64_read_fallback_{}", bitsize));
                }

                for (const auto& [bitsize, callback] : write_callbacks) {
                    code.align();
                    write_fallbacks[std::make_tuple(ordered, bitsize, vaddr_idx, value_idx)] = code.getCurr<void (*)()>();
                    ABI_PushCallerSaveRegistersAndAdjustStack(code);
                    // params = { this, vaddr, value, size }
                    if (vaddr_idx == code.ABI_PARAM3.getIdx() && value_idx == code.ABI_PARAM2.getIdx()) {
                        code.xchg(code.ABI_PARAM2, code.ABI_PARAM3);
                    } else if (vaddr_idx == code.ABI_PARAM3.getIdx()) {
                        code.mov(code.ABI_PARAM2, Xbyak::Reg64{vaddr_idx});
                        if (value_idx != code.ABI_PARAM3.getIdx()) {
                            code.mov(code.ABI_PARAM3, Xbyak::Reg64{value_idx});
                        }
                    } else {
                        if (value_idx != code.ABI_PARAM3.getIdx()) {
                            code.mov(code.ABI_PARAM3, Xbyak::Reg64{value_idx});
                        }
                        if (vaddr_idx != code.ABI_PARAM2.getIdx()) {
                            code.mov(code.ABI_PARAM2, Xbyak::Reg64{vaddr_idx});
                        }
                    }
                    code.ZeroExtendFrom(bitsize, code.ABI_PARAM3);
                    code.mov(code.ABI_PARAM4, bitsize / CHAR_BIT);
                    callback.EmitCall(code);
                    if (ordered) code.mfence();
                    ABI_PopCallerSaveRegistersAndAdjustStack(code);
                    code.ret();
                    PerfMapRegister(write_fallbacks[std::make_tuple(ordered, bitsize, vaddr_idx, value_idx)], code.getCurr(), fmt::format("a64_write_fallback_{}", bitsize));
                }

                for (const auto& [bitsize, callback] : exclusive_write_callbacks) {
                    code.align();
                    exclusive_write_fallbacks[std::make_tuple(ordered, bitsize, vaddr_idx, value_idx)] = code.getCurr<void (*)()>();
                    ABI_PushCallerSaveRegistersAndAdjustStackExcept(code, HostLoc::RAX);
                    if (vaddr_idx == code.ABI_PARAM3.getIdx() && value_idx == code.ABI_PARAM2.getIdx()) {
                        code.xchg(code.ABI_PARAM2, code.ABI_PARAM3);
                    } else if (vaddr_idx == code.ABI_PARAM3.getIdx()) {
                        code.mov(code.ABI_PARAM2, Xbyak::Reg64{vaddr_idx});
                        if (value_idx != code.ABI_PARAM3.getIdx()) {
                            code.mov(code.ABI_PARAM3, Xbyak::Reg64{value_idx});
                        }
                    } else {
                        if (value_idx != code.ABI_PARAM3.getIdx()) {
                            code.mov(code.ABI_PARAM3, Xbyak::Reg64{value_idx});
                        }
                        if (vaddr_idx != code.ABI_PARAM2.getIdx()) {
                            code.mov(code.ABI_PARAM2, Xbyak::Reg64{vaddr_idx});
                        }
                    }
                    code.ZeroExtendFrom(bitsize, code.ABI_PARAM3);
                    code.mov(code.ABI_PARAM4, rax);
                    code.ZeroExtendFrom(bitsize, code.ABI_PARAM4);
                    callback.EmitCall(code);
                    ABI_PopCallerSaveRegistersAndAdjustStackExcept(code, HostLoc::RAX);
                    code.ret();
                    PerfMapRegister(exclusive_write_fallbacks[std::make_tuple(ordered, bitsize, vaddr_idx, value_idx)], code.getCurr(), fmt::format("a64_exclusive_write_fallback_{}", bitsize));
                }
            }
        }
    }
}

#define Axx A64
#include "dynarmic/backend/x64/emit_x64_memory.cpp.inc"
#undef Axx

void A64EmitX64::EmitA64ReadMemory8(A64EmitContext& ctx, IR::Inst* inst) {
    EmitMemoryRead<8, &A64::UserCallbacks::MemoryRead>(ctx, inst);
}

void A64EmitX64::EmitA64ReadMemory16(A64EmitContext& ctx, IR::Inst* inst) {
    EmitMemoryRead<16, &A64::UserCallbacks::MemoryRead>(ctx, inst);
}

void A64EmitX64::EmitA64ReadMemory32(A64EmitContext& ctx, IR::Inst* inst) {
    EmitMemoryRead<32, &A64::UserCallbacks::MemoryRead>(ctx, inst);
}

void A64EmitX64::EmitA64ReadMemory64(A64EmitContext& ctx, IR::Inst* inst) {
    EmitMemoryRead<64, &A64::UserCallbacks::MemoryRead>(ctx, inst);
}

void A64EmitX64::EmitA64ReadMemory128(A64EmitContext& ctx, IR::Inst* inst) {
    EmitMemoryRead<128, &A64::UserCallbacks::MemoryRead128>(ctx, inst);
}

void A64EmitX64::EmitA64WriteMemory8(A64EmitContext& ctx, IR::Inst* inst) {
    EmitMemoryWrite<8, &A64::UserCallbacks::MemoryWrite>(ctx, inst);
}

void A64EmitX64::EmitA64WriteMemory16(A64EmitContext& ctx, IR::Inst* inst) {
    EmitMemoryWrite<16, &A64::UserCallbacks::MemoryWrite>(ctx, inst);
}

void A64EmitX64::EmitA64WriteMemory32(A64EmitContext& ctx, IR::Inst* inst) {
    EmitMemoryWrite<32, &A64::UserCallbacks::MemoryWrite>(ctx, inst);
}

void A64EmitX64::EmitA64WriteMemory64(A64EmitContext& ctx, IR::Inst* inst) {
    EmitMemoryWrite<64, &A64::UserCallbacks::MemoryWrite>(ctx, inst);
}

void A64EmitX64::EmitA64WriteMemory128(A64EmitContext& ctx, IR::Inst* inst) {
    EmitMemoryWrite<128, &A64::UserCallbacks::MemoryWrite>(ctx, inst);
}

void A64EmitX64::EmitA64ClearExclusive(A64EmitContext&, IR::Inst*) {
    code.mov(code.byte[code.ABI_JIT_PTR + offsetof(A64JitState, exclusive_state)], u8(0));
}

void A64EmitX64::EmitA64ExclusiveReadMemory8(A64EmitContext& ctx, IR::Inst* inst) {
    if (conf.fastmem_exclusive_access) {
        EmitExclusiveReadMemoryInline<8, &A64::UserCallbacks::MemoryRead>(ctx, inst);
    } else {
        EmitExclusiveReadMemory<8, &A64::UserCallbacks::MemoryRead>(ctx, inst);
    }
}

void A64EmitX64::EmitA64ExclusiveReadMemory16(A64EmitContext& ctx, IR::Inst* inst) {
    if (conf.fastmem_exclusive_access) {
        EmitExclusiveReadMemoryInline<16, &A64::UserCallbacks::MemoryRead>(ctx, inst);
    } else {
        EmitExclusiveReadMemory<16, &A64::UserCallbacks::MemoryRead>(ctx, inst);
    }
}

void A64EmitX64::EmitA64ExclusiveReadMemory32(A64EmitContext& ctx, IR::Inst* inst) {
    if (conf.fastmem_exclusive_access) {
        EmitExclusiveReadMemoryInline<32, &A64::UserCallbacks::MemoryRead>(ctx, inst);
    } else {
        EmitExclusiveReadMemory<32, &A64::UserCallbacks::MemoryRead>(ctx, inst);
    }
}

void A64EmitX64::EmitA64ExclusiveReadMemory64(A64EmitContext& ctx, IR::Inst* inst) {
    if (conf.fastmem_exclusive_access) {
        EmitExclusiveReadMemoryInline<64, &A64::UserCallbacks::MemoryRead>(ctx, inst);
    } else {
        EmitExclusiveReadMemory<64, &A64::UserCallbacks::MemoryRead>(ctx, inst);
    }
}

void A64EmitX64::EmitA64ExclusiveReadMemory128(A64EmitContext& ctx, IR::Inst* inst) {
    if (conf.fastmem_exclusive_access) {
        EmitExclusiveReadMemoryInline<128, &A64::UserCallbacks::MemoryRead128>(ctx, inst);
    } else {
        EmitExclusiveReadMemory<128, &A64::UserCallbacks::MemoryRead128>(ctx, inst);
    }
}

void A64EmitX64::EmitA64ExclusiveWriteMemory8(A64EmitContext& ctx, IR::Inst* inst) {
    if (conf.fastmem_exclusive_access) {
        EmitExclusiveWriteMemoryInline<8, &A64::UserCallbacks::MemoryWriteExclusive8>(ctx, inst);
    } else {
        EmitExclusiveWriteMemory<8, &A64::UserCallbacks::MemoryWriteExclusive8>(ctx, inst);
    }
}

void A64EmitX64::EmitA64ExclusiveWriteMemory16(A64EmitContext& ctx, IR::Inst* inst) {
    if (conf.fastmem_exclusive_access) {
        EmitExclusiveWriteMemoryInline<16, &A64::UserCallbacks::MemoryWriteExclusive16>(ctx, inst);
    } else {
        EmitExclusiveWriteMemory<16, &A64::UserCallbacks::MemoryWriteExclusive16>(ctx, inst);
    }
}

void A64EmitX64::EmitA64ExclusiveWriteMemory32(A64EmitContext& ctx, IR::Inst* inst) {
    if (conf.fastmem_exclusive_access) {
        EmitExclusiveWriteMemoryInline<32, &A64::UserCallbacks::MemoryWriteExclusive32>(ctx, inst);
    } else {
        EmitExclusiveWriteMemory<32, &A64::UserCallbacks::MemoryWriteExclusive32>(ctx, inst);
    }
}

void A64EmitX64::EmitA64ExclusiveWriteMemory64(A64EmitContext& ctx, IR::Inst* inst) {
    if (conf.fastmem_exclusive_access) {
        EmitExclusiveWriteMemoryInline<64, &A64::UserCallbacks::MemoryWriteExclusive64>(ctx, inst);
    } else {
        EmitExclusiveWriteMemory<64, &A64::UserCallbacks::MemoryWriteExclusive64>(ctx, inst);
    }
}

void A64EmitX64::EmitA64ExclusiveWriteMemory128(A64EmitContext& ctx, IR::Inst* inst) {
    if (conf.fastmem_exclusive_access) {
        EmitExclusiveWriteMemoryInline<128, &A64::UserCallbacks::MemoryWriteExclusive128>(ctx, inst);
    } else {
        EmitExclusiveWriteMemory<128, &A64::UserCallbacks::MemoryWriteExclusive128>(ctx, inst);
    }
}

template<size_t operation, size_t bitsize>
void A64EmitX64::EmitLseAtomic(A64EmitContext& ctx, IR::Inst* inst) {
    ASSERT(UseLseHostAtomics());
    auto args = ctx.reg_alloc.GetArgumentInfo(inst);
    ctx.reg_alloc.ScratchGpr(code, HostLoc::RAX);
    const auto address = ctx.reg_alloc.UseGpr(code, args[1]);
    const auto source = ctx.reg_alloc.UseGpr(code, args[2]);
    const auto pointer_tmp = ctx.reg_alloc.ScratchGpr(code);
    const auto abort = ctx.GenSharedLabel();
    const auto narrow = [](Xbyak::Reg64 reg) {
        if constexpr (bitsize == 8) {
            return reg.cvt8();
        } else if constexpr (bitsize == 16) {
            return reg.cvt16();
        } else if constexpr (bitsize == 32) {
            return reg.cvt32();
        } else {
            return reg;
        }
    };

    if constexpr (bitsize > 8) {
        code.test(address, u32(bitsize / 8 - 1));
        code.jnz(*abort, code.T_NEAR);
    }
    bool require_abort_handling = false;
    const auto pointer = EmitFastmemVAddr(code, ctx, *abort, address,
                                        require_abort_handling, pointer_tmp);
    const auto memory = [&] {
        if constexpr (bitsize == 8) {
            return code.byte[pointer];
        } else if constexpr (bitsize == 16) {
            return code.word[pointer];
        } else if constexpr (bitsize == 32) {
            return code.dword[pointer];
        } else {
            return code.qword[pointer];
        }
    }();
    const DoNotFastmemMarker marker{ctx.Location(), inst->GetName()};
    const void* atomic_location;
    const void* load_location = nullptr;

    if constexpr (operation == 0 || operation == 8) {
        code.mov(rax, source);
        atomic_location = code.getCurr();
        if constexpr (operation == 0) {
            code.lock();
            code.xadd(memory, narrow(rax));
        } else {
            code.xchg(memory, narrow(rax));
        }
    } else if constexpr (operation == 9) {
        const auto desired = ctx.reg_alloc.UseGpr(code, args[3]);
        code.mov(rax, source);
        atomic_location = code.getCurr();
        code.lock();
        code.cmpxchg(memory, narrow(desired));
    } else {
        const auto candidate = ctx.reg_alloc.ScratchGpr(code);
        load_location = EmitReadMemoryMov<bitsize>(code, rax.getIdx(), pointer, false);
        Xbyak::Label retry;
        code.L(retry);
        if constexpr (operation != 1) {
            code.mov(candidate, rax);
        }
        if constexpr (operation == 1) {
            code.mov(candidate, source);
            code.not_(candidate);
            code.and_(candidate, rax);
        } else if constexpr (operation == 2) {
            code.xor_(candidate, source);
        } else if constexpr (operation == 3) {
            code.or_(candidate, source);
        } else {
            Xbyak::Label keep;
            code.cmp(narrow(rax), narrow(source));
            if constexpr (operation == 4) {
                code.jge(keep);
            } else if constexpr (operation == 5) {
                code.jle(keep);
            } else if constexpr (operation == 6) {
                code.jae(keep);
            } else {
                static_assert(operation == 7);
                code.jbe(keep);
            }
            code.mov(candidate, source);
            code.L(keep);
        }
        // Even a no-op must perform the write access and the locked barrier.
        atomic_location = code.getCurr();
        code.lock();
        code.cmpxchg(memory, narrow(candidate));
        code.jne(retry);
    }
    code.ZeroExtendFrom(bitsize, rax);
    ctx.reg_alloc.DefineValue(code, inst, rax);

    const A64::LocationDescriptor location{IR::LocationDescriptor{inst->GetArg(0).GetU64()}};
    const u64 prior_cycles = (location.PC() - ctx.Location().PC()) / 4;
    ColdEmits(code, ctx).emplace_back([=, this] {
        code.L(*abort);
        // No atomic completed. Re-enter the original translation at this PC after
        // a range invalidation, with host LSE disabled only at this guest PC.
        code.mov(rax, location.PC());
        code.mov(qword[code.ABI_JIT_PTR + offsetof(A64JitState, pc)], rax);
        if (conf.enable_cycle_counting && prior_cycles != 0) {
            EmitAddCycles(prior_cycles);
        }
        code.mov(code.ABI_PARAM1, reinterpret_cast<u64>(this));
        code.mov(code.ABI_PARAM2, location.PC());
        code.CallLambda([](A64EmitX64* emitter, u64 pc) {
            emitter->lse_disabled_pcs.insert_or_assign(pc, true);
            emitter->jit_interface->InvalidateCacheRange(pc, 4);
        });
        code.ForceReturnFromRunCode();

        // The exception handler synthesizes a call. Return immediately into the
        // same deoptimization exit used by the explicit alignment/range guards.
        const auto trampoline = code.getCurr<void (*)()>();
        code.ret();
        AddFastmemPatch(atomic_location, abort->getAddress(), trampoline, marker, false);
        if (load_location) {
            AddFastmemPatch(load_location, abort->getAddress(), trampoline, marker, false);
        }
    });
}

void A64EmitX64::EmitA64LseAdd8(A64EmitContext& ctx, IR::Inst* inst) {
    EmitLseAtomic<0, 8>(ctx, inst);
}
void A64EmitX64::EmitA64LseAdd16(A64EmitContext& ctx, IR::Inst* inst) {
    EmitLseAtomic<0, 16>(ctx, inst);
}
void A64EmitX64::EmitA64LseAdd32(A64EmitContext& ctx, IR::Inst* inst) {
    EmitLseAtomic<0, 32>(ctx, inst);
}
void A64EmitX64::EmitA64LseAdd64(A64EmitContext& ctx, IR::Inst* inst) {
    EmitLseAtomic<0, 64>(ctx, inst);
}
void A64EmitX64::EmitA64LseClr8(A64EmitContext& ctx, IR::Inst* inst) {
    EmitLseAtomic<1, 8>(ctx, inst);
}
void A64EmitX64::EmitA64LseClr16(A64EmitContext& ctx, IR::Inst* inst) {
    EmitLseAtomic<1, 16>(ctx, inst);
}
void A64EmitX64::EmitA64LseClr32(A64EmitContext& ctx, IR::Inst* inst) {
    EmitLseAtomic<1, 32>(ctx, inst);
}
void A64EmitX64::EmitA64LseClr64(A64EmitContext& ctx, IR::Inst* inst) {
    EmitLseAtomic<1, 64>(ctx, inst);
}
void A64EmitX64::EmitA64LseEor8(A64EmitContext& ctx, IR::Inst* inst) {
    EmitLseAtomic<2, 8>(ctx, inst);
}
void A64EmitX64::EmitA64LseEor16(A64EmitContext& ctx, IR::Inst* inst) {
    EmitLseAtomic<2, 16>(ctx, inst);
}
void A64EmitX64::EmitA64LseEor32(A64EmitContext& ctx, IR::Inst* inst) {
    EmitLseAtomic<2, 32>(ctx, inst);
}
void A64EmitX64::EmitA64LseEor64(A64EmitContext& ctx, IR::Inst* inst) {
    EmitLseAtomic<2, 64>(ctx, inst);
}
void A64EmitX64::EmitA64LseSet8(A64EmitContext& ctx, IR::Inst* inst) {
    EmitLseAtomic<3, 8>(ctx, inst);
}
void A64EmitX64::EmitA64LseSet16(A64EmitContext& ctx, IR::Inst* inst) {
    EmitLseAtomic<3, 16>(ctx, inst);
}
void A64EmitX64::EmitA64LseSet32(A64EmitContext& ctx, IR::Inst* inst) {
    EmitLseAtomic<3, 32>(ctx, inst);
}
void A64EmitX64::EmitA64LseSet64(A64EmitContext& ctx, IR::Inst* inst) {
    EmitLseAtomic<3, 64>(ctx, inst);
}
void A64EmitX64::EmitA64LseSmax8(A64EmitContext& ctx, IR::Inst* inst) {
    EmitLseAtomic<4, 8>(ctx, inst);
}
void A64EmitX64::EmitA64LseSmax16(A64EmitContext& ctx, IR::Inst* inst) {
    EmitLseAtomic<4, 16>(ctx, inst);
}
void A64EmitX64::EmitA64LseSmax32(A64EmitContext& ctx, IR::Inst* inst) {
    EmitLseAtomic<4, 32>(ctx, inst);
}
void A64EmitX64::EmitA64LseSmax64(A64EmitContext& ctx, IR::Inst* inst) {
    EmitLseAtomic<4, 64>(ctx, inst);
}
void A64EmitX64::EmitA64LseSmin8(A64EmitContext& ctx, IR::Inst* inst) {
    EmitLseAtomic<5, 8>(ctx, inst);
}
void A64EmitX64::EmitA64LseSmin16(A64EmitContext& ctx, IR::Inst* inst) {
    EmitLseAtomic<5, 16>(ctx, inst);
}
void A64EmitX64::EmitA64LseSmin32(A64EmitContext& ctx, IR::Inst* inst) {
    EmitLseAtomic<5, 32>(ctx, inst);
}
void A64EmitX64::EmitA64LseSmin64(A64EmitContext& ctx, IR::Inst* inst) {
    EmitLseAtomic<5, 64>(ctx, inst);
}
void A64EmitX64::EmitA64LseUmax8(A64EmitContext& ctx, IR::Inst* inst) {
    EmitLseAtomic<6, 8>(ctx, inst);
}
void A64EmitX64::EmitA64LseUmax16(A64EmitContext& ctx, IR::Inst* inst) {
    EmitLseAtomic<6, 16>(ctx, inst);
}
void A64EmitX64::EmitA64LseUmax32(A64EmitContext& ctx, IR::Inst* inst) {
    EmitLseAtomic<6, 32>(ctx, inst);
}
void A64EmitX64::EmitA64LseUmax64(A64EmitContext& ctx, IR::Inst* inst) {
    EmitLseAtomic<6, 64>(ctx, inst);
}
void A64EmitX64::EmitA64LseUmin8(A64EmitContext& ctx, IR::Inst* inst) {
    EmitLseAtomic<7, 8>(ctx, inst);
}
void A64EmitX64::EmitA64LseUmin16(A64EmitContext& ctx, IR::Inst* inst) {
    EmitLseAtomic<7, 16>(ctx, inst);
}
void A64EmitX64::EmitA64LseUmin32(A64EmitContext& ctx, IR::Inst* inst) {
    EmitLseAtomic<7, 32>(ctx, inst);
}
void A64EmitX64::EmitA64LseUmin64(A64EmitContext& ctx, IR::Inst* inst) {
    EmitLseAtomic<7, 64>(ctx, inst);
}
void A64EmitX64::EmitA64LseSwp8(A64EmitContext& ctx, IR::Inst* inst) {
    EmitLseAtomic<8, 8>(ctx, inst);
}
void A64EmitX64::EmitA64LseSwp16(A64EmitContext& ctx, IR::Inst* inst) {
    EmitLseAtomic<8, 16>(ctx, inst);
}
void A64EmitX64::EmitA64LseSwp32(A64EmitContext& ctx, IR::Inst* inst) {
    EmitLseAtomic<8, 32>(ctx, inst);
}
void A64EmitX64::EmitA64LseSwp64(A64EmitContext& ctx, IR::Inst* inst) {
    EmitLseAtomic<8, 64>(ctx, inst);
}
void A64EmitX64::EmitA64LseCas8(A64EmitContext& ctx, IR::Inst* inst) {
    EmitLseAtomic<9, 8>(ctx, inst);
}
void A64EmitX64::EmitA64LseCas16(A64EmitContext& ctx, IR::Inst* inst) {
    EmitLseAtomic<9, 16>(ctx, inst);
}
void A64EmitX64::EmitA64LseCas32(A64EmitContext& ctx, IR::Inst* inst) {
    EmitLseAtomic<9, 32>(ctx, inst);
}
void A64EmitX64::EmitA64LseCas64(A64EmitContext& ctx, IR::Inst* inst) {
    EmitLseAtomic<9, 64>(ctx, inst);
}

void A64EmitX64::EmitCheckMemoryAbort(A64EmitContext&, IR::Inst* inst, Xbyak::Label* end) {
    if (!conf.check_halt_on_memory_access) {
        return;
    }

    Xbyak::Label skip;

    const A64::LocationDescriptor current_location{IR::LocationDescriptor{inst->GetArg(0).GetU64()}};

    code.test(dword[code.ABI_JIT_PTR + offsetof(A64JitState, halt_reason)], static_cast<u32>(HaltReason::MemoryAbort));
    if (end) {
        code.jz(*end, code.T_NEAR);
    } else {
        code.jz(skip, code.T_NEAR);
    }
    code.mov(rax, current_location.PC());
    code.mov(qword[code.ABI_JIT_PTR + offsetof(A64JitState, pc)], rax);
    code.ForceReturnFromRunCode();
    code.L(skip);
}

}  // namespace Dynarmic::Backend::X64
