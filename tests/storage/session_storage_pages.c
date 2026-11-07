// Read-only page accounting for the synthetic comparison driver.
// Build separately with SQLITE_ENABLE_DBSTAT_VTAB; production flags stay intact.
#include <sqlite3.h>
#include <stdio.h>
int main(int argc,char **argv){sqlite3 *db=0;sqlite3_stmt *q=0;if(argc!=2)return 1;if(sqlite3_open_v2(argv[1],&db,SQLITE_OPEN_READONLY,0))return 2;if(sqlite3_key(db,"synthetic-storage-comparison",28))return 3;const char *sql="SELECT name,count(*),sum(pgsize),sum(payload),sum(unused) FROM dbstat GROUP BY name ORDER BY name";if(sqlite3_prepare_v2(db,sql,-1,&q,0)){fprintf(stderr,"%s\n",sqlite3_errmsg(db));return 4;}int step;while((step=sqlite3_step(q))==SQLITE_ROW)printf("%s,%lld,%lld,%lld,%lld\n",sqlite3_column_text(q,0),sqlite3_column_int64(q,1),sqlite3_column_int64(q,2),sqlite3_column_int64(q,3),sqlite3_column_int64(q,4));if(step!=SQLITE_DONE){fprintf(stderr,"%s\n",sqlite3_errmsg(db));sqlite3_finalize(q);sqlite3_close(db);return 5;}sqlite3_finalize(q);return sqlite3_close(db);}
