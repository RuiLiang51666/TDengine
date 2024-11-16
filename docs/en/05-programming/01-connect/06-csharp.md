---
sidebar_label: C#
title: Connect with C#
description: This document describes how to connect to TDengine Cloud using the C# client library.
---
<!-- exclude -->
import Tabs from '@theme/Tabs';
import TabItem from '@theme/TabItem';

<!-- exclude-end -->
## Create Project

```bash
dotnet new console -o example
```

## Add C# TDengine Driver class lib

```bash
cd example
vim example.csproj
```

Add following ItemGroup and Task to your project file.

```XML
<ItemGroup>
    <PackageReference Include="TDengine.Connector" Version="3.0.*" GeneratePathProperty="true" />
  </ItemGroup>
  <Target Name="copyDLLDependency" BeforeTargets="BeforeBuild">
    <ItemGroup>
      <DepDLLFiles Include="$(PkgTDengine_Connector)\runtimes\**\*.*" />
    </ItemGroup>
    <Copy SourceFiles="@(DepDLLFiles)" DestinationFolder="$(OutDir)" />
  </Target>
```

```bash
dotnet add package TDengine.Connector
```

## Config

Run this command in your terminal to save TDengine cloud token as variables:

<Tabs defaultValue="bash">
<TabItem value="bash" label="Bash">

```bash
export TDENGINE_CLOUD_DSN="<DSN>"
```

</TabItem>
<TabItem value="cmd" label="CMD">

```bash
set TDENGINE_CLOUD_DSN=<DSN>
```

</TabItem>
<TabItem value="powershell" label="Powershell">

```powershell
$env:TDENGINE_CLOUD_DSN='<DSN>'
```

</TabItem>
</Tabs>

<!-- exclude -->
:::note IMPORTANT
Replace  &lt;DSN&gt; with real TDengine cloud DSN. To obtain the real value, please log in [TDengine Cloud](https://cloud.tdengine.com) and click "Programming" on the left menu, then select "C#".

:::
<!-- exclude-end -->

## Connect

``` XML
{{#include docs/examples/csharp/cloud-example/connect/connect.csproj}}
```

```C#
{{#include docs/examples/csharp/cloud-example/connect/Program.cs}}
```

The client connection is then established. For how to write data and query data, please refer to [Data In](https://docs.tdengine.com/cloud/data-in/) and [Tools](https://docs.tdengine.com/cloud/tools/).

For more details about how to write or query data via REST API, please check [REST API](https://docs.tdengine.com/cloud/programming/connect/rest-api/).
