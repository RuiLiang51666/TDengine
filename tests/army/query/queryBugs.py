###################################################################
#           Copyright (c) 2016 by TAOS Technologies, Inc.
#                     All rights reserved.
#
#  This file is proprietary and confidential to TAOS Technologies.
#  No part of this file may be reproduced, stored, transmitted,
#  disclosed or used in any form or by any means other than as
#  expressly provided by the written permission from Jianhui Tao
#
###################################################################

# -*- coding: utf-8 -*-

import sys
import time
import random

import taos
import frame
import frame.etool


from frame.log import *
from frame.cases import *
from frame.sql import *
from frame.caseBase import *
from frame import *


class TDTestCase(TBase):
    # fix
    def FIX_TD_30686(self):
        tdLog.info("check bug TD_30686 ...\n")
        sqls = [
            "create database db",
            "create table db.st(ts timestamp, age int) tags(area tinyint);",
            "insert into db.t1 using db.st tags(100) values('2024-01-01 10:00:01', 1);",
            "insert into db.t2 using db.st tags(110) values('2024-01-01 10:00:02', 2);",
            "insert into db.t3 using db.st tags(3) values('2024-01-01 10:00:03', 3);",
        ]
        tdSql.executes(sqls)

        sql = "select * from db.st where area < 139 order by ts;"
        results = [
            ["2024-01-01 10:00:01", 1, 100],
            ["2024-01-01 10:00:02", 2, 110],
            ["2024-01-01 10:00:03", 3, 3],
        ]
        tdSql.checkDataMem(sql, results)

    def FIX_TS_5105(self):
        tdLog.info("check bug TS_5105 ...\n")
        ts1 = "2024-07-03 10:00:00.000"
        ts2 = "2024-07-03 13:00:00.000"
        sqls = [
            "drop database if exists ts_5105",
            "create database ts_5105 cachemodel 'both';",
            "use ts_5105;",
            "CREATE STABLE meters (ts timestamp, current float) TAGS (location binary(64), groupId int);",
            "CREATE TABLE d1001 USING meters TAGS ('California.B', 2);",
            "CREATE TABLE d1002 USING meters TAGS ('California.S', 3);",
            f"INSERT INTO d1001 VALUES ('{ts1}', 10);",
            f"INSERT INTO d1002 VALUES ('{ts2}', 13);",
        ]
        tdSql.executes(sqls)

        sql = "select last(ts), last_row(ts) from meters;"

        # 执行多次，有些时候last_row(ts)会返回错误的值，详见TS-5105
        for i in range(1, 10):
            tdLog.debug(f"{i}th execute sql: {sql}")
            tdSql.query(sql)
            tdSql.checkRows(1)
            tdSql.checkData(0, 0, ts2)
            tdSql.checkData(0, 1, ts2)

    def FIX_TS_5143(self):
        tdLog.info("check bug TS_5143 ...\n")
        # 2024-07-11 17:07:38
        base_ts = 1720688857000
        new_ts = base_ts + 10
        sqls = [
            "drop database if exists ts_5143",
            "create database ts_5143 cachemodel 'both';",
            "use ts_5143;",
            "create table stb1 (ts timestamp, vval varchar(50), ival2 int, ival3 int, ival4 int) tags (itag int);",
            "create table ntb1 using stb1 tags(1);",
            f"insert into ntb1 values({base_ts}, 'nihao1', 12, 13, 14);",
            f"insert into ntb1 values({base_ts + 2}, 'nihao2', NULL, NULL, NULL);",
            f"delete from ntb1 where ts = {base_ts};",
            f"insert into ntb1 values('{new_ts}', 'nihao3', 32, 33, 34);",
        ]
        tdSql.executes(sqls)

        last_sql = "select last(vval), last(ival2), last(ival3), last(ival4) from stb1;"
        tdLog.debug(f"execute sql: {last_sql}")
        tdSql.query(last_sql)

        for i in range(1, 10):
            new_ts = base_ts + i * 1000
            num = i * 100
            v1, v2 = i * 10, i * 11
            sqls = [
                f"insert into ntb1 values({new_ts}, 'nihao{num}', {v1}, {v1}, {v1});",
                f"insert into ntb1 values({new_ts + 1}, 'nihao{num + 1}', NULL, NULL, NULL);",
                f"delete from ntb1 where ts = {new_ts};",
                f"insert into ntb1 values({new_ts + 2}, 'nihao{num + 2}', {v2}, {v2}, {v2});",
            ]
            tdSql.executes(sqls)

            tdLog.debug(f"{i}th execute sql: {last_sql}")
            tdSql.query(last_sql)
            tdSql.checkData(0, 0, f"nihao{num + 2}")
            tdSql.checkData(0, 1, f"{11*i}")

    def FIX_TS_5239(self):
        tdLog.info("check bug TS_5239 ...\n")
        sqls = [
            "drop database if exists ts_5239",
            "create database ts_5239 cachemodel 'both' stt_trigger 1;",
            "use ts_5239;",
            "CREATE STABLE st (ts timestamp, c1 int) TAGS (groupId int);",
            "CREATE TABLE ct1 USING st TAGS (1);"
        ]
        tdSql.executes(sqls)
        # 2024-07-03 06:00:00.000
        start_ts = 1719957600000
        # insert 100 rows
        sql = "insert into ct1 values "
        for i in range(100):
            sql += f"('{start_ts+i * 100}', {i+1})"
        sql += ";"
        tdSql.execute(sql)
        tdSql.execute("flush database ts_5239;")
        tdSql.execute("alter database ts_5239 stt_trigger 3;")
        tdSql.execute(f"insert into ct1(ts) values({start_ts - 100 * 100})")
        tdSql.execute("flush database ts_5239;")
        tdSql.execute(f"insert into ct1(ts) values({start_ts + 100 * 200})")
        tdSql.execute("flush database ts_5239;")
        tdSql.query("select count(*) from ct1;")
        tdSql.checkRows(1)
        tdSql.checkData(0, 0, 102)

    def FIX_TD_31684(self):
        tdLog.info("check bug TD_31684 ...\n")
        sqls = [
            "drop database if exists td_31684",
            "create database td_31684 cachemodel 'both' stt_trigger 1;",
            "use td_31684;",
            "create table t1 (ts timestamp, id int, name int) ;",
            "insert into t1 values(now, 1, 1);",
            "insert into t1 values(now, 1, 2);",
            "insert into t1 values(now, 2, 3);",
            "insert into t1 values(now, 3, 4);"
        ]
        tdSql.executes(sqls)
        sql1 = "select count(name) as total_name from t1 group by name"
        sql2 = "select id as new_id, last(name) as last_name from t1 group by id order by new_id"
        sql3 = "select id as new_id, count(id) as id from t1 group by id order by new_id"
        tdSql.query(sql1)
        tdSql.checkRows(4)

        tdSql.query(sql2)
        tdSql.checkRows(3)
        tdSql.checkData(0, 1, 2)
        tdSql.checkData(1, 1, 3)

        tdSql.query(sql3)
        tdSql.checkRows(3)
        tdSql.checkData(0, 1, 2)
        tdSql.checkData(2, 1, 1)

    def FIX_TD_xx(self):
        tdLog.info("check bug TD_xx ...\n")
        sqls = [
            "drop database if exists ctg_tsdb",
            "create database ctg_tsdb cachemodel 'both' stt_trigger 1;",
            "use ctg_tsdb;",
            "CREATE STABLE `stb_sxny_cn` (`dt` TIMESTAMP ENCODE 'delta-i' COMPRESS 'lz4' LEVEL 'medium', \
                `val` DOUBLE ENCODE 'delta-d' COMPRESS 'tsz' LEVEL 'medium') TAGS (`point` VARCHAR(50),  \
                `point_name` VARCHAR(64), `point_path` VARCHAR(2000), `index_name` VARCHAR(64),          \
                `country_equipment_code` VARCHAR(64), `index_code` VARCHAR(64), `ps_code` VARCHAR(50),   \
                `cnstationno` VARCHAR(255), `index_level` VARCHAR(10), `cz_flag` VARCHAR(255),           \
                `blq_flag` VARCHAR(255), `dcc_flag` VARCHAR(255))",
                
                
            "CREATE STABLE `stb_popo_power_station_all` (`ts` TIMESTAMP ENCODE 'delta-i' COMPRESS 'lz4' LEVEL 'medium',      \
                `assemble_capacity` DOUBLE ENCODE 'delta-d' COMPRESS 'lz4' LEVEL 'medium', `ps_status` DOUBLE ENCODE         \
                'delta-d' COMPRESS 'lz4' LEVEL 'medium') TAGS (`ps_type` VARCHAR(255), `ps_type_code` VARCHAR(255),          \
                `belorg_name` VARCHAR(255), `belorg_code` VARCHAR(255), `country_code` VARCHAR(255), `country_name`          \
                VARCHAR(255), `area_name` VARCHAR(255), `area_code` VARCHAR(255), `ps_name` VARCHAR(255), `ps_code`          \
                VARCHAR(255), `ps_aab` VARCHAR(255), `ps_type_sec_lvl` VARCHAR(255), `ps_type_sec_lvl_name` VARCHAR(255),    \
                `ps_type_name` VARCHAR(255), `longitude` DOUBLE, `latitude` DOUBLE, `is_china` VARCHAR(255), `is_access`     \
                VARCHAR(255), `first_put_production_date` VARCHAR(255), `all_put_production_date` VARCHAR(255), `merge_date` \
                VARCHAR(255), `sold_date` VARCHAR(255), `cap_detail` VARCHAR(500), `ps_unit` VARCHAR(255), `region_name`     \
                VARCHAR(255))",
        ]
        
        
        tdSql.executes(sqls)
        
        ts = 1657146000000
        
        # create subtable and insert data for super table stb_sxny_cn
        for i in range(1, 1000):
            sql = f"CREATE TABLE `stb_sxny_cn_{i}` USING `stb_sxny_cn` (point, point_name, point_path, index_name, country_equipment_code,                      \
                index_code, ps_code, cnstationno, index_level, cz_flag, blq_flag, dcc_flag) TAGS('point{i}', 'point_name{i}', 'point_path{i}', 'index_name{i}', \
                'country_equipment_code{i}', 'index_code{i}', 'ps_code{i%10}', 'cnstationno{i}', '{i}', 'cz_flag{i}', 'blq_flag{i}', 'dcc_flag{i}');"
            tdSql.execute(sql)
            sql = f"INSERT INTO `stb_sxny_cn_{i}` VALUES "
            values = []
            for j in range(1, 100):
                values.append(f"({ts+i*100 + j}, {i*100 + j%10})")
            sql += ", ".join(values)
            tdSql.execute(sql)
            tdLog.debug(f"create table stb_sxny_cn_{i} and insert data successfully")
                    
        # create subtable and insert data for super table stb_popo_power_station_all
        for i in range(1, 100):
            sql = f"CREATE TABLE `stb_popo_power_station_all_{i}` USING `stb_popo_power_station_all` (ps_type, ps_type_code, belorg_name, belorg_code,          \
                country_code, country_name, area_name, area_code, ps_name, ps_code, ps_aab, ps_type_sec_lvl, ps_type_sec_lvl_name, ps_type_name,                \
                longitude, latitude, is_china, is_access, first_put_production_date, all_put_production_date, merge_date, sold_date, cap_detail, ps_unit,       \
                region_name) TAGS ('ps_type{i}', 'ps_type_code{i}', 'belorg_name{i}', 'belorg_code{i}', 'country_code{i}', 'country_name{i}', 'area_name{i}',   \
                'area_code{i}', 'ps_name{i}', 'ps_code{i}', 'ps_aab{i}', 'ps_type_sec_lvl{i}', 'ps_type_sec_lvl_name{i}', 'ps_type_name{i}', {i},               \
                {i}, 'is_china{i}', 'is_access{i}', 'first_put_production_date{i}', 'all_put_production_date{i}', 'merge_date{i}', 'sold_date{i}',              \
                'cap_detail{i}', 'ps_unit{i}', 'region_name{i}');"
            tdSql.execute(sql)
            sql = f"INSERT INTO `stb_popo_power_station_all_{i}` VALUES "
            values = []
            for j in range(1, 100):
                values.append(f"({ts+i*100 + j}, {i*10 + j%10}, {j})")
            sql += ", ".join(values)
            tdSql.execute(sql)
            tdLog.debug(f"create table stb_popo_power_station_all_{i} and insert data successfully")
        
        querySql = f"select t2.ts ,tt.ps_code,t2.ps_code                                                                    \
                    from                                                                                                    \
                    (                                                                                                       \
	                    select                                                                                              \
	                       TIMETRUNCATE(t1.dt, 1d, 1) dt,                                                                   \
	                       cnstationno ,                                                                                    \
	                       t1.ps_code,                                                                                      \
	                       t1.ps_code gpstationno ,                                                                         \
	                       t1.country_equipment_code cnbatteryclusterno ,                                                   \
	                       t1.country_equipment_code gpbatteryclusterno ,                                                   \
	                       index_name alarmmsg ,                                                                            \
	                       concat_ws('_', cnstationno, country_equipment_code, country_equipment_code, point) alarmid ,     \
	                       case                                                                                             \
	                       	when index_code = 'index_code1' then '01'                                                       \
	                       	when index_code = 'index_code2' then '03'                                                       \
	                       	when index_code = 'index_code3' then '03'                                                       \
	                       	when index_code = 'index_code4' then '06'                                                       \
	                       end alarmtype ,                                                                                  \
	                       val alarmstatus ,                                                                                \
	                       to_char(first(dt),                                                                               \
	                       'yyyy-mm-dd hh24:mi:ss') alarmdate ,                                                             \
	                       point_path alarmcontent                                                                          \
                        from                                                                                                \
                        	ctg_tsdb.stb_sxny_cn t1                                                                         \
                        where                                                                                               \
                        	index_code in ('index_code1', 'index_code2', 'index_code3', 'index_code4')                      \
                        	and ps_code<>'0'                                                                                \
                        	and dt >= '2022-07-07 00:00:00.000'                                                             \
                        	and val>0                                                                                       \
                        	and t1.ps_code='ps_code3'                                                                       \
                        partition by point state_window(cast(val as int))                                                   \
                        order by TIMETRUNCATE(t1.dt, 1d, 0)                                                                 \
                        ) tt                                                                                                \
                        left join ctg_tsdb.stb_popo_power_station_all t2                                                    \
                        on TIMETRUNCATE(tt.dt, 1d, 1)=TIMETRUNCATE(t2.ts, 1d, 1)                                            \
                        --and tt.ps_code = t2.ps_code                                                                       \
                    "
        tdSql.query(querySql)

    # run
    def run(self):
        tdLog.debug(f"start to excute {__file__}")
        
        self.FIX_TD_xx()

        # TD BUGS
        self.FIX_TD_30686()
        self.FIX_TD_31684()

        # TS BUGS
        self.FIX_TS_5105()
        self.FIX_TS_5143()
        self.FIX_TS_5239()

        tdLog.success(f"{__file__} successfully executed")


tdCases.addLinux(__file__, TDTestCase())
tdCases.addWindows(__file__, TDTestCase())
