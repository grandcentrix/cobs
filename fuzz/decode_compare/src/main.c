#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include <string.h>
#include <zephyr/init.h>
#include <zephyr/irq.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/__assert.h>
#include <cobs.h>
#include <cobs/testutils.h>
#include <cobs/testutils_python.h>

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(fuzz, LOG_LEVEL_DBG);

static uint8_t decoded[4096];

const char cobs_testutils_script[] = {
#include <test.py.inc>
	0x00,
};

static void compare_result(const char *const logprefix, const void *const input,
			   const size_t input_size, const void *const python_decoded,
			   const size_t python_decoded_size, const int ret,
			   const void *const decoded, const size_t decoded_size)
{
	if ((python_decoded != NULL) != (ret == 0)) {
		LOG_ERR("%s: Disagreement about data validity", logprefix);
		PyErr_Print();

		LOG_HEXDUMP_DBG(input, input_size, "input");
		if (ret == 0) {
			LOG_HEXDUMP_DBG(decoded, decoded_size, "decoded");
		} else {
			LOG_DBG("C decoder failed with: %d", ret);
		}

		if (python_decoded) {
			LOG_HEXDUMP_DBG(python_decoded, python_decoded_size, "python_decoded");
		} else {
			LOG_DBG("Python decoder failed.");
		}

		__ASSERT_NO_MSG(false);
	}

	if (python_decoded && (decoded_size != python_decoded_size ||
			       memcmp(decoded, python_decoded, python_decoded_size))) {
		LOG_ERR("%s: stream-decoded data doesn't match python output", logprefix);
		PyErr_Print();

		LOG_HEXDUMP_DBG(input, input_size, "input");
		LOG_HEXDUMP_DBG(decoded, decoded_size, "decoded");
		LOG_HEXDUMP_DBG(python_decoded, python_decoded_size, "python_decoded");

		__ASSERT_NO_MSG(false);
	}
}

static int cobs_decode_withzero(const uint8_t *const restrict input, const size_t length,
				uint8_t *const restrict output, size_t *const decoded_size)
{
	if (length == 0) {
		return -EINVAL;
	}
	if (input[length - 1] != 0x00) {
		return -EINVAL;
	}

	return cobs_decode(input, length - 1, output, decoded_size);
}

static int cobs_decode_inplace_withzero(uint8_t *const input, const size_t length,
					size_t *const decoded_size)
{
	if (length == 0) {
		return -EINVAL;
	}
	if (input[length - 1] != 0x00) {
		return -EINVAL;
	}

	return cobs_decode_inplace(input, length - 1, decoded_size);
}

int fuzzer_test_one_input(const uint8_t *const input, const size_t input_size)
{
	size_t decoded_size;
	int ret;

	const uint8_t *python_decoded = NULL;
	size_t python_decoded_size = 0;
	PyObject *const python_return =
		run_python_fn(input, input_size, &python_decoded, &python_decoded_size);

	decoded_size = 0;
	ret = cobs_decode_stream_simple(input, input_size, decoded, sizeof(decoded), &decoded_size);
	compare_result("stream", input, input_size, python_decoded, python_decoded_size, ret,
		       decoded, decoded_size);

	decoded_size = 0;
	ret = cobs_decode_withzero(input, input_size, decoded, &decoded_size);
	compare_result("normal", input, input_size, python_decoded, python_decoded_size, ret,
		       decoded, decoded_size);

	__ASSERT_NO_MSG(input_size <= sizeof(decoded));
	memcpy(decoded, input, input_size);

	decoded_size = 0;
	ret = cobs_decode_inplace_withzero(decoded, input_size, &decoded_size);
	compare_result("inplace", input, input_size, python_decoded, python_decoded_size, ret,
		       decoded, decoded_size);

	if (python_return) {
		Py_DECREF(python_return);
	}

	PyErr_Clear();
	return 0;
}
