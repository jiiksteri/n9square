
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>


#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <CUnit/CUnit.h>
#include <CUnit/Basic.h>

#include "test.h"

int read_file(char **str, const char *name)
{
	struct stat sbuf;
	int fd, n;

	*str = NULL;

	fd = open(name, O_RDONLY);
	if (fd == -1) {
		fprintf(stderr, "open(%s): %s\n", name, strerror(errno));
		return errno;
	}

	if (fstat(fd, &sbuf) == -1) {
		fprintf(stderr, "stat(%s): %s\n", name, strerror(errno));
		close(fd);
		return errno;
	}

	*str = malloc(sbuf.st_size + 1);
	if (*str == NULL) {
		perror("malloc()");
		close(fd);
		return errno;
	}

	if ((n = read(fd, *str, sbuf.st_size)) != sbuf.st_size) {
		fprintf(stderr, "read(%s): short read (%d/%ld)\n",
			name, n, sbuf.st_size);
		free(*str);
		*str = NULL;
		close(fd);
		return errno;
	}
	(*str)[sbuf.st_size] = '\0';

	close(fd);

	return 0;
}



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
