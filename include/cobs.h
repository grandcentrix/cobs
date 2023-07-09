/*
 * Copyright (c) 2011 Jacques Fortier
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef COBS_H
#define COBS_H

#include <stddef.h>
#include <stdint.h>

#include <cobs/stream.h>

/**
 * Stuffs "length" bytes of data at the location pointed to by
 * "input", writing the output to the location pointed to by
 * "output". Returns the number of bytes written to "output".
 *
 * Remove the "restrict" qualifiers if compiling with a
 * pre-C99 C dialect.
 */
size_t cobs_encode(const uint8_t *restrict input, size_t length, uint8_t *restrict output);

/**
 * Unstuffs "length" bytes of data at the location pointed to by
 * "input", writing the output * to the location pointed to by
 * "output". Returns the number of bytes written to "output" if
 * "input" was successfully unstuffed, and 0 if there was an
 * error unstuffing "input".
 *
 * Remove the "restrict" qualifiers if compiling with a
 * pre-C99 C dialect.
 */
size_t cobs_decode(const uint8_t *restrict input, size_t length, uint8_t *restrict output);

/**
 * Unstuffs "length" bytes of data at the location pointed to by
 * "data", in-place, over-writing the original.
 * Returns the number of bytes of the decoded data if it was
 * successfully unstuffed, and 0 if there was an error unstuffing.
 */
size_t cobs_decode_inplace(uint8_t *restrict data, size_t max_length);

#endif
