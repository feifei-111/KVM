# IR Open Issues

> **TL;DR:** 历史问题记录。当前设计显式区分 value type 和 operation
> type：value type 可以携带 dtype/shape，operation type 保持轻量。

## Context

当前实现里，`Context` 拥有 value、operation、type、attr、operator。
对象之间不直接持有引用，而是通过 `Index` 间接访问。

这个设计避开了 Rust self-reference 和 ownership 问题，但也带来一些成本。

## Current Issues

### Type Access 语义已调整

早期设计把 tensor shape / dtype 放在 type 里，因此访问路径类似：

```text
ValueIndex -> Value -> TypeIndex -> Context -> dyn Type -> downcast
```

这个方向后来被细化。当前规则是：

- value type 保存 dialect/name 和必要 value 字段，例如 shape / dtype
- operation type 保存 dialect/name，用于选择 dialect hook
- dist desc 等 operation 实例配置是 operation attrs
- graph-level dist config 是 graph property

后续需要补 typed accessor，例如：

```text
hlo.value_as_tensor(value).shape()
hlo.operation_as_comm(op).dist()
```

其中前者读取 value type 字段，后者读取 operation attrs。

### Context Store 还可以更简单

现在 `Store` 使用 map 保存对象。这样实现方便，但对不可变、持久化、
只增不删的数据不一定合适。

对于 type、attr、operator 这类对象：

- 通常注册后不可变
- 生命周期接近整个 context
- 大多数时候只需要 index 查询

这些对象可以考虑用 `Vec` 或 arena 存储，让 `Index` 直接变成数组下标。
这样可以减少 map 查询，也更接近 Pliron / arena pointer 的模型。

value / operation 是否也改成 Vec，需要看后续是否真的需要删除和复用 slot。
如果需要删除检查和避免 stale index，可能要引入 generational index。

## Open Questions

- type / attr / operator 是否全部 immutable + interned？
- `Store` 是否拆成 append-only store 和 removable store？
- 是否需要 generational index 防止删除后旧 index 指向新对象？
- value type / operation type 是否在 host API 上显式拆成不同 wrapper？
- required attrs 的声明和 verifier 放在 dialect wrapper 还是 registry？
