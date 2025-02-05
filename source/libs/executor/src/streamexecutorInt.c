/*
 * Copyright (c) 2019 TAOS Data, Inc. <jhtao@taosdata.com>
 *
 * This program is free software: you can use, redistribute, and/or modify
 * it under the terms of the GNU Affero General Public License, version 3
 * or later ("AGPL"), as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "streamexecutorInt.h"

#include "executorInt.h"
#include "tdatablock.h"

#define NOTIFY_EVENT_NAME_CACHE_LIMIT_MB 16

typedef struct SStreamNotifyEvent {
  uint64_t gid;
  TSKEY    skey;
  bool     isEnd;
  char*    content;
} SStreamNotifyEvent;

void setStreamOperatorState(SSteamOpBasicInfo* pBasicInfo, EStreamType type) {
  if (type != STREAM_GET_ALL && type != STREAM_CHECKPOINT) {
    pBasicInfo->updateOperatorInfo = true;
  }
}

bool needSaveStreamOperatorInfo(SSteamOpBasicInfo* pBasicInfo) { return pBasicInfo->updateOperatorInfo; }

void saveStreamOperatorStateComplete(SSteamOpBasicInfo* pBasicInfo) { pBasicInfo->updateOperatorInfo = false; }

static void destroyStreamWindowEvent(void* ptr) {
  SStreamNotifyEvent* pEvent = ptr;
  if (pEvent == NULL || pEvent->content == NULL) return;
  cJSON_free(pEvent->content);
}

static void destroyStreamNotifyEventSupp(SStreamNotifyEventSupp* sup) {
  if (sup == NULL) return;
  tdListFreeP(sup->pWindowEvents, destroyStreamWindowEvent);
  taosHashCleanup(sup->pWindowEventHashMap);
  taosHashCleanup(sup->pTableNameHashMap);
  taosHashCleanup(sup->pResultHashMap);
  blockDataDestroy(sup->pEventBlock);
  *sup = (SStreamNotifyEventSupp){0};
}

static int32_t initStreamNotifyEventSupp(SStreamNotifyEventSupp* sup) {
  int32_t         code = TSDB_CODE_SUCCESS;
  int32_t         lino = 0;
  SSDataBlock*    pBlock = NULL;
  SColumnInfoData infoData = {0};

  if (sup == NULL) {
    goto _end;
  }

  code = createDataBlock(&pBlock);
  QUERY_CHECK_CODE(code, lino, _end);

  pBlock->info.type = STREAM_NOTIFY_EVENT;
  pBlock->info.watermark = INT64_MIN;

  infoData.info.type = TSDB_DATA_TYPE_VARCHAR;
  infoData.info.bytes = tDataTypes[infoData.info.type].bytes;
  code = blockDataAppendColInfo(pBlock, &infoData);
  QUERY_CHECK_CODE(code, lino, _end);

  sup->pWindowEvents = tdListNew(sizeof(SStreamNotifyEvent));
  QUERY_CHECK_NULL(sup->pWindowEvents, code, lino, _end, terrno);
  sup->pWindowEventHashMap = taosHashInit(4096, taosGetDefaultHashFunction(TSDB_DATA_TYPE_BINARY), false, HASH_NO_LOCK);
  QUERY_CHECK_NULL(sup->pWindowEventHashMap, code, lino, _end, terrno);
  sup->pTableNameHashMap = taosHashInit(1024, taosGetDefaultHashFunction(TSDB_DATA_TYPE_UBIGINT), false, HASH_NO_LOCK);
  QUERY_CHECK_NULL(sup->pTableNameHashMap, code, lino, _end, terrno);
  sup->pResultHashMap = taosHashInit(4096, taosGetDefaultHashFunction(TSDB_DATA_TYPE_BINARY), false, HASH_NO_LOCK);
  QUERY_CHECK_NULL(sup->pResultHashMap, code, lino, _end, terrno);
  taosHashSetFreeFp(sup->pResultHashMap, destroyStreamWindowEvent);
  sup->pEventBlock = pBlock;
  pBlock = NULL;

_end:
  if (code != TSDB_CODE_SUCCESS) {
    qError("%s failed at line %d since %s", __func__, lino, tstrerror(code));
    if (sup) {
      destroyStreamNotifyEventSupp(sup);
    }
  }
  if (pBlock != NULL) {
    blockDataDestroy(pBlock);
  }
  return code;
}

int32_t initStreamBasicInfo(SSteamOpBasicInfo* pBasicInfo) {
  pBasicInfo->primaryPkIndex = -1;
  pBasicInfo->updateOperatorInfo = false;
  return initStreamNotifyEventSupp(&pBasicInfo->notifyEventSup);
}

void destroyStreamBasicInfo(SSteamOpBasicInfo* pBasicInfo) {
  destroyStreamNotifyEventSupp(&pBasicInfo->notifyEventSup);
}

static int32_t encodeStreamNotifyEventSupp(void** buf, SStreamNotifyEventSupp* sup) {
  int32_t    tlen = 0;
  SListIter  iter = {0};
  SListNode* pNode = NULL;

  if (sup->pWindowEvents == NULL) {
    return tlen;
  }

  tlen += taosEncodeFixedI32(buf, listNEles(sup->pWindowEvents));
  tdListInitIter(sup->pWindowEvents, &iter, TD_LIST_FORWARD);
  while ((pNode = tdListNext(&iter)) != NULL) {
    SStreamNotifyEvent* pEvent = (SStreamNotifyEvent*)pNode->data;
    tlen += taosEncodeFixedU64(buf, pEvent->gid);
    tlen += taosEncodeFixedI64(buf, pEvent->skey);
    tlen += taosEncodeFixedBool(buf, pEvent->isEnd);
    tlen += taosEncodeString(buf, pEvent->content);
  }
  return tlen;
}

static int32_t decodeStreamNotifyEventSupp(void** buf, SStreamNotifyEventSupp* sup) {
  int32_t            code = TSDB_CODE_SUCCESS;
  int32_t            lino = 0;
  int32_t            size = 0;
  void*              p = *buf;
  SStreamNotifyEvent item = {0};
  SListNode*         pNode = NULL;

  p = taosDecodeFixedI32(p, &size);
  for (int32_t i = 0; i < size; i++) {
    SStreamNotifyEvent item = {0};
    p = taosDecodeFixedU64(p, &item.gid);
    p = taosDecodeFixedI64(p, &item.skey);
    p = taosDecodeFixedBool(p, &item.isEnd);
    p = taosDecodeString(p, &item.content);
    // Append to event list and hash map
    code = tdListAppend(sup->pWindowEvents, &item);
    QUERY_CHECK_CODE(code, lino, _end);
    pNode = tdListGetTail(sup->pWindowEvents);
    QUERY_CHECK_NULL(pNode, code, lino, _end, TSDB_CODE_INTERNAL_ERROR);
    code = taosHashPut(sup->pWindowEventHashMap, &item, sizeof(item.gid) + sizeof(item.skey) + sizeof(item.isEnd),
                       &pNode, POINTER_BYTES);
    QUERY_CHECK_CODE(code, lino, _end);
    pNode = NULL;
    item.content = NULL;
  }
  *buf = p;
_end:
  if (code != TSDB_CODE_SUCCESS) {
    qError("%s failed at line %d since %s", __func__, lino, tstrerror(code));
  }
  if (pNode != NULL) {
    pNode = tdListPopNode(sup->pWindowEvents, pNode);
    taosMemoryFreeClear(pNode);
  }
  destroyStreamWindowEvent(&item);
  return code;
}

int32_t encodeStreamBasicInfo(void** buf, SSteamOpBasicInfo* pBasicInfo) {
  return encodeStreamNotifyEventSupp(buf, &pBasicInfo->notifyEventSup);
}

int32_t decodeStreamBasicInfo(void** buf, SSteamOpBasicInfo* pBasicInfo) {
  return decodeStreamNotifyEventSupp(buf, &pBasicInfo->notifyEventSup);
}

static void streamNotifyGetEventWindowId(const SSessionKey* pSessionKey, char* buf) {
  uint64_t hash = 0;
  uint64_t ar[2];

  ar[0] = pSessionKey->groupId;
  ar[1] = pSessionKey->win.skey;
  hash = MurmurHash3_64((char*)ar, sizeof(ar));
  buf = u64toaFastLut(hash, buf);
}

#define JSON_CHECK_ADD_ITEM(obj, str, item) \
  QUERY_CHECK_CONDITION(cJSON_AddItemToObjectCS(obj, str, item), code, lino, _end, TSDB_CODE_OUT_OF_MEMORY)

static int32_t jsonAddColumnField(const char* colName, int16_t type, bool isNull, const char* pData, cJSON* obj) {
  int32_t code = TSDB_CODE_SUCCESS;
  int32_t lino = 0;
  char*   temp = NULL;

  QUERY_CHECK_NULL(colName, code, lino, _end, TSDB_CODE_INVALID_PARA);
  QUERY_CHECK_CONDITION(isNull || (pData != NULL), code, lino, _end, TSDB_CODE_INVALID_PARA);
  QUERY_CHECK_NULL(obj, code, lino, _end, TSDB_CODE_INVALID_PARA);

  if (isNull) {
    JSON_CHECK_ADD_ITEM(obj, colName, cJSON_CreateNull());
    goto _end;
  }

  switch (type) {
    case TSDB_DATA_TYPE_BOOL: {
      bool val = *(bool*)pData;
      JSON_CHECK_ADD_ITEM(obj, colName, cJSON_CreateBool(val));
      break;
    }

    case TSDB_DATA_TYPE_TINYINT: {
      int8_t val = *(int8_t*)pData;
      JSON_CHECK_ADD_ITEM(obj, colName, cJSON_CreateNumber(val));
      break;
    }

    case TSDB_DATA_TYPE_SMALLINT: {
      int16_t val = *(int16_t*)pData;
      JSON_CHECK_ADD_ITEM(obj, colName, cJSON_CreateNumber(val));
      break;
    }

    case TSDB_DATA_TYPE_INT: {
      int32_t val = *(int32_t*)pData;
      JSON_CHECK_ADD_ITEM(obj, colName, cJSON_CreateNumber(val));
      break;
    }

    case TSDB_DATA_TYPE_BIGINT:
    case TSDB_DATA_TYPE_TIMESTAMP: {
      int64_t val = *(int64_t*)pData;
      JSON_CHECK_ADD_ITEM(obj, colName, cJSON_CreateNumber(val));
      break;
    }

    case TSDB_DATA_TYPE_FLOAT: {
      float val = *(float*)pData;
      JSON_CHECK_ADD_ITEM(obj, colName, cJSON_CreateNumber(val));
      break;
    }

    case TSDB_DATA_TYPE_DOUBLE: {
      double val = *(double*)pData;
      JSON_CHECK_ADD_ITEM(obj, colName, cJSON_CreateNumber(val));
      break;
    }

    case TSDB_DATA_TYPE_VARCHAR:
    case TSDB_DATA_TYPE_NCHAR: {
      // cJSON requires null-terminated strings, but this data is not null-terminated,
      // so we need to manually copy the string and add null termination.
      const char* src = varDataVal(pData);
      int32_t     len = varDataLen(pData);
      temp = cJSON_malloc(len + 1);
      QUERY_CHECK_NULL(temp, code, lino, _end, TSDB_CODE_OUT_OF_MEMORY);
      memcpy(temp, src, len);
      temp[len] = '\0';

      cJSON* item = cJSON_CreateStringReference(temp);
      JSON_CHECK_ADD_ITEM(obj, colName, item);

      // let the cjson object to free memory later
      item->type &= ~cJSON_IsReference;
      temp = NULL;
      break;
    }

    case TSDB_DATA_TYPE_UTINYINT: {
      uint8_t val = *(uint8_t*)pData;
      JSON_CHECK_ADD_ITEM(obj, colName, cJSON_CreateNumber(val));
      break;
    }

    case TSDB_DATA_TYPE_USMALLINT: {
      uint16_t val = *(uint16_t*)pData;
      JSON_CHECK_ADD_ITEM(obj, colName, cJSON_CreateNumber(val));
      break;
    }

    case TSDB_DATA_TYPE_UINT: {
      uint32_t val = *(uint32_t*)pData;
      JSON_CHECK_ADD_ITEM(obj, colName, cJSON_CreateNumber(val));
      break;
    }

    case TSDB_DATA_TYPE_UBIGINT: {
      uint64_t val = *(uint64_t*)pData;
      JSON_CHECK_ADD_ITEM(obj, colName, cJSON_CreateNumber(val));
      break;
    }

    default: {
      JSON_CHECK_ADD_ITEM(obj, colName, cJSON_CreateStringReference("<Unable to display this data type>"));
      break;
    }
  }

_end:
  if (code != TSDB_CODE_SUCCESS) {
    qError("%s failed at line %d since %s", __func__, lino, tstrerror(code));
  }
  if (temp) {
    cJSON_free(temp);
  }
  return code;
}

int32_t addEventAggNotifyEvent(EStreamNotifyEventType eventType, const SSessionKey* pSessionKey,
                               const SSDataBlock* pInputBlock, const SNodeList* pCondCols, int32_t ri,
                               SStreamNotifyEventSupp* sup) {
  int32_t            code = TSDB_CODE_SUCCESS;
  int32_t            lino = 0;
  SNode*             node = NULL;
  cJSON*             event = NULL;
  cJSON*             fields = NULL;
  cJSON*             cond = NULL;
  SStreamNotifyEvent item = {0};
  SListNode*         pNode = NULL;
  char               windowId[32];

  QUERY_CHECK_NULL(pSessionKey, code, lino, _end, TSDB_CODE_INVALID_PARA);
  QUERY_CHECK_NULL(pInputBlock, code, lino, _end, TSDB_CODE_INVALID_PARA);
  QUERY_CHECK_NULL(pInputBlock->pDataBlock, code, lino, _end, TSDB_CODE_INVALID_PARA);
  QUERY_CHECK_NULL(pCondCols, code, lino, _end, TSDB_CODE_INVALID_PARA);
  QUERY_CHECK_NULL(sup, code, lino, _end, TSDB_CODE_INVALID_PARA);

  qDebug("add stream notify event from Event Window, type: %s, start: %" PRId64 ", end: %" PRId64,
         (eventType == SNOTIFY_EVENT_WINDOW_OPEN) ? "WINDOW_OPEN" : "WINDOW_CLOSE", pSessionKey->win.skey,
         pSessionKey->win.ekey);

  event = cJSON_CreateObject();
  QUERY_CHECK_NULL(event, code, lino, _end, TSDB_CODE_OUT_OF_MEMORY);

  // add basic info
  streamNotifyGetEventWindowId(pSessionKey, windowId);
  if (eventType == SNOTIFY_EVENT_WINDOW_OPEN) {
    JSON_CHECK_ADD_ITEM(event, "eventType", cJSON_CreateStringReference("WINDOW_OPEN"));
  } else if (eventType == SNOTIFY_EVENT_WINDOW_CLOSE) {
    JSON_CHECK_ADD_ITEM(event, "eventType", cJSON_CreateStringReference("WINDOW_CLOSE"));
  }
  JSON_CHECK_ADD_ITEM(event, "eventTime", cJSON_CreateNumber(taosGetTimestampMs()));
  JSON_CHECK_ADD_ITEM(event, "windowId", cJSON_CreateStringReference(windowId));
  JSON_CHECK_ADD_ITEM(event, "windowType", cJSON_CreateStringReference("Event"));
  JSON_CHECK_ADD_ITEM(event, "windowStart", cJSON_CreateNumber(pSessionKey->win.skey));
  if (eventType == SNOTIFY_EVENT_WINDOW_CLOSE) {
    JSON_CHECK_ADD_ITEM(event, "windowEnd", cJSON_CreateNumber(pSessionKey->win.ekey));
  }

  // create fields object to store matched column values
  fields = cJSON_CreateObject();
  QUERY_CHECK_NULL(fields, code, lino, _end, TSDB_CODE_OUT_OF_MEMORY);
  FOREACH(node, pCondCols) {
    SColumnNode*     pColDef = (SColumnNode*)node;
    SColumnInfoData* pColData = taosArrayGet(pInputBlock->pDataBlock, pColDef->slotId);
    code = jsonAddColumnField(pColDef->colName, pColData->info.type, colDataIsNull_s(pColData, ri),
                              colDataGetData(pColData, ri), fields);
    QUERY_CHECK_CODE(code, lino, _end);
  }

  // add trigger condition
  cond = cJSON_CreateObject();
  QUERY_CHECK_NULL(cond, code, lino, _end, TSDB_CODE_OUT_OF_MEMORY);
  JSON_CHECK_ADD_ITEM(cond, "conditionIndex", cJSON_CreateNumber(0));
  JSON_CHECK_ADD_ITEM(cond, "fieldValues", fields);
  fields = NULL;
  JSON_CHECK_ADD_ITEM(event, "triggerConditions", cond);
  cond = NULL;

  // convert json object to string value
  item.gid = pSessionKey->groupId;
  item.skey = pSessionKey->win.skey;
  item.isEnd = (eventType == SNOTIFY_EVENT_WINDOW_CLOSE);
  item.content = cJSON_PrintUnformatted(event);

  // Append to event list and add to the hash map.
  code = tdListAppend(sup->pWindowEvents, &item);
  QUERY_CHECK_CODE(code, lino, _end);
  pNode = tdListGetTail(sup->pWindowEvents);
  QUERY_CHECK_NULL(pNode, code, lino, _end, TSDB_CODE_INTERNAL_ERROR);
  code = taosHashPut(sup->pWindowEventHashMap, &item, sizeof(item.gid) + sizeof(item.skey) + sizeof(item.isEnd), &pNode,
                     POINTER_BYTES);
  QUERY_CHECK_CODE(code, lino, _end);
  pNode = NULL;
  item.content = NULL;

_end:
  if (code != TSDB_CODE_SUCCESS) {
    qError("%s failed at line %d since %s", __func__, lino, tstrerror(code));
  }
  if (pNode != NULL) {
    pNode = tdListPopNode(sup->pWindowEvents, pNode);
    taosMemoryFreeClear(pNode);
  }
  destroyStreamWindowEvent(&item);
  if (cond != NULL) {
    cJSON_Delete(cond);
  }
  if (fields != NULL) {
    cJSON_Delete(fields);
  }
  if (event != NULL) {
    cJSON_Delete(event);
  }
  return code;
}

int32_t addStateAggNotifyEvent(EStreamNotifyEventType eventType, const SSessionKey* pSessionKey,
                               const SStateKeys* pCurState, const SStateKeys* pAnotherState,
                               SStreamNotifyEventSupp* sup) {
  int32_t            code = TSDB_CODE_SUCCESS;
  int32_t            lino = 0;
  cJSON*             event = NULL;
  SStreamNotifyEvent item = {0};
  SListNode*         pNode = NULL;
  char               windowId[32];

  QUERY_CHECK_NULL(pSessionKey, code, lino, _end, TSDB_CODE_INVALID_PARA);
  QUERY_CHECK_NULL(pCurState, code, lino, _end, TSDB_CODE_INVALID_PARA);
  QUERY_CHECK_NULL(sup, code, lino, _end, TSDB_CODE_INVALID_PARA);

  qDebug("add stream notify event from State Window, type: %s, start: %" PRId64 ", end: %" PRId64,
         (eventType == SNOTIFY_EVENT_WINDOW_OPEN) ? "WINDOW_OPEN" : "WINDOW_CLOSE", pSessionKey->win.skey,
         pSessionKey->win.ekey);

  event = cJSON_CreateObject();
  QUERY_CHECK_NULL(event, code, lino, _end, TSDB_CODE_OUT_OF_MEMORY);

  // add basic info
  streamNotifyGetEventWindowId(pSessionKey, windowId);
  if (eventType == SNOTIFY_EVENT_WINDOW_OPEN) {
    JSON_CHECK_ADD_ITEM(event, "eventType", cJSON_CreateStringReference("WINDOW_OPEN"));
  } else if (eventType == SNOTIFY_EVENT_WINDOW_CLOSE) {
    JSON_CHECK_ADD_ITEM(event, "eventType", cJSON_CreateStringReference("WINDOW_CLOSE"));
  }
  JSON_CHECK_ADD_ITEM(event, "eventTime", cJSON_CreateNumber(taosGetTimestampMs()));
  JSON_CHECK_ADD_ITEM(event, "windowId", cJSON_CreateStringReference(windowId));
  JSON_CHECK_ADD_ITEM(event, "windowType", cJSON_CreateStringReference("State"));
  JSON_CHECK_ADD_ITEM(event, "windowStart", cJSON_CreateNumber(pSessionKey->win.skey));
  if (eventType == SNOTIFY_EVENT_WINDOW_CLOSE) {
    JSON_CHECK_ADD_ITEM(event, "windowEnd", cJSON_CreateNumber(pSessionKey->win.ekey));
  }

  // add state value
  if (eventType == SNOTIFY_EVENT_WINDOW_OPEN) {
    if (pAnotherState) {
      code = jsonAddColumnField("prevState", pAnotherState->type, pAnotherState->isNull, pAnotherState->pData, event);
      QUERY_CHECK_CODE(code, lino, _end);
    } else {
      code = jsonAddColumnField("prevState", pCurState->type, true, NULL, event);
      QUERY_CHECK_CODE(code, lino, _end);
    }
  }
  code = jsonAddColumnField("curState", pCurState->type, pCurState->isNull, pCurState->pData, event);
  QUERY_CHECK_CODE(code, lino, _end);
  if (eventType == SNOTIFY_EVENT_WINDOW_CLOSE) {
    // todo(kjq): pAnotherState should not be null
    if (pAnotherState) {
      code = jsonAddColumnField("nextState", pAnotherState->type, pAnotherState->isNull, pAnotherState->pData, event);
      QUERY_CHECK_CODE(code, lino, _end);
    } else {
      code = jsonAddColumnField("nextState", pCurState->type, true, NULL, event);
      QUERY_CHECK_CODE(code, lino, _end);
    }
  }

  // convert json object to string value
  item.gid = pSessionKey->groupId;
  item.skey = pSessionKey->win.skey;
  item.isEnd = (eventType == SNOTIFY_EVENT_WINDOW_CLOSE);
  item.content = cJSON_PrintUnformatted(event);

  // Append to event list and add to the hash map.
  code = tdListAppend(sup->pWindowEvents, &item);
  QUERY_CHECK_CODE(code, lino, _end);
  pNode = tdListGetTail(sup->pWindowEvents);
  QUERY_CHECK_NULL(pNode, code, lino, _end, TSDB_CODE_INTERNAL_ERROR);
  code = taosHashPut(sup->pWindowEventHashMap, &item, sizeof(item.gid) + sizeof(item.skey) + sizeof(item.isEnd), &pNode,
                     POINTER_BYTES);
  QUERY_CHECK_CODE(code, lino, _end);
  pNode = NULL;
  item.content = NULL;

_end:
  if (code != TSDB_CODE_SUCCESS) {
    qError("%s failed at line %d since %s", __func__, lino, tstrerror(code));
  }
  if (pNode != NULL) {
    pNode = tdListPopNode(sup->pWindowEvents, pNode);
    taosMemoryFreeClear(pNode);
  }
  destroyStreamWindowEvent(&item);
  if (event != NULL) {
    cJSON_Delete(event);
  }
  return code;
}

static int32_t addNormalAggNotifyEvent(const char* windowType, EStreamNotifyEventType eventType,
                                       const SSessionKey* pSessionKey, SStreamNotifyEventSupp* sup) {
  int32_t            code = TSDB_CODE_SUCCESS;
  int32_t            lino = 0;
  cJSON*             event = NULL;
  SStreamNotifyEvent item = {0};
  SListNode*         pNode = NULL;
  char               windowId[32];

  QUERY_CHECK_NULL(pSessionKey, code, lino, _end, TSDB_CODE_INVALID_PARA);
  QUERY_CHECK_NULL(sup, code, lino, _end, TSDB_CODE_INVALID_PARA);

  qDebug("add stream notify event from %s Window, type: %s, start: %" PRId64 ", end: %" PRId64, windowType,
         (eventType == SNOTIFY_EVENT_WINDOW_OPEN) ? "WINDOW_OPEN" : "WINDOW_CLOSE", pSessionKey->win.skey,
         pSessionKey->win.ekey);

  event = cJSON_CreateObject();
  QUERY_CHECK_NULL(event, code, lino, _end, TSDB_CODE_OUT_OF_MEMORY);

  // add basic info
  streamNotifyGetEventWindowId(pSessionKey, windowId);
  if (eventType == SNOTIFY_EVENT_WINDOW_OPEN) {
    JSON_CHECK_ADD_ITEM(event, "eventType", cJSON_CreateStringReference("WINDOW_OPEN"));
  } else if (eventType == SNOTIFY_EVENT_WINDOW_CLOSE) {
    JSON_CHECK_ADD_ITEM(event, "eventType", cJSON_CreateStringReference("WINDOW_CLOSE"));
  }
  JSON_CHECK_ADD_ITEM(event, "eventTime", cJSON_CreateNumber(taosGetTimestampMs()));
  JSON_CHECK_ADD_ITEM(event, "windowId", cJSON_CreateStringReference(windowId));
  JSON_CHECK_ADD_ITEM(event, "windowType", cJSON_CreateStringReference(windowType));
  JSON_CHECK_ADD_ITEM(event, "windowStart", cJSON_CreateNumber(pSessionKey->win.skey));
  if (eventType == SNOTIFY_EVENT_WINDOW_CLOSE) {
    JSON_CHECK_ADD_ITEM(event, "windowEnd", cJSON_CreateNumber(pSessionKey->win.ekey));
  }

  // convert json object to string value
  item.gid = pSessionKey->groupId;
  item.skey = pSessionKey->win.skey;
  item.isEnd = (eventType == SNOTIFY_EVENT_WINDOW_CLOSE);
  item.content = cJSON_PrintUnformatted(event);

  // Append to event list and add to the hash map.
  code = tdListAppend(sup->pWindowEvents, &item);
  QUERY_CHECK_CODE(code, lino, _end);
  pNode = tdListGetTail(sup->pWindowEvents);
  QUERY_CHECK_NULL(pNode, code, lino, _end, TSDB_CODE_INTERNAL_ERROR);
  code = taosHashPut(sup->pWindowEventHashMap, &item, sizeof(item.gid) + sizeof(item.skey) + sizeof(item.isEnd), &pNode,
                     POINTER_BYTES);
  QUERY_CHECK_CODE(code, lino, _end);
  pNode = NULL;
  item.content = NULL;

_end:
  if (code != TSDB_CODE_SUCCESS) {
    qError("%s failed at line %d since %s", __func__, lino, tstrerror(code));
  }
  if (pNode != NULL) {
    pNode = tdListPopNode(sup->pWindowEvents, pNode);
    taosMemoryFreeClear(pNode);
  }
  destroyStreamWindowEvent(&item);
  if (event != NULL) {
    cJSON_Delete(event);
  }
  return code;
}

int32_t addIntervalAggNotifyEvent(EStreamNotifyEventType eventType, const SSessionKey* pSessionKey,
                                  SStreamNotifyEventSupp* sup) {
  return addNormalAggNotifyEvent("Time", eventType, pSessionKey, sup);
}

int32_t addSessionAggNotifyEvent(EStreamNotifyEventType eventType, const SSessionKey* pSessionKey,
                                 SStreamNotifyEventSupp* sup) {
  return addNormalAggNotifyEvent("Session", eventType, pSessionKey, sup);
}

int32_t addCountAggNotifyEvent(EStreamNotifyEventType eventType, const SSessionKey* pSessionKey,
                               SStreamNotifyEventSupp* sup) {
  return addNormalAggNotifyEvent("Count", eventType, pSessionKey, sup);
}

int32_t addAggResultNotifyEvent(const SSDataBlock* pResultBlock, const SSchemaWrapper* pSchemaWrapper,
                                SStreamNotifyEventSupp* sup) {
  int32_t            code = TSDB_CODE_SUCCESS;
  int32_t            lino = 0;
  SNode*             node = NULL;
  cJSON*             event = NULL;
  cJSON*             result = NULL;
  SStreamNotifyEvent item = {0};
  SColumnInfoData*   pWstartCol = NULL;

  QUERY_CHECK_NULL(pResultBlock, code, lino, _end, TSDB_CODE_INVALID_PARA);
  QUERY_CHECK_NULL(pSchemaWrapper, code, lino, _end, TSDB_CODE_INVALID_PARA);
  QUERY_CHECK_NULL(sup, code, lino, _end, TSDB_CODE_INVALID_PARA);

  qDebug("add %" PRId64 " stream notify results from window agg", pResultBlock->info.rows);

  pWstartCol = taosArrayGet(pResultBlock->pDataBlock, 0);
  for (int32_t i = 0; i < pResultBlock->info.rows; ++i) {
    event = cJSON_CreateObject();
    QUERY_CHECK_NULL(event, code, lino, _end, TSDB_CODE_OUT_OF_MEMORY);

    // convert the result row into json
    result = cJSON_CreateObject();
    QUERY_CHECK_NULL(result, code, lino, _end, TSDB_CODE_OUT_OF_MEMORY);
    for (int32_t j = 0; j < pSchemaWrapper->nCols; ++j) {
      SSchema*         pCol = pSchemaWrapper->pSchema + j;
      SColumnInfoData* pColData = taosArrayGet(pResultBlock->pDataBlock, pCol->colId - 1);
      code = jsonAddColumnField(pCol->name, pColData->info.type, colDataIsNull_s(pColData, i),
                                colDataGetData(pColData, i), result);
      QUERY_CHECK_CODE(code, lino, _end);
    }
    JSON_CHECK_ADD_ITEM(event, "result", result);
    result = NULL;

    item.gid = pResultBlock->info.id.groupId;
    item.skey = *(uint64_t*)colDataGetNumData(pWstartCol, i);
    item.content = cJSON_PrintUnformatted(event);
    code = taosHashPut(sup->pResultHashMap, &item.gid, sizeof(item.gid) + sizeof(item.skey), &item, sizeof(item));
    TSDB_CHECK_CODE(code, lino, _end);
    item.content = NULL;

    cJSON_Delete(event);
    event = NULL;
  }

_end:
  if (code != TSDB_CODE_SUCCESS) {
    qError("%s failed at line %d since %s", __func__, lino, tstrerror(code));
  }
  destroyStreamWindowEvent(&item);
  if (result != NULL) {
    cJSON_Delete(result);
  }
  if (event != NULL) {
    cJSON_Delete(event);
  }
  return code;
}

static int32_t streamNotifyGetDestTableName(const SExecTaskInfo* pTaskInfo, uint64_t gid, char** pTableName) {
  int32_t                code = TSDB_CODE_SUCCESS;
  int32_t                lino = 0;
  const SStorageAPI*     pAPI = NULL;
  void*                  tbname = NULL;
  int32_t                winCode = TSDB_CODE_SUCCESS;
  char                   parTbName[TSDB_TABLE_NAME_LEN];
  const SStreamTaskInfo* pStreamInfo = NULL;

  QUERY_CHECK_NULL(pTaskInfo, code, lino, _end, TSDB_CODE_INVALID_PARA);
  QUERY_CHECK_NULL(pTableName, code, lino, _end, TSDB_CODE_INVALID_PARA);

  *pTableName = NULL;

  pAPI = &pTaskInfo->storageAPI;
  code = pAPI->stateStore.streamStateGetParName((void*)pTaskInfo->streamInfo.pState, gid, &tbname, false, &winCode);
  QUERY_CHECK_CODE(code, lino, _end);
  if (winCode != TSDB_CODE_SUCCESS) {
    parTbName[0] = '\0';
  } else {
    tstrncpy(parTbName, tbname, sizeof(parTbName));
  }
  pAPI->stateStore.streamStateFreeVal(tbname);

  pStreamInfo = &pTaskInfo->streamInfo;
  code = buildSinkDestTableName(parTbName, pStreamInfo->stbFullName, gid, pStreamInfo->newSubTableRule, pTableName);
  QUERY_CHECK_CODE(code, lino, _end);

_end:
  if (code != TSDB_CODE_SUCCESS) {
    qError("%s failed at line %d since %s", __func__, lino, tstrerror(code));
  }
  return code;
}

static int32_t streamNotifyFillTableName(const char* tableName, const SStreamNotifyEvent* pEvent,
                                         const SStreamNotifyEvent* pResult, char** pVal) {
  int32_t            code = TSDB_CODE_SUCCESS;
  int32_t            lino = 0;
  static const char* prefix = "{\"tableName\":\"";
  uint64_t           prefixLen = 0;
  uint64_t           nameLen = 0;
  uint64_t           eventLen = 0;
  uint64_t           resultLen = 0;
  uint64_t           valLen = 0;
  char*              val = NULL;
  char*              p = NULL;

  QUERY_CHECK_NULL(tableName, code, lino, _end, TSDB_CODE_INVALID_PARA);
  QUERY_CHECK_NULL(pEvent, code, lino, _end, TSDB_CODE_INVALID_PARA);
  QUERY_CHECK_NULL(pVal, code, lino, _end, TSDB_CODE_INVALID_PARA);

  *pVal = NULL;
  prefixLen = strlen(prefix);
  nameLen = strlen(tableName);
  eventLen = strlen(pEvent->content);

  if (pResult != NULL) {
    resultLen = strlen(pResult->content);
    valLen = VARSTR_HEADER_SIZE + prefixLen + nameLen + eventLen + resultLen;
  } else {
    valLen = VARSTR_HEADER_SIZE + prefixLen + nameLen + eventLen + 1;
  }
  val = taosMemoryMalloc(valLen);
  QUERY_CHECK_NULL(val, code, lino, _end, terrno);
  varDataSetLen(val, valLen - VARSTR_HEADER_SIZE);

  p = varDataVal(val);
  TAOS_STRNCPY(p, prefix, prefixLen);
  p += prefixLen;
  TAOS_STRNCPY(p, tableName, nameLen);
  p += nameLen;
  *(p++) = '\"';
  TAOS_STRNCPY(p, pEvent->content, eventLen);
  *p = ',';

  if (pResult != NULL) {
    p += eventLen - 1;
    TAOS_STRNCPY(p, pResult->content, resultLen);
    *p = ',';
  }
  *pVal = val;
  val = NULL;

_end:
  if (code != TSDB_CODE_SUCCESS) {
    qError("%s failed at line %d since %s", __func__, lino, tstrerror(code));
  }
  if (val != NULL) {
    taosMemoryFreeClear(val);
  }
  return code;
}

int32_t buildNotifyEventBlock(const SExecTaskInfo* pTaskInfo, SStreamNotifyEventSupp* sup) {
  int32_t          code = TSDB_CODE_SUCCESS;
  int32_t          lino = 0;
  SArray*          nodeArray = NULL;
  SListIter        iter = {0};
  SListNode*       pNode = NULL;
  int32_t          nWindowEvents = 0;
  int32_t          nWindowResults = 0;
  SColumnInfoData* pEventStrCol = NULL;
  char*            val = NULL;

  if (pTaskInfo == NULL || sup == NULL) {
    goto _end;
  }

  QUERY_CHECK_NULL(sup->pEventBlock, code, lino, _end, TSDB_CODE_INVALID_PARA);
  blockDataCleanup(sup->pEventBlock);

  // Filter out all events that can be pushed
  nodeArray = taosArrayInit(listNEles(sup->pWindowEvents), POINTER_BYTES * 2);
  QUERY_CHECK_NULL(nodeArray, code, lino, _end, terrno);
  tdListInitIter(sup->pWindowEvents, &iter, TD_LIST_FORWARD);
  while ((pNode = tdListNext(&iter)) != NULL) {
    SStreamNotifyEvent* pEvent = (SStreamNotifyEvent*)pNode->data;
    SStreamNotifyEvent* pResult = NULL;
    if (pEvent->isEnd) {
      pResult = taosHashGet(sup->pResultHashMap, pEvent, sizeof(pEvent->gid) + sizeof(pEvent->skey));
      if (pResult == NULL) {
        // due to stream watermark, current window cannot push agg result yet
        continue;
      }
    }
    SStreamNotifyEvent* ar[] = {pEvent, pResult};
    void*               px = taosArrayPush(nodeArray, ar);
    QUERY_CHECK_NULL(px, code, lino, _end, terrno);
  }

  nWindowEvents = taosArrayGetSize(nodeArray);
  nWindowResults = taosHashGetSize(sup->pResultHashMap);
  qDebug("start to build stream notify event block, nWindowEvents: %d, nWindowResults: %d", nWindowEvents,
         nWindowResults);

  code = blockDataEnsureCapacity(sup->pEventBlock, nWindowEvents);
  QUERY_CHECK_CODE(code, lino, _end);

  pEventStrCol = taosArrayGet(sup->pEventBlock->pDataBlock, NOTIFY_EVENT_STR_COLUMN_INDEX);
  QUERY_CHECK_NULL(pEventStrCol, code, lino, _end, terrno);

  // Append all events content into data block.
  for (int32_t i = 0; i < nWindowEvents; ++i) {
    SStreamNotifyEvent** ar = taosArrayGet(nodeArray, i);
    SStreamNotifyEvent*  pEvent = ar[0];
    SStreamNotifyEvent*  pResult = ar[1];
    char*                tableName = taosHashGet(sup->pTableNameHashMap, &pEvent->gid, sizeof(pEvent->gid));
    if (tableName == NULL) {
      code = streamNotifyGetDestTableName(pTaskInfo, pEvent->gid, &tableName);
      QUERY_CHECK_CODE(code, lino, _end);
      code = taosHashPut(sup->pTableNameHashMap, &pEvent->gid, sizeof(pEvent->gid), tableName, strlen(tableName) + 1);
      taosMemoryFreeClear(tableName);
      QUERY_CHECK_CODE(code, lino, _end);
      tableName = taosHashGet(sup->pTableNameHashMap, &pEvent->gid, sizeof(pEvent->gid));
      QUERY_CHECK_NULL(tableName, code, lino, _end, TSDB_CODE_INTERNAL_ERROR);
    }
    code = streamNotifyFillTableName(tableName, pEvent, pResult, &val);
    QUERY_CHECK_CODE(code, lino, _end);
    code = colDataSetVal(pEventStrCol, sup->pEventBlock->info.rows, val, false);
    QUERY_CHECK_CODE(code, lino, _end);
    sup->pEventBlock->info.rows++;
    taosMemoryFreeClear(val);
  }

  // Clear all events that are to be pushed
  for (int32_t i = 0; i < nWindowEvents; ++i) {
    SStreamNotifyEvent** ar = taosArrayGet(nodeArray, i);
    SStreamNotifyEvent*  pEvent = ar[0];
    code = taosHashRemove(sup->pWindowEventHashMap, pEvent,
                          sizeof(pEvent->gid) + sizeof(pEvent->skey) + sizeof(pEvent->isEnd));
    if (code != TSDB_CODE_SUCCESS) {
      qWarn("failed to remove window events in hash map since %s", tstrerror(code));
      // ignore failure and go on to process.
      code = TSDB_CODE_SUCCESS;
    }
    pNode = listNode(pEvent);
    pNode = tdListPopNode(sup->pWindowEvents, pNode);
    destroyStreamWindowEvent(pEvent);
    taosMemoryFreeClear(pNode);
  }
  taosHashClear(sup->pResultHashMap);

  if (taosHashGetMemSize(sup->pTableNameHashMap) >= NOTIFY_EVENT_NAME_CACHE_LIMIT_MB * 1024 * 1024) {
    taosHashClear(sup->pTableNameHashMap);
  }

_end:
  if (code != TSDB_CODE_SUCCESS) {
    qError("%s failed at line %d since %s", __func__, lino, tstrerror(code));
  }
  if (val != NULL) {
    taosMemoryFreeClear(val);
  }
  if (nodeArray != NULL) {
    taosArrayDestroy(nodeArray);
    nodeArray = NULL;
  }
  return code;
}
