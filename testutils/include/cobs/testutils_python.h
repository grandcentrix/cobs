#ifndef COBS_TESTUTILS_PYTHON_H
#define COBS_TESTUTILS_PYTHON_H

extern const char cobs_testutils_script[];

PyObject *run_python_fn(const uint8_t *const input_data, const size_t input_data_size,
			const uint8_t **const output, size_t *const output_size);

#endif /* COBS_TESTUTILS_PYTHON_H */
