
#include <stdlib.h>

#include <CUnit/CUnit.h>

#include "../checkin.h"

#include "test.h"

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
		"You've been here 411 times!",
		NULL,
	};

	CU_ASSERT(read_file(&str, "test/checkin_response.json") == 0);

	CU_ASSERT(checkin_parse(&checkin, str) == 0);
	CU_ASSERT(checkin != NULL);

	memset(&ctx, 0, sizeof(ctx));
	ctx.expected = expected;
	checkin_message_foreach(checkin, match_strings, &ctx);
	CU_ASSERT(ctx.count == num_strings(expected));

	CU_ASSERT_STRING_EQUAL(checkin_venue(checkin), "Lidl (Ratavartijankatu 3)");

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
