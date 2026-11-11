/* The candidate prefix of fill_shortlist_slot 0x83f3a0 (0x83fa25..0x83fcb5).

 * The recompiled loop examines every row of a position list. Most of that time is the flag machinery
 * between a few compares and the calls. This walk applies the same tests, in the same order, and makes
 * the same calls. A candidate who fails is not returned to the recompiled loop. A candidate who reaches
 * 0x83fcbb is, with the registers and the stack slots that tail reads.
 *
 * The list is already ordered by the value at entry + 8 (shortlist_compare_by_value_then_key 0x83b670).
 * The value test (0x83fb97) and the wage test (0x83fc31, 0x83fc4e) are the range cuts on that order.
 * Job, nation, contract status and the shortlist count are the sets. They are applied per candidate
 * because a later test can call, and the calls stay in the original order. The shortlist count at
 * [row+0xb] changes in add_shortlist_entry 0x8423f0, which this loop calls only after the list
 * (0x840ad3), so the count is stable while one list is walked.
 *
 * RECOMP_SL_FAST=0 returns at once and the recompiled loop runs. RECOMP_SL_LOG is a file of the staff
 * id stored at 0x840ad0, one per line.
 */
#include "win.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void f_0069fc60(void);
void f_00600a50(void);
void f_0085fa00(void);
void f_00544590(void);

static void sl_report_once(void);
static int sl_off = -1;
static uint64_t sl_seen, sl_pass, sl_human, sl_rej, sl_ft, sl_wage_hit, sl_wage_miss;
static FILE *sl_log;

static int sl_disabled(void)
{
    if (sl_off < 0) {
        const char *v = getenv("RECOMP_SL_FAST");
        sl_off = v && (!strcmp(v, "0") || !strcmp(v, "original"));
        const char *path = getenv("RECOMP_SL_LOG");
        if (path && *path) {
            sl_log = fopen(path, "w");
            if (!sl_log) { fprintf(stderr, "recomp: shortlist log %s: ", path); perror(""); }
        }
    }
    return sl_off;
}

static void sl_esp(uint32_t want, const char *what)
{
    if (cpu.esp == want) return;
    fprintf(stderr, "recomp: shortlist scan: esp after %s is 0x%x, wanted 0x%x\n", what, cpu.esp, want);
    abort();
}

/* 0x4cf5a0. ecx is the contract manager; the staff pointer is the argument. Writes nothing. */
static uint32_t contract_of(uint32_t staff, uint32_t manager)
{
    if (!staff) return 0;
    uint32_t id = rd32(g_mem, staff), limit = rd32(g_mem, 0xae2364u);
    if ((int32_t)id >= (int32_t)limit) return 0;
    int32_t word = (int32_t)rd32(g_mem, rd32(g_mem, 0xae2d0cu) + id * 79u);
    if (word < 0) return 0;
    return (uint32_t)word * 80u + rd32(g_mem, manager);
}

/* The club pointer 0x83fc79..0x83fc8f computes from a contract, or 0 when the contract is null. */
static uint32_t club_from_contract(uint32_t contract)
{
    if (!contract) return 0;
    uint32_t id = rd32(g_mem, contract + 4u);
    uint32_t ecx = rd32(g_mem, 0xae23b4u) + id;
    uint32_t edx = ((id + id * 8u) << 4) + id;
    return ecx + edx * 4u;
}

/* Signed divide by 7, the imul at 0x83fc1c through 0x83fc28. */
static int32_t div7(int32_t n)
{
    int64_t q = (int64_t)(int32_t)0x92492493 * (int64_t)n;
    int32_t edx = (int32_t)(q >> 32);
    edx = (int32_t)((uint32_t)edx + (uint32_t)n);
    if (edx < 0) edx = (int32_t)(((uint32_t)edx >> 2) | 0xC0000000u);
    else edx = (int32_t)((uint32_t)edx >> 2);
    return edx + (int32_t)((uint32_t)edx >> 31);
}

/* 0x85fa00 then the wage at 0x83fc09..0x83fc2e. 0 when there is no contract or its type is not 2. */
static int32_t wage_call(uint32_t frame, uint32_t contract, uint32_t ebx, uint32_t ebp, uint32_t esi, uint32_t edi)
{
    uint32_t call = frame - 4u;
    wr32(g_mem, call, 0x83fc10u);
    cpu.esp = call;
    cpu.ecx = contract;
    cpu.ebx = ebx;
    cpu.ebp = ebp;
    cpu.esi = esi;
    cpu.edi = edi;
    f_0085fa00();
    sl_esp(frame, "0x85fa00");
    int32_t days = (int32_t)(int16_t)(uint16_t)cpu.eax;
    int32_t per = (int32_t)rd32(g_mem, contract + 0xcu);
    return div7((int32_t)((int64_t)days * (int64_t)per));
}

/* Date::days_since 0x54cdb0 reads the 8-byte date at the contract + 0x2d (0x417c10 copies those bytes) and the
 * two date words at 0xae2c90 and 0xae2c94. Its scratch at 0xae2470 is written only by that function. The contract
 * object is index * 80 + [contract manager] (0x4cf5d5). One slot walks the same contracts as the last, so the
 * wage is kept by that index until the date, the rate at +0xc or the contract date changes. */
#define WAGE_CAP 262144u
static int32_t *wage_val;
static uint32_t *wage_rate;
static uint64_t *wage_date;
static uint8_t *wage_set;
static uint32_t wage_day0, wage_day1;

static int32_t wage_of(uint32_t frame, uint32_t contract, uint32_t ebx, uint32_t ebp, uint32_t esi, uint32_t edi)
{
    if (!contract) return 0;
    if ((rd8(g_mem, contract + 0x35u) & 0x3fu) != 2u) return 0;
    uint32_t day0 = rd32(g_mem, 0xae2c90u), day1 = rd32(g_mem, 0xae2c94u);
    uint32_t base = rd32(g_mem, rd32(g_mem, 0xae1050u));
    uint32_t idx = contract - base;
    int direct = base && idx % 80u == 0 && (idx /= 80u) < WAGE_CAP;
    if (direct && (!wage_val || !wage_rate || !wage_date || !wage_set)) {
        if (!wage_val) wage_val = malloc(WAGE_CAP * sizeof *wage_val);
        if (!wage_rate) wage_rate = malloc(WAGE_CAP * sizeof *wage_rate);
        if (!wage_date) wage_date = malloc(WAGE_CAP * sizeof *wage_date);
        if (!wage_set) wage_set = calloc(WAGE_CAP, 1);
        if (!wage_val || !wage_rate || !wage_date || !wage_set) direct = 0;
    }
    if (direct && (day0 != wage_day0 || day1 != wage_day1)) {
        memset(wage_set, 0, WAGE_CAP);
        wage_day0 = day0;
        wage_day1 = day1;
    }
    uint32_t rate = rd32(g_mem, contract + 0xcu);
    uint64_t date = rd64(g_mem, contract + 0x2du);
    if (direct && wage_set[idx] && wage_rate[idx] == rate && wage_date[idx] == date) {
        sl_wage_hit++;
        return wage_val[idx];
    }
    int32_t w = wage_call(frame, contract, ebx, ebp, esi, edi);
    if (direct) {
        wage_val[idx] = w;
        wage_rate[idx] = rate;
        wage_date[idx] = date;
        wage_set[idx] = 1;
    }
    sl_wage_miss++;
    return w;
}

/* The common path of 0x600a50: a computer club that is not a national side. The same writes as
 * human_club_host, relative to the call's esp (the return address is at call). */
static int human_fast(uint32_t club, uint32_t call, uint32_t ebx, uint32_t ebp, uint32_t staff, uint32_t edi)
{
    if (!club) return 0;
    uint32_t mgr = rd32(g_mem, club + 0xcfu);
    if (mgr && (int32_t)rd32(g_mem, mgr) >= (int32_t)(rd32(g_mem, 0xae2364u) - 0x10u)) return 0;
    uint32_t limit = rd32(g_mem, 0xae235cu) - rd32(g_mem, 0xae2350u);
    if ((int32_t)rd32(g_mem, club) >= (int32_t)limit) return 0;
    wr32(g_mem, call - 0x204u, ebx);
    wr32(g_mem, call - 0x208u, ebp);
    wr32(g_mem, call - 0x20cu, staff);
    wr32(g_mem, call - 0x210u, edi);
    wr32(g_mem, call - 0x214u, club);
    wr32(g_mem, call - 0x218u, 0x600bebu);
    wr32(g_mem, call - 0x41cu, mgr);
    return 1;
}

static void prepare_human_call(uint32_t call, uint32_t club, uint32_t ebx, uint32_t ebp, uint32_t staff, uint32_t edi)
{
    wr32(g_mem, call + 4u, club);
    wr32(g_mem, call + 8u, 1u);
    wr32(g_mem, call + 12u, 1u);
    wr32(g_mem, call + 16u, 0u);
    wr32(g_mem, call + 20u, 0u);
    wr32(g_mem, call, 0x83fab8u);
    cpu.esp = call;
    cpu.ebx = ebx;
    cpu.ebp = ebp;
    cpu.esi = staff;
    cpu.edi = edi;
}

/* 0 when the list is finished, 1 at 0x83fcbb, 2 at 0x83fabf (the human club path), 3 to run this
 * candidate from 0x83fa25. cpu.ecx is the index and cpu.edx the list; cpu.esp is the frame. */
int rt_sl_streak(void)
{
    sl_report_once();
    if (sl_disabled()) return 3;
    uint8_t *m = g_mem;
    uint32_t frame = cpu.esp, ebp = cpu.ebp, list = cpu.edx, index = cpu.ecx;
    uint32_t count = rd32(m, frame + 0x8cu);
    uint32_t slot = rd32(m, ebp + 8u), club = rd32(m, ebp + 0xcu), excluded = rd32(m, ebp + 0x10u);
    uint32_t edi = cpu.edi;
    uint32_t path = rd32(m, frame + 0x14u);
    uint32_t human_limit = rd32(m, 0xae2364u) - 0x10u;
    double scale = rdf64(m, 0x968de8u);
    int32_t rep = (int32_t)(int16_t)rd16(m, club + 0x80u);
    int32_t wage_cap2 = (int32_t)((uint32_t)(rep + 0x1f4) * 10u);
    uint32_t nation_ban = rd32(m, 0x9cf458u);
    uint8_t slot_byte = rd8(m, slot + 0x24u);

    while ((int32_t)index < (int32_t)count) {
        sl_seen++;
        if ((index & 3u) == 0) rt_shortlist_ahead(m, list, index, count);
        uint32_t entry = list + index * 12u;
        uint32_t id = rd32(m, entry);
        uint32_t staff = rd32(m, 0xae23bcu) + id * 110u;
        if (staff == excluded || (int32_t)rd32(m, staff) >= (int32_t)human_limit
            || rd8(m, staff + 0x3du) == 5u || rd8(m, staff + 0x28u) == 5u) {
            sl_rej++;
            index++;
            continue;
        }
        uint32_t job = rd32(m, staff + 0x69u);
        uint32_t vac = rd32(m, 0xb67a44u);
        if (job && vac) {
            uint32_t call = frame - 8u;
            wr32(m, call + 4u, staff);
            wr32(m, call, 0x83fa93u);
            cpu.esp = call;
            cpu.ecx = vac;
            cpu.ebx = entry;
            cpu.ebp = ebp;
            cpu.esi = staff;
            cpu.edi = edi;
            f_0069fc60();
            sl_esp(frame, "0x69fc60");
            if (cpu.eax & 0xffu) {
                sl_rej++;
                index++;
                continue;
            }
        }
        uint32_t shown = 0;   /* [esp+0x34] at 0x83fcbb: the wage-path contract, else 0 */
        if (path == 0) {
            if (!job) { sl_rej++; index++; continue; }
            shown = contract_of(staff, rd32(m, 0xae1050u));
            edi = shown;
            int32_t wage = wage_of(frame, shown, entry, ebp, staff, edi);
            int32_t cap = (int32_t)rd32(m, slot + 7u);
            if (wage >= cap || wage > wage_cap2) { sl_rej++; index++; continue; }
        } else {
            edi = club;
            uint32_t call = frame - 24u;
            if (!human_fast(club, call, entry, ebp, staff, edi)) {
                prepare_human_call(call, club, entry, ebp, staff, edi);
                f_00600a50();
                sl_esp(call + 4u, "0x600a50");
                if (cpu.eax) {
                    wr32(m, frame + 0x24u, entry);
                    wr32(m, frame + 0x34u, 0);
                    wr32(m, frame + 0x40u, index);
                    cpu.esp = frame;
                    cpu.ebp = ebp;
                    cpu.ebx = entry;
                    cpu.esi = staff;
                    cpu.ecx = index;
                    cpu.edx = list;
                    sl_human++;
                    return 2;
                }
                edi = cpu.edi;
            }
            if (rd8(m, staff + 0x3du) != 0x0bu) { sl_rej++; index++; continue; }
            uint32_t p24 = rd32(m, staff + 0x24u);
            if (p24 && rd8(m, staff + 0x28u) == 5u) { sl_rej++; index++; continue; }
            uint32_t p61 = rd32(m, staff + 0x61u);
            if (!p61) { sl_rej++; index++; continue; }
            uint8_t st = rd8(m, rd32(m, 0xae2d0cu) + rd32(m, staff) * 79u + 0xbu);
            if ((st & 0x30u) == 0x10u) { sl_rej++; index++; continue; }
            uint32_t nat = rd32(m, staff + 0x1au);
            if (nat && rd32(m, nat) == nation_ban) { sl_rej++; index++; continue; }
            if ((slot_byte & 0x1eu) == 0x0au && (int8_t)rd8(m, staff + 0x18u) > 0x17) {
                sl_rej++;
                index++;
                continue;
            }
            if (slot_byte & 1u) {
                int32_t value = (int32_t)rd32(m, entry + 8u);
                uint32_t cur = rd32(m, staff + 0x39u);
                if (!(cur && value <= 0)) {
                    uint32_t c2 = frame - 12u;
                    wr32(m, c2 + 4u, staff);
                    wr32(m, c2 + 8u, 0);
                    wr32(m, c2, 0x83fbd2u);
                    cpu.esp = c2;
                    cpu.ebx = entry;
                    cpu.ebp = ebp;
                    cpu.esi = staff;
                    cpu.edi = edi;
                    f_00544590();
                    sl_esp(frame - 8u, "0x544590");
                    if (!cpu.eax) { sl_rej++; index++; continue; }
                }
            } else {
                double prod = (double)(int32_t)rd32(m, entry + 8u) * scale;
                double need = (double)(int32_t)rd32(m, slot + 7u);
                if (!(need >= prod)) { sl_rej++; index++; continue; }
            }
        }
        if (rd32(m, staff + 0x39u) == club) { sl_rej++; index++; continue; }
        uint32_t contract = contract_of(staff, rd32(m, 0xae1050u));
        wr32(m, 0xae1080u, contract);
        if (club_from_contract(contract) == club) { sl_rej++; index++; continue; }
        if ((rd8(m, rd32(m, 0xae2d0cu) + rd32(m, staff) * 79u + 0xbu) & 0x0fu) >= 10u) {
            sl_rej++;
            index++;
            continue;
        }
        wr32(m, frame + 0x24u, entry);
        wr32(m, frame + 0x34u, shown);
        wr32(m, frame + 0x40u, index);
        cpu.esp = frame;
        cpu.ebp = ebp;
        cpu.ebx = entry;
        cpu.esi = staff;
        cpu.edi = path == 0 ? shown : edi;
        cpu.ecx = index;
        cpu.edx = list;
        sl_pass++;
        return 1;
    }
    wr32(m, frame + 0x40u, index);
    cpu.esp = frame;
    cpu.ebp = ebp;
    cpu.edi = edi;
    cpu.ecx = index;
    cpu.edx = list;
    return 0;
}

void rt_sl_note(uint32_t staff_id)
{
    if (sl_log) fprintf(sl_log, "%u\n", staff_id);
}

int rt_ft_clear(uint32_t self, uint32_t staff)
{
    if (!self || !staff) return 0;
    uint32_t base = rd32(g_mem, self + 0x86eu);
    if (!base) return 0;
    uint32_t id = rd32(g_mem, staff);
    if (rd8(g_mem, base + id * 8u) & 0x10u) return 0;
    sl_ft++;
    return 1;
}

static void sl_report(void)
{
    if (!sl_seen && !sl_ft) return;
    fprintf(stderr, "recomp: shortlist scan: %llu candidates, %llu rejected here, %llu to the tail, "
            "%llu human, %llu future-transfer skips, wage %llu hits %llu new\n",
            (unsigned long long)sl_seen, (unsigned long long)sl_rej, (unsigned long long)sl_pass,
            (unsigned long long)sl_human, (unsigned long long)sl_ft,
            (unsigned long long)sl_wage_hit, (unsigned long long)sl_wage_miss);
    if (sl_log) fclose(sl_log);
}

static void sl_report_once(void)
{
    static int armed;
    if (!armed) { armed = 1; atexit(sl_report); }
}

#ifdef __GNUC__
static void sl_arm(void) __attribute__((constructor));
static void sl_arm(void) { sl_report_once(); }
#endif
