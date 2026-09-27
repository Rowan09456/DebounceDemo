#pragma once

//#include <stdint>
#include "pico/stdlib.h"

// -----------------------------------------------------------------------------
// Single-channel tolerant/intolerant debounce for one relay-driven digital
// input, interrupt-driven for precise edge timing.
//
// A GPIO edge interrupt latches the timestamp of the very first raw
// transition the instant it happens, and also watches for chatter during
// the intolerant window. An internal repeating timer (started by di_init())
// services the two window timeouts, so you don't have to poll anything
// tightly -- just check di_event_ready() from your main loop whenever
// convenient.
//
// TIMING MODEL
//   TOLERANT window     chatter is expected here; the interrupt is masked,
//                        so bouncing costs nothing.
//   INTOLERANT window    chatter here means "not settled yet" -> restart the
//                        tolerant window and try again, keeping the
//                        ORIGINAL first-edge timestamp.
// A transition is confirmed only once the input holds steady through a
// full, untouched intolerant window.
// -----------------------------------------------------------------------------

const uint32_t DI_TOLERANT_US   = 3000; // chatter allowed / ignored
const uint32_t DI_INTOLERANT_US = 2000; // chatter disallowed / verified

void     di_init(uint pin, bool pull_up = true);
bool     di_event_ready();   // true once, when a new transition is confirmed
bool     di_level();         // last confirmed (debounced) level
uint64_t di_first_edge_us(); // timestamp of that transition's first raw edge
