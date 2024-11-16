---
sidebar_label: Rust
title: Connect with Rust
description: This document describes how to connect to TDengine Cloud using the Rust client library.
---
<!-- exclude -->
import Tabs from '@theme/Tabs';
import TabItem from '@theme/TabItem';

<!-- exclude-end -->
## Create Project

```bash
cargo new --bin cloud-example
```

## Add Dependency

Add dependency to `Cargo.toml`.

```toml title="Cargo.toml"
[package]
name = "cloud-example"
version = "0.1.0"
edition = "2021"

[dependencies]
taos = { version = "*", default-features = false, features = ["ws"] }
tokio = { version = "1", features = ["full"]}
anyhow = "1.0.0" 
```

## Config

Run this command in your terminal to save TDengine cloud DSN as variables:

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
Replace  &lt;DSN&gt; with real TDengine cloud DSN. To obtain the real value, please log in [TDengine Cloud](https://cloud.tdengine.com) and click "Programming" on the left menu, then select "Rust".

:::
<!-- exclude-end -->

## Connect

Copy following code to `main.rs`.

```rust title="main.rs"
{{#include docs/examples/rust/cloud-example/src/main.rs}}
```

Then you can execute `cargo run` to test the connection. For how to write data and query data, please refer to [Data In](https://docs.tdengine.com/cloud/data-in/) and [Tools](https://docs.tdengine.com/cloud/tools/).

For more details about how to write or query data via REST API, please check [REST API](https://docs.tdengine.com/cloud/programming/connect/rest-api/).
