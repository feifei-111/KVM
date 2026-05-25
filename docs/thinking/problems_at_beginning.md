我现在的大致模块构想是这样的，但是但是具体策略还需要详细讨论：


# IR & compiler

## high level IR

为了让 KVM 处理一套稳定的语义边界，我们应该自己有一套 high level IR 用来描述语义功能

因为我们需要的语义有特殊性，它需要记录所有的通信，而不是只有普通计算

对于一种新的外部计算图描述，只需要增加一个额外的 parser 之类的东西即可

比如可以对 mlir，fxgraph 之类的做变换，至少我们把难度放到变换本身了

## executable IR

1. 这套 IR 的语义实际上分成两层：
    1. 这个行为是怎么 launch 的：和谁并行，和谁同步
    2. 这个行为实际上做了什么：比如 store register, mma
2. 这个行为尽可能是 “特性独立” 的，比如说只有计算，或者只有 io，如果混合会让后面的 lowering 变复杂
3. IR 中的行为应该是 kernel 内部执行的语义，因此，它必须有让 kernel 能够辨识的层次。不能说 load store，只能 load gmem，store register
4. 这套 IR 的语义应该足够具体，从而减免 executor 中的逻辑，比如指定到底哪些 thread 执行，而哪些不执行

这里和一般的 executor 不同之处在于，普通的 executor 是内嵌了选择策略的拓扑序执行，但是 executable IR 应该表达出更多的信息，executor 没有额外策略

因此，EIR 的主体问题可能不是“做什么”，而是“如何并行地做”。具体策略可以后面重构，
但 IR 需要允许表达 thread/warp/block 级别的执行范围和同步关系。

## lowering

1. decompose
    显然，hir -> eir 的过程中，必然需要一个 decompose 的行为，将 kernel 级别的语义拆分为 kernel 内操作单元级别
    这可以视为一个 transform，按照 mlir 的说法，就是换了一套 dialect

2. optimizer
    这里实际上涉及到两个问题，对应 IR 的两个层次
    1. 怎么并行
    2. 执行什么
    这两个问题事实上是相关的

这个 lowering 显然非常复杂，尤其是 optimizer，但是基本思路是有的

- resources manager：gpu 硬件资源管理，防止出现不可执行的 parallel workflow
    - resource 需要分类，比如存储类、计算类、io 类、同步类资源
    - 不同 resource 的 scope 也不同，比如 thread / warp / block / device
- task analyser：可以估算操作在指定资源时需要的时间
- basic overlap stragety：尽可能 overlap 不同的硬件资源消耗，此处需要 cost model 或者什么其他的启发性判别依据。
    - 其实还有更 trivial 的做法，本质上就是我总是预留几个 thread 做 io，在算前一个 operator 逻辑时，先加载后一个 operator 的 weight
    - 这个问题本质上是一个依赖分析的问题，即，我在 compute bound 算子中，能预先 io 什么（这个比较常见）？以及我在 io bound 算子中，能触发什么计算？（网络同理）
    - 这里的依赖分析很复杂，比如前后两个 operator，后一个可能不依赖前者的全量输出，这个算是一个 advanced feature 吧

另外，“特性独立” 只是为了方便 cost / dependency 分析，不是必须保持的执行形态。
optimizer / codegen 仍然可以把 io 和 compute fuse 到一起。

## simulator

其实还有一个方案来实现 lowering，上面说的 lowering 本质上是基于策略的，我们还能基于验证来做

虽然我们不知道什么最优策略，但是给我一个生成的 ir，我可以验证它或者给它打分，即存在一个 executable IR 的 simulator

目前我觉得策略与验证都是需要的，但是总体来看，我认为基于验证是一定要做的，因为即便策略不行，我还能手动调整。这种策略驱动，一般来说是需要大量 case 堆积，慢慢提升的

而且还有一个好处，基于验证的方案有一个保底：我们可以直接采样

另外，需要 simulator 的一个原因是，写 simulator 的时候其实能反推 strategy

这里的 simulator 不需要负责预测真实 latency，latency 应该交给 benchmark。它更像是
一个 verifier / rough scorer，用来检查 dependency、resource、sync 和明显的 pipeline
问题。

# executor

## executor 与 ir 如何交互

如果是一个单纯的 dsl executor，它可能会有一套分发逻辑，然后对每个指令实现一套具体的逻辑

但是我们的 executor 本质上就是 consistant kernel，所以它做的越少越好

我们把 scheduler 的事情放到了上面说的 compiler（另一种做法是，直接把一个更简单的 ir 给 executor，让它动态决定计算，我觉得这样不好，还是需要静态优化）

现在，假如我们已经有了一个合适的 ir，executor 要做什么呢？

我认为这里也有两个不同的做法

1. executor 的确有一套 ir parser 逻辑，问题是我们的 ir 显然包含了并行方案和执行内容量两个语义，所以这里的 parse 就会变复杂，只能想办法改 ir 的形式，比如说，在 ir 里面插入并行源语之类的
2. executor 事实上是专用的，换句话说，给我一个 ir，我应该生成一个对应的 executor kernel，从某种意义上说它是一个非通用的算子——如果这样做，还需要 consistant kernel 吗？看起来也不需要 consistant 吧？

## executor 的执行形式

我设想的执行形式是，kernel 一开始就 alloc 了全部的  smem 和 register，所有的 ir 执行内容调用，直接变成传递指针（或者什么别的表示符号）的 cuda device kernel 调用

- 需要一个类似 scope 之类的 manager

不过这里的问题是，cudac 里面写 "int x = 0;" 这个 x 可能绑到不同的硬件
1. 绑定到 register：不能拿指针，不能传地址，不能共享到别的 funciton（除非 inline）
2. 绑定到 local memory：可以拿指针，但是它其实不是 register，比 register 慢

所以看起来我们只能去做 codegen 的方案了

## dynamic executor v.s. codegen

看起来 dynamic executor 的收益不大。

1. 如果一个 step 只有一个 kernel launch，launch overhead 已经不太是问题
2. dynamic executor 需要 runtime parse ir，还会引入 virtual register，容易退化到 local memory
3. codegen 可以 inline，让 compiler 看到完整数据流，更容易获得真实 register
4. consistent kernel 不一定等于 dynamic executor，提前编译好的 executor 也可以 persistent，然后按 batch=1/2/4... 做有限分支

# benchmark

我们需要一套 benchmark 逻辑来方便地进行精度和性能的验证

1. 对于精度，应该直接和 torch api 构建的计算图对齐，相当于需要一个 HIR -> torch api 的映射，这个可能不难
2. 性能：这个对比的对象不太好整，所以搞一个计时即可
