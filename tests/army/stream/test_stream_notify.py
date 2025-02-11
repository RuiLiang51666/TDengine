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

from frame import etool
from frame.etool import *
from frame.log import *
from frame.cases import *
from frame.sql import *
from frame.caseBase import *
from frame.common import *

class TDTestCase(TBase):
    def init(self, conn, logSql, replicaVar=1):
        self.replicaVar = int(replicaVar)
        tdLog.debug(f"start to excute {__file__}")
        tdSql.init(conn.cursor(), True)

    def parse(self, log_file, out_file, stream_name):
        message_ids = set()
        events_map = {}
        with open(log_file, "r", encoding="utf-8") as f:
            for line in f:
                data = json.loads(line)

                # Check if the data has the required fields: messageId, timestamp, streams
                if "messageId" not in data:
                    print(f"Error: Missing 'messageId' in data {data}")
                    return False
                if "timestamp" not in data:
                    print(f"Error: Missing 'timestamp' in data {data}")
                    return False
                if "streams" not in data:
                    print(f"Error: Missing 'streams' in data {data}")
                    return False

                # Check if the message id is duplicated
                if message_ids.__contains__(data["messageId"]):
                    print(f"Error: Duplicate message id {data['messageId']}")
                    return False
                message_ids.add(data["messageId"])

                # Check if the streams is correct
                for stream in data["streams"]:
                    # Check if the stream has the required fields: streamName, events
                    if "streamName" not in stream:
                        print(f"Error: Missing 'streamName' in stream {stream}")
                        return False
                    if "events" not in stream:
                        print(f"Error: Missing 'events' in stream {stream}")
                        return False

                    # Check if the stream name is correct
                    if stream["streamName"] != stream_name:
                        print(f"Error: Incorrect stream name {stream['streamName']}")
                        return False

                    # Check if the events are correct
                    for event in stream["events"]:
                        # Check if the event has the required fields: tableName, eventType, eventTime, windowId, windowType
                        if "tableName" not in event:
                            print(f"Error: Missing 'tableName' in event {event}")
                            return False
                        if "eventType" not in event:
                            print(f"Error: Missing 'eventType' in event {event}")
                            return False
                        if "eventTime" not in event:
                            print(f"Error: Missing 'eventTime' in event {event}")
                            return False
                        if "windowId" not in event:
                            print(f"Error: Missing 'windowId' in event {event}")
                            return False
                        if "windowType" not in event:
                            print(f"Error: Missing 'windowType' in event {event}")
                            return False
                        if event["eventType"] not in ["WINDOW_OPEN", "WINDOW_CLOSE", "WINDOW_INVALIDATION"]:
                            print(f"Error: Invalid event type {event['eventType']}")
                            return False
                        if event["windowType"] not in ["Time", "State", "Session", "Event", "Count"]:
                            print(f"Error: Invalid window type {event['windowType']}")
                            return False

                        if event["eventType"] == "WINDOW_INVALIDATION":
                            # WINDOW_INVALIDATION must have fields: windowStart, windowEnd
                            if "windowStart" not in event:
                                print(f"Error: Missing 'windowStart' in event {event}")
                                return False
                            if "windowEnd" not in event:
                                print(f"Error: Missing 'windowEnd' in event {event}")
                                return False
                            events_map.pop((event["tableName"], event["windowId"]), None)
                            continue

                        # Get the event from the event map; if it doesn't exist, create a new one
                        e = events_map.get((event["tableName"], event["windowId"]))
                        if e is None:
                            events_map[(event["tableName"], event["windowId"])] = {
                                "opened": False,
                                "closed": False,
                                "wstart": 0,
                                "wend": 0
                            }
                            e = events_map.get((event["tableName"], event["windowId"]))

                        if event["eventType"] == "WINDOW_OPEN":
                            # WINDOW_OPEN for all windows must have field: windowStart
                            if "windowStart" not in event:
                                print(f"Error: Missing 'windowStart' in event {event}")
                                return False
                            if event["windowType"] == "State":
                                # WINDOW_OPEN for State window must also have fields: prevState, curState
                                if "prevState" not in event:
                                    print(f"Error: Missing 'prevState' in event {event}")
                                    return False
                                if "curState" not in event:
                                    print(f"Error: Missing 'curState' in event {event}")
                                    return False
                            elif event["windowType"] == "Event":
                                # WINDOW_OPEN for Event window must also have fields: triggerCondition
                                if "triggerCondition" not in event:
                                    print(f"Error: Missing 'triggerCondition' in event {event}")
                                    return False
                            e["opened"] = True
                            e["wstart"] = event["windowStart"]
                        elif event["eventType"] == "WINDOW_CLOSE":
                            # WINDOW_CLOSE for all windows must have fields: windowStart, windowEnd, result
                            if "windowStart" not in event:
                                print(f"Error: Missing 'windowStart' in event {event}")
                                return False
                            if "windowEnd" not in event:
                                print(f"Error: Missing 'windowEnd' in event {event}")
                                return False
                            if "result" not in event:
                                print(f"Error: Missing 'result' in event {event}")
                                return False
                            if event["windowType"] == "State":
                                # WINDOW_CLOSE for State window must also have fields: curState, nextState
                                if "curState" not in event:
                                    print(f"Error: Missing 'curState' in event {event}")
                                    return False
                                if "nextState" not in event:
                                    print(f"Error: Missing 'nextState' in event {event}")
                                    return False
                            elif event["windowType"] == "Event":
                                # WINDOW_CLOSE for Event window must also have fields: triggerCondition
                                if "triggerCondition" not in event:
                                    print(f"Error: Missing 'triggerCondition' in event {event}")
                                    return False
                            e["closed"] = True
                            e["wstart"] = event["windowStart"]
                            e["wend"] = event["windowEnd"]

        # Collect all the windows that closed
        windows_map = {}
        for k, v in events_map.items():
            if not v["closed"]:
                continue
            e = windows_map.get(k[0])
            if e is None:
                windows_map[k[0]] = []
                e = windows_map.get(k[0])
            e.append((v["wstart"], v["wend"]))

        # Sort the windows by start time
        for k, v in windows_map.items():
            v.sort(key=lambda x: x[0])

        # Write all collected window info to the specified output file in sorted order as csv format
        with open(out_file, "w", encoding="utf-8") as f:
            for k, v in sorted(windows_map.items()):
                for w in v:
                    f.write(f"{w[0]},{w[1]},{k}\n")
        return True

    def create_streams(self):
        tdLog.printNoPrefix("==========step1:create table")
        tdSql.execute("create database test keep 3650;")
        tdSql.execute("use test;")
        tdSql.execute(
            f'''create table if not exists test.st
            (ts timestamp, c0 tinyint, c1 smallint, c2 int, c3 bigint, c4 double, c5 float, c6 bool, c7 varchar(10), c8 nchar(10), c9 tinyint unsigned, c10 smallint unsigned, c11 int unsigned, c12 bigint unsigned);
            ''')
        streams = [
            "create stream "
        ]


    def insert_data(self):
        tdLog.info("insert stream notify test data.")
        # taosBenchmark run
        json = etool.curFile(__file__, "insert.json")
        etool.benchMark(json = json)

    def create_streams(self):
        tdSql.execute("use test;")
        streams = [
            "create stream stream1 fill_history 1 into sta as select _wstart, _wend, _wduration, count(*) from test.st where ts < '2020-10-01 00:07:19' interval(1m, auto);",
            "create stream stream2 fill_history 1 into stb as select _wstart, _wend, _wduration, count(*) from test.st where ts = '2020-11-01 23:45:00' interval(1h, auto) sliding(27m);",
            "create stream stream3 fill_history 1 into stc as select _wstart, _wend, _wduration, count(*) from test.st where ts in ('2020-11-12 23:32:00') interval(1n, auto) sliding(13d);",
            "create stream stream4 fill_history 1 into std as select _wstart, _wend, _wduration, count(*) from test.st where ts in ('2020-10-09 01:23:00', '2020-11-09 01:23:00', '2020-12-09 01:23:00') interval(1s, auto);",
            "create stream stream5 fill_history 1 into ste as select _wstart, _wend, _wduration, count(*) from test.st where ts > '2020-12-09 01:23:00' interval(1d, auto) sliding(17h);",
            "create stream stream6 fill_history 1 into stf as select _wstart, _wend, _wduration, count(*) from test.st where ts >= '2020-10-09 01:23:00' interval(1n, auto);",
            "create stream stream7 fill_history 1 into stg as select _wstart, _wend, _wduration, count(*) from test.st where ts >= '2020-11-09 01:23:00' interval(1n, auto) sliding(13d);",
        ]
        for sql in streams:
            tdSql.execute(sql)
        for i in range(50):
            rows = tdSql.query("select * from information_schema.ins_stream_tasks where history_task_status is not null;")
            if rows == 0:
                break;
            tdLog.info(f"i={i} wait for history data calculation finish ...")
            time.sleep(1)

    def query_test(self):
        # read sql from .sql file and execute
        tdLog.info("test normal query.")
        self.sqlFile = etool.curFile(__file__, f"in/interval.in")
        self.ansFile = etool.curFile(__file__, f"ans/interval.csv")

        tdCom.compare_testcase_result(self.sqlFile, self.ansFile, "interval")

    def run(self):
        self.insert_data()
        self.create_streams()
        self.query_test()

    def stop(self):
        tdSql.execute("drop stream stream1;")
        tdSql.execute("drop stream stream2;")
        tdSql.execute("drop stream stream3;")
        tdSql.execute("drop stream stream4;")
        tdSql.execute("drop stream stream5;")
        tdSql.execute("drop stream stream6;")
        tdSql.execute("drop stream stream7;")
        tdLog.success(f"{__file__} successfully executed")


tdCases.addLinux(__file__, TDTestCase())
tdCases.addWindows(__file__, TDTestCase())
