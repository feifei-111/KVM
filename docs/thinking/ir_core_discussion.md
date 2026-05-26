# IR Core 讨论记录

> **TL;DR:** 目前倾向于自己维护一套小的 core IR：core 只描述实例和关系，
> dialect 定义语义；保留 type slot，并增加 op signature 这一层。

## 背景

我们讨论过是否直接使用 MLIR / Pliron。Pliron 的模型比较接近需求，
但是语法和 API 看起来偏重、偏丑，所以当前更倾向于在 repo 内维护一套
更小的 IR core。

这里需要区分三层：

- core IR：描述具体实例和引用关系
- dialect：描述语言规则和语义
- typed wrapper：把裸的 op / value handle 包成更好用的语义接口

## 当前方向

core IR 应该只保留共享的图结构：

- `Context` 拥有 IR 对象
- `OperationIndex` / `ValueIndex` 是指向 `Context` 内对象的 handle
- `Operation` 是具体实例：name、operands、results、actual attrs
- `Value` 是具体实例：name、type handle、attrs、def/users
- dialect 注册 `Operator<Sig>`、具体 type 和具体 attr

`Operation` 不应该暴露 `push_operand` / `push_result` 这种增量接口。
operands、results、attrs 应该在构造时一次性给完整，通常由 dialect
specific wrapper 来构造。

需要额外区分两层：

- `Operator<Sig>`：语言定义层，描述 operand / result / attr 的约束
- operation：实例层，保存具体 operands / results / attrs

例如 `hir.add` 的 signature 可以描述：

```text
operands: tensor, tensor
results: tensor
attrs: optional broadcast flag
```

而具体 `Operation` 只记录这一次调用实际使用了哪些 value 和 attr。

这里的 `Operator<Sig>` 是一个接受 signature 的泛型定义：

```text
Operator<Sig>
```

`Sig` 负责描述和验证输入、输出、attr 约束。registry 内部可以把不同
`Operator<Sig>` erase 成 `OperatorObj`，但每个 dialect 仍然能用自己的
signature 结构定义 op。

## Dialect 和 Wrapper

具体 op，比如 `hir.add`，不应该是 core 里的另一种对象类型。core 里
仍然只有统一的 `Operation`。

dialect-specific op wrapper 应该只是一个 typed view：

```text
HirAddOp(OperationIndex)
```

这个 wrapper 不拥有 operands / results / attrs。它通过 `Context` 读取
底层 `Operation`，然后提供 `lhs`、`rhs`、`output` 之类的语义 accessor。

这和 Pliron 的模型类似：

- `Ptr<Operation>` 是实例 handle
- op name 标识 operation kind
- 具体 op struct 是 operation handle 外面的薄 wrapper

## Type 讨论

`type` 这个词容易让人误解成一套统一的静态类型系统。MLIR 里的 type
更像是挂在 SSA value 上的 privileged / fast metadata slot。

它不是一个固定的大 enum。具体 type 由 dialect 扩展。

所以 core 不应该定义这种通用结构：

```text
Type { name, attrs }
```

这样只是把真正的结构推到 attr map 里，并没有解决抽象问题。

更合理的模型是：

```text
Context / dialect registry 存 opaque type object
Value 存 TypeIndex
Dialect 定义具体的 TensorType / RegisterType / SmemType
Dialect 代码需要 shape / lanes 等具体字段时再 downcast
```

这更接近 Pliron / MLIR：

- context 拥有或者 unique type object
- value 持有 type handle
- concrete type payload 由 dialect 定义
- core 最多只能查询最小公共信息，比如 type name

type 和 attr 更像 dialect 注册进来的 singleton / definition，不应该和
operation / value 一样通过 `Store` 分配 index。`Store` 更适合存具体 IR
实例，比如 value 和 operation。type / attr 可以按 name 存在 registry 中。

现在倾向于保留 type system / type slot。原因是如果所有 shape、dtype、
register lanes 等信息都只从 value attrs 里读，后续 verifier、lowering、
rewriter 会变复杂。type slot 可以作为 value 的主 metadata，op signature
可以基于它描述输入输出约束。

这里的 type system 不意味着 core 要定义统一的 `TensorType` / `RegisterType`
大 enum。core 只需要能存 opaque type object，并让 dialect 在需要时 downcast
到具体 type。

## 未决问题

- KVM 的每个 value 是否都必须有 type handle？如果有 token / control value，
  是给它们定义专门的 type，还是允许 typeless？
- attribute 目前也倾向于采用类似 `Box<dyn AttrObj>` 的 opaque object 模型；
  但 builtin attr 集合还需要继续收敛。
- op signature 的结构应该多强：只记录 operand / result 数量，还是直接支持
  type constraint / attr spec？
- type uniquing 是否需要现在就做，还是先让 `Context` 普通存储 type object？
