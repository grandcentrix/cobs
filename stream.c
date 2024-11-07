/* SPDX-License-Identifier: MIT */

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <cobs.h>

static inline struct cobs_buf_cursor cobs_buf_cursor_new(struct net_buf *buf)
{
	return (struct cobs_buf_cursor){
		.buf = net_buf_ref(buf),
		.offset = 0,
	};
}

static void cobs_buf_cursor_delete(struct cobs_buf_cursor *cursor)
{
	if (cursor->buf) {
		net_buf_unref(cursor->buf);
		cursor->buf = NULL;
	}

	cursor->offset = 0;
}

static int cobs_buf_cursor_read(struct cobs_buf_cursor *const cursor, void *const output_,
				size_t length)
{
	uint8_t *output = output_;

	while (cursor->buf && length) {
		struct net_buf *const buf = cursor->buf;
		const size_t read_length = MIN(buf->len - cursor->offset, length);
		if (read_length == 0) {
			if (buf->frags) {
				cursor->buf = net_buf_ref(buf->frags);
				net_buf_unref(buf);
			} else {
				cursor->buf = NULL;
				net_buf_unref(buf);
			}
			cursor->offset = 0;
			continue;
		}

		memcpy(output, buf->data + cursor->offset, read_length);
		output += read_length;
		length -= read_length;
		cursor->offset += read_length;
	}

	if (length) {
		return -ENOBUFS;
	}

	return 0;
}

static int cursor_find_zero(struct cobs_buf_cursor *cursor, size_t *num_processed,
			    size_t *zero_position)
{
	size_t offset = 0;

	struct net_buf *buf = cursor->buf;
	size_t start_offset = cursor->offset;
	while (buf) {
		for (uint16_t i = start_offset; i < buf->len; i += 1, offset += 1) {
			if (buf->data[i] == 0) {
				*num_processed = offset + 1;
				*zero_position = offset;
				return 0;
			}
		}

		buf = buf->frags;
		start_offset = 0;
	}

	*num_processed = offset;
	return -ENOENT;
}

/* NOTE: It's important to inline this, to make cobs_decode_stream faster. */
ALWAYS_INLINE
enum cobs_decode_result cobs_decode_stream_single(struct cobs_decode *decode, uint8_t input_byte,
						  uint8_t *output_byte, bool *output_available)
{
	*output_available = false;

	switch (decode->state) {
	case COBS_DECODE_STATE_CODE:
		if (input_byte == 0) {
			decode->state = COBS_DECODE_STATE_FINISHED;
			return COBS_DECODE_RESULT_FINISHED;
		}

		if (decode->pending_zero) {
			*output_byte = 0x00;
			*output_available = true;

			decode->pending_zero = false;
		}

		if (input_byte == 1) {
			decode->pending_zero = true;
		} else {
			decode->code = input_byte - 1;
			decode->state = COBS_DECODE_STATE_DATA;
			decode->pending_zero = input_byte != 0xFF;
		}
		return COBS_DECODE_RESULT_CONSUMED;

	case COBS_DECODE_STATE_DATA:
		if (input_byte == 0) {
			decode->state = COBS_DECODE_STATE_FINISHED;
			return COBS_DECODE_RESULT_UNEXPECTED_ZERO;
		}

		*output_byte = input_byte;
		*output_available = true;
		decode->code -= 1;

		if (decode->code == 0) {
			decode->state = COBS_DECODE_STATE_CODE;
			return COBS_DECODE_RESULT_CONSUMED;
		}
		return COBS_DECODE_RESULT_CONSUMED;

	case COBS_DECODE_STATE_FINISHED:
	default:
		return COBS_DECODE_RESULT_ERROR;
	}
}

enum cobs_decode_result cobs_decode_stream(struct cobs_decode *decode, const uint8_t *input,
					   size_t input_size, uint8_t *output, size_t output_size,
					   size_t *num_read, size_t *num_written)
{
	*num_read = 0;
	*num_written = 0;

	while (input_size > 0 && (output_size > 0 || input[0] == 0)) {
		bool output_available = false;
		enum cobs_decode_result result =
			cobs_decode_stream_single(decode, input[0], output, &output_available);

		*num_read += 1;
		input++;
		input_size -= 1;

		if (output_available) {
			*num_written += 1;
			output++;
			output_size -= 1;
		}

		if (result != COBS_DECODE_RESULT_CONSUMED) {
			return result;
		}
	}

	return COBS_DECODE_RESULT_CONSUMED;
}

void cobs_encode_stream_init(struct cobs_encode *encode, struct net_buf *buf)
{
	*encode = (struct cobs_encode){
		.cursor = cobs_buf_cursor_new(buf),
	};

	size_t num_processed;
	size_t zero_position;
	int ret = cursor_find_zero(&encode->cursor, &num_processed, &zero_position);
	if (ret == 0) {
		__ASSERT_NO_MSG(num_processed != 0);

		encode->state = zero_position ? COBS_ENCODE_STATE_ZEROS_CODE
					      : COBS_ENCODE_STATE_ZEROS_FIRSTBYTE;
		encode->u.zeros = (struct cobs_encode_zeros){
			.next_zero = zero_position,
		};
	} else {
		encode->state = COBS_ENCODE_STATE_NOZEROS_CODE;
		encode->u.nozeros = (struct cobs_encode_nozeros){
			.total_length = num_processed,
		};
	}
}

void cobs_encode_stream_free(struct cobs_encode *encode)
{
	cobs_buf_cursor_delete(&encode->cursor);
	*encode = (struct cobs_encode){};
}

static inline bool cobs_encode_stream_single(struct cobs_encode *encode, uint8_t *output)
{
	int ret;

	switch (encode->state) {
	case COBS_ENCODE_STATE_ZEROS_FIRSTBYTE:
		*output = 0x01;
		encode->state = COBS_ENCODE_STATE_ZEROS_CODE;
		break;
	case COBS_ENCODE_STATE_ZEROS_CODE: {
		if (encode->u.zeros.next_zero == 0) {
			uint8_t zero;
			ret = cobs_buf_cursor_read(&encode->cursor, &zero, sizeof(zero));
			__ASSERT_NO_MSG(ret == 0);
			ARG_UNUSED(ret);

			__ASSERT_NO_MSG(zero == 0);

			size_t num_processed;
			size_t zero_position;
			int ret = cursor_find_zero(&encode->cursor, &num_processed, &zero_position);
			if (num_processed == 0) {
				*output = 0x01;
				encode->state = COBS_ENCODE_STATE_FINAL_ZERO;
			} else if (ret == 0) {
				__ASSERT_NO_MSG(num_processed != 0);

				encode->u.zeros.next_zero = zero_position;
				if (encode->u.zeros.next_zero == 0) {
					*output = 0x01;
				} else if (encode->u.zeros.next_zero < 254) {
					*output = encode->u.zeros.next_zero + 1;

					encode->state = COBS_ENCODE_STATE_ZEROS_DATA;
					encode->u.zeros.data_left = encode->u.zeros.next_zero;
					encode->u.zeros.post_data_state =
						COBS_ENCODE_STATE_ZEROS_CODE;
				} else {
					*output = 0xFF;

					encode->state = COBS_ENCODE_STATE_ZEROS_DATA;
					encode->u.zeros.data_left = 254;
					if (encode->u.zeros.next_zero == 254) {
						encode->u.zeros.post_data_state =
							COBS_ENCODE_STATE_ZEROS_FIRSTBYTE;
					} else {
						encode->u.zeros.post_data_state =
							COBS_ENCODE_STATE_ZEROS_CODE;
					}
				}
			} else {
				encode->state = COBS_ENCODE_STATE_NOZEROS_DATA;
				encode->u.nozeros = (struct cobs_encode_nozeros){
					.total_length = num_processed,
				};

				const size_t total_length = num_processed;
				if (total_length < 254) {
					*output = encode->u.nozeros.total_length + 1;

					encode->u.nozeros.data_left =
						encode->u.nozeros.total_length;
				} else {
					*output = 0xFF;

					encode->u.nozeros.data_left = 254;
				}
			}
		} else if (encode->u.zeros.next_zero < 254) {
			*output = encode->u.zeros.next_zero + 1;

			encode->state = COBS_ENCODE_STATE_ZEROS_DATA;
			encode->u.zeros.data_left = encode->u.zeros.next_zero;
			encode->u.zeros.post_data_state = COBS_ENCODE_STATE_ZEROS_CODE;
		} else {
			*output = 0xFF;

			encode->state = COBS_ENCODE_STATE_ZEROS_DATA;
			encode->u.zeros.data_left = 254;
			if (encode->u.zeros.next_zero == 254) {
				encode->u.zeros.post_data_state = COBS_ENCODE_STATE_ZEROS_FIRSTBYTE;
			} else {
				encode->u.zeros.post_data_state = COBS_ENCODE_STATE_ZEROS_CODE;
			}
		}

		break;
	}

	case COBS_ENCODE_STATE_ZEROS_DATA: {
		ret = cobs_buf_cursor_read(&encode->cursor, output, 1);
		__ASSERT_NO_MSG(ret == 0);
		ARG_UNUSED(ret);

		encode->u.zeros.data_left -= 1;
		encode->u.zeros.next_zero -= 1;

		if (encode->u.nozeros.data_left == 0) {
			encode->state = encode->u.zeros.post_data_state;
		}
		break;
	}

	case COBS_ENCODE_STATE_NOZEROS_CODE: {
		if (encode->u.nozeros.total_length == 0) {
			*output = 0x01;

			encode->state = COBS_ENCODE_STATE_FINAL_ZERO;
		} else if (encode->u.nozeros.total_length < 254) {
			*output = encode->u.nozeros.total_length + 1;

			encode->u.nozeros.data_left = encode->u.nozeros.total_length;
			encode->state = COBS_ENCODE_STATE_NOZEROS_DATA;
		} else {
			*output = 0xFF;

			encode->u.nozeros.data_left = 254;
			encode->state = COBS_ENCODE_STATE_NOZEROS_DATA;
		}
		break;
	}

	case COBS_ENCODE_STATE_NOZEROS_DATA: {
		ret = cobs_buf_cursor_read(&encode->cursor, output, 1);
		__ASSERT_NO_MSG(ret == 0);
		ARG_UNUSED(ret);

		encode->u.nozeros.data_left -= 1;
		encode->u.nozeros.total_length -= 1;

		if (encode->u.nozeros.total_length == 0) {
			__ASSERT_NO_MSG(encode->u.nozeros.data_left == 0);
			encode->state = COBS_ENCODE_STATE_FINAL_ZERO;
		} else if (encode->u.nozeros.data_left == 0) {
			encode->state = COBS_ENCODE_STATE_NOZEROS_CODE;
		}
		break;
	}

	case COBS_ENCODE_STATE_FINAL_ZERO:
		*output = 0x00;
		encode->state = COBS_ENCODE_STATE_FINISHED;
		break;

	case COBS_ENCODE_STATE_FINISHED:
		return true;
	}

	return false;
}

size_t cobs_encode_stream(struct cobs_encode *encode, uint8_t *output, size_t output_length)
{
	size_t i;

	for (i = 0; i < output_length; i += 1) {
		const bool done = cobs_encode_stream_single(encode, &output[i]);
		if (done) {
			return i;
		}
	}

	return i;
}
