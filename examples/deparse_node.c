// Render a single node back to SQL, including one assembled by hand that was
// never part of any statement.
//
//   cc -I../ -L../ deparse_node.c -lpg_query -o deparse_node

#include <pg_query.h>
#include <protobuf/pg_query.pb-c.h>
#include <stdio.h>
#include <stdlib.h>

static void
deparse_and_print(const char *what, PgQuery__Node *node)
{
	size_t		len = pg_query__node__get_packed_size(node);
	void	   *buf = malloc(len);
	PgQueryProtobuf pbuf;
	PgQueryDeparseResult result;

	pg_query__node__pack(node, buf);
	pbuf.len = len;
	pbuf.data = buf;

	result = pg_query_deparse_node_protobuf(pbuf);
	if (result.error)
		printf("%-22s ERROR %s\n", what, result.error->message);
	else
		printf("%-22s %s\n", what, result.query);

	pg_query_free_deparse_result(result);
	free(buf);
}

/* lower(t."Name") -- built from scratch, not parsed from anything */
static void
synthesized_func_call(void)
{
	PgQuery__String funcname = PG_QUERY__STRING__INIT;
	PgQuery__Node funcname_node = PG_QUERY__NODE__INIT;
	PgQuery__String field1 = PG_QUERY__STRING__INIT;
	PgQuery__String field2 = PG_QUERY__STRING__INIT;
	PgQuery__Node field1_node = PG_QUERY__NODE__INIT;
	PgQuery__Node field2_node = PG_QUERY__NODE__INIT;
	PgQuery__Node *fields[2];
	PgQuery__ColumnRef colref = PG_QUERY__COLUMN_REF__INIT;
	PgQuery__Node colref_node = PG_QUERY__NODE__INIT;
	PgQuery__Node *args[1];
	PgQuery__FuncCall funccall = PG_QUERY__FUNC_CALL__INIT;
	PgQuery__Node funccall_node = PG_QUERY__NODE__INIT;

	funcname.sval = "lower";
	funcname_node.node_case = PG_QUERY__NODE__NODE_STRING;
	funcname_node.string = &funcname;

	field1.sval = "t";
	field1_node.node_case = PG_QUERY__NODE__NODE_STRING;
	field1_node.string = &field1;
	field2.sval = "Name";
	field2_node.node_case = PG_QUERY__NODE__NODE_STRING;
	field2_node.string = &field2;
	fields[0] = &field1_node;
	fields[1] = &field2_node;

	colref.n_fields = 2;
	colref.fields = fields;
	colref.location = -1;
	colref_node.node_case = PG_QUERY__NODE__NODE_COLUMN_REF;
	colref_node.column_ref = &colref;
	args[0] = &colref_node;

	funccall.n_funcname = 1;
	funccall.funcname = (PgQuery__Node *[]) { &funcname_node };
	funccall.n_args = 1;
	funccall.args = args;
	funccall.location = -1;
	funccall_node.node_case = PG_QUERY__NODE__NODE_FUNC_CALL;
	funccall_node.func_call = &funccall;

	deparse_and_print("synthesized FuncCall", &funccall_node);
}

/* Pull nodes back out of a parsed statement and render them individually. */
static void
nodes_lifted_from_a_parse_tree(void)
{
	const char *sql = "SELECT lower(t.name) AS n FROM public.tbl t WHERE a + 1 > 2 ORDER BY n DESC";
	PgQueryProtobufParseResult parsed = pg_query_parse_protobuf(sql);
	PgQuery__ParseResult *tree;
	PgQuery__SelectStmt *select;

	if (parsed.error)
	{
		printf("parse failed: %s\n", parsed.error->message);
		pg_query_free_protobuf_parse_result(parsed);
		return;
	}

	tree = pg_query__parse_result__unpack(NULL, parsed.parse_tree.len,
										  (const uint8_t *) parsed.parse_tree.data);
	select = tree->stmts[0]->stmt->select_stmt;

	deparse_and_print("ResTarget", select->target_list[0]);
	deparse_and_print("  its FuncCall", select->target_list[0]->res_target->val);
	deparse_and_print("RangeVar", select->from_clause[0]);
	deparse_and_print("A_Expr (where)", select->where_clause);
	deparse_and_print("SortBy", select->sort_clause[0]);
	deparse_and_print("whole SelectStmt", tree->stmts[0]->stmt);

	pg_query__parse_result__free_unpacked(tree, NULL);
	pg_query_free_protobuf_parse_result(parsed);
}

int
main(void)
{
	synthesized_func_call();
	nodes_lifted_from_a_parse_tree();
	pg_query_exit();
	return 0;
}
