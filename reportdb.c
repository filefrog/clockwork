#include "reportdb.h"

#define PREP(db, stmt, sql) do { \
	rc = sqlite3_prepare_v2((db),(sql), -1, &(stmt), NULL); \
	if (rc != SQLITE_OK) { goto failure; } \
} while (0)

#define FINALIZE(stmt) do { \
	if (stmt) { \
		rc = sqlite3_finalize(stmt); \
		(stmt) = NULL; \
	} \
} while (0);

#define BIND_INT(stmt,n,v) do { \
	rc = sqlite3_bind_int((stmt),(n),(v)); \
	if (rc != SQLITE_OK) { goto failure; } \
} while (0)

#define BIND_INT64(stmt,n,v) do { \
	rc = sqlite3_bind_int64((stmt),(n),(v)); \
	if (rc != SQLITE_OK) { goto failure; } \
} while (0)

#define BIND_TEXT(stmt,n,v) do { \
	rc = sqlite3_bind_text((stmt),(n),(v),-1,SQLITE_STATIC); \
	if (rc != SQLITE_OK) { goto failure; } \
} while (0)

#define EXEC_SQL(stmt) do { \
	rc = sqlite3_step(stmt); \
	if (rc != SQLITE_DONE) { goto failure; } \
} while (0)

#define RESET_SQL(stmt) do { \
	rc = sqlite3_reset(stmt); \
	if (rc != SQLITE_OK) { goto failure; } \
} while (0)

struct reportdb* reportdb_open(enum reportdb_type type, const char *path)
{
	assert(path);

	struct reportdb *db = xmalloc(sizeof(struct reportdb));
	db->db_type = type;

	if (sqlite3_open(path, &db->db) != 0) {
		sqlite3_close(db->db);
		free(db);
		DEBUG("sqlite3_open failed");
		return NULL;
	}

	db->path = xstrdup(path);
	return db;
}

int reportdb_close(struct reportdb *db)
{
	assert(db);
	sqlite3_close(db->db);
	db->db = NULL;
	return 0;
}

/* FIXME: split this up into statics; let the compiler optimize */
rowid masterdb_host(struct reportdb *db, const char *host)
{
	rowid host_id = NULL_ROWID;
	const char *select = "SELECT id FROM hosts WHERE host_name = ?;";
	const char *create = "INSERT INTO hosts (name) VALUES (?);";
	sqlite3_stmt *stmt = NULL;
	int rc;

	PREP(db->db, stmt, select);
	BIND_TEXT(stmt, 1, host);

	rc = sqlite3_step(stmt);
	switch (rc) {
	case SQLITE_DONE:
		FINALIZE(stmt);
		break;

	case SQLITE_ROW:
		host_id = sqlite3_column_int(stmt, 0);
		/* no break; fall-through */

	default:
		sqlite3_finalize(stmt);
		return host_id;
	}

	/* at this point, create a new one */
	PREP(db->db, stmt, create);
	BIND_TEXT(stmt, 1, host);

	EXEC_SQL(stmt);
	host_id = sqlite3_last_insert_rowid(db->db);
	FINALIZE(stmt);
	return host_id;

failure:
	DEBUG("Sqlite3 Error: %s", sqlite3_errmsg(db->db));
	FINALIZE(stmt);
	return NULL_ROWID;
}

/* FIXME: split this up into statics; let the compiler optimize */
int masterdb_store_report(struct reportdb *db, rowid host_id, struct list *reports)
{
	const char *j_sql = "INSERT INTO jobs (host_id) VALUES (?)";
	sqlite3_stmt *j_stmt = NULL;
	rowid j_id = 0;

	const char *r_sql = "INSERT INTO resources (job_id, type, name, sequence, compliant, fixed) VALUES (?,?,?,?,?,?)";
	sqlite3_stmt *r_stmt = NULL;
	rowid r_id = 0;
	int r_seq = 0;

	const char *a_sql = "INSERT INTO actions (resource_id, summary, sequence, result) VALUES (?,?,?,?)";
	sqlite3_stmt *a_stmt = NULL;
	int a_seq = 0;

	struct report *report;
	struct action *action;

	int rc;

	PREP(db->db, j_stmt, j_sql);
	PREP(db->db, r_stmt, r_sql);
	PREP(db->db, a_stmt, a_sql);

	/* insert job */
	BIND_INT64(j_stmt, 1, host_id);
	EXEC_SQL(j_stmt);
	FINALIZE(j_stmt);
	j_id = sqlite3_last_insert_rowid(db->db);

	for_each_node(report, reports, rep) {
		BIND_INT64(r_stmt, 1, j_id);
		BIND_TEXT(r_stmt,  2, report->res_type);
		BIND_TEXT(r_stmt,  3, report->res_key);
		BIND_INT(r_stmt,   4, r_seq++);
		BIND_INT(r_stmt,   5, report->compliant ? 1 : 0);
		BIND_INT(r_stmt,   6, report->fixed     ? 1 : 0);
		EXEC_SQL(r_stmt);

		r_id = sqlite3_last_insert_rowid(db->db);
		a_seq = 0;
		for_each_node(action, &report->actions, report) {
			BIND_INT64(a_stmt, 0, r_id);
			BIND_TEXT(a_stmt,  1, action->summary);
			BIND_INT(a_stmt,   2, a_seq++);
			BIND_INT(a_stmt,   3, action->result);

			EXEC_SQL(a_stmt);
			RESET_SQL(a_stmt);
		}
		RESET_SQL(r_stmt);
	}

	return 0;

failure:
	DEBUG("Sqlite3 Error: %s", sqlite3_errmsg(db->db));
	FINALIZE(j_stmt);
	FINALIZE(r_stmt);
	FINALIZE(a_stmt);
	return -1;
}

/* FIXME: split this up into statics; let the compiler optimize */
int agentdb_store_report(struct reportdb *db, struct list *reports, struct timeval *start, struct timeval *end)
{
	assert(start);
	assert(end);

	struct timeval diff;

	const char *j_sql = "INSERT INTO jobs (started_at, ended_at, duration) VALUES (?,?,?)";
	sqlite3_stmt *j_stmt = NULL;
	rowid j_id = 0;

	const char *r_sql = "INSERT INTO resources (job_id, type, name, sequence, compliant, fixed) VALUES (?,?,?,?,?,?)";
	sqlite3_stmt *r_stmt = NULL;
	rowid r_id = 0;
	int r_seq = 0;

	const char *a_sql = "INSERT INTO actions (resource_id, summary, sequence, result) VALUES (?,?,?,?)";
	sqlite3_stmt *a_stmt = NULL;
	int a_seq = 0;

	struct report *report;
	struct action *action;

	int rc;

	timersub(end, start, &diff);

	PREP(db->db, j_stmt, j_sql);
	PREP(db->db, r_stmt, r_sql);
	PREP(db->db, a_stmt, a_sql);

	/* job */
	BIND_INT(j_stmt,  1, start->tv_sec);
	BIND_INT(j_stmt,  2, end->tv_sec);
	BIND_INT(j_stmt,  3, diff.tv_sec * 1000000 + diff.tv_usec);

	EXEC_SQL(j_stmt);
	FINALIZE(j_stmt);
	j_id = sqlite3_last_insert_rowid(db->db);

	for_each_node(report, reports, rep) {
		BIND_INT64(r_stmt, 1, j_id);
		BIND_TEXT(r_stmt,  2, report->res_type);
		BIND_TEXT(r_stmt,  3, report->res_key);
		BIND_INT(r_stmt,   4, r_seq++);
		BIND_INT(r_stmt,   5, report->compliant);
		BIND_INT(r_stmt,   6, report->fixed);

		EXEC_SQL(r_stmt);
		r_id = sqlite3_last_insert_rowid(db->db);

		a_seq = 0;
		for_each_node(action, &report->actions, report) {
			BIND_INT64(a_stmt, 1, r_id);
			BIND_TEXT( a_stmt, 2, action->summary);
			BIND_INT(  a_stmt, 3, a_seq++);
			BIND_INT(  a_stmt, 4, action->result);

			EXEC_SQL(a_stmt);
			RESET_SQL(a_stmt);
		}
		RESET_SQL(r_stmt);
	}

	return 0;

failure:
	DEBUG("Sqlite3 Error: %s", sqlite3_errmsg(db->db));
	FINALIZE(r_stmt);
	FINALIZE(a_stmt);
	return -1;
}

