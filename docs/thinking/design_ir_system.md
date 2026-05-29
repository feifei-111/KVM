# IR + Dialect，为什么要这样写？

这种 mlir 的设计范式想要表达：IR 是相同的（语法），而 Dialect 是不同的语言元素

作为一个编译器，首先得确定输入是什么，我认为 mlir 的输入就是 “基于抽象数学操作的计算流程”

- 抽象数学操作 -> 需要有 operator 这样的东西代替背后的复杂语义，并且容易扩展。因为不像机器码或者冯诺依曼模型，很难说有什么“可以进行的操作的全集”，这个东西无法建模，只能具体化
  - 当然，所有数学操作不都能拆解成初等计算？其实也没问题，只不过这样做的确非常复杂，因为这个拆解混合着硬件细节，不是纯粹的代数
- 计算流程 -> 因为不直接拆解抽象数学操作，所以需要一个图来描述计算依赖

总结：mlir 提供的是一个允许将局部复杂度藏在 operator 内部的计算图语义

那么为什么这样设计呢？

因为 mlir 面对的输入就是这样用，本质上 mlir 只不过是 compiler 的前端，就是专门处理输入的

# 我们应该有 IR + Dialect 吗？

我认为我们需要的东西可能比这个更简单，不需要那么通用的语义，不过抄一下 mlir 其实没毛病，尤其是我们会有两个明显有差异的 dialect

# 关于 IR system 的实现

## mlir 如何描述图结构

按照 mlir 的写法，这个图相当的复杂

每个 operation 和 result 是一起 alloc 的，result 是一个 value，实际上是持有一个 impl 的 wrapper

所以 op 和 result 是相互知道的

此外 op 和 operand 会有一个额外结构来记录，这个东西叫做 use list，这里已经记不清了，总之比较绕

## rust 怎么描述图结构

我觉得这里没必要搞得太复杂，而且 rust 也不太能这样写

不过至少有一点是可以学习一下的，就是 context 来管理可复用的 type 和 attr 之类的

而且这里还有更关键的，就是 rust 压根没有指针和继承之类的东西

所以我觉得只能在 context 管理一切（不然怎么说明 operation 和 value 的所有权），所有的引用都通过一个 Index 间接去 context 搜索

这样的代价是，cpp 只要 deref 就行了，这里会变成一个 map 查询，暂时也不管了，慢就慢吧

（是否有更好的办法？）

## 类型系统

类型系统的功能就是对语法做出额外的约束

在 mlir 中，事实上它的 type 并不是真 type，更像一种 fast attr，用来记录 value 的信息

此外，operation 作为一个类似 funtion 的语义，没有 signature 之类的东西

但是同样也有检查，这些检查事实上是 runtime 的

我觉得这里有点绕的是，必须区分几个不同的层次

1. 原生语言的编译期行为
2. 自定义语言的编译期行为 = 原生语言的 runtime
3. 自定义语言的 runtime

这里有一个混淆，就是假如我们的 ir 需要类型系统，它可能只是一个在 rust 中的 runtime 行为
一种自然的想法是，难道我就不能利用 rust 的类型系统，让 rust 编译器帮我把事情干了吗？

我觉得这个有点难
1. value 的 type 到底是什么？实际上它可能是一个 data，内置了 dtype 和 shape 等数据，这个东西不能在编译期使用（用来比较之类的）
2. 也许可以写成一个带 const data 的模版，但是编译出来的产物只能有某些 shape 的实例，而不是任意 shape（dtype 是枚举的，所以没什么问题）
3. 那可以把 shape 踢出 type 语义，这回到了我们的目的。目的在于，我希望存在 operator 对 value 的检查，这个检查包含 shape，如果 shape 不在 type，那这个检查还是一个 rust runtime 的事情，是一样的

## 什么叫 name

还有一个命名上的问题，我发现 gpt 对这些不太好的地方很不敏感，写出来的东西相当奇怪

对于 name 这个 prop，value 的 name 是用来区分想通 type 的 value 的实例

而 operation 的 name 是一个操作的类型，或者说它其实是个 type 之类的东西，但是 operation 的 type，从某种程度上说，似乎是一个 tensor, tensor -> tensor 之类的东西

这里需要一些明确的区分，至少不要混用 name 这个词表达不同的东西

## 接口与分化

现在的做法是，语法元素的实体以及 type attr 之类的东西全都存在 context

所以所有的 value 和 op 在数据以及类型（rust 的）上没有任何区别

这里的问题是，一般来说 method 是绑在类型上的，至少是相关的

所以还需要一个层次分化相同的数据（rust 能 downcast）

比如说一个 op 可能是接口 + 一个 op index，这个 index 能从 context 索引到一个真正的 opdata 或者 opimpl 之类的东西

之前提到的 ir 类型检查，只能绑定到这个为止

为了让 opimpl 和 op 是统一的，应该有一个 builder 之类的东西生成 op，而不是手动创建 add op 之类的，这样丧失了与 impl 相关的约束
