#include "og/fs/ramfs.h"

#include "memory.h"
#include "og/mem/heap.h"

namespace og::fs::ramfs {

namespace {

constexpr uint32_t kMaxNodes = 64;
constexpr uint32_t kMaxName = 32;
constexpr uint32_t kMaxChildren = 8;

struct RamNode {
  Node node;
  char name[kMaxName];
  RamNode* parent;
  RamNode* children[kMaxChildren];
  uint32_t child_count;
  uint8_t* data;
  uint64_t size;
  uint64_t capacity;
  bool used;
};

RamNode g_nodes[kMaxNodes];

[[nodiscard]] RamNode* NodeOf(Node* node) noexcept {
  return node == nullptr ? nullptr : static_cast<RamNode*>(node->fs_data);
}

[[nodiscard]] RamNode* AllocateNode(NodeType type, const char* name,
                                    RamNode* parent) noexcept;

Status FileRead(Node* node, uint64_t offset, std::span<uint8_t> buffer,
                uint64_t& out_read) noexcept {
  out_read = 0;
  if (node == nullptr) {
    return Status::kInvalidArg;
  }
  if (buffer.empty()) {
    return Status::kInvalidArg;
  }
  RamNode* file = NodeOf(node);
  if (file == nullptr) {
    return Status::kNotInit;
  }
  if (offset >= file->size) {
    return Status::kOk;
  }

  uint64_t available = file->size - offset;
  if (available > buffer.size()) {
    available = buffer.size();
  }
  mem::Copy(buffer.data(), file->data + offset, available);
  out_read = available;
  return Status::kOk;
}

Status FileWrite(Node* node, uint64_t offset, std::span<const uint8_t> buffer,
                 uint64_t& out_written) noexcept {
  out_written = 0;
  if (node == nullptr) {
    return Status::kInvalidArg;
  }
  if (buffer.empty()) {
    return Status::kInvalidArg;
  }
  RamNode* file = NodeOf(node);
  if (file == nullptr) {
    return Status::kNotInit;
  }

  const uint64_t end = offset + buffer.size();
  if (end < offset) {
    return Status::kInvalidArg;
  }

  if (end > file->capacity) {
    uint64_t capacity = file->capacity == 0 ? 64U : file->capacity;
    while (capacity < end) {
      capacity *= 2U;
    }
    const Result<void*> grown = mem::HeapAlloc(capacity);
    if (grown.is_err()) {
      return grown.status();
    }
    if (file->data != nullptr) {
      mem::Copy(grown.value(), file->data, file->size);
      (void)mem::HeapFree(file->data);
    }
    file->data = static_cast<uint8_t*>(grown.value());
    file->capacity = capacity;
  }

  mem::Copy(file->data + offset, buffer.data(), buffer.size());
  if (end > file->size) {
    file->size = end;
    file->node.size = end;
  }
  out_written = buffer.size();
  return Status::kOk;
}

Status DirLookup(Node* directory, const char* name, Node** out) noexcept {
  if (out == nullptr) {
    return Status::kInvalidArg;
  }
  *out = nullptr;
  if (directory == nullptr || name == nullptr) {
    return Status::kInvalidArg;
  }
  RamNode* dir = NodeOf(directory);
  if (dir == nullptr) {
    return Status::kNotInit;
  }
  for (uint32_t index = 0; index < dir->child_count; ++index) {
    RamNode* child = dir->children[index];
    if (mem::Same(child->name, name)) {
      *out = &child->node;
      return Status::kOk;
    }
  }
  return Status::kNotFound;
}

Status DirIterate(Node* directory, uint32_t index, DirEntry& entry,
                  bool& found) noexcept {
  found = false;
  if (directory == nullptr) {
    return Status::kInvalidArg;
  }
  RamNode* dir = NodeOf(directory);
  if (dir == nullptr) {
    return Status::kNotInit;
  }
  if (index >= dir->child_count) {
    return Status::kOk;
  }
  const RamNode* child = dir->children[index];
  mem::Zero(&entry, sizeof(entry));
  const std::size_t length = mem::Length(child->name);
  if (length >= sizeof(entry.name)) {
    return Status::kInvalidArg;
  }
  mem::Copy(entry.name, child->name, length + 1U);
  entry.type = child->node.type;
  entry.size = child->node.size;
  found = true;
  return Status::kOk;
}

Status DirCreate(Node* directory, const char* name, NodeType type,
                 Node** out) noexcept {
  if (out == nullptr) {
    return Status::kInvalidArg;
  }
  *out = nullptr;
  if (directory == nullptr || directory->type != NodeType::kDirectory ||
      name == nullptr || name[0] == '\0') {
    return Status::kInvalidArg;
  }
  if (type != NodeType::kFile && type != NodeType::kDirectory) {
    return Status::kInvalidArg;
  }
  RamNode* dir = NodeOf(directory);
  if (dir == nullptr) {
    return Status::kNotInit;
  }
  if (dir->child_count >= kMaxChildren) {
    return Status::kNoMem;
  }
  for (uint32_t index = 0; index < dir->child_count; ++index) {
    if (mem::Same(dir->children[index]->name, name)) {
      return Status::kExists;
    }
  }
  RamNode* child = AllocateNode(type, name, dir);
  if (child == nullptr) {
    return Status::kNoMem;
  }
  dir->children[dir->child_count] = child;
  ++dir->child_count;
  *out = &child->node;
  return Status::kOk;
}

Status DirRemove(Node* directory, Node* node) noexcept {
  if (directory == nullptr || node == nullptr) {
    return Status::kInvalidArg;
  }
  RamNode* dir = NodeOf(directory);
  RamNode* target = NodeOf(node);
  if (dir == nullptr || target == nullptr) {
    return Status::kNotInit;
  }
  uint32_t index = 0;
  while (index < dir->child_count && dir->children[index] != target) {
    ++index;
  }
  if (index >= dir->child_count) {
    return Status::kNotFound;
  }
  if (target->node.type == NodeType::kDirectory &&
      target->child_count != 0U) {
    return Status::kBusy;
  }
  if (target->data != nullptr) {
    (void)mem::HeapFree(target->data);
  }
  for (uint32_t current = index; (current + 1U) < dir->child_count; ++current) {
    dir->children[current] = dir->children[current + 1U];
  }
  --dir->child_count;
  mem::Zero(target, sizeof(*target));
  return Status::kOk;
}

Status DirRename(Node* directory, Node* node, Node* new_directory,
                 const char* new_name) noexcept {
  if (directory == nullptr || node == nullptr || new_directory == nullptr ||
      new_name == nullptr || new_name[0] == '\0') {
    return Status::kInvalidArg;
  }
  RamNode* old_dir = NodeOf(directory);
  RamNode* target = NodeOf(node);
  RamNode* new_dir = NodeOf(new_directory);
  if (old_dir == nullptr || target == nullptr || new_dir == nullptr) {
    return Status::kNotInit;
  }
  if (new_directory->type != NodeType::kDirectory) {
    return Status::kInvalidArg;
  }
  const std::size_t name_length = mem::Length(new_name);
  if (name_length >= kMaxName) {
    return Status::kInvalidArg;
  }
  for (uint32_t index = 0; index < new_dir->child_count; ++index) {
    if (new_dir->children[index] != target &&
        mem::Same(new_dir->children[index]->name, new_name)) {
      return Status::kExists;
    }
  }
  if (old_dir != new_dir) {
    if (new_dir->child_count >= kMaxChildren) {
      return Status::kNoMem;
    }
    uint32_t index = 0;
    while (index < old_dir->child_count &&
           old_dir->children[index] != target) {
      ++index;
    }
    if (index >= old_dir->child_count) {
      return Status::kNotFound;
    }
    for (uint32_t current = index; (current + 1U) < old_dir->child_count;
         ++current) {
      old_dir->children[current] = old_dir->children[current + 1U];
    }
    --old_dir->child_count;
    new_dir->children[new_dir->child_count] = target;
    ++new_dir->child_count;
  }
  mem::Copy(target->name, new_name, name_length + 1U);
  target->parent = new_dir;
  return Status::kOk;
}

const NodeOps kFileOps{nullptr, &FileRead, &FileWrite, nullptr, nullptr,
                       nullptr, nullptr};
const NodeOps kDirOps{&DirLookup, nullptr, nullptr, &DirIterate, &DirCreate,
                      &DirRemove, &DirRename};

[[nodiscard]] RamNode* AllocateNode(NodeType type, const char* name,
                                    RamNode* parent) noexcept {
  if (name == nullptr || name[0] == '\0') {
    return nullptr;
  }
  const std::size_t length = mem::Length(name);
  if (length >= kMaxName) {
    return nullptr;
  }
  RamNode* node = nullptr;
  for (uint32_t index = 0; index < kMaxNodes; ++index) {
    if (!g_nodes[index].used) {
      node = &g_nodes[index];
      break;
    }
  }
  if (node == nullptr) {
    return nullptr;
  }
  mem::Zero(node, sizeof(*node));
  node->used = true;
  node->node.type = type;
  node->node.ops = type == NodeType::kFile ? &kFileOps : &kDirOps;
  node->node.fs_data = node;
  node->parent = parent;
  node->node.name = node->name;
  mem::Copy(node->name, name, length + 1U);
  return node;
}

}  // namespace

Result<Node*> Create() noexcept {
  mem::Zero(g_nodes, sizeof(g_nodes));

  RamNode* root = &g_nodes[0];
  root->used = true;
  root->node.name = "/";
  root->node.type = NodeType::kDirectory;
  root->node.ops = &kDirOps;
  root->node.fs_data = root;
  root->parent = root;
  return Result<Node*>::Ok(&root->node);
}

Result<Node*> CreateDir(Node* parent, const char* name) noexcept {
  if (parent == nullptr || parent->type != NodeType::kDirectory) {
    return Result<Node*>::Err(Status::kInvalidArg);
  }
  RamNode* dir = NodeOf(parent);
  if (dir == nullptr) {
    return Result<Node*>::Err(Status::kNotInit);
  }
  if (dir->child_count >= kMaxChildren) {
    return Result<Node*>::Err(Status::kNoMem);
  }
  RamNode* child = AllocateNode(NodeType::kDirectory, name, dir);
  if (child == nullptr) {
    return Result<Node*>::Err(Status::kNoMem);
  }
  dir->children[dir->child_count] = child;
  ++dir->child_count;
  return Result<Node*>::Ok(&child->node);
}

Result<Node*> CreateFile(Node* parent, const char* name, const void* data,
                         uint64_t size) noexcept {
  if (parent == nullptr || parent->type != NodeType::kDirectory) {
    return Result<Node*>::Err(Status::kInvalidArg);
  }
  if (size != 0U && data == nullptr) {
    return Result<Node*>::Err(Status::kInvalidArg);
  }
  RamNode* dir = NodeOf(parent);
  if (dir == nullptr) {
    return Result<Node*>::Err(Status::kNotInit);
  }
  if (dir->child_count >= kMaxChildren) {
    return Result<Node*>::Err(Status::kNoMem);
  }

  RamNode* child = AllocateNode(NodeType::kFile, name, dir);
  if (child == nullptr) {
    return Result<Node*>::Err(Status::kNoMem);
  }

  if (size != 0U) {
    const Result<void*> buffer = mem::HeapAlloc(size);
    if (buffer.is_err()) {
      mem::Zero(child, sizeof(*child));
      return Result<Node*>::Err(buffer.status());
    }
    mem::Copy(buffer.value(), data, size);
    child->data = static_cast<uint8_t*>(buffer.value());
    child->capacity = size;
    child->size = size;
    child->node.size = size;
  }

  dir->children[dir->child_count] = child;
  ++dir->child_count;
  return Result<Node*>::Ok(&child->node);
}

}  // namespace og::fs::ramfs
