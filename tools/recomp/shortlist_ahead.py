"""Bring the next shortlist candidates' rows in before the loop reads them.

fill_shortlist_slot 0x83f3a0, at the head of the candidate loop 0x83fa25 (lea eax, [ecx+ecx*2]).
edx is the position list and ecx is the index; the count is the dword at esp+0x8c. The entry is
12 bytes and its first dword is the staff id. The staff record (110 bytes at [0xae23bc] + id*110,
the scale at 0x83fa39) and the contract row (79 bytes at [0xae2d0c] + id*79, the scale at
0x83fb46) are the loads the loop misses. rt_shortlist_ahead reads those addresses for the next
candidates and does not write. A December 2025 start with every league, the clock fixed, signed
the same 3558 staff as the loop without it.
"""

INSNS = {
    0x83FA25: (
        "lea eax, [ecx + ecx*2]",
        "rt_shortlist_ahead(M, edx, ecx, R32(esp + 0x8cu)); "
        "eax = (uint32_t)(ecx + ecx * 2u);",
    ),
}
