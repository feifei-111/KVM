use std::fmt;

use crate::compiler::dialects::hlo::DIALECT;
use crate::compiler::ir::{Attr, AttrMap, Graph, TypeExpr};

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct DistConfig {
    backend: String,
    ranks: Vec<RankPlacement>,
}

impl DistConfig {
    pub fn new(backend: impl Into<String>) -> Self {
        Self { backend: backend.into(), ranks: Vec::new() }
    }

    pub fn nccl() -> Self {
        Self::new("nccl")
    }

    pub fn with_rank(
        mut self,
        rank: i64,
        host: impl Into<String>,
        process: i64,
        device: impl Into<String>,
    ) -> Self {
        self.ranks.push(RankPlacement {
            rank,
            host: host.into(),
            process,
            device: device.into(),
        });
        self
    }

    pub fn backend(&self) -> &str {
        &self.backend
    }

    pub fn ranks(&self) -> &[RankPlacement] {
        &self.ranks
    }
}

impl From<DistConfig> for Attr {
    fn from(value: DistConfig) -> Self {
        let mut fields = AttrMap::new();
        fields.insert("backend".to_string(), Attr::Symbol(value.backend));
        fields.insert(
            "ranks".to_string(),
            Attr::List(value.ranks.into_iter().map(Attr::from).collect()),
        );
        Attr::Dict(fields)
    }
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct RankPlacement {
    rank: i64,
    host: String,
    process: i64,
    device: String,
}

impl RankPlacement {
    pub fn new(
        rank: i64,
        host: impl Into<String>,
        process: i64,
        device: impl Into<String>,
    ) -> Self {
        Self { rank, host: host.into(), process, device: device.into() }
    }

    pub fn rank(&self) -> i64 {
        self.rank
    }

    pub fn host(&self) -> &str {
        &self.host
    }

    pub fn process(&self) -> i64 {
        self.process
    }

    pub fn device(&self) -> &str {
        &self.device
    }
}

impl From<RankPlacement> for Attr {
    fn from(value: RankPlacement) -> Self {
        let mut fields = AttrMap::new();
        fields.insert("rank".to_string(), Attr::Int(value.rank));
        fields.insert("host".to_string(), Attr::String(value.host));
        fields.insert("process".to_string(), Attr::Int(value.process));
        fields.insert("device".to_string(), Attr::String(value.device));
        Attr::Dict(fields)
    }
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub enum DistDesc {
    Collective { ranks: Vec<i64> },
    Broadcast { ranks: Vec<i64>, root: i64 },
    Pair { src: i64, dst: i64 },
}

impl DistDesc {
    pub fn collective(ranks: impl IntoIterator<Item = i64>) -> Self {
        Self::Collective { ranks: ranks.into_iter().collect() }
    }

    pub fn broadcast(ranks: impl IntoIterator<Item = i64>, root: i64) -> Self {
        Self::Broadcast { ranks: ranks.into_iter().collect(), root }
    }

    pub fn pair(src: i64, dst: i64) -> Self {
        Self::Pair { src, dst }
    }
}

impl From<DistDesc> for Attr {
    fn from(value: DistDesc) -> Self {
        let mut fields = AttrMap::new();
        match value {
            DistDesc::Collective { ranks } => {
                fields.insert("ranks".to_string(), rank_list(ranks));
            }
            DistDesc::Broadcast { ranks, root } => {
                fields.insert("ranks".to_string(), rank_list(ranks));
                fields.insert("root".to_string(), Attr::Int(root));
            }
            DistDesc::Pair { src, dst } => {
                fields.insert("src".to_string(), Attr::Int(src));
                fields.insert("dst".to_string(), Attr::Int(dst));
            }
        }
        Attr::Dict(fields)
    }
}

#[derive(Clone, Copy, Debug, Eq, PartialEq)]
pub enum CollectiveOp {
    AllReduceSum,
    AllReduceMax,
    AllGather,
    ReduceScatterSum,
}

impl fmt::Display for CollectiveOp {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        let name = match self {
            Self::AllReduceSum => "all_reduce.sum",
            Self::AllReduceMax => "all_reduce.max",
            Self::AllGather => "all_gather",
            Self::ReduceScatterSum => "reduce_scatter.sum",
        };
        write!(f, "{name}")
    }
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct CollectiveType {
    op: CollectiveOp,
    desc: DistDesc,
}

impl CollectiveType {
    pub fn new(op: CollectiveOp, desc: DistDesc) -> Self {
        Self { op, desc }
    }

    pub fn op(&self) -> CollectiveOp {
        self.op
    }

    pub fn desc(&self) -> &DistDesc {
        &self.desc
    }

    pub(crate) fn attrs(&self) -> AttrMap {
        AttrMap::from([("dist".to_string(), self.desc.clone().into())])
    }
}

impl From<CollectiveType> for TypeExpr {
    fn from(value: CollectiveType) -> Self {
        TypeExpr::new(DIALECT, format!("comm.{}", value.op))
    }
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct BroadcastType {
    desc: DistDesc,
}

impl BroadcastType {
    pub fn new(desc: DistDesc) -> Self {
        Self { desc }
    }

    pub(crate) fn attrs(&self) -> AttrMap {
        AttrMap::from([("dist".to_string(), self.desc.clone().into())])
    }
}

impl From<BroadcastType> for TypeExpr {
    fn from(_: BroadcastType) -> Self {
        TypeExpr::new(DIALECT, "comm.broadcast")
    }
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct DispatchType {
    desc: DistDesc,
}

impl DispatchType {
    pub fn new(desc: DistDesc) -> Self {
        Self { desc }
    }

    pub(crate) fn attrs(&self) -> AttrMap {
        AttrMap::from([("dist".to_string(), self.desc.clone().into())])
    }
}

impl From<DispatchType> for TypeExpr {
    fn from(_: DispatchType) -> Self {
        TypeExpr::new(DIALECT, "comm.dispatch")
    }
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct CombineType {
    desc: DistDesc,
}

impl CombineType {
    pub fn new(desc: DistDesc) -> Self {
        Self { desc }
    }

    pub(crate) fn attrs(&self) -> AttrMap {
        AttrMap::from([("dist".to_string(), self.desc.clone().into())])
    }
}

impl From<CombineType> for TypeExpr {
    fn from(_: CombineType) -> Self {
        TypeExpr::new(DIALECT, "comm.combine")
    }
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct SendType {
    desc: DistDesc,
}

impl SendType {
    pub fn new(desc: DistDesc) -> Self {
        Self { desc }
    }

    pub(crate) fn attrs(&self) -> AttrMap {
        AttrMap::from([("dist".to_string(), self.desc.clone().into())])
    }
}

impl From<SendType> for TypeExpr {
    fn from(_: SendType) -> Self {
        TypeExpr::new(DIALECT, "comm.send")
    }
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct RecvType {
    desc: DistDesc,
}

impl RecvType {
    pub fn new(desc: DistDesc) -> Self {
        Self { desc }
    }

    pub(crate) fn attrs(&self) -> AttrMap {
        AttrMap::from([("dist".to_string(), self.desc.clone().into())])
    }
}

impl From<RecvType> for TypeExpr {
    fn from(_: RecvType) -> Self {
        TypeExpr::new(DIALECT, "comm.recv")
    }
}

pub fn set_dist_config(graph: &mut Graph, config: DistConfig) {
    graph.set_property("dist.config", config.into());
}

fn rank_list(ranks: Vec<i64>) -> Attr {
    Attr::List(ranks.into_iter().map(Attr::Int).collect())
}
