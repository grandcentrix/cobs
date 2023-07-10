# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(fuzz)

set(ENABLE_PYTHONUTILS ON)
add_subdirectory(../../testutils testutils)
find_package(Python3 REQUIRED COMPONENTS Development)

generate_inc_file_for_target(
	app
	test.py
	${ZEPHYR_BINARY_DIR}/include/generated/test.py.inc
)

target_sources(app PRIVATE src/main.c)
target_link_libraries(app PRIVATE
	COBS
	Python3::Python
	cobs_testutils
)
