"""The shortlist search of fill_shortlist_slot 0x83f3a0.

At 0x83fa25 (lea eax, [ecx+ecx*2], ecx the index and edx the list) the host walks every candidate the
recompiled loop would reject before 0x83fcbb, and returns to that loop only for a candidate who reaches
the tail. The tests and the calls are the ones at 0x83fa25..0x83fcb5 (shortlist_scan.c). RECOMP_SL_FAST=0
runs the recompiled loop.

At 0x83fecb (call get_future_transfer 0x8cca10) a clear bit 0x10 returns 0 from the call site. The clear
path (0x8cca50 to 0x8ccb02) builds an empty list, destroys it and returns 0, and on this start it rejects
nobody. The local at esp+0x84 is 0 for the whole call (the store at 0x83f3d1), so the branch at 0x83fee1
is not taken and ecx and edx are written before they are read. The two arguments are already on the stack.

At 0x840ad0 the chosen staff id is stored. RECOMP_SL_LOG names a file that receives one id per line.
"""

INSNS = {
    0x83FA25: (
        "lea eax, [ecx + ecx*2]",
        "SYNC();"
        "int sl_how = rt_sl_streak();"
        "LOAD();"
        "if (sl_how == 0) goto L_840a99;"
        "if (sl_how == 1) goto L_83fcbb;"
        "if (sl_how == 2) goto L_83fabf;"
        "eax = (uint32_t)(ecx + ecx * 2u);",
    ),
    0x83FECB: (
        "call 0x8cca10",
        "if (R32(esp + 0x8cu) == 0 && rt_ft_clear(ecx, R32(esp))) {"
        " eax = 0; esp += 8;"
        "} else {"
        " esp -= 4; W32(esp, 0x83fed0u); SYNC(); f_008cca10(); LOAD(); RT_AFTER_CALL();"
        "}",
    ),
    0x840AD0: (
        "mov dword ptr [esi + 0xf], eax",
        "W32(esi + 0xfu, eax); rt_sl_note(eax);",
    ),
}
