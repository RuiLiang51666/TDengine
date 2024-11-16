---
sidebar_label: Node.js
title: Connect with Node.js
description: This document describes how to connect to TDengine Cloud using the Node.js client library.
---

<!-- exclude -->

import Tabs from '@theme/Tabs';
import TabItem from '@theme/TabItem';

<!-- exclude-end -->

## Install Client Library

```bash
npm install @tdengine/websocket
```

## Config

Run this command in your terminal to save TDengine cloud token as variables:

<Tabs defaultValue="bash">
<TabItem value="bash" label="Bash">

```bash
export TDENGINE_CLOUD_TOKEN="<token>"
export TDENGINE_CLOUD_URL="<url>"
```

</TabItem>
<TabItem value="cmd" label="CMD">

```bash
set TDENGINE_CLOUD_TOKEN=<token>
set TDENGINE_CLOUD_URL=<url>
```

</TabItem>
<TabItem value="powershell" label="Powershell">

```powershell
$env:TDENGINE_CLOUD_TOKEN='<token>'
$env:TDENGINE_CLOUD_URL='<url>'
```

</TabItem>
</Tabs>

<!-- exclude -->

:::note IMPORTANT
Replace &lt;token&gt; and &lt;url&gt; with cloud token and URL.
To obtain the value of cloud token and URL, please log in [TDengine Cloud](https://cloud.tdengine.com) and click "Programming" on left menu, then select "Node.js".

:::

<!-- exclude-end -->

## Connect

```javascript
{{#include docs/examples/node/connect.js}}
```

For how to write data and query data, please refer to [Data In](https://docs.tdengine.com/cloud/data-in/) and [Tools](https://docs.tdengine.com/cloud/tools/).

For more details about how to write or query data via REST API, please check [REST API](https://docs.tdengine.com/cloud/programming/connect/rest-api/).
