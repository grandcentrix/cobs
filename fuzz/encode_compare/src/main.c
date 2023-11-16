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

#define MAX_INPUT_SIZE  4096
#define MAX_OVERHEAD(n) (1 + (n) / 254 + 1)

static uint8_t encoded[MAX_INPUT_SIZE + MAX_OVERHEAD(MAX_INPUT_SIZE)];

const char cobs_testutils_script[] = {
#include <test.py.inc>
	0x00,
};

static void compare_result(const char *const logprefix, const void *const input,
			   const size_t input_size, const void *const python_encoded,
			   const size_t python_encoded_size, const void *const encoded,
			   const size_t encoded_size)
{
	if (encoded_size != python_encoded_size ||
	    memcmp(encoded, python_encoded, python_encoded_size)) {
		LOG_ERR("%s: stream-encoded data doesn't match python output", logprefix);
		PyErr_Print();

		LOG_HEXDUMP_DBG(input, input_size, "input");
		LOG_HEXDUMP_DBG(encoded, encoded_size, "encoded");
		LOG_HEXDUMP_DBG(python_encoded, python_encoded_size, "python_encoded");

		__ASSERT_NO_MSG(false);
	}
}

static void fuzzer_test_one_input(const uint8_t *const input, const size_t input_size)
{
	size_t encoded_size;
	int ret;

	const uint8_t *python_encoded = NULL;
	size_t python_encoded_size = 0;
	PyObject *const python_return =
		run_python_fn(input, input_size, &python_encoded, &python_encoded_size);
	if (!python_return) {
		PyErr_Print();
		LOG_HEXDUMP_DBG(input, input_size, "input");
		__ASSERT(false, "Python failed to encode data");
	}

	encoded_size = cobs_encode_stream_simple(input, input_size, encoded, sizeof(encoded));
	compare_result("stream", input, input_size, python_encoded, python_encoded_size, encoded,
		       encoded_size - 1);

	encoded_size = cobs_encode(input, input_size, encoded);
	__ASSERT_NO_MSG(encoded_size <= sizeof(encoded));
	compare_result("normal", input, input_size, python_encoded, python_encoded_size, encoded,
		       encoded_size);

	Py_DECREF(python_return);
	PyErr_Clear();
}

static K_SEM_DEFINE(fuzz_sem, 0, K_SEM_MAX_LIMIT);

static void fuzz_isr(const void *arg)
{
	/* We could call check0() to execute the fuzz case here, but
	 * pass it through to the main thread instead to get more OS
	 * coverage.
	 */
	k_sem_give(&fuzz_sem);
}

int main(void)
{
	extern const uint8_t *posix_fuzz_buf;
	extern size_t posix_fuzz_sz;

	IRQ_CONNECT(CONFIG_ARCH_POSIX_FUZZ_IRQ, 0, fuzz_isr, NULL, 0);
	irq_enable(CONFIG_ARCH_POSIX_FUZZ_IRQ);

	while (true) {
		k_sem_take(&fuzz_sem, K_FOREVER);

		/* Execute the fuzz case we got from LLVM and passed
		 * through an interrupt to this thread.
		 */
		fuzzer_test_one_input(posix_fuzz_buf, posix_fuzz_sz);
	}

	return 0;
}
