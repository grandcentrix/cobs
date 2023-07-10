#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include <cobs/testutils_python.h>

#include <zephyr/init.h>
#include <zephyr/sys/__assert.h>

static PyObject *main_module;
static PyObject *global_dict;
static PyObject *python_fn_process;

PyObject *run_python_fn(const uint8_t *const input_data, const size_t input_data_size,
			const uint8_t **const output, size_t *const output_size)
{
	PyObject *const args = PyTuple_New(1);
	__ASSERT_NO_MSG(args);

	PyObject *const python_input_data = PyBytes_FromStringAndSize(input_data, input_data_size);
	__ASSERT_NO_MSG(python_input_data);
	PyTuple_SetItem(args, 0, python_input_data);

	PyObject *const return_value = PyObject_CallObject(python_fn_process, args);
	Py_DECREF(args);

	PyObject *const exception = PyErr_Occurred();
	if (exception) {
		return NULL;
	}

	__ASSERT_NO_MSG(return_value);

	*output = PyBytes_AsString(return_value);
	__ASSERT_NO_MSG(*output);
	*output_size = (size_t)PyBytes_Size(return_value);

	return return_value;
}

static int fuzz_init(void)
{
	int ret;

	Py_Initialize();

	ret = PyRun_SimpleString(cobs_testutils_script);
	__ASSERT_NO_MSG(ret == 0);

	main_module = PyImport_AddModule("__main__");
	__ASSERT_NO_MSG(main_module);
	global_dict = PyModule_GetDict(main_module);
	__ASSERT_NO_MSG(global_dict);
	python_fn_process = PyDict_GetItemString(global_dict, "process");
	__ASSERT_NO_MSG(python_fn_process);

	return 0;
}
SYS_INIT(fuzz_init, APPLICATION, CONFIG_KERNEL_INIT_PRIORITY_DEVICE);
