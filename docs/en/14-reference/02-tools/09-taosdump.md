---
title: taosdump Reference
sidebar_label: taosdump
slug: /tdengine-reference/tools/taosdump
---

taosdump is a tool application that supports backing up data from a running TDengine cluster and restoring the backed-up data to the same or another running TDengine cluster.

taosdump can back up data using databases, supertables, or basic tables as logical data units, and can also back up data records within a specified time period from databases, supertables, and basic tables. You can specify the directory path for data backup; if not specified, taosdump defaults to backing up data to the current directory.

If the specified location already has data files, taosdump will prompt the user and exit immediately to avoid data being overwritten. This means the same path can only be used for one backup.
If you see related prompts, please operate carefully.

taosdump is a logical backup tool, it should not be used to back up any raw data, environment settings, hardware information, server configuration, or cluster topology. taosdump uses [Apache AVRO](https://avro.apache.org/) as the data file format to store backup data.

## Installation

There are two ways to install taosdump:

- Install the official taosTools package, please find taosTools on the [release history page](../../../release-history/taostools/) and download it for installation.

- Compile taos-tools separately and install, please refer to the [taos-tools](https://github.com/taosdata/taos-tools) repository for details.

## Common Use Cases

### taosdump Backup Data

1. Backup all databases: specify the `-A` or `--all-databases` parameter;
2. Backup multiple specified databases: use the `-D db1,db2,...` parameter;
3. Backup certain supertables or basic tables in a specified database: use the `dbname stbname1 stbname2 tbname1 tbname2 ...` parameter, note that this input sequence starts with the database name, supports only one database, and the second and subsequent parameters are the names of the supertables or basic tables in that database, separated by spaces;
4. Backup the system log database: TDengine clusters usually include a system database named `log`, which contains data for TDengine's own operation, taosdump does not back up the log database by default. If there is a specific need to back up the log database, you can use the `-a` or `--allow-sys` command line parameter.
5. "Tolerant" mode backup: Versions after taosdump 1.4.1 provide the `-n` and `-L` parameters, used for backing up data without using escape characters and in "tolerant" mode, which can reduce backup data time and space occupied when table names, column names, and label names do not use escape characters. If unsure whether to use `-n` and `-L`, use the default parameters for "strict" mode backup. For an explanation of escape characters, please refer to the [official documentation](../../sql-manual/escape-characters/).

:::tip

- Versions after taosdump 1.4.1 provide the `-I` parameter, used for parsing avro file schema and data, specifying the `-s` parameter will only parse the schema.
- Backups after taosdump 1.4.2 use the `-B` parameter to specify the number of batches, the default value is 16384. If "Error actual dump .. batch .." occurs due to insufficient network speed or disk performance in some environments, you can try adjusting the `-B` parameter to a smaller value.
- taosdump's export does not support interruption recovery, so the correct way to handle an unexpected termination of the process is to delete all related files that have been exported or generated.
- taosdump's import supports interruption recovery, but when the process restarts, you may receive some "table already exists" prompts, which can be ignored.

:::

### taosdump Restore Data

Restore data files from a specified path: use the `-i` parameter along with the data file path. As mentioned earlier, the same directory should not be used to back up different data sets, nor should the same path be used to back up the same data set multiple times, otherwise, the backup data will cause overwriting or multiple backups.

:::tip
taosdump internally uses the TDengine stmt binding API to write restored data, currently using 16384 as a batch for writing. If there are many columns in the backup data, it may cause a "WAL size exceeds limit" error, in which case you can try adjusting the `-B` parameter to a smaller value.

:::

## Detailed Command Line Parameters List

Below is the detailed command line parameters list for taosdump:

```text
Usage: taosdump [OPTION...] dbname [tbname ...]
  or:  taosdump [OPTION...] --databases db1,db2,...
  or:  taosdump [OPTION...] --all-databases
  or:  taosdump [OPTION...] -i inpath
  or:  taosdump [OPTION...] -o outpath

  -h, --host=HOST            Server host dumping data from. Default is
                             localhost.
  -p, --password             User password to connect to server. Default is
                             taosdata.
  -P, --port=PORT            Port to connect
  -u, --user=USER            User name used to connect to server. Default is
                             root.
  -c, --config-dir=CONFIG_DIR   Configure directory. Default is /etc/taos
  -i, --inpath=INPATH        Input file path.
  -o, --outpath=OUTPATH      Output file path.
  -r, --resultFile=RESULTFILE   DumpOut/In Result file path and name.
  -a, --allow-sys            Allow to dump system database
  -A, --all-databases        Dump all databases.
  -D, --databases=DATABASES  Dump inputted databases. Use comma to separate
                             databases' name.
  -e, --escape-character     Use escaped character for database name
  -N, --without-property     Dump database without its properties.
  -s, --schemaonly           Only dump tables' schema.
  -d, --avro-codec=snappy    Choose an avro codec among null, deflate, snappy,
                             and lzma.
  -S, --start-time=START_TIME   Start time to dump. Either epoch or
                             ISO8601/RFC3339 format is acceptable. ISO8601
                             format example: 2017-10-01T00:00:00.000+0800 or
                             2017-10-0100:00:00:000+0800 or '2017-10-01
                             00:00:00.000+0800'
  -E, --end-time=END_TIME    End time to dump. Either epoch or ISO8601/RFC3339
                             format is acceptable. ISO8601 format example:
                             2017-10-01T00:00:00.000+0800 or
                             2017-10-0100:00:00.000+0800 or '2017-10-01
                             00:00:00.000+0800'
  -B, --data-batch=DATA_BATCH   Number of data per query/insert statement when
                             backup/restore. Default value is 16384. If you see
                             'error actual dump .. batch ..' when backup or if
                             you see 'WAL size exceeds limit' error when
                             restore, please adjust the value to a smaller one
                             and try. The workable value is related to the
                             length of the row and type of table schema.
  -I, --inspect              inspect avro file content and print on screen
  -L, --loose-mode           Using loose mode if the table name and column name
                             use letter and number only. Default is NOT.
  -n, --no-escape            No escape char '`'. Default is using it.
  -Q, --dot-replace          Replace dot character with underline character in
                             the table name.(Version 2.5.3)
  -T, --thread-num=THREAD_NUM   Number of thread for dump in file. Default is
                             8.
  -C, --cloud=CLOUD_DSN      specify a DSN to access TDengine cloud service
  -R, --restful              Use RESTful interface to connect TDengine
  -t, --timeout=SECONDS      The timeout seconds for websocket to interact.
  -g, --debug                Print debug info.
  -?, --help                 Give this help list
      --usage                Give a short usage message
  -V, --version              Print program version
  -W, --rename=RENAME-LIST   Rename database name with new name during
                             importing data. RENAME-LIST: 
                             "db1=newDB1|db2=newDB2" means rename db1 to newDB1
                             and rename db2 to newDB2 (Version 2.5.4)

Mandatory or optional arguments to long options are also mandatory or optional
for any corresponding short options.
```
