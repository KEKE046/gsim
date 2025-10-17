#include "Node.h"
#include "PNode.h"
#include "common.h"
#include "graph.h"
#include "lexpr.h"
#include <cstddef>
#include <unordered_map>

using namespace sexpr;

using SuperMap = std::unordered_map<SuperNode*, size_t>;
using NodeMap = std::unordered_map<Node*, size_t>;

static bool isValidSuper(SuperNode * super) {
  return !super->instsEmpty()
    || super->superType == SUPER_EXTMOD
    || super->superType == SUPER_ASYNC_RESET;
}

extern int maxConcatNum;
extern std::string computeExtMod(SuperNode* super);

template<typename V, typename T = std::decay_t<decltype(*std::declval<V>().begin())>>
static std::vector<T> positives(const V & vec) {
  std::vector<T> result;
  for(auto & item: vec) {
    if(item >= 0) result.push_back(item);
  }
  return result;
}
#define RESET_NAME(node) (node->name + "$RESET")


struct SchIREmitterV2 {
  Emitter e;
  SchIREmitterV2(std::ostream & out): e(out) {
  }

  struct NodeInfo {
    Node * node;
    bool isReset = false;
    bool isExtIO = false;
    int reset_id = -1;
  };

  NodeMap node2idx;
  std::vector<NodeInfo> nodes;
  void addNode(Node * node) {
    if (node->type == NODE_SPECIAL || node->type == NODE_REG_RESET || (node->status != VALID_NODE)) return;
    if (node->type == NODE_REG_DST && !node->regSplit) return;
    if (node->type == NODE_WRITER) return;
    if (node->isLocal()) return;
    if (node2idx.count(node)) return;
    auto node_id = nodes.size();
    node2idx[node] = node_id;
    nodes.push_back({node});
    if(node->isReset() && node->type == NODE_REG_SRC) {
      auto reset_id = nodes.size();
      nodes.push_back({node, true});
      nodes[node_id].reset_id = reset_id;
    }
  }

  std::set<int> alwaysActive;

  struct SuperInfo {
    SuperNode * super;
    std::set<int> reads;
    std::set<int> writes;
  };

  std::vector<SuperInfo> supers;
  void prepare(graph * graph) {
    for(auto * super: graph->sortedSuper) {
      if(!isValidSuper(super)) continue;
      auto id = supers.size();
      super->cppId = id;
      supers.push_back({super});
      if(super->superType == SUPER_EXTMOD) {
        alwaysActive.insert(super->cppId);
      }
    }
    for(auto * super: graph->sortedSuper) {
      for(auto * node: super->member) {
        if(node->status == VALID_NODE) {
          node->updateActivate();
          node->updateNeedActivate(alwaysActive);
        }
      }
    }
    for(auto * super: graph->sortedSuper) {
      if (super->superType == SUPER_VALID || super->superType == SUPER_ASYNC_RESET) {
        for(auto * node: super->member) addNode(node);
      }
      if (super->superType == SUPER_EXTMOD) {
        for (size_t i = 1; i < super->member.size(); i ++) {
          addNode(super->member[i]);
        }
      }
    }
    for (auto * mem : graph->memory) addNode(mem);
  }

  std::set<int> getNodeReaders(Node * node) {
    std::set<int> readers;
    if(node->type == NODE_MEMORY) {
      for(auto member: node->member) {
        if(member->type == NODE_READER || member->type == NODE_READWRITER) {
          readers.insert(member->super->cppId);
        }
      }
    }
    else {
      for(auto * next: node->next) {
        if(next->super->cppId < 0) continue;
        if(next->super != node->super || node->super->findIndex(node) >= node->super->findIndex(next)) {
          readers.insert(next->super->cppId);
        }
      }
      if(node->type == NODE_REG_DST) {
        auto target = node->getSrc();
        if(target->super->cppId != -1) {
          readers.insert(target->super->cppId);
        }
      }
    }
    return readers;
  }

  std::set<int> getNodeWriters(Node * node) {
    std::set<int> writers;
    if(node->type == NODE_MEMORY) {
      for(auto member: node->member) {
        if(member->type == NODE_WRITER || member->type == NODE_READWRITER) {
          writers.insert(member->super->cppId);
        }
      }
    }
    else {
      if(node->super->cppId >= 0) {
        writers.insert(node->super->cppId);
      }
    }
    return writers;
  }

  void populateRWInfo(NodeInfo & info) {
    auto node = info.node;
    for(auto read: getNodeReaders(node)) {
      supers[read].reads.insert(node2idx.at(node));
    }
    for(auto write: getNodeWriters(node)) {
      supers[write].writes.insert(node2idx.at(node));
    }
    if(node->type == NODE_INP || node->type == NODE_OUT) {
      info.isExtIO = true;
    }
    // auto * node = info.node;
    // auto node_id = node2idx[node];
    // if(node->type == NODE_MEMORY) {
    //   for(auto member: node->member) {
    //     if(member->type == NODE_READER) {
    //       supers[member->super->cppId].reads.insert(node_id);
    //     }
    //     else if(member->type == NODE_WRITER) {
    //       supers[member->super->cppId].writes.insert(node_id);
    //     }
    //     else if(member->type == NODE_READWRITER) {
    //       supers[member->super->cppId].reads.insert(node_id);
    //       supers[member->super->cppId].writes.insert(node_id);
    //     }
    //   }
    // } else {
    //   if(node->type == NODE_INP || node->type == NODE_OUT) {
    //     info.isExtIO = true;
    //   }
    //   if(node->super->cppId < 0) {
    //     info.isExtIO = true;
    //   } else {
    //     supers[node->super->cppId].writes.insert(node_id);
    //   }
    //   for(auto * next: node->next) {
    //     if(next->super->cppId < 0) continue;
    //     if(next->super != node->super) {
    //       supers[next->super->cppId].reads.insert(node_id);
    //     } else if(node->super->findIndex(node) >= node->super->findIndex(next)) {
    //       supers[node->super->cppId].reads.insert(node_id);
    //     }
    //   }
    //   if(node->type == NODE_REG_DST) {
    //     auto target = node->getSrc();
    //     if(target->super->cppId != -1) {
    //       supers[target->super->cppId].reads.insert(node_id);
    //     }
    //   }
    // }
  }

  void verifySuperRW(SuperInfo & info) {

  }

  void emitNode(const NodeInfo & info) {
    e << inlined << list;
    auto * node = info.node;
    if(info.isReset) {
      e << kv("name", RESET_NAME(node));
    } else {
      e << kv("name", node->name);
    }
    e << kv("width", node->width);
    if(node->type == NODE_MEMORY) {
      std::vector<int> dims;
      dims.push_back(node->depth);
      for(auto dim: node->dimension) {
        dims.push_back(dim);
      }
      e << kvs("dims", dims);
    }
    else if(!node->dimension.empty()) {
      e << kvs("dims", node->dimension);
    }
    if(info.isExtIO) {
      e << kv("is-ext", true);
    }
    e << end << pretty;
  }

  Node * getStateNode(Node * node) {
    if(node->type == NODE_WRITER) {
      return node->parent;
    }
    else if(node->type == NODE_REG_RESET) {
      return node->getResetSrc();
    }
    else {
      return node;
    }
  }

  int getStateId(Node * node) {
    if(node->type == NODE_WRITER) {
      return node2idx.at(node->parent);
    }
    else if(node->type == NODE_REG_RESET) {
      node = node->getResetSrc();
      if(!node2idx.count(node)) {
        std::cerr << "node not found in state: " << node->name << " " << node->type << " " << node->super->cppId << std::endl;
      }
      return node2idx.at(node);
    }
    else {
      if(!node2idx.count(node)) {
        std::cerr << "node not found in state: " << node->name << " " << node->type << " " << node->super->cppId << std::endl;
      }
      return node2idx.at(node);
    }
  }

  void emitInsts(const std::vector<InstInfo>& insts) {
    for(auto inst : insts) {
      switch(inst.infoType) {
        case SUPER_INFO_IF:
        case SUPER_INFO_ELSE:
        case SUPER_INFO_DEDENT:
        case SUPER_INFO_STR:
          e << kv("cpp-code", inst.inst);
          break;
        case SUPER_INFO_ASSIGN_BEG:
          if(inst.node->isLocal()) break;
          e << inlined << named("write-pre")
            << kv("name", inst.node->name)
            << kv("sid", getStateId(inst.node))
            << end << pretty;
          break;
        case SUPER_INFO_ASSIGN_END:
          if(inst.node->isLocal()) break;
          e << inlined << named("write-post")
            << kv("name", inst.node->name)
            << kv("sid", getStateId(inst.node))
            << kvs("acts", positives(inst.node->nextNeedActivate))
            << end << pretty;
          break;
      }
    }
  }

  void emitBlock(const SuperInfo & info) {
    auto super = info.super;
    e << list; // block
    e << kv("id", super->cppId);
    e << kv("always", super->superType == SUPER_EXTMOD);
    e << kvs("reads", info.reads);
    e << kvs("writes", info.writes);
    e << named("insts");
    for(auto read: info.reads) {
      e << inlined << named("read")
        << kv("name", nodes[read].node->name)
        << kv("sid", read)
        << end << pretty;
    }
    if(super->superType == SUPER_EXTMOD) {
      for(size_t i = 1; i < super->member.size(); i++) {
        auto node_id = node2idx.at(super->member[i]);
        e << inlined << named("write-pre")
          << kv("name", nodes[node_id].node->name)
          << kv("sid", node_id)
          << end << pretty;
        assert(!super->member[i]->isLocal());
      }
      emitInsts(super->insts);
      for(size_t i = 1; i < super->member.size(); i++) {
        auto node_id = node2idx.at(super->member[i]);
        e << inlined << named("write-post")
          << kv("name", nodes[node_id].node->name)
          << kv("sid", node_id)
          << kvs("acts", positives(super->member[i]->nextNeedActivate))
          << end << pretty;
      }
    } else {
      for(auto * node: super->member) {
        if(node->isLocal()) {
          e << kv("cpp-code", format("%s %s;", widthUType(node->width).c_str(), node->name.c_str()));
        }
      }
      emitInsts(super->insts);
    }
    e << end; // insts
    e << end; // block
  }

  void emitReset(const SuperNode * super) {
    int reset_id = 0;
    // if(super->resetNode->type == NODE_REG_SRC) {
    //   reset_id = nodes[node2idx.at(super->resetNode)].reset_id;
    // } else {
    reset_id = node2idx.at(super->resetNode);
    // }
    // #define RESET_NAME(node) (node->name + "$RESET")
    // std::string resetName = super->resetNode->type == NODE_REG_SRC 
    //   ? RESET_NAME(super->resetNode).c_str()
    //   : super->resetNode->name.c_str();
    e << list; // reset
    e << kv("reset", reset_id);
    std::set<int> nexts;
    // for(auto & inst: super->insts) {
    //   if(inst.infoType == SUPER_INFO_ASSIGN_END) {
    //     auto node = getStateNode(inst.node);
    //     auto readers = getNodeReaders(node);
    //     auto writers = getNodeWriters(node);
    //     for(auto reader: readers) {
    //       nexts.insert(reader);
    //     }
    //     for(auto writer: writers) {
    //       nexts.insert(writer);
    //     }
    //   }
    // }
    for(auto * node: super->member) {
      if(node->type == NODE_REG_RESET) {
        auto src = node->getResetSrc();
        auto dst = src->getDst();
        for(auto reader: getNodeReaders(dst)) {
          nexts.insert(reader);
        }
        for(auto writer: getNodeWriters(dst)) {
          nexts.insert(writer);
        }
        for(auto reader: getNodeReaders(src)) {
          nexts.insert(reader);
        }
        for(auto writer: getNodeWriters(src)) {
          nexts.insert(writer);
        }
      } else {
        for(auto * next: node->next) {
          if(next->super->cppId >= 0) {
            nexts.insert(next->super->cppId);
          }
        }
      }
      // for(auto * next: node->next) {
      //   if(next->super->cppId >= 0) {
      //     nexts.insert(next->super->cppId);
      //   }
      // }
      // for(auto act: node->nextActiveId) {
      //   if(act >= 0) {
      //     nexts.insert(act);
      //   }
      // }
    }
    e << kvs("acts", nexts);
    e << named("insts");
    // emitInsts(super->insts);
    for(auto &inst: super->insts) {
      switch(inst.infoType) {
        case SUPER_INFO_IF:
        case SUPER_INFO_ELSE:
        case SUPER_INFO_DEDENT:
        case SUPER_INFO_STR:
          e << kv("cpp-code", inst.inst);
          break;
        default: break;
      }
    }
    e << end; // insts
    e << end; // reset
  }

  void exportSchIR(graph * graph) {
    prepare(graph);
    for(auto & node: nodes) {
      populateRWInfo(node);
    }
    for(auto & super: supers) {
      verifySuperRW(super);
    }
    e << list; // design
    e << kv("name", graph->name);
    e << kv("max-concat", maxConcatNum);
    e << named("ext-fns");
    for(auto super: graph->sortedSuper) {
      if(super->superType == SUPER_EXTMOD) {
        e << computeExtMod(super);
        // compute ext mod will insert a inst inside
        super->insts.pop_back();
      }
    }
    e << end; // ext-fns
    e << named("states");
    for(auto & node: nodes) {
      emitNode(node);
    }
    e << end; // states
    e << named("blocks");
    for(auto super: supers) {
      emitBlock(super);
    }
    e << end; // blocks
    e << named("resets");
    for(auto super: graph->allReset) {
      if(super->resetNode->status == CONSTANT_NODE) {
        continue;
      }
      emitReset(super);
    }
    e << end; // reset
    e << end; // design
  }
};

void emitSchIRv2(graph * graph, std::ostream & out) {
  SchIREmitterV2 emitter(out);
  emitter.exportSchIR(graph);
}