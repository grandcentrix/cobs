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
 * "input", writing the output to the location pointed to by
 * "output". On success, returns 0 and writes the the number of bytes
 * that were written to "output" to "decoded_size". On failure, it
 * returns a negative errno code.
 *
 * Remove the "restrict" qualifiers if compiling with a
 * pre-C99 C dialect.
 */
int cobs_decode(const uint8_t *restrict input, size_t length, uint8_t *restrict output,
		size_t *decoded_size);

/**
 * Unstuffs "max_length" bytes of data at the location pointed to by
 * "data", in-place, over-writing the original.
 * On success, returns 0 and writes the the number of bytes
 * that were written to "data" to "decoded_size". On failure, it
 * returns a negative errno code.
 */
int cobs_decode_inplace(uint8_t *restrict data, size_t max_length, size_t *decoded_size);

#endif
