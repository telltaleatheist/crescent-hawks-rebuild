# Function map

Every function Ghidra finds in the unpacked `BTECH.EXE` (docs/original.md), by segment.
Addresses are `segment:offset` with the program loaded at 0x1000; *linear* is Ghidra's flat
address. *Span* is the distance to the next function (an upper bound on its size). *Label* and
*Status* fill in as each function is read and rebuilt. Plain words only: no decompiled code.

Status: `-` not read, `read` understood and documented, `rebuilt` in `src/`, `replaced` (hardware,
DOS or run-time library: stood in for by the platform layer or the C++ library).


## CODE_0: segment 1000, 21,114 bytes

| Address | Linear | Ghidra name | Span | Label | Status |
|---|---|---|---|---|---|
| 1000:0000 | 0x10000 | `switchD_1000:f462::caseD_6` | 45 |  | - |
| 1000:002d | 0x1002d | `FUN_1000_002d` | 142 |  | - |
| 1000:00bb | 0x100bb | `FUN_1000_00bb` | 313 |  | - |
| 1000:01f4 | 0x101f4 | `FUN_1000_01f4` | 807 |  | - |
| 1000:051b | 0x1051b | `FUN_1000_051b` | 2352 |  | - |
| 1000:0e4b | 0x10e4b | `FUN_1000_0e4b` | 2279 |  | - |
| 1000:1732 | 0x11732 | `FUN_1000_1732` | 137 |  | - |
| 1000:17bb | 0x117bb | `FUN_1000_17bb` | 92 |  | - |
| 1000:1817 | 0x11817 | `FUN_1000_1817` | 88 |  | - |
| 1000:186f | 0x1186f | `FUN_1000_186f` | 172 |  | - |
| 1000:191b | 0x1191b | `FUN_1000_191b` | 164 |  | - |
| 1000:19bf | 0x119bf | `FUN_1000_19bf` | 30 |  | - |
| 1000:19dd | 0x119dd | `FUN_1000_19dd` | 22 |  | - |
| 1000:19f3 | 0x119f3 | `FUN_1000_19f3` | 32 |  | - |
| 1000:1a13 | 0x11a13 | `FUN_1000_1a13` | 234 |  | - |
| 1000:1afd | 0x11afd | `FUN_1000_1afd` | 277 |  | - |
| 1000:1c12 | 0x11c12 | `FUN_1000_1c12` | 1405 |  | - |
| 1000:218f | 0x1218f | `FUN_1000_218f` | 398 |  | - |
| 1000:231d | 0x1231d | `FUN_1000_231d` | 238 |  | - |
| 1000:240b | 0x1240b | `FUN_1000_240b` | 183 |  | - |
| 1000:24c2 | 0x124c2 | `FUN_1000_24c2` | 933 |  | - |
| 1000:2867 | 0x12867 | `FUN_1000_2867` | 59 |  | - |
| 1000:28a2 | 0x128a2 | `FUN_1000_28a2` | 42 |  | - |
| 1000:28cc | 0x128cc | `FUN_1000_28cc` | 71 |  | - |
| 1000:2913 | 0x12913 | `FUN_1000_2913` | 226 |  | - |
| 1000:29f5 | 0x129f5 | `FUN_1000_29f5` | 54 |  | - |
| 1000:2a2b | 0x12a2b | `FUN_1000_2a2b` | 36 |  | - |
| 1000:2a4f | 0x12a4f | `FUN_1000_2a4f` | 26 |  | - |
| 1000:2a69 | 0x12a69 | `FUN_1000_2a69` | 21 |  | - |
| 1000:2a7e | 0x12a7e | `FUN_1000_2a7e` | 21 |  | - |
| 1000:2a93 | 0x12a93 | `FUN_1000_2a93` | 445 |  | - |
| 1000:2c50 | 0x12c50 | `FUN_1000_2c50` | 344 |  | - |
| 1000:2da8 | 0x12da8 | `FUN_1000_2da8` | 1123 |  | - |
| 1000:320b | 0x1320b | `FUN_1000_320b` | 168 |  | - |
| 1000:32b3 | 0x132b3 | `FUN_1000_32b3` | 800 |  | - |
| 1000:35d3 | 0x135d3 | `FUN_1000_35d3` | 442 |  | - |
| 1000:378d | 0x1378d | `FUN_1000_378d` | 1091 |  | - |
| 1000:3bd0 | 0x13bd0 | `FUN_1000_3bd0` | 368 |  | - |
| 1000:3d40 | 0x13d40 | `FUN_1000_3d40` | 622 |  | - |
| 1000:3fae | 0x13fae | `FUN_1000_3fae` | 1556 |  | - |
| 1000:45c2 | 0x145c2 | `FUN_1000_45c2` | 95 |  | - |
| 1000:4621 | 0x14621 | `FUN_1000_4621` | 134 |  | - |
| 1000:46a7 | 0x146a7 | `FUN_1000_46a7` | 198 |  | - |
| 1000:476d | 0x1476d | `FUN_1000_476d` | 330 |  | - |
| 1000:48b7 | 0x148b7 | `FUN_1000_48b7` | 495 |  | - |
| 1000:4aa6 | 0x14aa6 | `FUN_1000_4aa6` | 14 |  | - |
| 1000:4ab4 | 0x14ab4 | `FUN_1000_4ab4` | 269 |  | - |
| 1000:4bc1 | 0x14bc1 | `FUN_1000_4bc1` | 235 |  | - |
| 1000:4cac | 0x14cac | `FUN_1000_4cac` | 171 |  | - |
| 1000:4d57 | 0x14d57 | `FUN_1000_4d57` | 112 |  | - |
| 1000:4dc7 | 0x14dc7 | `FUN_1000_4dc7` | 769 |  | - |
| 1000:50c8 | 0x150c8 | `FUN_1000_50c8` | 434 |  | - |

## CODE_1: segment 1527, 2,104 bytes

| Address | Linear | Ghidra name | Span | Label | Status |
|---|---|---|---|---|---|
| 1527:000a | 0x1527a | `FUN_1527_000a` | 58 |  | - |
| 1527:0044 | 0x152b4 | `FUN_1527_0044` | 2046 |  | - |

## CODE_2: segment 15ab, 8,982 bytes

| Address | Linear | Ghidra name | Span | Label | Status |
|---|---|---|---|---|---|
| 15ab:0002 | 0x15ab2 | `FUN_15ab_0002` | 1271 |  | - |
| 15ab:04f9 | 0x15fa9 | `FUN_15ab_04f9` | 1106 |  | - |
| 15ab:094b | 0x163fb | `FUN_15ab_094b` | 531 |  | - |
| 15ab:0b5e | 0x1660e | `FUN_15ab_0b5e` | 55 |  | - |
| 15ab:0b95 | 0x16645 | `FUN_15ab_0b95` | 250 |  | - |
| 15ab:0c8f | 0x1673f | `FUN_15ab_0c8f` | 131 |  | - |
| 15ab:0d12 | 0x167c2 | `FUN_15ab_0d12` | 43 |  | - |
| 15ab:0d3d | 0x167ed | `FUN_15ab_0d3d` | 1834 |  | - |
| 15ab:1467 | 0x16f17 | `FUN_15ab_1467` | 741 |  | - |
| 15ab:174c | 0x171fc | `FUN_15ab_174c` | 268 |  | - |
| 15ab:1858 | 0x17308 | `FUN_15ab_1858` | 144 |  | - |
| 15ab:18e8 | 0x17398 | `FUN_15ab_18e8` | 534 |  | - |
| 15ab:1afe | 0x175ae | `FUN_15ab_1afe` | 2074 |  | - |

## CODE_3: segment 17dc, 7,610 bytes

| Address | Linear | Ghidra name | Span | Label | Status |
|---|---|---|---|---|---|
| 17dc:0008 | 0x17dc8 | `FUN_17dc_0008` | 440 |  | - |
| 17dc:01c0 | 0x17f80 | `FUN_17dc_01c0` | 1079 |  | - |
| 17dc:05f7 | 0x183b7 | `FUN_17dc_05f7` | 50 |  | - |
| 17dc:0629 | 0x183e9 | `FUN_17dc_0629` | 1824 |  | - |
| 17dc:0d49 | 0x18b09 | `FUN_17dc_0d49` | 1538 |  | - |
| 17dc:134b | 0x1910b | `FUN_17dc_134b` | 147 |  | - |
| 17dc:13de | 0x1919e | `FUN_17dc_13de` | 520 |  | - |
| 17dc:15e6 | 0x193a6 | `FUN_17dc_15e6` | 467 |  | - |
| 17dc:17b9 | 0x19579 | `FUN_17dc_17b9` | 573 |  | - |
| 17dc:19f6 | 0x197b6 | `FUN_17dc_19f6` | 48 |  | - |
| 17dc:1a26 | 0x197e6 | `FUN_17dc_1a26` | 283 |  | - |
| 17dc:1b41 | 0x19901 | `FUN_17dc_1b41` | 346 |  | - |
| 17dc:1c9b | 0x19a5b | `FUN_17dc_1c9b` | 149 |  | - |
| 17dc:1d30 | 0x19af0 | `FUN_17dc_1d30` | 146 |  | - |

## CODE_4: segment 19b8, 6,738 bytes

| Address | Linear | Ghidra name | Span | Label | Status |
|---|---|---|---|---|---|
| 19b8:080a | 0x1a38a | `FUN_19b8_080a` | 283 |  | - |
| 19b8:0925 | 0x1a4a5 | `FUN_19b8_0925` | 1075 |  | - |
| 19b8:0d58 | 0x1a8d8 | `FUN_19b8_0d58` | 758 |  | - |
| 19b8:104e | 0x1abce | `FUN_19b8_104e` | 817 |  | - |
| 19b8:137f | 0x1aeff | `FUN_19b8_137f` | 194 |  | - |
| 19b8:1441 | 0x1afc1 | `FUN_19b8_1441` | 238 |  | - |
| 19b8:152f | 0x1b0af | `FUN_19b8_152f` | 387 |  | - |
| 19b8:16b2 | 0x1b232 | `FUN_19b8_16b2` | 176 |  | - |
| 19b8:1762 | 0x1b2e2 | `FUN_19b8_1762` | 754 |  | - |

## CODE_5: segment 1b5d, 3,397 bytes

| Address | Linear | Ghidra name | Span | Label | Status |
|---|---|---|---|---|---|
| 1b5d:0004 | 0x1b5d4 | `FUN_1b5d_0004` | 485 |  | - |
| 1b5d:01e9 | 0x1b7b9 | `FUN_1b5d_01e9` | 159 |  | - |
| 1b5d:0288 | 0x1b858 | `FUN_1b5d_0288` | 32 |  | - |
| 1b5d:02a8 | 0x1b878 | `FUN_1b5d_02a8` | 42 |  | - |
| 1b5d:02d2 | 0x1b8a2 | `FUN_1b5d_02d2` | 85 |  | - |
| 1b5d:0327 | 0x1b8f7 | `FUN_1b5d_0327` | 131 |  | - |
| 1b5d:03aa | 0x1b97a | `FUN_1b5d_03aa` | 257 |  | - |
| 1b5d:04ab | 0x1ba7b | `FUN_1b5d_04ab` | 175 |  | - |
| 1b5d:055a | 0x1bb2a | `FUN_1b5d_055a` | 578 |  | - |
| 1b5d:079c | 0x1bd6c | `FUN_1b5d_079c` | 375 |  | - |
| 1b5d:0913 | 0x1bee3 | `FUN_1b5d_0913` | 109 |  | - |
| 1b5d:0980 | 0x1bf50 | `FUN_1b5d_0980` | 310 |  | - |
| 1b5d:0ab6 | 0x1c086 | `FUN_1b5d_0ab6` | 659 |  | - |

## CODE_6: segment 1c31, 857 bytes

| Address | Linear | Ghidra name | Span | Label | Status |
|---|---|---|---|---|---|
| 1c31:000a | 0x1c31a | `FUN_1c31_000a` | 856 |  | - |

## CODE_7: segment 1c67, 3,522 bytes

| Address | Linear | Ghidra name | Span | Label | Status |
|---|---|---|---|---|---|
| 1c67:0002 | 0x1c672 | `FUN_1c67_0002` | 2214 |  | - |
| 1c67:08a8 | 0x1cf18 | `FUN_1c67_08a8` | 752 |  | - |
| 1c67:0b98 | 0x1d208 | `FUN_1c67_0b98` | 486 |  | - |
| 1c67:0d7e | 0x1d3ee | `FUN_1c67_0d7e` | 70 |  | - |

## CODE_8: segment 1d43, 3,810 bytes

| Address | Linear | Ghidra name | Span | Label | Status |
|---|---|---|---|---|---|
| 1d43:0004 | 0x1d434 | `FUN_1d43_0004` | 1991 |  | - |
| 1d43:07cb | 0x1dbfb | `FUN_1d43_07cb` | 618 |  | - |
| 1d43:0a35 | 0x1de65 | `FUN_1d43_0a35` | 573 |  | - |
| 1d43:0c72 | 0x1e0a2 | `FUN_1d43_0c72` | 108 |  | - |
| 1d43:0cde | 0x1e10e | `FUN_1d43_0cde` | 520 |  | - |

## CODE_9: segment 1e31, 8,356 bytes

| Address | Linear | Ghidra name | Span | Label | Status |
|---|---|---|---|---|---|
| 1e31:0006 | 0x1e316 | `FUN_1e31_0006` | 734 |  | - |
| 1e31:02e4 | 0x1e5f4 | `FUN_1e31_02e4` | 75 |  | - |
| 1e31:032f | 0x1e63f | `FUN_1e31_032f` | 124 |  | - |
| 1e31:03ab | 0x1e6bb | `FUN_1e31_03ab` | 2058 |  | - |
| 1e31:0bb5 | 0x1eec5 | `FUN_1e31_0bb5` | 174 |  | - |
| 1e31:0c63 | 0x1ef73 | `FUN_1e31_0c63` | 705 |  | - |
| 1e31:0f24 | 0x1f234 | `FUN_1e31_0f24` | 307 |  | - |
| 1e31:1057 | 0x1f367 | `FUN_1e31_1057` | 75 |  | - |
| 1e31:10a2 | 0x1f3b2 | `FUN_1e31_10a2` | 128 |  | - |
| 1e31:1122 | 0x1f432 | `FUN_1e31_1122` | 137 |  | - |
| 1e31:11ab | 0x1f4bb | `FUN_1e31_11ab` | 1103 |  | - |
| 1e31:15fa | 0x1f90a | `FUN_1e31_15fa` | 68 |  | - |
| 1e31:163e | 0x1f94e | `FUN_1e31_163e` | 109 |  | - |
| 1e31:16ab | 0x1f9bb | `FUN_1e31_16ab` | 1177 |  | - |
| 1e31:1b44 | 0x1fe54 | `FUN_1e31_1b44` | 75 |  | - |
| 1e31:1b8f | 0x1fe9f | `FUN_1e31_1b8f` | 111 |  | - |
| 1e31:1bfe | 0x1ff0e | `FUN_1e31_1bfe` | 429 |  | - |
| 1e31:1dab | 0x200bb | `FUN_1e31_1dab` | 33 |  | - |
| 1e31:1dcc | 0x200dc | `FUN_1e31_1dcc` | 44 |  | - |
| 1e31:1df8 | 0x20108 | `FUN_1e31_1df8` | 170 |  | - |
| 1e31:1ea2 | 0x201b2 | `FUN_1e31_1ea2` | 103 |  | - |
| 1e31:1f09 | 0x20219 | `FUN_1e31_1f09` | 106 |  | - |
| 1e31:1f73 | 0x20283 | `FUN_1e31_1f73` | 108 |  | - |
| 1e31:1fdf | 0x202ef | `FUN_1e31_1fdf` | 203 |  | - |

## CODE_10: segment 203b, 10,962 bytes

| Address | Linear | Ghidra name | Span | Label | Status |
|---|---|---|---|---|---|
| 203b:000a | 0x203ba | `FUN_203b_000a` | 5240 |  | - |
| 203b:1482 | 0x21832 | `FUN_203b_1482` | 65 |  | - |
| 203b:14c3 | 0x21873 | `FUN_203b_14c3` | 689 |  | - |
| 203b:1774 | 0x21b24 | `FUN_203b_1774` | 455 |  | - |
| 203b:193b | 0x21ceb | `FUN_203b_193b` | 740 |  | - |
| 203b:1c1f | 0x21fcf | `FUN_203b_1c1f` | 1554 |  | - |
| 203b:2231 | 0x225e1 | `FUN_203b_2231` | 139 |  | - |
| 203b:22bc | 0x2266c | `FUN_203b_22bc` | 440 |  | - |
| 203b:2474 | 0x22824 | `FUN_203b_2474` | 124 |  | - |
| 203b:24f0 | 0x228a0 | `FUN_203b_24f0` | 102 |  | - |
| 203b:2556 | 0x22906 | `FUN_203b_2556` | 59 |  | - |
| 203b:2591 | 0x22941 | `FUN_203b_2591` | 428 |  | - |
| 203b:273d | 0x22aed | `FUN_203b_273d` | 140 |  | - |
| 203b:27c9 | 0x22b79 | `FUN_203b_27c9` | 108 |  | - |
| 203b:2835 | 0x22be5 | `FUN_203b_2835` | 166 |  | - |
| 203b:28db | 0x22c8b | `FUN_203b_28db` | 456 |  | - |
| 203b:2aa3 | 0x22e53 | `FUN_203b_2aa3` | 57 |  | - |

## CODE_11: segment 22e8, 7,848 bytes

| Address | Linear | Ghidra name | Span | Label | Status |
|---|---|---|---|---|---|
| 22e8:000c | 0x22e8c | `FUN_22e8_000c` | 4795 |  | - |
| 22e8:12c7 | 0x24147 | `FUN_22e8_12c7` | 2943 |  | - |
| 22e8:1e46 | 0x24cc6 | `FUN_22e8_1e46` | 110 |  | - |

## CODE_12: segment 24d3, 6,191 bytes

| Address | Linear | Ghidra name | Span | Label | Status |
|---|---|---|---|---|---|
| 24d3:0004 | 0x24d34 | `FUN_24d3_0004` | 6082 |  | - |
| 24d3:17c6 | 0x264f6 | `FUN_24d3_17c6` | 36 |  | - |
| 24d3:17ea | 0x2651a | `FUN_24d3_17ea` | 31 |  | - |
| 24d3:1809 | 0x26539 | `FUN_24d3_1809` | 21 |  | - |
| 24d3:181e | 0x2654e | `FUN_24d3_181e` | 21 |  | - |

## CODE_13: segment 2656, 3,699 bytes

| Address | Linear | Ghidra name | Span | Label | Status |
|---|---|---|---|---|---|
| 2656:0004 | 0x26564 | `FUN_2656_0004` | 483 |  | - |
| 2656:01e7 | 0x26747 | `FUN_2656_01e7` | 154 |  | - |
| 2656:0281 | 0x267e1 | `FUN_2656_0281` | 263 |  | - |
| 2656:0388 | 0x268e8 | `FUN_2656_0388` | 109 |  | - |
| 2656:03f5 | 0x26955 | `FUN_2656_03f5` | 982 |  | - |
| 2656:07cb | 0x26d2b | `FUN_2656_07cb` | 624 |  | - |
| 2656:0a3b | 0x26f9b | `FUN_2656_0a3b` | 170 |  | - |
| 2656:0ae5 | 0x27045 | `FUN_2656_0ae5` | 121 |  | - |
| 2656:0b5e | 0x270be | `FUN_2656_0b5e` | 447 |  | - |
| 2656:0d1d | 0x2727d | `FUN_2656_0d1d` | 345 |  | - |

## CODE_14: segment 273d, 2,172 bytes

| Address | Linear | Ghidra name | Span | Label | Status |
|---|---|---|---|---|---|
| 273d:0006 | 0x273d6 | `FUN_273d_0006` | 41 |  | - |
| 273d:002f | 0x273ff | `FUN_273d_002f` | 36 |  | - |
| 273d:0053 | 0x27423 | `FUN_273d_0053` | 51 |  | - |
| 273d:0086 | 0x27456 | `FUN_273d_0086` | 79 |  | - |
| 273d:00d5 | 0x274a5 | `FUN_273d_00d5` | 294 |  | - |
| 273d:01fb | 0x275cb | `FUN_273d_01fb` | 94 |  | - |
| 273d:0259 | 0x27629 | `FUN_273d_0259` | 195 |  | - |
| 273d:031c | 0x276ec | `FUN_273d_031c` | 207 |  | - |
| 273d:03eb | 0x277bb | `FUN_273d_03eb` | 178 |  | - |
| 273d:049d | 0x2786d | `FUN_273d_049d` | 136 |  | - |
| 273d:0525 | 0x278f5 | `FUN_273d_0525` | 151 |  | - |
| 273d:05bc | 0x2798c | `FUN_273d_05bc` | 127 |  | - |
| 273d:063b | 0x27a0b | `FUN_273d_063b` | 136 |  | - |
| 273d:06c3 | 0x27a93 | `FUN_273d_06c3` | 71 |  | - |
| 273d:070a | 0x27ada | `FUN_273d_070a` | 266 |  | - |
| 273d:0814 | 0x27be4 | `FUN_273d_0814` | 86 |  | - |
| 273d:086a | 0x27c3a | `FUN_273d_086a` | 24 |  | - |

## CODE_15: segment 27c5, 2,155 bytes

| Address | Linear | Ghidra name | Span | Label | Status |
|---|---|---|---|---|---|
| 27c5:0002 | 0x27c52 | `FUN_27c5_0002` | 673 |  | - |
| 27c5:02a3 | 0x27ef3 | `FUN_27c5_02a3` | 72 |  | - |
| 27c5:02eb | 0x27f3b | `FUN_27c5_02eb` | 90 |  | - |
| 27c5:0345 | 0x27f95 | `FUN_27c5_0345` | 99 |  | - |
| 27c5:03a8 | 0x27ff8 | `FUN_27c5_03a8` | 99 |  | - |
| 27c5:040b | 0x2805b | `FUN_27c5_040b` | 99 |  | - |
| 27c5:046e | 0x280be | `FUN_27c5_046e` | 13 |  | - |
| 27c5:047b | 0x280cb | `FUN_27c5_047b` | 105 |  | - |
| 27c5:04e4 | 0x28134 | `FUN_27c5_04e4` | 13 |  | - |
| 27c5:04f1 | 0x28141 | `FUN_27c5_04f1` | 169 |  | - |
| 27c5:059a | 0x281ea | `FUN_27c5_059a` | 169 |  | - |
| 27c5:0643 | 0x28293 | `FUN_27c5_0643` | 179 |  | - |
| 27c5:06f6 | 0x28346 | `FUN_27c5_06f6` | 81 |  | - |
| 27c5:0747 | 0x28397 | `FUN_27c5_0747` | 147 |  | - |
| 27c5:07da | 0x2842a | `FUN_27c5_07da` | 147 |  | - |

## CODE_16: segment 284b, 831 bytes

| Address | Linear | Ghidra name | Span | Label | Status |
|---|---|---|---|---|---|
| 284b:0048 | 0x284f8 | `FUN_284b_0048` | 73 |  | - |
| 284b:0091 | 0x28541 | `FUN_284b_0091` | 44 |  | - |
| 284b:00bd | 0x2856d | `FUN_284b_00bd` | 551 |  | - |
| 284b:02e4 | 0x28794 | `FUN_284b_02e4` | 34 |  | - |
| 284b:0306 | 0x287b6 | `FUN_284b_0306` | 54 |  | - |
| 284b:033c | 0x287ec | `FUN_284b_033c` | 16 |  | - |

## CODE_17: segment 287f, 16,068 bytes (Microsoft C run-time library; entry point here)

| Address | Linear | Ghidra name | Span | Label | Status |
|---|---|---|---|---|---|
| 287f:001c | 0x2880c | `FUN_287f_001c` | 20 |  | replaced |
| 287f:0030 | 0x28820 | `FUN_287f_0030` | 33 |  | replaced |
| 287f:0051 | 0x28841 | `FUN_287f_0051` | 22 |  | replaced |
| 287f:0067 | 0x28857 | `FUN_287f_0067` | 22 |  | replaced |
| 287f:007d | 0x2886d | `FUN_287f_007d` | 44 |  | replaced |
| 287f:00a9 | 0x28899 | `FUN_287f_00a9` | 40 |  | replaced |
| 287f:00d1 | 0x288c1 | `FUN_287f_00d1` | 123 |  | replaced |
| 287f:014c | 0x2893c | `FUN_287f_014c` | 23 |  | replaced |
| 287f:0163 | 0x28953 | `FUN_287f_0163` | 116 |  | replaced |
| 287f:01d7 | 0x289c7 | `FUN_287f_01d7` | 60 |  | replaced |
| 287f:0213 | 0x28a03 | `FUN_287f_0213` | 23 |  | replaced |
| 287f:022a | 0x28a1a | `FUN_287f_022a` | 54 |  | replaced |
| 287f:0260 | 0x28a50 | `FUN_287f_0260` | 179 |  | replaced |
| 287f:0313 | 0x28b03 | `FUN_287f_0313` | 100 |  | replaced |
| 287f:0377 | 0x28b67 | `FUN_287f_0377` | 507 |  | replaced |
| 287f:0572 | 0x28d62 | `FUN_287f_0572` | 77 |  | replaced |
| 287f:05bf | 0x28daf | `FUN_287f_05bf` | 17 |  | replaced |
| 287f:05d0 | 0x28dc0 | `FUN_287f_05d0` | 432 |  | replaced |
| 287f:0780 | 0x28f70 | `FUN_287f_0780` | 289 |  | replaced |
| 287f:08a1 | 0x29091 | `FUN_287f_08a1` | 144 |  | replaced |
| 287f:0931 | 0x29121 | `FUN_287f_0931` | 64 |  | replaced |
| 287f:0971 | 0x29161 | `FUN_287f_0971` | 181 |  | replaced |
| 287f:0a26 | 0x29216 | `FUN_287f_0a26` | 80 |  | replaced |
| 287f:0a76 | 0x29266 | `FUN_287f_0a76` | 41 |  | replaced |
| 287f:0a9f | 0x2928f | `FUN_287f_0a9f` | 135 |  | replaced |
| 287f:0b26 | 0x29316 | `FUN_287f_0b26` | 26 |  | replaced |
| 287f:0b40 | 0x29330 | `FUN_287f_0b40` | 51 |  | replaced |
| 287f:0b73 | 0x29363 | `FUN_287f_0b73` | 23 |  | replaced |
| 287f:0b8a | 0x2937a | `FUN_287f_0b8a` | 29 |  | replaced |
| 287f:0ba7 | 0x29397 | `FUN_287f_0ba7` | 25 |  | replaced |
| 287f:0bc0 | 0x293b0 | `FUN_287f_0bc0` | 59 |  | replaced |
| 287f:0bfb | 0x293eb | `FUN_287f_0bfb` | 268 |  | replaced |
| 287f:0d07 | 0x294f7 | `FUN_287f_0d07` | 114 |  | replaced |
| 287f:0d79 | 0x29569 | `FUN_287f_0d79` | 127 |  | replaced |
| 287f:0df8 | 0x295e8 | `FUN_287f_0df8` | 502 |  | replaced |
| 287f:0fee | 0x297de | `FUN_287f_0fee` | 96 |  | replaced |
| 287f:104e | 0x2983e | `FUN_287f_104e` | 365 |  | replaced |
| 287f:11bb | 0x299ab | `FUN_287f_11bb` | 255 |  | replaced |
| 287f:12ba | 0x29aaa | `FUN_287f_12ba` | 31 |  | replaced |
| 287f:12d9 | 0x29ac9 | `FUN_287f_12d9` | 25 |  | replaced |
| 287f:12f2 | 0x29ae2 | `FUN_287f_12f2` | 17 |  | replaced |
| 287f:1303 | 0x29af3 | `FUN_287f_1303` | 17 |  | replaced |
| 287f:1314 | 0x29b04 | `FUN_287f_1314` | 197 |  | replaced |
| 287f:13d9 | 0x29bc9 | `FUN_287f_13d9` | 435 |  | replaced |
| 287f:158c | 0x29d7c | `FUN_287f_158c` | 175 |  | replaced |
| 287f:163b | 0x29e2b | `FUN_287f_163b` | 168 |  | replaced |
| 287f:16e3 | 0x29ed3 | `FUN_287f_16e3` | 226 |  | replaced |
| 287f:17c5 | 0x29fb5 | `FUN_287f_17c5` | 193 |  | replaced |
| 287f:1886 | 0x2a076 | `FUN_287f_1886` | 82 |  | replaced |
| 287f:18d8 | 0x2a0c8 | `FUN_287f_18d8` | 23 |  | replaced |
| 287f:18ef | 0x2a0df | `FUN_287f_18ef` | 441 |  | replaced |
| 287f:1aa8 | 0x2a298 | `FUN_287f_1aa8` | 38 |  | replaced |
| 287f:1ace | 0x2a2be | `FUN_287f_1ace` | 38 |  | replaced |
| 287f:1af4 | 0x2a2e4 | `FUN_287f_1af4` | 38 |  | replaced |
| 287f:1b1a | 0x2a30a | `FUN_287f_1b1a` | 87 |  | replaced |
| 287f:1b71 | 0x2a361 | `FUN_287f_1b71` | 35 |  | replaced |
| 287f:1b94 | 0x2a384 | `FUN_287f_1b94` | 75 |  | replaced |
| 287f:1bdf | 0x2a3cf | `FUN_287f_1bdf` | 29 |  | replaced |
| 287f:1bfc | 0x2a3ec | `FUN_287f_1bfc` | 135 |  | replaced |
| 287f:1c83 | 0x2a473 | `FUN_287f_1c83` | 53 |  | replaced |
| 287f:1cb8 | 0x2a4a8 | `FUN_287f_1cb8` | 130 |  | replaced |
| 287f:1d3a | 0x2a52a | `FUN_287f_1d3a` | 82 |  | replaced |
| 287f:1d8c | 0x2a57c | `FUN_287f_1d8c` | 28 |  | replaced |
| 287f:1da8 | 0x2a598 | `FUN_287f_1da8` | 19 |  | replaced |
| 287f:1dbb | 0x2a5ab | `FUN_287f_1dbb` | 61 |  | replaced |
| 287f:1df8 | 0x2a5e8 | `FUN_287f_1df8` | 63 |  | replaced |
| 287f:1e37 | 0x2a627 | `FUN_287f_1e37` | 151 |  | replaced |
| 287f:1ece | 0x2a6be | `FUN_287f_1ece` | 54 |  | replaced |
| 287f:1f04 | 0x2a6f4 | `FUN_287f_1f04` | 77 |  | replaced |
| 287f:1f51 | 0x2a741 | `FUN_287f_1f51` | 75 |  | replaced |
| 287f:1f9c | 0x2a78c | `FUN_287f_1f9c` | 17 |  | replaced |
| 287f:1fad | 0x2a79d | `FUN_287f_1fad` | 17 |  | replaced |
| 287f:1fbe | 0x2a7ae | `FUN_287f_1fbe` | 80 |  | replaced |
| 287f:200e | 0x2a7fe | `FUN_287f_200e` | 281 |  | replaced |
| 287f:2127 | 0x2a917 | `FUN_287f_2127` | 129 |  | replaced |
| 287f:21a8 | 0x2a998 | `FUN_287f_21a8` | 97 |  | replaced |
| 287f:2209 | 0x2a9f9 | `FUN_287f_2209` | 72 |  | replaced |
| 287f:2251 | 0x2aa41 | `FUN_287f_2251` | 84 |  | replaced |
| 287f:22a5 | 0x2aa95 | `FUN_287f_22a5` | 83 |  | replaced |
| 287f:22f8 | 0x2aae8 | `FUN_287f_22f8` | 112 |  | replaced |
| 287f:2368 | 0x2ab58 | `FUN_287f_2368` | 132 |  | replaced |
| 287f:23ec | 0x2abdc | `FUN_287f_23ec` | 112 |  | replaced |
| 287f:245c | 0x2ac4c | `FUN_287f_245c` | 123 |  | replaced |
| 287f:24d7 | 0x2acc7 | `FUN_287f_24d7` | 645 |  | replaced |
| 287f:275c | 0x2af4c | `FUN_287f_275c` | 332 |  | replaced |
| 287f:28a8 | 0x2b098 | `FUN_287f_28a8` | 67 |  | replaced |
| 287f:28eb | 0x2b0db | `FUN_287f_28eb` | 602 |  | replaced |
| 287f:2b45 | 0x2b335 | `FUN_287f_2b45` | 66 |  | replaced |
| 287f:2b87 | 0x2b377 | `FUN_287f_2b87` | 346 |  | replaced |
| 287f:2ce1 | 0x2b4d1 | `FUN_287f_2ce1` | 22 |  | replaced |
| 287f:2cf7 | 0x2b4e7 | `FUN_287f_2cf7` | 109 |  | replaced |
| 287f:2d64 | 0x2b554 | `FUN_287f_2d64` | 30 |  | replaced |
| 287f:2d82 | 0x2b572 | `entry` | 402 |  | replaced |
| 287f:2f14 | 0x2b704 | `FUN_287f_2f14` | 94 |  | replaced |
| 287f:2f72 | 0x2b762 | `FUN_287f_2f72` | 45 |  | replaced |
| 287f:2f9f | 0x2b78f | `FUN_287f_2f9f` | 19 |  | replaced |
| 287f:2fb2 | 0x2b7a2 | `FUN_287f_2fb2` | 36 |  | replaced |
| 287f:2fd6 | 0x2b7c6 | `FUN_287f_2fd6` | 6 |  | replaced |
| 287f:2fdc | 0x2b7cc | `FUN_287f_2fdc` | 36 |  | replaced |
| 287f:3000 | 0x2b7f0 | `FUN_287f_3000` | 38 |  | replaced |
| 287f:3026 | 0x2b816 | `FUN_287f_3026` | 424 |  | replaced |
| 287f:31ce | 0x2b9be | `FUN_287f_31ce` | 124 |  | replaced |
| 287f:324a | 0x2ba3a | `FUN_287f_324a` | 43 |  | replaced |
| 287f:3275 | 0x2ba65 | `FUN_287f_3275` | 43 |  | replaced |
| 287f:32a0 | 0x2ba90 | `FUN_287f_32a0` | 85 |  | replaced |
| 287f:32f5 | 0x2bae5 | `FUN_287f_32f5` | 19 |  | replaced |
| 287f:3308 | 0x2baf8 | `FUN_287f_3308` | 46 |  | replaced |
| 287f:3336 | 0x2bb26 | `FUN_287f_3336` | 32 |  | replaced |
| 287f:3356 | 0x2bb46 | `FUN_287f_3356` | 122 |  | replaced |
| 287f:33d0 | 0x2bbc0 | `FUN_287f_33d0` | 415 |  | replaced |
| 287f:356f | 0x2bd5f | `FUN_287f_356f` | 17 |  | replaced |
| 287f:3580 | 0x2bd70 | `FUN_287f_3580` | 234 |  | replaced |
| 287f:366a | 0x2be5a | `FUN_287f_366a` | 180 |  | replaced |
| 287f:371e | 0x2bf0e | `FUN_287f_371e` | 75 |  | replaced |
| 287f:3769 | 0x2bf59 | `FUN_287f_3769` | 17 |  | replaced |
| 287f:377a | 0x2bf6a | `FUN_287f_377a` | 58 |  | replaced |
| 287f:37b4 | 0x2bfa4 | `FUN_287f_37b4` | 38 |  | replaced |
| 287f:37da | 0x2bfca | `FUN_287f_37da` | 70 |  | replaced |
| 287f:3820 | 0x2c010 | `FUN_287f_3820` | 21 |  | replaced |
| 287f:3835 | 0x2c025 | `FUN_287f_3835` | 63 |  | replaced |
| 287f:3874 | 0x2c064 | `FUN_287f_3874` | 110 |  | replaced |
| 287f:38e2 | 0x2c0d2 | `FUN_287f_38e2` | 27 |  | replaced |
| 287f:38fd | 0x2c0ed | `FUN_287f_38fd` | 227 |  | replaced |
| 287f:39e0 | 0x2c1d0 | `FUN_287f_39e0` | 58 |  | replaced |
| 287f:3a1a | 0x2c20a | `FUN_287f_3a1a` | 34 |  | replaced |
| 287f:3a3c | 0x2c22c | `FUN_287f_3a3c` | 34 |  | replaced |
| 287f:3a5e | 0x2c24e | `FUN_287f_3a5e` | 110 |  | replaced |
| 287f:3acc | 0x2c2bc | `FUN_287f_3acc` | 86 |  | replaced |
| 287f:3b22 | 0x2c312 | `FUN_287f_3b22` | 70 |  | replaced |
| 287f:3b68 | 0x2c358 | `FUN_287f_3b68` | 54 |  | replaced |
| 287f:3b9e | 0x2c38e | `FUN_287f_3b9e` | 24 |  | replaced |
| 287f:3bb6 | 0x2c3a6 | `FUN_287f_3bb6` | 28 |  | replaced |
| 287f:3bd2 | 0x2c3c2 | `FUN_287f_3bd2` | 10 |  | replaced |
| 287f:3bdc | 0x2c3cc | `FUN_287f_3bdc` | 44 |  | replaced |
| 287f:3c08 | 0x2c3f8 | `FUN_287f_3c08` | 100 |  | replaced |
| 287f:3c6c | 0x2c45c | `FUN_287f_3c6c` | 22 |  | replaced |
| 287f:3c82 | 0x2c472 | `FUN_287f_3c82` | 154 |  | replaced |
| 287f:3d1c | 0x2c50c | `FUN_287f_3d1c` | 40 |  | replaced |
| 287f:3d44 | 0x2c534 | `FUN_287f_3d44` | 40 |  | replaced |
| 287f:3d6c | 0x2c55c | `FUN_287f_3d6c` | 38 |  | replaced |
| 287f:3d92 | 0x2c582 | `FUN_287f_3d92` | 156 |  | replaced |
| 287f:3e2e | 0x2c61e | `FUN_287f_3e2e` | 52 |  | replaced |
| 287f:3e62 | 0x2c652 | `FUN_287f_3e62` | 98 |  | replaced |
| 287f:3ec4 | 0x2c6b4 | `FUN_287f_3ec4` | 12 |  | replaced |
