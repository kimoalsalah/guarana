// SPDX-FileCopyrightText: Copyright 2026 Guarana Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <algorithm>
#include <array>
#include <barrier>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <optional>
#include <thread>
#include <vector>

#include <catch2/catch_test_macros.hpp>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif

#include "common/common_types.h"
#include "dynarmic/interface/A64/a64.h"
#include "dynarmic/interface/exclusive_monitor.h"
#include "dynarmic/ir/opcodes.h"

namespace {

using namespace Dynarmic;
constexpr size_t ArenaSize = 0x20000;
constexpr u64 DataAddress = 0x10000;
constexpr u32 Svc = 0xD4000001;

class Arena {
public:
    Arena() {
#ifdef _WIN32
        data = static_cast<u8*>(VirtualAlloc(nullptr, ArenaSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
        REQUIRE(data != nullptr);
#else
        void* mapping = mmap(nullptr, ArenaSize, PROT_READ | PROT_WRITE,
                             MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        REQUIRE(mapping != MAP_FAILED);
        data = static_cast<u8*>(mapping);
#endif
    }
    ~Arena() {
#ifdef _WIN32
        VirtualFree(data, 0, MEM_RELEASE);
#else
        munmap(data, ArenaSize);
#endif
    }
    Arena(const Arena&) = delete;
    Arena& operator=(const Arena&) = delete;

    void ProtectData() {
#ifdef _WIN32
        DWORD old_protection;
        REQUIRE(VirtualProtect(data + DataAddress, 0x10000, PAGE_READONLY, &old_protection) != 0);
#else
        REQUIRE(mprotect(data + DataAddress, 0x10000, PROT_READ) == 0);
#endif
    }
    u8* data = nullptr;
};

class Env final : public A64::UserCallbacks {
public:
    explicit Env(Arena& memory) : arena(memory) {}
    Arena& arena;
    A64::Jit* jit = nullptr;
    bool finished = false;
    bool callback_only = false;
    u64 callback_value = 0;
    size_t reads = 0;
    size_t writes = 0;
    u64 ticks_left = 0;
    std::vector<u32> code_mem;
    A64::UserConfig::Counters counters{};

    std::optional<u32> MemoryReadCode(u64 address) override {
        if (address / 4 >= code_mem.size()) {
            return std::nullopt;
        }
        return code_mem[address / 4];
    }
    A64::Vector MemoryRead128(u64) override {
        FAIL("Unexpected 128-bit read");
        return {};
    }
    void MemoryWrite128(u64, A64::Vector) override {
        FAIL("Unexpected 128-bit write");
    }
    void ExceptionRaised(u64, A64::Exception) override {
        FAIL("Unexpected guest exception");
    }
    void AddTicks(u64 ticks) override {
        ticks_left -= std::min(ticks, ticks_left);
    }
    u64 GetTicksRemaining() override {
        return ticks_left;
    }
    u64 GetCNTPCT() override {
        return 0;
    }

    u64 MemoryRead(u64 address, size_t size) override {
        ++reads;
        u64 value = 0;
        if (callback_only) {
            std::memcpy(&value, &callback_value, size);
        } else {
            std::memcpy(&value, arena.data + address % ArenaSize, size);
        }
        return value;
    }
    void MemoryWrite(u64 address, u64 value, size_t size) override {
        ++writes;
        if (callback_only) {
            std::memcpy(&callback_value, &value, size);
        } else {
            std::memcpy(arena.data + address % ArenaSize, &value, size);
        }
    }
    template<typename T>
    bool Exclusive(u64 address, T value, T expected) {
        // Single-thread fallback oracle; contention tests must stay on fastmem.
        if (static_cast<T>(MemoryRead(address, sizeof(T))) != expected) {
            return false;
        }
        MemoryWrite(address, value, sizeof(T));
        return true;
    }
    bool MemoryWriteExclusive8(u64 address, u8 value, u8 expected) override {
        return Exclusive(address, value, expected);
    }
    bool MemoryWriteExclusive16(u64 address, u16 value, u16 expected) override {
        return Exclusive(address, value, expected);
    }
    bool MemoryWriteExclusive32(u64 address, u32 value, u32 expected) override {
        return Exclusive(address, value, expected);
    }
    bool MemoryWriteExclusive64(u64 address, u64 value, u64 expected) override {
        return Exclusive(address, value, expected);
    }
    void CallSVC(u32) override {
        finished = true;
        jit->HaltExecution();
    }
};

u32 Atomic(u32 operation, u32 size, u32 ordering, u32 rs = 1, u32 rn = 0, u32 rt = 2) {
    const u32 acquire = ordering & 1;
    const u32 release = (ordering >> 1) & 1;
    if (operation == 9) {
        return 0x08A07C00 | (size << 30) | (acquire << 22) | (release << 15)
            | (rs << 16) | (rn << 5) | rt;
    }
    return 0x38200000 | (size << 30) | (acquire << 23) | (release << 22)
        | (rs << 16) | (operation << 12) | (rn << 5) | rt;
}

A64::UserConfig Config(Env& env, ExclusiveMonitor& monitor, bool enabled, bool fastmem = true) {
    A64::UserConfig config{};
    config.callbacks = &env;
    config.counters = &env.counters;
    config.global_monitor = &monitor;
    config.unsafe_optimizations = true;
    config.optimizations |= OptimizationFlag::Unsafe_IgnoreGlobalMonitor;
    if (enabled) {
        config.optimizations |= OptimizationFlag::Experimental_LseHostAtomics;
    }
    config.fastmem_exclusive_access = fastmem;
    if (fastmem) {
        config.fastmem_pointer = reinterpret_cast<std::uintptr_t>(env.arena.data);
    }
    config.fastmem_address_space_bits = 17;
    config.silently_mirror_fastmem = false;
    config.enable_cycle_counting = false;
    return config;
}

bool RunToSvc(Env& env) {
    // One extra Run may be needed when the host atomic deoptimizes.
    for (size_t attempt = 0; attempt < 8 && !env.finished; ++attempt) {
        env.jit->Run();
    }
    return env.finished;
}

u64 Mask(u32 size) {
    return size == 3 ? ~u64{0} : (u64{1} << (8U << size)) - 1;
}

u64 Expected(u32 operation, u32 size, u64 old, u64 source, u64 desired) {
    const u64 mask = Mask(size);
    old &= mask;
    source &= mask;
    const u64 sign = u64{1} << ((8U << size) - 1);
    const s64 signed_old = std::bit_cast<s64>((old ^ sign) - sign);
    const s64 signed_source = std::bit_cast<s64>((source ^ sign) - sign);
    switch (operation) {
    case 0: return (old + source) & mask;
    case 1: return old & ~source;
    case 2: return old ^ source;
    case 3: return old | source;
    case 4: return signed_old >= signed_source ? old : source;
    case 5: return signed_old <= signed_source ? old : source;
    case 6: return std::max(old, source);
    case 7: return std::min(old, source);
    case 8: return source;
    case 9: return old == source ? desired & mask : old;
    default: std::abort();
    }
}

struct Result {
    std::array<u64, 31> registers;
    u64 memory;
    u32 flags;
};

class Runner {
public:
    Runner(u32 operation, u32 size, u32 ordering, bool enabled)
        : env(arena), monitor(1), jit(Config(env, monitor, enabled)) {
        env.jit = &jit;
        env.code_mem = {Atomic(operation, size, ordering), Svc};
    }

    Result Run(u32 size, u64 old, u64 source, u64 desired) {
        const size_t bytes = size_t{1} << size;
        std::memcpy(arena.data + DataAddress, &old, bytes);
        env.finished = false;
        jit.ClearHalt();
        jit.SetPC(0);
        jit.SetRegister(0, DataAddress);
        jit.SetRegister(1, source);
        jit.SetRegister(2, desired);
        jit.SetPstate(0xA0000000);
        REQUIRE(RunToSvc(env));
        u64 value = 0;
        std::memcpy(&value, arena.data + DataAddress, bytes);
        return {jit.GetRegisters(), value, jit.GetPstate()};
    }

    Arena arena;
    Env env;
    ExclusiveMonitor monitor;
    A64::Jit jit;
};

Result Execute(u32 operation, u32 size, u32 ordering, u64 old, u64 source,
               u64 desired, bool enabled, u64 address = DataAddress,
               bool fastmem = true, bool read_only = false, u32 rs = 1, u32 rt = 2, u32 rn = 0) {
    Arena arena;
    const size_t bytes = size_t{1} << size;
    std::memcpy(arena.data + address % ArenaSize, &old, bytes);
    Env env{arena};
    env.code_mem = {Atomic(operation, size, ordering, rs, rn, rt), Svc};
    env.callback_value = old & Mask(size);
    env.callback_only = read_only;
    if (read_only) {
        arena.ProtectData();
    }
    ExclusiveMonitor monitor{1};
    A64::Jit jit{Config(env, monitor, enabled, fastmem)};
    env.jit = &jit;
    jit.SetRegister(0, address);
    jit.SetSP(address);
    if (rs != 31) {
        jit.SetRegister(rs, source);
    }
    if (rt != 31 && rt != rs) {
        jit.SetRegister(rt, desired);
    }
    jit.SetPstate(0xA0000000);
    REQUIRE(RunToSvc(env));
    u64 value = 0;
    if (read_only) {
        value = env.callback_value;
        REQUIRE(env.writes == 1);
    } else {
        std::memcpy(&value, arena.data + address % ArenaSize, bytes);
    }
    if (enabled && fastmem && !read_only && address < ArenaSize && address % bytes == 0) {
        // Detect a configuration error that silently tests only the old path.
        REQUIRE(env.reads == 0);
        REQUIRE(env.writes == 0);
        REQUIRE(env.counters.blocks_compiled == 1);
    }
    if (enabled && fastmem && address >= ArenaSize) {
        REQUIRE(env.writes >= 1);
    }
    return {jit.GetRegisters(), value, jit.GetPstate()};
}

TEST_CASE("A64: LSE zero operands and SP base", "[a64][lse]") {
    for (u32 operation = 0; operation < 10; ++operation) {
        for (u32 size = 0; size < 4; ++size) {
            const Result result = Execute(operation, size, 3, 5, 0, 9, true,
                                          DataAddress, true, false, 31, 2, 31);
            CHECK(result.memory == Expected(operation, size, 5, 0, 9));
            if (operation != 9) {
                CHECK(result.registers[2] == 5);
            }
            if (operation == 9) {
                const Result zero_desired = Execute(operation, size, 3, 5, 5, 0, true,
                                                    DataAddress, true, false, 1, 31, 31);
                CHECK(zero_desired.memory == 0);
                CHECK(zero_desired.registers[1] == 5);
            }
        }
    }
}

} // namespace

TEST_CASE("A64: LSE host atomics match exclusive lowering", "[a64][lse]") {
    for (u32 operation = 0; operation < 10; ++operation) {
        for (u32 size = 0; size < 4; ++size) {
            const u64 mask = Mask(size);
            const u64 sign = u64{1} << ((8U << size) - 1);
            const std::array<u64, 8> values{0, 1, mask, sign, sign - 1, sign + 1,
                                           0x5555555555555555ULL & mask,
                                           0xAAAAAAAAAAAAAAAAULL & mask};
            for (u32 ordering = 0; ordering < 4; ++ordering) {
                Runner baseline_runner{operation, size, ordering, false};
                Runner changed_runner{operation, size, ordering, true};
                for (size_t index = 0; index < values.size(); ++index) {
                    const u64 old = values[index];
                    // Dirty upper input bits exercise truncation and mismatch results.
                    const u64 source = values[(index + 3) % values.size()] | ~mask;
                    const u64 desired = 0xFEDCBA9876543210ULL;
                    CAPTURE(operation, size, ordering, index);
                    const Result baseline = baseline_runner.Run(size, old, source, desired);
                    const Result changed = changed_runner.Run(size, old, source, desired);
                    CHECK(changed.registers == baseline.registers);
                    CHECK(changed.memory == baseline.memory);
                    CHECK(changed.memory == Expected(operation, size, old, source, desired));
                    CHECK(changed.flags == 0xA0000000);
                    CHECK(changed.registers[operation == 9 ? 1 : 2] == old);
                }
                // CAS success and every operation's same-value/zero-source case.
                const Result result = changed_runner.Run(size, mask, mask, 0);
                CHECK(result.memory == Expected(operation, size, mask, mask, 0));
                CHECK(result.registers[operation == 9 ? 1 : 2] == mask);
                CHECK(changed_runner.env.counters.blocks_compiled == 1);
                CHECK(baseline_runner.env.counters.blocks_compiled == 2);
            }
        }
    }
}

TEST_CASE("A64: LSE fallback and aliases", "[a64][lse]") {
    for (u32 operation = 0; operation < 10; ++operation) {
        for (u32 size = 0; size < 4; ++size) {
            for (const u32 ordering : {3U}) {
                CAPTURE(operation, size, ordering);
                const u64 old = Mask(size);
                for (const u64 address : {DataAddress + 1, DataAddress - 1, ArenaSize + DataAddress}) {
                    const Result baseline = Execute(operation, size, ordering, old, 3, 9, false, address);
                    const Result changed = Execute(operation, size, ordering, old, 3, 9, true, address);
                    CHECK(changed.memory == baseline.memory);
                    CHECK(changed.registers == baseline.registers);
                }
                const Result callback = Execute(operation, size, ordering, old, 3, 9, true,
                                                DataAddress, false);
                CHECK(callback.memory == Expected(operation, size, old, 3, 9));
                // Write-protected memory must fault even for failed CAS or no-op RMW.
                const u64 noop_source = operation >= 4 && operation <= 8 ? old : 0;
                const Result protected_result = Execute(operation, size, ordering, old, noop_source, 0, true,
                                                        DataAddress, true, true);
                CHECK(protected_result.memory == old);
                const Result alias = Execute(operation, size, ordering, old, 3, 3, true,
                                             DataAddress, true, false, 1, 1);
                CHECK(alias.registers[1] == old);
                CHECK(alias.memory == Expected(operation, size, old, 3, 3));
                if (operation != 9) {
                    const Result discard = Execute(operation, size, ordering, old, 3, 0, true,
                                                   DataAddress, true, false, 1, 31);
                    CHECK(discard.memory == Expected(operation, size, old, 3, 0));
                }
            }
        }
    }
}

TEST_CASE("A64: LSE LDADD and CAS contend on one address", "[a64][lse][thread]") {
    constexpr size_t ThreadCount = 4;
    constexpr u64 Iterations = 1000;
    for (const bool cas : {false, true}) {
        Arena arena;
        ExclusiveMonitor monitor{ThreadCount};
        std::array<std::unique_ptr<Env>, ThreadCount> envs;
        std::array<std::unique_ptr<A64::Jit>, ThreadCount> jits;
        std::array<bool, ThreadCount> completed{};
        for (size_t index = 0; index < ThreadCount; ++index) {
            envs[index] = std::make_unique<Env>(arena);
            auto& env = *envs[index];
            if (cas) {
                env.code_mem = {
                    0xAA0103E4, // MOV X4, X1 (preserve expected)
                    0x91000422, // ADD X2, X1, #1
                    Atomic(9, 3, 3),
                    0xEB04003F, // CMP X1, X4
                    0x54FFFF81, // B.NE retry CAS
                    0xF1000463, // SUBS X3, X3, #1
                    0x54FFFF41, // B.NE next increment
                    Svc,
                };
            } else {
                env.code_mem = {Atomic(0, 3, 3), 0xF1000463, 0x54FFFFC1, Svc};
            }
            auto config = Config(env, monitor, true);
            config.processor_id = static_cast<u8>(index);
            jits[index] = std::make_unique<A64::Jit>(config);
            env.jit = jits[index].get();
            env.jit->SetRegister(0, DataAddress);
            env.jit->SetRegister(1, cas ? 0 : 1);
            env.jit->SetRegister(3, Iterations);
        }
        std::vector<std::thread> threads;
        std::barrier start{static_cast<std::ptrdiff_t>(ThreadCount)};
        for (size_t index = 0; index < ThreadCount; ++index) {
            threads.emplace_back([&, index] {
                start.arrive_and_wait();
                completed[index] = RunToSvc(*envs[index]);
            });
        }
        for (auto& thread : threads) {
            thread.join();
        }
        u64 total;
        std::memcpy(&total, arena.data + DataAddress, sizeof(total));
        CHECK(total == ThreadCount * Iterations);
        for (size_t index = 0; index < ThreadCount; ++index) {
            CHECK(completed[index]);
            CHECK(envs[index]->reads == 0);
            CHECK(envs[index]->writes == 0);
        }
    }
}

TEST_CASE("A64: LSE deoptimization preserves prior guest state", "[a64][lse]") {
    Arena arena;
    Env env{arena};
    ExclusiveMonitor monitor{1};
    env.code_mem = {
        0xD2800061, // MOV X1, #3
        Atomic(0, 3, 3),
        0xD2800121, // MOV X1, #9 (must not eliminate the pre-atomic setter)
        Svc,
    };
    auto config = Config(env, monitor, true);
    config.enable_cycle_counting = true;
    A64::Jit jit{config};
    env.jit = &jit;
    env.ticks_left = 100;
    jit.SetRegister(0, ArenaSize + DataAddress); // Range guard, then callback fallback.
    jit.SetRegister(1, 7);
    REQUIRE(RunToSvc(env));
    u64 result;
    std::memcpy(&result, arena.data + DataAddress, sizeof(result));
    CHECK(result == 3);
    CHECK(jit.GetRegister(1) == 9);
    CHECK(jit.GetRegister(2) == 0);
    CHECK(env.writes >= 1);
    CHECK(env.ticks_left == 96);
}

TEST_CASE("A64: changed LSE value invalidates a saved exclusive value", "[a64][lse]") {
    for (u32 operation = 0; operation < 10; ++operation) {
        for (u32 size = 0; size < 4; ++size) {
            CAPTURE(operation, size);
            Arena arena;
            Env reserver{arena};
            Env writer{arena};
            ExclusiveMonitor monitor{2};
            const u64 old = 5;
            std::memcpy(arena.data + DataAddress, &old, sizeof(old));
            reserver.code_mem = {
                0x085F7C04 | (size << 30), // LDXR Rt=4, Rn=0
                0x08057C06 | (size << 30), // STXR Rs=5, Rt=6, Rn=0
                Svc,
            };
            writer.code_mem = {Atomic(operation, size, 3), Svc};
            A64::Jit reader{Config(reserver, monitor, true)};
            auto writer_config = Config(writer, monitor, true);
            writer_config.processor_id = 1;
            A64::Jit updater{writer_config};
            reserver.jit = &reader;
            writer.jit = &updater;
            reader.SetRegister(0, DataAddress);
            reader.SetRegister(6, 42);
            updater.SetRegister(0, DataAddress);
            const u64 source = operation == 4 || operation == 6 ? 9 : operation == 9 ? 5 : 3;
            updater.SetRegister(1, source);
            updater.SetRegister(2, 9);
            reader.Step();
            REQUIRE(reader.GetRegister(4) == old);
            REQUIRE(RunToSvc(writer));
            reader.Step();
            CHECK(reader.GetRegister(5) == 1);
            u64 value = 0;
            std::memcpy(&value, arena.data + DataAddress, size_t{1} << size);
            CHECK(value == Expected(operation, size, old, source, 9));
        }
    }
}

TEST_CASE("A64: LSE IR is a non-eliminable read and write", "[a64][lse]") {
    CHECK((all_safe_optimizations & OptimizationFlag::Experimental_LseHostAtomics) == no_optimizations);
    for (size_t opcode = static_cast<size_t>(IR::Opcode::A64LseAdd8);
         opcode <= static_cast<size_t>(IR::Opcode::A64LseCas64); ++opcode) {
        const auto op = static_cast<IR::Opcode>(opcode);
        CHECK(IR::IsMemoryRead(op));
        CHECK(IR::IsMemoryWrite(op));
        CHECK(IR::MayHaveSideEffects(op));
    }
}

TEST_CASE("A64: LSE strict monitor mode retains original lowering", "[a64][lse]") {
    Arena arena;
    Env env{arena};
    ExclusiveMonitor monitor{1};
    env.code_mem = {Atomic(0, 3, 3), Svc};
    auto config = Config(env, monitor, true);
    config.optimizations &= ~OptimizationFlag::Unsafe_IgnoreGlobalMonitor;
    A64::Jit jit{config};
    env.jit = &jit;
    jit.SetRegister(0, DataAddress);
    jit.SetRegister(1, 3);
    REQUIRE(RunToSvc(env));
    u64 value;
    std::memcpy(&value, arena.data + DataAddress, sizeof(value));
    CHECK(value == 3);
    CHECK(env.counters.blocks_compiled == 2);
}

TEST_CASE("A64: LSE fallback only disables the affected PC", "[a64][lse]") {
    Arena arena;
    Env env{arena};
    ExclusiveMonitor monitor{1};
    env.code_mem = {Atomic(0, 3, 3), Atomic(0, 3, 3, 1, 3, 4), Svc};
    A64::Jit jit{Config(env, monitor, true)};
    env.jit = &jit;
    jit.SetRegister(0, ArenaSize + DataAddress);
    jit.SetRegister(1, 3);
    jit.SetRegister(3, DataAddress + 8);
    REQUIRE(RunToSvc(env));
    CHECK(env.writes == 1);
    // Initial two-atomic block, replay through old atomic, then inline second atomic + SVC.
    CHECK(env.counters.blocks_compiled == 3);
    CHECK(env.counters.cache_clears == 0);
    u64 first;
    u64 second;
    std::memcpy(&first, arena.data + DataAddress, sizeof(first));
    std::memcpy(&second, arena.data + DataAddress + 8, sizeof(second));
    CHECK(first == 3);
    CHECK(second == 3);

    // A later external code invalidation permits that PC to use the fast path again.
    const u64 compiled = env.counters.blocks_compiled;
    env.finished = false;
    env.writes = 0;
    jit.SetPC(0);
    jit.SetRegister(0, DataAddress);
    jit.InvalidateCacheRange(0, 4);
    REQUIRE(RunToSvc(env));
    CHECK(env.counters.blocks_compiled == compiled + 1);
    CHECK(env.counters.cache_clears == 0);
    CHECK(env.writes == 0);
}

TEST_CASE("A64: one Step executes an LSE instruction requiring fallback", "[a64][lse]") {
    Arena arena;
    Env env{arena};
    ExclusiveMonitor monitor{1};
    env.code_mem = {Atomic(0, 3, 3), Svc};
    A64::Jit jit{Config(env, monitor, true)};
    env.jit = &jit;
    jit.SetRegister(0, ArenaSize + DataAddress);
    jit.SetRegister(1, 3);
    jit.SetRegister(2, 77);
    const auto reason = jit.Step();
    CHECK(Has(reason, HaltReason::Step));
    CHECK(!Has(reason, HaltReason::CacheInvalidation));
    CHECK(jit.GetPC() == 4);
    CHECK(jit.GetRegister(2) == 0);
    CHECK(env.writes == 1);
    u64 value;
    std::memcpy(&value, arena.data + DataAddress, sizeof(value));
    CHECK(value == 3);
}

TEST_CASE("A64: LSE replay does not repeat a preceding atomic", "[a64][lse]") {
    Arena arena;
    Env env{arena};
    ExclusiveMonitor monitor{1};
    env.code_mem = {Atomic(0, 3, 3, 1, 3, 4), Atomic(0, 3, 3), Svc};
    A64::Jit jit{Config(env, monitor, true)};
    env.jit = &jit;
    jit.SetRegister(0, ArenaSize + DataAddress);
    jit.SetRegister(1, 3);
    jit.SetRegister(3, DataAddress);
    REQUIRE(RunToSvc(env));
    u64 value;
    std::memcpy(&value, arena.data + DataAddress, sizeof(value));
    CHECK(value == 6);
    CHECK(jit.GetRegister(4) == 0);
    CHECK(jit.GetRegister(2) == 3);
    CHECK(env.writes == 1);
}

TEST_CASE("A64: LSE ABA and same-value writes retain value-based exclusives", "[a64][lse]") {
    for (const bool aba : {false, true}) {
        Arena arena;
        Env reserver{arena};
        Env writer{arena};
        ExclusiveMonitor monitor{2};
        const u64 old = 5;
        std::memcpy(arena.data + DataAddress, &old, sizeof(old));
        reserver.code_mem = {0xC85F7C04, 0xC8057C06, Svc}; // LDXR X4; STXR W5,X6
        writer.code_mem = {Atomic(8, 3, 3), Svc};
        A64::Jit reader{Config(reserver, monitor, true)};
        auto writer_config = Config(writer, monitor, true);
        writer_config.processor_id = 1;
        A64::Jit updater{writer_config};
        reserver.jit = &reader;
        writer.jit = &updater;
        reader.SetRegister(0, DataAddress);
        reader.SetRegister(6, 42);
        updater.SetRegister(0, DataAddress);
        updater.SetRegister(1, aba ? 6 : 5);
        reader.Step();
        REQUIRE(RunToSvc(writer));
        if (aba) {
            writer.finished = false;
            updater.SetPC(0);
            updater.SetRegister(1, 5);
            REQUIRE(RunToSvc(writer));
        }
        reader.Step();
        CHECK(reader.GetRegister(5) == 0);
        u64 value;
        std::memcpy(&value, arena.data + DataAddress, sizeof(value));
        CHECK(value == 42);
    }
}
