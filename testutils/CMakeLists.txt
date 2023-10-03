# SPDX-License-Identifier: MIT

find_package(Python3 REQUIRED COMPONENTS Development)

add_library(cobs_testutils
	common.c
	$<$<BOOL:${ENABLE_PYTHONUTILS}>:python.c>
)
target_link_libraries(cobs_testutils PRIVATE
	COBS
	$<$<BOOL:${ENABLE_PYTHONUTILS}>:Python3::Python>
)
target_include_directories(cobs_testutils PUBLIC include)
