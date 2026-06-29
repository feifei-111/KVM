#pragma once

// The tile-graph dialect ("tile"): registers its value/operation impl codecs
// and its operator signatures into a serialization Registry, so graphs using
// tile.* ops can be serialized and parsed.
//
// The tile dialect is what the task graph gets refined into during tile search:
// the same op set as task, but values carry a slice descriptor and ops have a
// stricter signature (input shape/dtype/loc must be a legal tile for the op on
// the device). See Notion "tile graph". Op names are tile.<unit>.<fn>.

#include "serialization/registry.h"

namespace kvm::ir::tile {

// Register every tile-dialect operator + impl codec into `r`.
void RegisterTileDialect(serial::Registry& r);

}  // namespace kvm::ir::tile
