# uGraph

Header‑only C++17 utilities for *static* DAGs:

* `Topology` – compile‑time ordering & cycle detection (no storage, no allocs)
* `GraphView` – runtime traversal + minimal reusable buffer slot mapping

Single include:
```cpp
#include <ugraph.hpp>
```

---

## Topology

Provide edge *types*; at compile time `Topology` tells you:
* Cycle present? (`is_cyclic()`)
* A deterministic ordering (`ids()`)
* Node count (`size()`)
* Tagged visitation (`for_each`, `apply`)

### NodeTag

Pure type descriptor: `NodeTag<ID, Payload>` = stable integer id + payload type. No runtime object needed.

### Edges

`Link<Src, Dst>` encodes dependency (Src before Dst). Collect links in the `Topology` parameter pack.

### Example

Compile‑time enforced startup order:

```cpp
// Subsystems
struct Config    { static void init() {/* load config */} };
struct Logger    { static void init() {/* needs Config */} };
struct Database  { static void init() {/* needs Config + Logger */} };
struct HttpServer{ static void init() {/* needs Database */} };

// Ids
using config_t = ugraph::NodeTag<1, Config>;
using server_t = ugraph::NodeTag<2, HttpServer>;
using database_t = ugraph::NodeTag<3, Database>;
using logger_t = ugraph::NodeTag<4, Logger>;

// Dependencies (Src -> Dst)
using AppTopo = 
ugraph::Topology<
    ugraph::Link<config_t, logger_t>,   // config must start before logger
    ugraph::Link<config_t, database_t>, // config must start before DB
    ugraph::Link<logger_t, database_t>, // logger must start before DB
    ugraph::Link<database_t, server_t>  // DB must start before server
>;

static_assert(!AppTopo::is_cyclic());
constexpr auto order = AppTopo::ids(); // e.g. {1,4,3,2}
static_assert(AppTopo::size() == 4);

// Run in guaranteed safe order (e.g. config -> logger -> DB -> server)
AppTopo::apply(
    [](auto... tag){ 
        (decltype(tag)::module_type::init(), ...);
    }
);
```

### Topology API

```cpp
using T = ugraph::Topology</* Links... */>;
static_assert(!T::is_cyclic());
constexpr auto ids = T::ids();      // std::array<...>
T::for_each([](auto tag){ /* per tag */ });
auto result = T::apply([](auto... tags){ return sizeof...(tags); });
```

---

## GraphView

Use `GraphView` when you have concrete module instances to execute. Feed it edge *values* produced by wiring `Node` objects. It:
* Reuses `Topology` (ordering + cycle check)
* Offers `apply` (variadic) & `for_each` (per node)
* Computes minimal output buffer slot reuse via lifetime coloring

### Defining Runtime Nodes

```cpp
struct Source  { void run() {/* produce */} };        // 0 in, 1 out
struct Filter  { void run() {/* transform */} };      // 1 in, 1 out
struct Sink    { void run() {/* consume */} };        // 1 in, 0 out

Source src;
Filter filt;
Sink sink;

ugraph::Node<10, Source, 0,1> nSrc(src);
ugraph::Node<20, Filter, 1,1> nFlt(filt);
ugraph::Node<30, Sink,   1,0> nSnk(sink);

auto e1 = nSrc.out() >> nFlt.in();
auto e2 = nFlt.out() >> nSnk.in();

auto gv = ugraph::GraphView(e1, e2); // static_assert inside ensures acyclic
```

### Executing the Pipeline

```cpp
gv.apply([](auto&... nodes){ (nodes.module().run(), ...); });
// or
gv.for_each([](auto& node){ node.module().run(); });
```

### GraphView API

```cpp
auto ids = decltype(gv)::ids();                 // ordering
constexpr auto N = decltype(gv)::size();
constexpr auto slots = decltype(gv)::data_instance_count();
gv.for_each([](auto& node){ /* node.id(), node.module() */ });
```

---

## Core Concepts

| Concept | Type | Purpose |
|---------|------|---------|
| Compile-time id | `NodeTag<ID, Module>` | Id + payload type only |
| Runtime node | `Node<ID, Module, In, Out>` | Wraps user instance + ports |
| Edge | `Link<Src, Dst>` | Src precedes Dst |
| Static graph | `Topology<Link...>` | Order, cycle check, visitation |
| Runtime view | `GraphView<Link...>` | Traverse + buffer slots |

---

## Detailed API Reference

### Topology<Link...>
* `is_cyclic()`
* `ids()` -> `std::array<...>`
* `size()`
* `find_type_by_id<Id>::type`
* `for_each(f)` per tag
* `apply(f)` all tags

### GraphView<Link...>
* `ids()`
* `size()`
* `data_instance_count()`
* `output_data_index<VID,PORT>()`
* `input_data_index<VID,PORT>()`
* `apply(f)` runtime variadic
* `for_each(f)` runtime per node

### Node<ID, Module, InCount, OutCount>
* `id()`
* `in<port>()`, `out<port>()`
* `module()`
* `input_count()`, `output_count()`

---

## Use Cases

* Subsystem / service init ordering
* Static registration or table generation
* Fixed pipelines (audio, image, robotics, ETL)
* Buffer reuse (slot coloring)
* Compile‑time reflection (dispatch tables, switches)

---