#pragma once

namespace gestalt::foundation {
  struct alignas(16) MeshTaskCommand {
    uint32 meshDrawId;
    uint32 taskOffset;
    uint32 taskCount;
  };
}  // namespace gestalt