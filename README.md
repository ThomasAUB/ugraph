# uGraph

Tiny C++17 header‑only utilities for DAGs with a clean split between pure type‑level topology and optional runtime routing / buffer reuse.

Include once:
```cpp
#include <ugraph.hpp>
```

## 1. Concepts

- Topology: compile‑time only. Given edge types it computes (constexpr) topological order, detects cycles, and lets you visit vertex types. No runtime storage.

- PipelineGraph : builds on Topology; wires concrete vertex instances, assigns and reuses data buffers (interval coloring), and offers runtime traversal & buffer introspection.

## 2. Pure Topology First (Vertex + Edge)
Model a DAG purely at the type level—no instances, no buffers—useful for:
* Enforcing build / init order among stateless components
* Generating registration code or static tables in a guaranteed order
* Early cycle detection before adding routing concerns

Real‑life style example: ordered subsystem initialization for an application.
```cpp
// Subsystem tags (each supplies a static init)
struct Config    { static void init() {/* load config file */} };
struct Logger    { static void init() {/* requires Config */} };
struct Database  { static void init() {/* requires Config + logs */} };
struct HttpServer{ static void init() {/* requires Database */} };

// Assign stable ids (any positive unique numbers)
using VCfg  = ugraph::Vertex<1, Config>;
using VLog  = ugraph::Vertex<2, Logger>;
using VDb   = ugraph::Vertex<3, Database>;
using VSrv  = ugraph::Vertex<4, HttpServer>;

// Declare dependencies (edges go source -> destination)
using AppTopo = ugraph::Topology<
    ugraph::Edge<VCfg, VLog>,     // Config before Logger
    ugraph::Edge<VCfg, VDb>,      // Config before Database
    ugraph::Edge<VLog, VDb>,      // Logger before Database (for logging during init)
    ugraph::Edge<VDb,  VSrv>      // Database before Server
>;

static_assert(!AppTopo::is_cyclic());
constexpr auto order = AppTopo::ids(); // e.g. {1,2,3,4}
static_assert(AppTopo::size() == 4);

// Run all subsystem inits in a guaranteed safe order
AppTopo::apply(
    [] (auto... tag) {
        (decltype(tag)::type::init(), ...); // Config::init(), Logger::init(), ...
    }
);
```
Result: `Config::init()` → `Logger::init()` → `Database::init()` → `HttpServer::init()` (order may collapse parallel-ready nodes but always respects dependencies). This mode provides order, cycle detection, and type visitation.

## 3. Define Vertices (for routing)
Use `RoutingVertex<id, Impl, inputs, outputs, Data>` for anything that will be routed (it still participates in pure topology via its type).
```cpp
struct A { void run() { /*...*/ } };  // 0 in, 1 out
struct B { void run() { /*...*/ } };  // 1 in, 1 out
struct C { void run() { /*...*/ } };  // 1 in, 0 out

A a; B b; C c;
ugraph::RoutingVertex<1, A, 0, 1, int> vA(a);
ugraph::RoutingVertex<2, B, 1, 1, int> vB(b);
ugraph::RoutingVertex<3, C, 1, 0, int> vC(c);
```

## 4. Pipeline Graph (execution + buffers)
```cpp
auto g = ugraph::PipelineGraph(
    vA.out() >> vB.in(),
    vB.out() >> vC.in()
);

g.apply([](auto&... impl){ (impl.run(), ...); }); // runs A,B,C in order
static_assert(decltype(g)::buffer_count() == 2);
```

## 5. Buffer Reuse Introspection
Compile‑time:
```cpp
constexpr std::size_t buf = decltype(g)::buffer_index_for<1,0>();
```
Runtime:
```cpp
std::size_t idx = g.buffer_index(1,0);
```

## 6. Cycle Detection
Any cycle in the provided edge types triggers a static_assert in `Topology` (and thus in `PipelineGraph`).

## 7. Pure Topology (with ports defined via RoutingVertex)
```cpp
using T = ugraph::Topology<decltype(vA.out() >> vB.in()), decltype(vB.out() >> vC.in())>;
static_assert(!T::is_cyclic());
constexpr auto order = T::ids();      // vertex ids in topological order
static_assert(T::size() == 3);
// Visit vertex types
T::for_each([](auto tag){ using V = typename decltype(tag)::type; (void)sizeof(V); });

// Or gather all vertex tags in one call (variadic) if you need pack processing:
auto all_ids = T::apply([](auto... tags) {
    return std::array<std::size_t, sizeof...(tags)>{ decltype(tags)::id()... };
});
```

---
Minimal, fast, and explicit: pick `Topology` when you only need order & reflection; pick `PipelineGraph` when you also need wiring + buffer reuse.

## 8. API Reference (Topology visitation)

There are two complementary compile-time visitation primitives on `Topology`:

1. for_each(F&& f)
    Invokes `f(tag)` once per vertex in topological order. The tag is the `Vertex<id, UserType>` wrapper type (default constructed) so you can retrieve:
    - `decltype(tag)::id()` – the stable id
    - `typename decltype(tag)::type` – the user payload type (e.g. Config)

    Example:
    ```cpp
    AppTopo::for_each([](auto tag){ using S = typename decltype(tag)::type; S::init(); });
    ```

2. apply(F&& f)
    Invokes `f` exactly once with every vertex tag as a separate parameter in topological order. Useful for building constexpr arrays, forwarding packs, or generating aggregate structures without intermediate storage.

    Example:
    ```cpp
    constexpr auto ids = AppTopo::apply([](auto... tags){
         return std::array<std::size_t, sizeof...(tags)>{ decltype(tags)::id()... };
    });
    static_assert(ids[0] == 1);
    ```

        `apply` also accepts a lambda that returns `void`; in that case nothing is returned (unlike `for_each`, it still passes all tags in a single invocation). Example:
        ```cpp
        AppTopo::apply([](auto... tags){ (void)std::initializer_list<int>{ ( (void)decltype(tags)::id(), 0)... }; });
        ```

Choose `for_each` for simple independent actions per vertex; choose `apply` when you need the whole pack simultaneously.

