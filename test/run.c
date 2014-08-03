
#include <fcntl.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <CUnit/CUnit.h>
#include <CUnit/Basic.h>

#include "test.h"

/*
 * Settable via --interactive in order to do the gtk tests
 * as that part's not automated yet.
 */
static int interactive;

static struct option opts[] = {
	{
		.name = "interactive",
		.has_arg = no_argument,
		.val = 'i',
	},
	{ NULL }
};

static int parse_cmdline(int argc, char **argv)
{
	int ch;
	int err;

	err = 0;
	while ((ch = getopt_long_only(argc, argv, "", opts, NULL)) != -1) {
		switch (ch) {
		case 'i':
			interactive = 1;
			break;
		default:
			err = 1;
			break;
		}
	}

	return err;
}

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

	if ((err = parse_cmdline(argc, argv)) != 0) {
		return err;
	}

	if (CU_initialize_registry() != CUE_SUCCESS) {
		fprintf(stderr, "%s\n", CU_get_error_msg());
		return EXIT_FAILURE;
	}

	SUITE(checkin);
	if (interactive) {
		/* FIXME: have the dialog tested "automatically"
		 * to avoid this "interactive" hack.
		 */
		SUITE(checkin_result);
	}

	err = CU_basic_run_tests() == CUE_SUCCESS ? 0 : 1;
	if (err == 0) {
		err =
			CU_get_number_of_suites_failed() +
			CU_get_number_of_tests_failed();
	}

	CU_cleanup_registry();

	return err;
}
