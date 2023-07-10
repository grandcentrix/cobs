/*
 * Copyright (c) 2011 Jacques Fortier
 *
 * SPDX-License-Identifier: MIT
 */

#include <errno.h>
#include <stddef.h>
#include <stdint.h>

size_t cobs_encode(const uint8_t *restrict input, size_t length, uint8_t *restrict output)
{
	size_t read_index = 0;
	size_t write_index = 1;
	size_t code_index = 0;
	uint8_t code = 1;

	while (read_index < length) {
		if (input[read_index] == 0) {
			output[code_index] = code;
			code = 1;
			code_index = write_index++;
			read_index++;
		} else {
			output[write_index++] = input[read_index++];
			code++;
			if (code == 0xFF) {
				output[code_index] = code;
				code = 1;
				code_index = write_index++;
			}
		}
	}

	output[code_index] = code;

	return write_index;
}

int cobs_decode(const uint8_t *restrict input, size_t length, uint8_t *restrict output,
		size_t *decoded_size)
{
	size_t read_index = 0;
	size_t write_index = 0;
	uint8_t code;
	uint8_t i;

	while (read_index < length) {
		code = input[read_index];
		if (code == 0) {
			return -EINVAL;
		}

		if (read_index + code > length && code != 1) {
			return -EINVAL;
		}

		read_index++;

		for (i = 1; i < code; i++) {
			const uint8_t byte = input[read_index++];
			if (byte == 0) {
				return -EINVAL;
			}

			output[write_index++] = byte;
		}

		if (code != 0xFF && read_index != length) {
			output[write_index++] = '\0';
		}
	}

	*decoded_size = write_index;
	return 0;
}

int cobs_decode_inplace(uint8_t *data, size_t max_length, size_t *decoded_size)
{
	size_t read_index = 0;
	size_t write_index = 0;
	uint8_t code, i;

	while (read_index < max_length) {
		code = data[read_index];
		if (code == 0) {
			return -EINVAL;
		}

		if ((read_index + code > max_length) && (code != 1)) {
			return -EINVAL;
		}

		read_index++;

		for (i = 1; i < code; i++) {
			const uint8_t byte = data[read_index++];
			if (byte == 0) {
				return -EINVAL;
			}

			data[write_index++] = byte;
		}

		if (code != 0xFF && read_index != max_length) {
			data[write_index++] = '\0';
		}
	}

	*decoded_size = write_index;
	return 0;
}
