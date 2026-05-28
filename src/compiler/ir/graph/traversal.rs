use std::collections::VecDeque;

use super::Graph;
use crate::compiler::ir::{IrError, Operation};

impl Graph {
    pub fn topological_operations(&self) -> Result<Vec<Operation>, IrError> {
        let live_ops: Vec<_> = self
            .operations
            .iter()
            .enumerate()
            .filter_map(|(index, op)| (!op.erased).then_some(Operation::new(index)))
            .collect();
        let mut indegree = vec![0usize; self.operations.len()];
        let mut edges = vec![Vec::new(); self.operations.len()];

        for op in &live_ops {
            let data = &self.operations[op.index()];
            for operand in &data.operands {
                if let Some(producer) = self.values[operand.index()].producer {
                    if producer != *op && !self.operations[producer.index()].erased {
                        edges[producer.index()].push(*op);
                        indegree[op.index()] += 1;
                    }
                }
            }
        }

        let mut queue = VecDeque::new();
        for op in &live_ops {
            if indegree[op.index()] == 0 {
                queue.push_back(*op);
            }
        }

        let mut order = Vec::new();
        while let Some(op) = queue.pop_front() {
            order.push(op);
            for next in &edges[op.index()] {
                indegree[next.index()] -= 1;
                if indegree[next.index()] == 0 {
                    queue.push_back(*next);
                }
            }
        }

        if order.len() != live_ops.len() {
            let cycle =
                live_ops.into_iter().filter(|op| indegree[op.index()] > 0).collect();
            return Err(IrError::Cycle(cycle));
        }
        Ok(order)
    }

    pub fn walk_topological(
        &self,
        mut visitor: impl FnMut(Operation) -> Result<(), IrError>,
    ) -> Result<(), IrError> {
        for op in self.topological_operations()? {
            visitor(op)?;
        }
        Ok(())
    }
}
