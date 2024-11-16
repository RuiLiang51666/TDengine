---
sidebar_label: Python
title: Connect with Python
description: This document describes how to connect to TDengine Cloud using the Python client library.
---

<!-- exclude -->
import Tabs from '@theme/Tabs';
import TabItem from '@theme/TabItem';

<!-- exclude-end -->
## Install Client Library

First, you need to install the `taospy` module version >= `2.6.2`. Run the command below in your terminal.

<Tabs defaultValue="pip" groupID="package">
<TabItem value="pip" label="pip">

```bash
pip3 install -U taospy
```

You'll need to have Python3 installed.

</TabItem>
<TabItem value="conda" label="conda">

```bash
conda install -c conda-forge taospy
```

</TabItem>
</Tabs>

## Config

Run this command in your terminal to save TDengine cloud token and URL as variables:

<Tabs defaultValue="bash">
<TabItem value="bash" label="Bash">

```bash
export TDENGINE_CLOUD_TOKEN="<token>"
export TDENGINE_CLOUD_URL="<url>"
```

</TabItem>
<TabItem value="cmd" label="CMD">

```shell
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

Alternatively, you can also set environment variables in your IDE's run configurations.

<!-- exclude -->
:::note IMPORTANT
Replace  &lt;token&gt; and &lt;url&gt; with cloud token and URL.
To obtain the value of cloud token and URL, please login [TDengine Cloud](https://cloud.tdengine.com) and click "Programming" on the left menu, then select "Python".

:::
<!-- exclude-end -->

## Connect

Copy code bellow to your editor, then run it. If you are using jupyter, assuming you have followed the guide about Jupyter, you can copy the code into Jupyter editor in your browser.

<Tabs defaultValue="rest">
<TabItem value="rest" label="REST">

```python
{{#include docs/examples/python/develop_tutorial.py:connect}}
```

</TabItem>
<TabItem value="websocket" label="WebSocket">

```python
{{#include docs/examples/python/develop_tutorial_ws.py:connect}}
```

</TabItem>
</Tabs>

For how to write data and query data, please refer to [Data In](https://docs.tdengine.com/cloud/data-in/) and [Tools](https://docs.tdengine.com/cloud/tools/).

For more details about how to write or query data via REST API, please check [REST API](https://docs.tdengine.com/cloud/programming/connect/rest-api/).

## Jupyter

### Step 1: Install

For the users who are familiar with Jupyter to program in Python, both TDengine Python client library and Jupyter need to be ready in your environment. If you have not done yet, please use the commands below to install them.

<Tabs defaultValue="pip" groupID="package">
<TabItem value="pip" label="pip">

```bash
pip install jupyterlab
pip3 install -U taospy
```

You'll need to have Python3 installed.

</TabItem>
<TabItem value="conda" label="conda">

```bash
conda install -c conda-forge jupyterlab
conda install -c conda-forge taospy
```

</TabItem>
</Tabs>

### Step 2: Configure

In order for Jupyter to connect to TDengine cloud service, before launching Jupyter, the environment setting must be performed. We use Linux bash as example.

```bash
export TDENGINE_CLOUD_TOKEN="<token>"
export TDENGINE_CLOUD_URL="<url>"
jupyter lab
```

### Step 3: Connect

Once jupyter lab is launched, Jupyter lab service is automatically connected and shown in your browser. You can create a new notebook and copy the sample code below and run it.

```python
{{#include docs/examples/python/develop_tutorial.py:connect}}
```
