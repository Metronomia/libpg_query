#include "pg_query.h"
#include "pg_query_internal.h"

#include "utils/builtins.h"

/*
 * Expose Postgres' own identifier quoting.
 *
 * Deciding whether an identifier can be written bare needs the keyword table:
 * "name", "value" and "type" are safe unquoted while "create", "or" and "select"
 * are not, and that distinction is not derivable from the spelling. Rendering a
 * ColumnRef through the deparser gets the same answer, but pays for a protobuf
 * round trip and a DeparseState to do it.
 *
 * Note that this does not enforce NAMEDATALEN. Postgres truncates identifiers to
 * 63 bytes when it reads them, so a longer name quotes fine here but will not
 * round-trip against the catalog.
 */
PgQueryQuoteIdentifierResult pg_query_quote_identifier(const char* ident)
{
	MemoryContext ctx = NULL;
	PgQueryQuoteIdentifierResult result = {0};

	ctx = pg_query_enter_memory_context();

	PG_TRY();
	{
		const char *quoted = quote_identifier(ident);

		/*
		 * quote_identifier hands back the pointer it was given when no quoting was
		 * required, and a palloc'd copy otherwise, which is the cheapest way to tell
		 * the two apart.
		 */
		result.needs_quotes = quoted != ident;
		result.quoted_identifier = strdup(quoted);
	}
	PG_CATCH();
	{
		ErrorData* error_data;
		PgQueryError* error;

		MemoryContextSwitchTo(ctx);
		error_data = CopyErrorData();

		error = malloc(sizeof(PgQueryError));
		error->message   = strdup(error_data->message);
		error->filename  = strdup(error_data->filename);
		error->funcname  = strdup(error_data->funcname);
		error->context   = NULL;
		error->lineno    = error_data->lineno;
		error->cursorpos = error_data->cursorpos;

		result.error = error;
		FlushErrorState();
	}
	PG_END_TRY();

	pg_query_exit_memory_context(ctx);

	return result;
}

/*
 * The same decision without producing a string, for callers that only want to know
 * whether a name can be written bare. The palloc'd copy quote_identifier may make
 * dies with the memory context, so nothing crosses the boundary but the bool.
 *
 * Errors are reported as "needs quotes": quoting a name that did not need it is
 * harmless, leaving one unquoted that did is not.
 */
bool pg_query_identifier_needs_quotes(const char* ident)
{
	MemoryContext ctx = NULL;
	bool needs_quotes = true;

	ctx = pg_query_enter_memory_context();

	PG_TRY();
	{
		needs_quotes = quote_identifier(ident) != ident;
	}
	PG_CATCH();
	{
		MemoryContextSwitchTo(ctx);
		FlushErrorState();
		needs_quotes = true;
	}
	PG_END_TRY();

	pg_query_exit_memory_context(ctx);

	return needs_quotes;
}

void pg_query_free_quote_identifier_result(PgQueryQuoteIdentifierResult result)
{
	if (result.error) {
		free(result.error->message);
		free(result.error->filename);
		free(result.error->funcname);
		free(result.error);
	}

	free(result.quoted_identifier);
}
