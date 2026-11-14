// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

/* This file is part of the dynarmic project.
 * Copyright (c) 2024 MerryMage
 * SPDX-License-Identifier: 0BSD
 */

#include "dynarmic/backend/riscv64/emit_riscv64.h"

#include <bit>

#include <biscuit/assembler.hpp>
#include <fmt/ostream.h>
#include "dynarmic/mcl/bit.hpp"

#include "dynarmic/backend/riscv64/a32_jitstate.h"
#include "dynarmic/backend/riscv64/abi.h"
#include "dynarmic/backend/riscv64/emit_context.h"
#include "dynarmic/backend/riscv64/reg_alloc.h"
#include "dynarmic/ir/basic_block.h"
#include "dynarmic/ir/microinstruction.h"
#include "dynarmic/ir/opcodes.h"

namespace Dynarmic::Backend::RV64 {

template<>
void EmitIR<IR::Opcode::A64LseAdd8>(biscuit::Assembler&, EmitContext&, IR::Inst*) {
    UNREACHABLE(); // The x64-only translation option is never enabled here.
}
template<>
void EmitIR<IR::Opcode::A64LseAdd16>(biscuit::Assembler&, EmitContext&, IR::Inst*) {
    UNREACHABLE(); // The x64-only translation option is never enabled here.
}
template<>
void EmitIR<IR::Opcode::A64LseAdd32>(biscuit::Assembler&, EmitContext&, IR::Inst*) {
    UNREACHABLE(); // The x64-only translation option is never enabled here.
}
template<>
void EmitIR<IR::Opcode::A64LseAdd64>(biscuit::Assembler&, EmitContext&, IR::Inst*) {
    UNREACHABLE(); // The x64-only translation option is never enabled here.
}
template<>
void EmitIR<IR::Opcode::A64LseClr8>(biscuit::Assembler&, EmitContext&, IR::Inst*) {
    UNREACHABLE(); // The x64-only translation option is never enabled here.
}
template<>
void EmitIR<IR::Opcode::A64LseClr16>(biscuit::Assembler&, EmitContext&, IR::Inst*) {
    UNREACHABLE(); // The x64-only translation option is never enabled here.
}
template<>
void EmitIR<IR::Opcode::A64LseClr32>(biscuit::Assembler&, EmitContext&, IR::Inst*) {
    UNREACHABLE(); // The x64-only translation option is never enabled here.
}
template<>
void EmitIR<IR::Opcode::A64LseClr64>(biscuit::Assembler&, EmitContext&, IR::Inst*) {
    UNREACHABLE(); // The x64-only translation option is never enabled here.
}
template<>
void EmitIR<IR::Opcode::A64LseEor8>(biscuit::Assembler&, EmitContext&, IR::Inst*) {
    UNREACHABLE(); // The x64-only translation option is never enabled here.
}
template<>
void EmitIR<IR::Opcode::A64LseEor16>(biscuit::Assembler&, EmitContext&, IR::Inst*) {
    UNREACHABLE(); // The x64-only translation option is never enabled here.
}
template<>
void EmitIR<IR::Opcode::A64LseEor32>(biscuit::Assembler&, EmitContext&, IR::Inst*) {
    UNREACHABLE(); // The x64-only translation option is never enabled here.
}
template<>
void EmitIR<IR::Opcode::A64LseEor64>(biscuit::Assembler&, EmitContext&, IR::Inst*) {
    UNREACHABLE(); // The x64-only translation option is never enabled here.
}
template<>
void EmitIR<IR::Opcode::A64LseSet8>(biscuit::Assembler&, EmitContext&, IR::Inst*) {
    UNREACHABLE(); // The x64-only translation option is never enabled here.
}
template<>
void EmitIR<IR::Opcode::A64LseSet16>(biscuit::Assembler&, EmitContext&, IR::Inst*) {
    UNREACHABLE(); // The x64-only translation option is never enabled here.
}
template<>
void EmitIR<IR::Opcode::A64LseSet32>(biscuit::Assembler&, EmitContext&, IR::Inst*) {
    UNREACHABLE(); // The x64-only translation option is never enabled here.
}
template<>
void EmitIR<IR::Opcode::A64LseSet64>(biscuit::Assembler&, EmitContext&, IR::Inst*) {
    UNREACHABLE(); // The x64-only translation option is never enabled here.
}
template<>
void EmitIR<IR::Opcode::A64LseSmax8>(biscuit::Assembler&, EmitContext&, IR::Inst*) {
    UNREACHABLE(); // The x64-only translation option is never enabled here.
}
template<>
void EmitIR<IR::Opcode::A64LseSmax16>(biscuit::Assembler&, EmitContext&, IR::Inst*) {
    UNREACHABLE(); // The x64-only translation option is never enabled here.
}
template<>
void EmitIR<IR::Opcode::A64LseSmax32>(biscuit::Assembler&, EmitContext&, IR::Inst*) {
    UNREACHABLE(); // The x64-only translation option is never enabled here.
}
template<>
void EmitIR<IR::Opcode::A64LseSmax64>(biscuit::Assembler&, EmitContext&, IR::Inst*) {
    UNREACHABLE(); // The x64-only translation option is never enabled here.
}
template<>
void EmitIR<IR::Opcode::A64LseSmin8>(biscuit::Assembler&, EmitContext&, IR::Inst*) {
    UNREACHABLE(); // The x64-only translation option is never enabled here.
}
template<>
void EmitIR<IR::Opcode::A64LseSmin16>(biscuit::Assembler&, EmitContext&, IR::Inst*) {
    UNREACHABLE(); // The x64-only translation option is never enabled here.
}
template<>
void EmitIR<IR::Opcode::A64LseSmin32>(biscuit::Assembler&, EmitContext&, IR::Inst*) {
    UNREACHABLE(); // The x64-only translation option is never enabled here.
}
template<>
void EmitIR<IR::Opcode::A64LseSmin64>(biscuit::Assembler&, EmitContext&, IR::Inst*) {
    UNREACHABLE(); // The x64-only translation option is never enabled here.
}
template<>
void EmitIR<IR::Opcode::A64LseUmax8>(biscuit::Assembler&, EmitContext&, IR::Inst*) {
    UNREACHABLE(); // The x64-only translation option is never enabled here.
}
template<>
void EmitIR<IR::Opcode::A64LseUmax16>(biscuit::Assembler&, EmitContext&, IR::Inst*) {
    UNREACHABLE(); // The x64-only translation option is never enabled here.
}
template<>
void EmitIR<IR::Opcode::A64LseUmax32>(biscuit::Assembler&, EmitContext&, IR::Inst*) {
    UNREACHABLE(); // The x64-only translation option is never enabled here.
}
template<>
void EmitIR<IR::Opcode::A64LseUmax64>(biscuit::Assembler&, EmitContext&, IR::Inst*) {
    UNREACHABLE(); // The x64-only translation option is never enabled here.
}
template<>
void EmitIR<IR::Opcode::A64LseUmin8>(biscuit::Assembler&, EmitContext&, IR::Inst*) {
    UNREACHABLE(); // The x64-only translation option is never enabled here.
}
template<>
void EmitIR<IR::Opcode::A64LseUmin16>(biscuit::Assembler&, EmitContext&, IR::Inst*) {
    UNREACHABLE(); // The x64-only translation option is never enabled here.
}
template<>
void EmitIR<IR::Opcode::A64LseUmin32>(biscuit::Assembler&, EmitContext&, IR::Inst*) {
    UNREACHABLE(); // The x64-only translation option is never enabled here.
}
template<>
void EmitIR<IR::Opcode::A64LseUmin64>(biscuit::Assembler&, EmitContext&, IR::Inst*) {
    UNREACHABLE(); // The x64-only translation option is never enabled here.
}
template<>
void EmitIR<IR::Opcode::A64LseSwp8>(biscuit::Assembler&, EmitContext&, IR::Inst*) {
    UNREACHABLE(); // The x64-only translation option is never enabled here.
}
template<>
void EmitIR<IR::Opcode::A64LseSwp16>(biscuit::Assembler&, EmitContext&, IR::Inst*) {
    UNREACHABLE(); // The x64-only translation option is never enabled here.
}
template<>
void EmitIR<IR::Opcode::A64LseSwp32>(biscuit::Assembler&, EmitContext&, IR::Inst*) {
    UNREACHABLE(); // The x64-only translation option is never enabled here.
}
template<>
void EmitIR<IR::Opcode::A64LseSwp64>(biscuit::Assembler&, EmitContext&, IR::Inst*) {
    UNREACHABLE(); // The x64-only translation option is never enabled here.
}
template<>
void EmitIR<IR::Opcode::A64LseCas8>(biscuit::Assembler&, EmitContext&, IR::Inst*) {
    UNREACHABLE(); // The x64-only translation option is never enabled here.
}
template<>
void EmitIR<IR::Opcode::A64LseCas16>(biscuit::Assembler&, EmitContext&, IR::Inst*) {
    UNREACHABLE(); // The x64-only translation option is never enabled here.
}
template<>
void EmitIR<IR::Opcode::A64LseCas32>(biscuit::Assembler&, EmitContext&, IR::Inst*) {
    UNREACHABLE(); // The x64-only translation option is never enabled here.
}
template<>
void EmitIR<IR::Opcode::A64LseCas64>(biscuit::Assembler&, EmitContext&, IR::Inst*) {
    UNREACHABLE(); // The x64-only translation option is never enabled here.
}

template<>
void EmitIR<IR::Opcode::Void>(biscuit::Assembler&, EmitContext&, IR::Inst*) {}

template<>
void EmitIR<IR::Opcode::Identity>(biscuit::Assembler&, EmitContext& ctx, IR::Inst* inst) {
    auto args = ctx.reg_alloc.GetArgumentInfo(inst);
    ctx.reg_alloc.DefineAsExisting(inst, args[0]);
}

template<>
void EmitIR<IR::Opcode::Breakpoint>(biscuit::Assembler&, EmitContext&, IR::Inst*) {
    ASSERT(false && "Unimplemented instruction");
}

template<>
void EmitIR<IR::Opcode::CallHostFunction>(biscuit::Assembler&, EmitContext&, IR::Inst*) {
    ASSERT(false && "Unimplemented instruction");
}

template<>
void EmitIR<IR::Opcode::PushRSB>(biscuit::Assembler&, EmitContext&, IR::Inst*) {
    ASSERT(false && "Unimplemented instruction");
}

template<>
void EmitIR<IR::Opcode::GetCarryFromOp>(biscuit::Assembler&, EmitContext& ctx, IR::Inst* inst) {
    [[maybe_unused]] auto args = ctx.reg_alloc.GetArgumentInfo(inst);
    ASSERT(ctx.reg_alloc.IsValueLive(inst));
}

template<>
void EmitIR<IR::Opcode::GetOverflowFromOp>(biscuit::Assembler&, EmitContext&, IR::Inst*) {
    ASSERT(false && "Unimplemented instruction");
}

template<>
void EmitIR<IR::Opcode::GetGEFromOp>(biscuit::Assembler&, EmitContext&, IR::Inst*) {
    ASSERT(false && "Unimplemented instruction");
}

template<>
void EmitIR<IR::Opcode::GetNZCVFromOp>(biscuit::Assembler&, EmitContext& ctx, IR::Inst* inst) {
    [[maybe_unused]] auto args = ctx.reg_alloc.GetArgumentInfo(inst);
    ASSERT(ctx.reg_alloc.IsValueLive(inst));
}

template<>
void EmitIR<IR::Opcode::GetNZFromOp>(biscuit::Assembler& as, EmitContext& ctx, IR::Inst* inst) {
    auto args = ctx.reg_alloc.GetArgumentInfo(inst);

    auto Xvalue = ctx.reg_alloc.ReadX(args[0]);
    auto Xnz = ctx.reg_alloc.WriteX(inst);
    RegAlloc::Realize(Xvalue, Xnz);

    as.SEQZ(Xnz, Xvalue);
    as.SLLI(Xnz, Xnz, 30);
    as.SLTZ(Xscratch0, Xvalue);
    as.SLLI(Xscratch0, Xscratch0, 31);
    as.OR(Xnz, Xnz, Xscratch0);
}

template<>
void EmitIR<IR::Opcode::GetUpperFromOp>(biscuit::Assembler&, EmitContext&, IR::Inst*) {
    ASSERT(false && "Unimplemented instruction");
}

template<>
void EmitIR<IR::Opcode::GetLowerFromOp>(biscuit::Assembler&, EmitContext&, IR::Inst*) {
    ASSERT(false && "Unimplemented instruction");
}

template<>
void EmitIR<IR::Opcode::GetCFlagFromNZCV>(biscuit::Assembler& as, EmitContext& ctx, IR::Inst* inst) {
    auto args = ctx.reg_alloc.GetArgumentInfo(inst);

    auto Xc = ctx.reg_alloc.WriteX(inst);
    auto Xnzcv = ctx.reg_alloc.ReadX(args[0]);
    RegAlloc::Realize(Xc, Xnzcv);

    as.LUI(Xscratch0, 0x20000);
    as.AND(Xc, Xnzcv, Xscratch0);
}

template<>
void EmitIR<IR::Opcode::NZCVFromPackedFlags>(biscuit::Assembler&, EmitContext&, IR::Inst*) {
    ASSERT(false && "Unimplemented instruction");
}

EmittedBlockInfo EmitRV64(biscuit::Assembler& as, IR::Block block, const EmitConfig& emit_conf) {
    using namespace biscuit;

    EmittedBlockInfo ebi;

    RegAlloc reg_alloc{as, GPR_ORDER, FPR_ORDER};
    EmitContext ctx{block, reg_alloc, emit_conf, ebi};

    ebi.entry_point = reinterpret_cast<CodePtr>(as.GetCursorPointer());

    for (auto iter = block.instructions.begin(); iter != block.instructions.end(); ++iter) {
        IR::Inst* inst = &*iter;

        switch (inst->GetOpcode()) {
#define OPCODE(name, type, ...)                  \
    case IR::Opcode::name:                       \
        EmitIR<IR::Opcode::name>(as, ctx, inst); \
        break;
#define A32OPC(name, type, ...)                       \
    case IR::Opcode::A32##name:                       \
        EmitIR<IR::Opcode::A32##name>(as, ctx, inst); \
        break;
#define A64OPC(name, type, ...)                       \
    case IR::Opcode::A64##name:                       \
        EmitIR<IR::Opcode::A64##name>(as, ctx, inst); \
        break;
#include "dynarmic/ir/opcodes.inc"
#undef OPCODE
#undef A32OPC
#undef A64OPC
        default:
            UNREACHABLE();
        }
    }

    reg_alloc.UpdateAllUses();
    reg_alloc.AssertNoMoreUses();

    if (emit_conf.enable_cycle_counting) {
        const size_t cycles_to_add = block.CycleCount();
        as.LD(Xscratch0, offsetof(StackLayout, cycles_remaining), sp);
        if (mcl::bit::sign_extend<12>(-cycles_to_add) == -cycles_to_add) {
            as.ADDI(Xscratch0, Xscratch0, -cycles_to_add);
        } else {
            as.LI(Xscratch1, cycles_to_add);
            as.SUB(Xscratch0, Xscratch0, Xscratch1);
        }
        as.SD(Xscratch0, offsetof(StackLayout, cycles_remaining), sp);
    }

    EmitA32Terminal(as, ctx);

    ebi.size = reinterpret_cast<CodePtr>(as.GetCursorPointer()) - ebi.entry_point;
    return ebi;
}

void EmitRelocation(biscuit::Assembler& as, EmitContext& ctx, LinkTarget link_target) {
    ctx.ebi.relocations.emplace_back(Relocation{reinterpret_cast<CodePtr>(as.GetCursorPointer()) - ctx.ebi.entry_point, link_target});
    as.NOP();
}

}  // namespace Dynarmic::Backend::RV64
