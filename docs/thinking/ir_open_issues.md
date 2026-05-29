# IR Open Issues

> **TL;DR:** 当前 IR 设计先保证语义清楚，但 type 访问和 context 存储还没有优化。

## Context

当前实现里，`Context` 拥有 value、operation、type、attr、operator。
对象之间不直接持有引用，而是通过 `Index` 间接访问。

这个设计避开了 Rust self-reference 和 ownership 问题，但也带来一些成本。

## Current Issues

### Type Access 不够快

`Value` 只保存 `TypeIndex`。如果需要访问 tensor 的 shape / dtype，需要：

```text
ValueIndex -> Value -> TypeIndex -> Context -> dyn Type -> downcast
```

这比 C++ / MLIR 风格的 `value.getType().dyn_cast<TensorType>()` 绕。

当前保留 `TypeIndex` 的主要意义不是性能，而是语义：

- type 是 value 的主 metadata
- operator verifier 基于 type 做检查
- type equality 可以退化成 `TypeIndex == TypeIndex`
- shape / dtype 不需要散落在 attrs 里

后续需要补 typed accessor，例如：

```text
context.value_type_as::<TensorType>(value)
```

或者在 dialect wrapper 里封装 type 查询。

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
- type 是否继续用 `Box<dyn Type>`，还是改成 KVM 自己的 closed enum？
- typed accessor 放在 `Context`，还是放在 dialect wrapper？
