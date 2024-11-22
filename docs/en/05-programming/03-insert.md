---
sidebar_label: Insert
title: Insert Data Using SQL
description: This document describes how to insert data into TDengine using SQL.
---


import Tabs from '@theme/Tabs';
import TabItem from '@theme/TabItem';

## SQL Examples

Here are some brief examples for `INSERT` statement. You can execute these statements manually by TDengine CLI or TDengine Cloud Explorer or programmatically by TDengine client libraries.

### Insert Single Row

The below SQL statement is used to insert one row into table "d101".

```sql
INSERT INTO test.d101 VALUES (1538548685000, 10.3, 219, 0.31);
```

### Insert Multiple Rows

Multiple rows can be inserted in a single SQL statement. The example below inserts 2 rows into table "d101".

```sql
INSERT INTO test.d101 VALUES (1538548684000, 10.2, 220, 0.23) (1538548696650, 10.3, 218, 0.25);
```

### Insert into Multiple Tables

Data can be inserted into multiple tables in the same SQL statement. The example below inserts 2 rows into table "d101" and 1 row into table "d102".

```sql
INSERT INTO test.d101 VALUES (1538548685000, 10.3, 219, 0.31) (1538548695000, 12.6, 218, 0.33) test.d102 VALUES (1538548696800, 12.3, 221, 0.31);
```

For more details about `INSERT` please refer to [INSERT](https://docs.tdengine.com/cloud/taos-sql/insert/).

## Client Library Examples

Here's an smart meters example to show how to use connectors in different languages, to create a super table called `meters` in a `power` database, with columns for timestamp, current, voltage, phase, and tags for group ID and location. 

:::note IMPORTANT
Before executing the sample code in this section, please create a database named `power` on the[TDengine Cloud - Explorer](https://cloud.taosdata.com/explorer) page.

How to establish connection to TDegnine Cloud service, please refer to [Connect to TDengine Cloud Service](../../programming/connect/).
:::

<Tabs>
<TabItem value="python" label="Python">

In this example, we use `execute` method to execute SQL and get affected rows. The variable `conn` is an instance of class  `taosrest.TaosRestConnection` we just created at [Connect Tutorial](../../programming/connect/python#connect).

```python
{{#include docs/examples/python/develop_tutorial.py:insert}}
```

</TabItem>
<TabItem value="java" label="Java">

```java
{{#include docs/examples/java/src/main/java/com/taos/example/CloudTutorial.java:insert}}
```

</TabItem>
<TabItem value="go" label="Go">

```go
{{#include docs/examples/go/tutorial/main.go:insert}}
```

</TabItem>
<TabItem value="rust" label="Rust">

In this example, we use `exec` method to execute SQL. `exec` is designed for some non-query SQL statements, all returned data would be ignored.

```rust
{{#include docs/examples/rust/cloud-example/examples/tutorial.rs:insert}}
```

</TabItem>
<TabItem value="node" label="Node.js">

```javascript
{{#include docs/examples/node/insert.js}}
```

</TabItem>

<TabItem value="C#" label="C#">

``` XML
{{#include docs/examples/csharp/cloud-example/inout/inout.csproj}}
```

```csharp
{{#include docs/examples/csharp/cloud-example/inout/Program.cs:insert}}
```

</TabItem>

</Tabs>

:::note IMPORTANT
`Use` statement is not applicable for cloud service since REST API is stateless.
:::
