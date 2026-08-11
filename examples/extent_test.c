// Dump the raw parse tree as JSON for a handful of queries, so the
// location/location_end pairs can be checked against the source text.
//
//   cc -I. -L. extent_test.c -lpg_query -o extent_test

#include <pg_query.h>
#include <stdio.h>

static const char *tests[] = {
	"SELECT foo.bar FROM public.tbl AS t WHERE lower(t.name) = 'x'",
	"SELECT a FROM t ORDER BY a LIMIT 5",
	"SELECT 1 UNION SELECT 2",
	"SELECT count(*) FROM \"MyTable\" m JOIN other o ON o.id = m.id",
};

int
main(void)
{
	size_t		i;

	for (i = 0; i < sizeof(tests) / sizeof(tests[0]); i++)
	{
		PgQueryParseResult result = pg_query_parse(tests[i]);

		printf("###QUERY %s\n", tests[i]);
		if (result.error)
			printf("###ERROR %s at %d\n", result.error->message, result.error->cursorpos);
		else
			printf("###TREE %s\n", result.parse_tree);

		pg_query_free_parse_result(result);
	}

	pg_query_exit();
	return 0;
}
