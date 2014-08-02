
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <CUnit/CUnit.h>

#include "../checkin.h"


static int read_file(char **str, const char *name)
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

struct iter_ctx {
	int count;
	int gone_bad;
	const char **expected;
};

static void match_strings(const char *actual, void *_ctx)
{
	struct iter_ctx *ctx = _ctx;
	const char *expected;
	int ok;

	if (ctx->gone_bad) {
		return;
	}

	expected = ctx->expected[ctx->count];
	ok = expected != NULL && (ctx->gone_bad = strcmp(expected, actual)) == 0;
	printf("\n%s(): expected vs actual (%s):"
	       "\n  '%s'"
	       "\n  '%s'"
	       "\n",
	       __func__,
	       ok ? "OK" : "NOT OK",
	       expected, actual);

	if (!ok) {
		CU_FAIL();
	}
	ctx->count++;
}

static int num_strings(const char **strs)
{
	int i = 0;
	while (*(strs++)) {
		i++;
	}
	return i;
}

void test_parse(void)
{
	struct iter_ctx ctx;
	struct checkin *checkin;
	char *str;
	const char *expected[] = {
		"That's 50 straight weeks at Lidl. You set a new record!",
		NULL,
	};

	CU_ASSERT(read_file(&str, "test/checkin_response.json") == 0);

	CU_ASSERT(checkin_parse(&checkin, str) == 0);
	CU_ASSERT(checkin != NULL);

	memset(&ctx, 0, sizeof(ctx));
	ctx.expected = expected;
	checkin_message_foreach(checkin, match_strings, &ctx);
	CU_ASSERT(ctx.count == num_strings(expected));

	checkin_free(checkin);
	free(str);
}


static int suite_init_checkin(void)
{
	return 0;
}

static int suite_cleanup_checkin(void)
{
	return 0;
}

void register_suite_checkin(void)
{
	CU_pSuite suite;

	suite = CU_add_suite("checkin", suite_init_checkin, suite_cleanup_checkin);
	CU_add_test(suite, "Simple checkin parse test", test_parse);
}
