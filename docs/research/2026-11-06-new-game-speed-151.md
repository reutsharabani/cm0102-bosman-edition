# New game speed, second pass (0.6.2 item 151)

Max saw a new game take a long time on "Player histories", the shortlists and the other progress steps, with a 2025/26 start of England, Germany, Italy and Spain (3.9.68 data). This page gives the time of each step on main and on the branch `claude/s062-151-newgame-speed`, where the time goes, and the changes. No change alters a game decision (rule Q152). The first pass is `2026-10-29-new-game-speed.md`.

## How the times are measured

The driver makes one new game with `RECOMP_PROGRESS_LOG=1` and `RECOMP_SAVE_AFTER_DAYS=0` (the new game leg of `tests/seasons/soak_run.py`), with seed 1, the dummy video and audio drivers and `changing_rules = on`. A step ends at the next progress text. Two steps hold waits of the test script: "Loading database" holds the clicks on Select League(s), and "Creating Shortlists" holds the typing of the manager's name. The table gives the time of the game's own work for those two steps: from the progress text to the wait of the script. A run with the real window (SDL cocoa) gives the same times as the dummy driver.

Other sessions ran games at the same time (load 15 to 22), so the times change by about 10 % between runs. Each "before" and "after" pair below ran at the same time.

## Times (seconds)

Main (22e068ce, the game code of db62ef0d) and the branch ran at the same time, load about 22, 3.9.68 data.

| Step | 4 nations, main | 4 nations, branch | All nations, main | All nations, branch |
|---|---|---|---|---|
| Player Initialisation | 3.6 | 4.5 | 7.7 | 8.9 |
| Initialising game data (1) | 0.8 | 0.8 | 0.6 | 0.7 |
| Player histories | 4.7 | 2.4 | 23.7 | 16.9 |
| Initialising game data (2) | 1.8 | 1.3 | 16.3 | 3.4 |
| Initialising Leagues | 4.3 | 4.8 | 32.2 | 30.1 |
| Assessing Club Squads | 1.3 | 1.3 | 2.3 | 2.4 |
| Initialising game data (3) | 2.2 | 1.4 | 3.5 | 1.9 |
| Up to the manager's name, and after it | 29.3 + 2.3 | 27.7 + 1.5 | 141.6 in all | 120.2 in all |

"Loading database" (7.5 s with all nations) and "Creating Shortlists" (47.8 and 48.4 s with all nations) did not change. Player Initialisation varies by about 1 s from run to run under this load.

Max's own case is slower for another reason. His settings use `database = folder:December 2025`, an Update Team data set. With a copy of his user folder, the 4-nation 2025/26 start took 722 s on the 0.6.0 app he has installed, 94.9 s on main and 85.5 s on the branch, up to the manager's name. The leftover .tmp files of his folder made no difference (99.0 s without them on main), and a fresh folder with only that data set took 89.8 s on main. With that data set "Player histories" is 88 % the sort after each new row: 28,339 sorts of about 59,000 rows. That sort is changed on the branch `claude/s062-151b-history-sort` (item 151b, below).

## The history sort (item 151b)

With Max's data set the rows before the new row are always in strict order (no two compare equal), and the new row lands anywhere. Then the rows after the sort are known: the old rows with the new row at its place. `qsort6_insert.c` writes them so, and finds the stack words that the sort leaves and its eax by a walk of the sort's steps backwards in time, which skips every part that can no longer write a word that is still open. Parts in order need no compare. The file's head comment gives the reasoning. In a test harness 20,000 random sorts of 2 to 70,000 rows gave the same words and eax as the full steps, with about 24 parts visited in place of about 15,000. RECOMP_QSORT=check: 28,339 sorts equal with the December 2025 data set, 8,322 with the 3.9.68 data and all nations. "Player histories" with the December 2025 data set: 35.9 s to 3.8 s.

## Where the time went on main

A `sample` of each step (main thread):

- "Creating Shortlists": the own code of fill_shortlist_slot 0x83f3a0 (35 %), human_manager_is_human_club 0x600a50 (13 %), club_is_national_team 0x5409b0 (6 %) and player_needs_work_permit 0x8c7560 (6 %), below create_initial_shortlists 0x83a1b0 and allocate_transfer_budget 0x83b6e0. 0x83f3a0 calls 0x600a50 at 0x83fab3 for each candidate, with the club that makes the shortlist.
- "Initialising Leagues": game_run_to_start_date 0x5cc850 plays the matches before the start date: play_matches 0x6b4360, pick_team 0x6dbc50, score_player_for_role 0x6e2f20 and the match engine 0x6b98c0. Not changed.
- "Player histories": the sort after each new row (add_language 0x7c6a30, qsort 0x945333 in host code since 0.6.1) took 72 % with all nations. With four nations the file work took most of the step: PlayerStats::PlayerStats 0x7bfd50 opens player stats history.tmp 16,816 times through FILE_LLIST::add_node_list 0x58e9b0, each time with a path lookup.
- The second "Initialising game data": is_retirement_pending 0x7ae350 (already a host copy, a linear scan of the retirement list) took 80 %, from is_staff_suitable_for_job 0x69f810 in add_job_vacancy 0x69ba50: 790,068 searches with all nations.
- "Player Initialisation": the own code of database_player_initialisation 0x535a80 (49 %) and database_player_to_attribute_vector 0x53a610 (36 %), about 20 million calls with all nations.
- The last "Initialising game data": FOG_OF_WAR_MAP::FOG_OF_WAR_MAP 0x5aff00 reads the history of each player (build_career_history 0x7c3e40, FILE_LLIST::get_nodes 0x58f120) and opens the history file for read 14,706 times with four nations.

## The changes

All are in `src/recomp_rt/win/hot_loops.c` and `files.c`. The code comments give the evidence for each.

1. An index for 0x7ae350. While a call of add_job_vacancy 0x69ba50 runs, the host copy finds the row in a hash index of the ids (the first row of each id), with the same eax, edx and stack. The index is made again when the list, its rows pointer or its count change. Every member of PLAYER_REGEN that may write the list drops it, whatever calls the member. It lives only while esp is below the esp of the outermost 0x69ba50.
2. A host copy of the common path of 0x600a50: a club of the computer that is no national side. Every other path runs the recompiled code.
3. A host copy of 0x53a610: no branch on the data, 77 writes in the order of the code.
4. The kept history file. The season change 0x7c1350 kept player stats history.tmp open since 0.6.2 (81c036e6). Now the scopes 0x7bfd50 ("Player histories") and 0x5aff00 (the last "Initialising game data") keep it too, for write and for read. A new open with the same name, access and disposition takes the kept file at position 0 without the path lookup. Every delete, rename, new folder, change of the current folder and memory file closes the kept files.

A fifth change was tried and dropped: a host copy of the first tests of the candidate loop of 0x83f3a0 (at 0x83fa25, through an INSNS entry). The check mode passed (899,815 loop heads equal with all nations), but most candidates pass those tests, and the step was not faster. The patch is in the run logs.

## Switches and checks

- `RECOMP_HOT=original` turns off changes 1 to 3; `RECOMP_HOT=check` runs the copy and the recompiled code on each call and stops at the first difference. `RECOMP_KEEP_FILES=0` turns off change 4. The same-save runs host-loops-off and files-off use them.
- ctest qsort6 runs the self-test (`RECOMP_QSORT_TEST`): the index on lists of up to 3,000 rows with repeated ids, a stale row and a search from an older frame; each common case of 0x600a50 and the bound 4984; 0x53a610 on random records.
- RECOMP_HOT=check on a 2025/26 start with all nations: 204,468,233 calls equal.
- Same new-game save as main: a 2001/02 base game of England, a 2025/26 start of the four nations (byte for byte), and a 2025/26 start with all nations (one file differs between two runs of main too, see below).
- A run of the four nations to July 2026, across a season change, with RECOMP_HOT=check: 1,365,775,019 calls equal, and the save the same as main's.
- The same-save set against main (`--ref-build`, `--jobs 1`): new game, start.sav and prematch.sav PASS. The match check gave one difference, in season-code-off, at a load that rose to 18.4; run again at load 15 it passed on main and on the branch, and ref-build was the same as the branch in that check.

## A fault found on the way

The save part bosman_season.dat of a new game holds the changed competition names (season_change.c, the renames). Each name is kept with snprintf in a realloc block of 51 bytes, so the bytes after the name's end are not set, and the save writes them. Two all-nations runs of main gave two saves that differ only in those bytes. The game reads only up to the end of the name, so no decision changes, but the save is not the same from run to run. Fixed on its own branch (open item 152).

## What is left

- "Creating Shortlists" is still the longest step. After the vacancy index below, the time that is left is the straight code of fill_shortlist_slot 0x83f3a0, get_future_transfer 0x8cca10 and the contract loads of 0x4cf5a0.
- "Initialising Leagues" with all nations is match play before the start date.
- "Player histories" with all nations still sorts the whole list after each new row. A faster exact sort must give the same order of equal rows and the same stack bytes.

## The vacancy list (8 November 2026)

A December 2025 start, every league, season 2025/26, `faster_days` and `changing_rules` on, dummy video and audio. The clock is thread CPU, so the holiday and the clicks are not in it. The same run's progress clock is in brackets. Before is the tree without the index. After is the index and a host copy of get_contract 0x4cf5a0.

| Step | Before | After |
|---|---|---|
| Creating Shortlists | 55.6 s (66.6 s) | 45.5 s (56.5 s) |
| Initialising Leagues | 20.6 s (20.5 s) | 21.4 s (21.3 s) |
| Create game, other than the leagues | 16.5 s | 16.0 s |

The index was built 104 times and answered 217,716,172 searches. The walk of 0x69fc60 left the top of the samples. get_contract did not: its hottest instruction was 3,326 samples before and 3,368 after, so that time is the loads of the contract table, not the translated instructions. The host copy stays because it gives the same registers and the check covered it.

Played days did not fall by a clear amount. Dates 192 to 204 of 2025 (11 to 23 July) took 21.8 s in matches before and 22.0 s after, and 5.3 s and 5.9 s in the daily staff update. The raw day totals are 98.1 s and 98.2 s, and the second run played further into 24 July, because the start finished sooner and the run ends on a fixed wait. The top of the day samples is now the body of 0x83f3a0, the same contract loads, get_future_transfer 0x8cca10 and the matches.

RECOMP_HOT=check on that start, through the shortlists and on to 18 July: 930,535,131 calls equal, and the program exited 0. The self-test compares the index with the recompiled walk, including a changed tail id, a dropped index and a shorter count.

What is still slow, from the samples after the change (Creating Shortlists, 45.5 s):

- fill_shortlist_slot 0x83f3a0 itself. The hottest instruction, a loop compare at +0x2a43 of the recompiled function, is 9.1 %. A copy of only the first tests of that loop was tried in November and did not help.
- get_contract 0x4cf5a0, now host code, 7.4 %. One load from the contract table per candidate.
- get_future_transfer 0x8cca10, 4.5 %. It builds a temporary linked list and walks it. A host copy has to match that frame, including the SEH words.
- Initialising Leagues is still the match engine (17.3 s of 21.4 s). A split across cores has to keep the one RNG cursor 0x90cfc0 and the writes to the shared tables in the same order, or the game changes. The same RNG is still in the played-day samples. The match engine is about a quarter of a played day.

## What was tried on the candidate loop (8 November 2026)

The same December 2025 start, every league, 2025/26. Creating Shortlists is one call of fill_shortlist_slot 0x83f3a0 per slot (8577 slots). The candidate loop is 0x83fa25..0x840a99. The staff index the slot keeps is the store at 0x840ad0. A log of that index, with the loop left as the recompiled code, is 3552 lines. That log is the behaviour check below. Thread CPU of that step on the vacancy-index binary is 45.5 s.

sort_position_lists 0x83a780 sorts each list with shortlist_compare_by_value_then_key 0x83b670 (the pushes at 0x83a98b and 0x83a9a4, the call 0x945333). The compare returns the signed difference of the dwords at entry + 8. The reject at 0x83fbad is the value test at 0x83fb97 (the slot requirement at slot + 7 against that dword times the double at 0x968de8). Ending the candidate loop at the first such reject, on the theory that every later entry costs at least as much, changed the log (3616 lines, 3503 positions different) and the step was 48.3 s. A later entry is still chosen after an earlier one has failed that test, so the list is not a safe cutoff while the loop runs.

A host walk that rejects a candidate before 0x83fcbb and then runs the recompiled body for the rest was measured the same way:

| Host rejects | Shortlist lines | Positions different from the recompiled loop | Creating Shortlists |
|---|---|---|---|
| Same player, a human, job bytes 0x3d and 0x28 equal to 5, a vacancy, and a candidate with no link on the path from 0x83fbdf | 3552 | 0 | 61.1 s |
| Those, plus the computer-club tests from 0x83fb1e through the age test at 0x83fb8d | 3567 | 3497 | 48.4 s |
| Those, plus the loan test 0x544590, the value test and the club and contract-nibble rejects at 0x83fc56..0x83fcb5 | 3552 on one build, then 3641 after the same source was rebuilt | 0, then 3516 | 45.1 s, then 45.8 s |

The walk that matched spent its time on the same random staff and contract loads as the prefix it skipped, and the call at the top of every candidate made the step slower (61.1 s against 45.5 s). The tests after 0x83faa7 are not free of the call they skip. 0x600a50's path at 0x600be5 calls club_is_national_team 0x5409b0 before it returns. Skipping that call and applying the later rejects changed which staff were shortlisted. Rejoining the recompiled body at 0x83fcbb, and a host wage for the path from 0x83fbdf (0x85fa00 and Date::days_since 0x54cdb0), also changed the log (3641 and 3592 lines). The wage run was the only one under 45 s (37.4 s) and it shortlisted different staff.

Compiling the generated file that holds 0x83f3a0 at -O3, with the loop unchanged, also changed the log (3515 lines) and the step was 51.3 s. The generated files stay at -O1.

Initialising Leagues was 21.4 to 21.8 s in these runs, of which the matches were about 17.5 s. Nothing in the candidate loop changes that. The samples there are spread across the match engine, not one instruction. A faster match has to play the same match. The RNG at 0x90cfc0 can move and the season still has to read as the same kind of game; the shortlist log cannot, because a different staff index is a different signing.

## The screens and the simulation are different calls

The human turn is `game_run_human_turn` 0x5cde40 (the day loop calls it at 0x5cdbf9 and 0x5cdc0a). It pushes the news or team screen and runs the screen loop. The start of a game does not. `game_run_to_start_date` 0x5cc850 calls `play_matches` 0x6b4360 at 0x5cc96e and `create_initial_shortlists` 0x83a1b0 at 0x5ccc91. `play_matches` opens the live match screens only for a match that is watched. The shortlist search does not open a screen. The Bosman Edition already replaces logic of that kind (the season rules, the vacancy index) and leaves the screens on the recompiled code. The check is the recompiled function, or the staff index stored at 0x840ad0.

A count of where each candidate of the December 2025 shortlist leaves the loop, with the loop itself unchanged (266,378,247 candidates):

| Where | Candidates |
|---|---|
| Wage reject 0x83fc34, or the cap reject 0x83fc50 | 52,713,469 and 8,020,518 |
| Past the cheap filters, at 0x83fcbb | 87,037,590 |
| Leave at 0x83fd51, 0x83fdae, 0x83fdca, 0x83fdf8 or 0x83fe26, before the future-transfer call | 16,038,974, 2,245, 1,581,560, 12,729 and 416,615 |
| Call get_future_transfer 0x8cca10, and none of them is rejected at 0x83fed2 | 68,985,467 |
| Leave at 0x83ff4f, 0x83ff7c, 0x83ffa3, 0x83ffca, 0x840009 or 0x84001f | 11,302,332, 13,339,783, 7,344,388, 4,495,972, 461,854 and 340,003 |
| Leave at 0x840058 | 19,859,770 |
| Leave at 0x840142 | 10,781,514 |
| Reach value_player_for_club 0x840d80, and a player is chosen at 0x840943 | 3,746,852 and 7,587 |

The largest single exit after the cheap filters is 0x840058. Only 3.7 million reach the valuation. Skipping every instruction after 0x83fcbb, on a later run of the same start, left Creating Shortlists at 30.3 s (268,356,301 candidates, and no one was signed, because the tail is where the choice is made). The code before 0x83fcbb is the larger part of the step. The 87 million are the ones who pay for both.

get_future_transfer 0x8cca10 tests the byte at `[[this+0x86e] + id*8]` (0x8cca4c). Bit 0x10 clear jumps to 0x8ccb02, destroys the empty list and returns 0 (0x8ccb13, ret 8 at 0x8ccb27). The list constructor 0x68d020 and an empty destructor 0x68d040 (the head test at 0x68d048) touch only that stack list. A replacement that returned 0 on a clear bit, without building the list, took that path for every call of the run (68,769,136 clear, none searched). Creating Shortlists was 40.2 s. The staff log was 3616 lines and the same file as the value-cutoff run above, so the signings were not the recompiled loop's. The replacement is not in the game. A later return that does match the epilogue was checked in the process: 68,237,279 clear calls, the registers, esp and fs:[0] the same on every one, and the staff index at 0x840ad0 the same 3558 lines as the recompiled function with the clock fixed. Creating Shortlists was 46.9 s. The clear path's cost is the load of that bit, which the return still does. It is not in the game either.

## The next rows, brought in early (10 November 2026)

The same start, the clock fixed (`RECOMP_FIXED_CLOCK`), every league. The candidate loop's staff record and contract row are not next to the list entry, and the list is ordered by value, not by id, so each candidate misses. At 0x83fa25 the loop now asks the host to bring in the staff record (both cache lines of the 110 bytes) and the contract row of the next six candidates. Nothing is written.

| | Creating Shortlists | Candidates | Staff indexes at 0x840ad0 |
|---|---|---|---|
| The loop, clock fixed | 46.9 s | 260,321,401 | 3558 lines |
| The same loop, the next rows brought in | 39.6 s | 260,321,401 | the same 3558 lines |

A copy of the contract index word at `[0xae2d0c] + id*79` stayed equal to the table for every `get_contract` of that run (115,154 ids, 0 differences). The status byte at +0xb of the same row did not: 1,494,317 loads differed from a copy taken at the start of the search. Using the word copy in `get_contract` took 41.9 s and was left out. The status byte is still read from the row.

## The prefix of the candidate loop, in the host (11 November 2026)

The same December 2025 start, every league, season 2025/26, `faster_days` and `changing_rules` on, the clock fixed, seed 1, dummy video and audio. Creating Shortlists is the time from the progress text to the manager-name screen. The staff file is the id stored at 0x840ad0, one per line.

At 0x83fa25 the host walks the position list (`shortlist_scan.c`). A candidate who fails a test the recompiled loop fails at 0x83fa4f..0x83fcb5 is not returned to that loop. A candidate who reaches 0x83fcbb returns there, with the staff in esi, the entry in ebx, the contract or the club in edi, and the stack slots at +0x24, +0x34 and +0x40 set as the tail reads them. The human-club path returns at 0x83fabf. The end of the list returns at 0x840a99. The calls stay in the original order: the vacancy search 0x69fc60, `human_manager_is_human_club` 0x600a50 (the computer club writes the same scratch as the host copy and every other case calls the function), the wage 0x85fa00, the loan test 0x544590, and `get_contract` 0x4cf5a0. `RECOMP_SL_FAST=0` runs the recompiled loop from the `lea`.

The wage is the signed day count from 0x85fa00 times the rate at contract + 0xc, divided by 7 as the `imul` at 0x83fc1c does. `Date::days_since` 0x54cdb0 reads the 8-byte date at contract + 0x2d and the game date at 0xae2c90, and its scratch at 0xae2470 is written only by that function. The result is kept by the contract index (`index * 80 + [contract manager]`, 0x4cf5d5) until the game date, the rate or that 8-byte date changes. The first use of a contract still calls 0x85fa00.

At 0x83fecb, bit 0x10 clear in `[[this+0x86e] + id*8]` returns 0 from the call site. The clear path at 0x8ccb02 destroys the empty list and returns 0, and it does not store that byte. With the local at esp+0x84 equal to 0 the branch at 0x83fee1 is not taken, and the fall-through writes ecx and edx before they are read.

`update_club_shortlist` 0x83a730 calls `allocate_transfer_budget` 0x83b6e0 at 0x83a76b, and that calls `fill_shortlist_slot` at 0x83bab7. `update_staff_status` 0x843e10 calls it at 0x844108. A day that rebuilds shortlists pays for this same walk. The matches played while the leagues are initialised are the other large cost of reaching the start date, and of a played day after it.

| | Creating Shortlists | Staff ids at 0x840ad0 | Initialising Leagues, thread CPU |
|---|---|---|---|
| Recompiled loop (`RECOMP_SL_FAST=0`) | 38.4 s | 3773 lines | 19.8 s |
| The prefix in the host | 28.8 s | the same 3773 lines | 19.6 s |
| The prefix, and the wage kept by contract | 24.8 s | the same 3773 lines | 19.1 s |

The host saw 261,960,293 candidates, rejected 176,628,747 of them, and sent 85,331,546 on to the tail. No candidate took the human-club path during this start (the manager is named after the search). The future-transfer bit was clear for 67,282,979 calls. The wage was computed for 25,138 contracts and reused 84,765,336 times. This tree's position lists are longer than the lists in the table above (260,321,401 candidates, 3558 lines): the check is the two runs against each other.

The time that is left in Creating Shortlists is the tail after 0x83fcbb, where the roll at 0x90cfc0 and `value_player_for_club` 0x840d80 choose the staff. Those calls stay in order, so the same staff are signed. A played day still spends its match time in the match engine. Initialising Leagues, which is that match play before the start date, stayed on 19.1 to 19.8 s.
