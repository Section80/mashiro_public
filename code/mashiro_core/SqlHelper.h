#pragma once

#ifndef _SQL_HELPER_
#define _SQL_HELPER_

// ODBC sql helper

#include <sql.h>
#include <sqlext.h>
#include <buffer/Pool.h>

#define MAX_PARAM_COUNT 8
#define MAX_COLUMN_COUNT 8

enum class eDBType
{
	None, 
	MySQL,
	PostgreSQL, 

};

struct SqlHelper
{
	eDBType			dbType = eDBType::None;

	SQLINTEGER		native = 0;
	SQLWCHAR		state[7] = { 0 };
	SQLWCHAR		message[256] = { 0 };
	SQLSMALLINT		msgLength = 0;

	SQLHENV			hEnv = nullptr;

	SQLSMALLINT		hDbcRecord = 1;
	SQLHDBC			hDbc = nullptr;

	SQLSMALLINT		hStmtRecord = 1;
	SQLHSTMT		hStmt = nullptr;

	SQLLEN			paramBufferLenArr[MAX_PARAM_COUNT] = { 0 };
	SQLLEN			columnBufferLenArr[MAX_COLUMN_COUNT] = { 0 };

	bool			isConnected = false;
	bool			isPrepared = false;

	SQLWCHAR*		dns;
	SQLWCHAR*		user;
	SQLWCHAR*		pw;

};
extern thread_local SqlHelper* t_pSqlHelper;		// t_ means it's thread_local

bool sql_init_helper(SqlHelper* pHelper, eDBType dbType, const SQLWCHAR* dns, const SQLWCHAR* user, const SQLWCHAR* pw);

void sql_ping(SqlHelper* pHelper);		// keep alive

void sql_prepare_query(SqlHelper* pHelper, const WCHAR* query);

void sql_bind_param_wstring(SqlHelper* pHelper, SQLSMALLINT pos, std::wstring_view param);

// isPrepared: false -> true
// ret: rowsAffected
SQLLEN sql_execute(SqlHelper* pHelper);

void sql_bind_col_wstring(SqlHelper* pHelper, SQLSMALLINT pos, WCHAR* buffer, SQLLEN wcharCount);			// SQL_C_WCHAR
void sql_bind_col_tinyint(SqlHelper* pHelper, SQLSMALLINT pos, BYTE* buffer);								// SQL_C_UTINYINT
void sql_bind_col_tinyint(SqlHelper* pHelper, SQLSMALLINT pos, CHAR* buffer);								// SQL_C_TINYINT
void sql_bind_col_smallint(SqlHelper* pHelper, SQLSMALLINT pos, USHORT* buffer);							// SQL_C_USHORT
void sql_bind_col_smallint(SqlHelper* pHelper, SQLSMALLINT pos, SHORT* buffer);								// SQL_C_SSHORT
void sql_bind_col_int(SqlHelper* pHelper, SQLSMALLINT pos, UINT* buffer);									// SQL_C_ULONG
void sql_bind_col_int(SqlHelper* pHelper, SQLSMALLINT pos, INT* buffer);									// SQL_C_SLONG
void sql_bind_col_bigint(SqlHelper* pHelper, SQLSMALLINT pos, ULONGLONG* buffer);							// SQL_C_UBIGINT
void sql_bind_col_bigint(SqlHelper* pHelper, SQLSMALLINT pos, LONGLONG* buffer);							// SQL_C_SBIGINT

bool	sql_fetch(SqlHelper* pHelper);
void	sql_free_stmt(SqlHelper* pHelper);
SQLLEN	sql_fetched_size(SqlHelper* pHelper, SQLSMALLINT colPos);
void	sql_clean_up_helper(SqlHelper* pHelper);

#endif
