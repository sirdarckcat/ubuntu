//
//   BSD LICENSE
//
//   Copyright(c) 2016 Mellanox Technologies, Ltd. All rights reserved.
//   All rights reserved.
//
//   Redistribution and use in source and binary forms, with or without
//   modification, are permitted provided that the following conditions
//   are met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above copyright
//       notice, this list of conditions and the following disclaimer in
//       the documentation and/or other materials provided with the
//       distribution.
//     * Neither the name of Mellanox Technologies nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
//   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
//   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
//   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
//   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
//   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
//   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
//   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
//   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
//   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
//   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
//   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//

#ifndef __PKA_CPU_H__
#define __PKA_CPU_H__

#include <linux/types.h>
#include <linux/timex.h>

#define PKA_AARCH_64
#define MAX_CPU_NUMBER 16      // BlueField specific

#define MEGA 1000000
#define GIGA 1000000000

#define MS_PER_S 1000
#define US_PER_S 1000000
#define NS_PER_S 1000000000

// Initial guess at our CPU speed.  We set this to be larger than any
// possible real speed, so that any calculated delays will be too long,
// rather than too short.
//
//*Warning: use dummy value for frequency
//#define CPU_HZ_MAX      (2 * GIGA) // Cortex A72 : 2 GHz max -> 2.5 GHz max
#define CPU_HZ_MAX        (1255 * MEGA) // CPU Freq for High/Bin Chip

// YIELD hints the CPU to switch to another thread if possible
// and executes as a NOP otherwise.
#define pka_cpu_yield() ({ asm volatile("yield" : : : "memory"); })
// ISB flushes the pipeline, then restarts. This is guaranteed to
// stall the CPU a number of cycles.
#define pka_cpu_relax() ({ asm volatile("isb" : : : "memory"); })

// Processor speed in hertz; used in routines which might be called very
// early in boot.
static inline uint64_t pka_early_cpu_speed(void)
{
  return CPU_HZ_MAX;
}

#endif // __PKA_CPU_H__
