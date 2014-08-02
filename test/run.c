
#include <stdio.h>
#include <stdlib.h>

#include <CUnit/CUnit.h>
#include <CUnit/Basic.h>

#define SUITE(n)						\
	do {							\
		void register_suite_##n(void);		\
		register_suite_##n();				\
	} while (0)


int main(int argc, char **argv)
{
	int err;

	if (CU_initialize_registry() != CUE_SUCCESS) {
		fprintf(stderr, "%s\n", CU_get_error_msg());
		return EXIT_FAILURE;
	}

	SUITE(checkin);

	err = CU_basic_run_tests() == CUE_SUCCESS ? 0 : 1;
	if (err == 0) {
		err =
			CU_get_number_of_suites_failed() +
			CU_get_number_of_tests_failed();
	}

	CU_cleanup_registry();

	return err;
}
