/* SPDX-License-Identifier: MPL-2.0
 * Copyright (C) 2026 swtdr34mz (REEVES) */
/* Reading a Prophet Rev2's own program files: SysEx program dumps (F0 01 2F 02 bank program <2339 packed bytes> F7,
 * or the edit buffer: F0 01 2F 03 <packed> F7), as a Rev2 sends them or librarians keep them (.syx). Each becomes a
 * REEVES program: both layers, A/B mode and split point, the gated and the poly sequence, the name.
 *
 * REEVES carries none of these sounds itself: the user loads the file they have (their own Rev2's, or a bank they
 * own), and the programs are kept with their own (Documents/REEVES/Programs). */
#pragma once
#include "Params.h"
#include <cstddef>
#include <cstdint>
#include <vector>

namespace rv
{
struct Rev2Import {
    Program program;
    int bank = -1, number = -1;          /* where it was on the Rev2 (-1: the edit buffer) */
};

/* every Rev2 program in the bytes (any other SysEx and anything between is skipped) */
std::vector<Rev2Import> rev2ReadSyx(const uint8_t *data, size_t size);

/* one program from its 2046 unpacked bytes */
void rev2ToProgram(const uint8_t *raw, Program &p);
} // namespace rv
