#include <algorithm>
#include <cstddef>
#include <unordered_map>
#include "Node.h"
#include "common.h"
#include "lexpr.h"

using namespace sexpr;

using SuperMap = std::unordered_map<SuperNode*, size_t>;
using NodeMap = std::unordered_map<Node*, size_t>;

static bool isValidSuper(SuperNode * super) {
  return !super->instsEmpty()
    || super->superType == SUPER_EXTMOD
    || super->superType == SUPER_ASYNC_RESET;
}

static std::string_view stringifyNodeType(NodeType ty) {
  switch(ty) {
    case NODE_INVALID: return "invalid";
    case NODE_REG_SRC: return "reg-src";
    case NODE_REG_DST: return "reg-dst";
    case NODE_SPECIAL: return "special";
    case NODE_INP:     return "input";
    case NODE_OUT:     return "output";
    case NODE_MEMORY:  return "memory";
    case NODE_READER:  return "reader";
    case NODE_WRITER:  return "writer";
    case NODE_READWRITER: return "readwriter";
    case NODE_INFER:   return "infer";
    case NODE_OTHERS:  return "others";
    case NODE_REG_RESET: return "reg-reset";
    case NODE_EXT_IN:  return "ext-in";
    case NODE_EXT_OUT: return "ext-out";
    case NODE_EXT:     return "ext";
  }
}
// static std::string_view stringifySuperType(SuperType ty) {
//     switch(ty) {
//         case SUPER_VALID: return "valid";
//         case SUPER_EXTMOD: return "extmod";
//         case SUPER_ASYNC_RESET: return "async-reset";
//         case SUPER_UINT_RESET: return "unit-reset";
//         case SUPER_UPDATE_REG: return "update-reg";
//     }
// }
// static std::string_view stringifyInstType(SuperInfo ty) {
//   switch(ty) {
//     case SUPER_INFO_IF: return "if";
//     case SUPER_INFO_ELSE: return "else";
//     case SUPER_INFO_DEDENT: return "dedent";
//     case SUPER_INFO_STR: return "str";
//     case SUPER_INFO_ASSIGN_BEG: return "save";
//     case SUPER_INFO_ASSIGN_END: return "act";
//   }
// }

extern int maxConcatNum;

// struct StateInfo {
//   std::set<int> combNexts;
//   std::set<int> seqNexts;
// };

// static StateInfo getStateInfo(Node * node) {
//   StateInfo info;
//   for(Node * next: node->next) {
//     if(next->super->cppId < 0) continue;
//     if(next->super->cppId > node->super->cppId) {
//       info.combNexts.insert(next->super->cppId);
//     }
//   }
//   if(node->type == NODE_REG_DST) {
//     auto target = node->getSrc();
//     if(target->super->cppId != -1) {
//       info.seqNexts.insert(target->super->cppId);
//     }
//   }
//   if(node->type == NODE_MEMORY) {
//     for(Node * port: node->member) {
      
//     }
//   }
// }
#define RESET_NAME(node) (node->name + "$RESET")


struct SchIREmitter {
  Emitter e;
  SchIREmitter(std::ostream & out): e(out) {}
  std::vector<SuperNode*> allSupers;
  NodeMap node2idx;
  struct NodeWrapper {
    Node * node;
    bool isReset = false;
  };
  std::vector<NodeWrapper> allNodes;
  void addNode(Node * node) {
    if (node->type == NODE_SPECIAL || node->type == NODE_REG_RESET || (node->status != VALID_NODE)) return;
    if (node->type == NODE_REG_DST && !node->regSplit) return;
    if (node->type == NODE_WRITER) return;
    if (node->isLocal()) return;
    if (node2idx.count(node)) return;
    node2idx[node] = allNodes.size();
    allNodes.push_back({node});
    if(node->isReset() && node->type == NODE_REG_SRC) {
      allNodes.push_back({node, true});
    }
  }
  std::set<int> alwaysActive;
  void prepare(graph * graph) {
    for (SuperNode* super : graph->sortedSuper) {
      if(!isValidSuper(super)) continue;
      auto id = allSupers.size();
      super->cppId = id;
      allSupers.push_back(super);
      if (super->superType == SUPER_EXTMOD) {
        alwaysActive.insert(super->cppId);
      }
    }
    for (auto* super : graph->sortedSuper) {
      for (Node* member : super->member) {
        if (member->status == VALID_NODE) {
          member->updateActivate();
          member->updateNeedActivate(alwaysActive);
        }
      }
    }
    for(auto * super: graph->sortedSuper) {
      if (super->superType == SUPER_VALID || super->superType == SUPER_ASYNC_RESET) {
        for (auto* n : super->member) addNode(n);
      }
      if (super->superType == SUPER_EXTMOD) {
        for (size_t i = 1; i < super->member.size(); i ++) {
          addNode(super->member[i]);
        }
      }
    }
    for (auto * mem : graph->memory) addNode(mem);
  }
  template<typename T>
  void emitActIds(const T & ids) {
    for(const auto & id: ids) {
      if(id < 0) continue;
      e << id;
    }
  }
  void emitNode(Node * node, bool isReset) {
    e << inlined << tup;
    if(isReset) e << kw("reset");
    else e<< kw(stringifyNodeType(node->type));
    if(isReset) e << RESET_NAME(node);
    else e << node->name;
      // << node->name;
    e  << node->width;
    e << tup;
    if(node->type == NODE_MEMORY) {
      e << node->depth;
    }
    for(auto dim: node->dimension) {
      e << dim;
    }
    e << end;
    // std::set<int> cReads, cWrites, sWrites;
    std::set<int> cReads, sReads, writes;
    if(node->type == NODE_MEMORY) {
      std::set<int> reads;
      for(auto member: node->member) {
        if(member->type == NODE_READER) {
          reads.insert(member->super->cppId);
        }
        else if(member->type == NODE_WRITER) {
          writes.insert(member->super->cppId);
        }
        else if(member->type == NODE_READWRITER) {
          reads.insert(member->super->cppId);
          writes.insert(member->super->cppId);
        }
      }
      if(reads.size() > 0 && writes.size() > 0) {
        auto max_read = *std::max_element(reads.begin(), reads.end());
        auto min_write = *std::min_element(writes.begin(), writes.end());
        assert(max_read <= min_write);
      }
      sReads.insert(reads.begin(), reads.end());
    } else {
      writes.insert(node->super->cppId);
      for(auto * next: node->next) {
        if(next->super->cppId < 0) continue;
        if(next->super->cppId > node->super->cppId) {
          cReads.insert(next->super->cppId);
        }
      }
      if(node->type == NODE_REG_DST) {
        auto target = node->getSrc();
        if(target->super->cppId != -1) {
          sReads.insert(target->super->cppId);
        }
      }
      if(node->type == NODE_READWRITER) {
        std::cerr << "readwriter as state: " << node->name << std::endl;
      } else if(node->type == NODE_WRITER) {
        std::cerr << "writer as state: " << node->name << std::endl;
      }
      // assert(node->type != NODE_READWRITER);
      // assert(node->type != NODE_WRITER);
    }
    // e << tup;
    // for(auto write: writes) e << write;
    // e << end;
    // e << tup;
    // for(auto read: cReads) e << read;
    // e << end;
    // e << tup;
    // for(auto read: sReads) e << read;
    // e << end;
    assert(!(sReads.size() > 0 && cReads.size() > 0));
    e << (sReads.size() > 0); // is_seq
    e << tup;
    if(sReads.size() > 0) {
      for(auto read: sReads) e << read;
    } else {
      for(auto read: cReads) e << read;
    }
    e << end;
    e << tup;
    for(auto write: writes) {
      if(write < 0) continue;
      e << write;
    }
    e << end;
    if(sReads.size() > 0 && writes.size() > 1) {
      std::cerr << "seq read with multiple writes: " << node->name << std::endl;
    }
    e << tup;
    for(auto act: node->nextActiveId) {
      if(act < 0) continue;
      e << act;
    }
    e << end;
    e << end << pretty;
  }
  int emitSave(Node * node) {
    int node_id;
    if(node->type == NODE_WRITER) {
      node_id = node2idx.at(node->parent);
    } else {
      node_id = node2idx.at(node);
    }
    bool isAlwaysActivate = node->isArray() || node->type == NODE_WRITER;
    e << inlined << named("save") << node_id << node->name << !isAlwaysActivate << node->width << end << pretty;
    return node_id;
  }
  void emitAct(Node * node) {
    int node_id;
    if(node->type == NODE_WRITER) {
      node_id = node2idx.at(node->parent);
    } else {
      node_id = node2idx.at(node);
    }
    e << inlined << named("act") << node_id << node->name;
    bool isAlwaysActivate = node->isArray() || node->type == NODE_WRITER;
    e << !isAlwaysActivate;
    e << tup;
    for(auto act: node->nextActiveId) {
      if(act < 0) continue;
      e << act;
    }
    e << end;
    e << end << pretty;
  }
  void emitLogic(SuperNode * super) {
    e << list;
    e << kv("id", super->cppId);
    e << kv("always", super->superType == SUPER_EXTMOD);
    e << inlined << named("next");
    for(auto next: super->depNext) {
      if(next->cppId < 0) continue;
      e << next->cppId;
    }
    e << end << pretty;
    e << named("insts");
    std::set<int> owned;
    if(super->superType == SUPER_EXTMOD) {
      for(size_t i = 1; i < super->member.size(); i++) {
        owned.insert(emitSave(super->member[i]));
      }
    }
    for(auto * node: super->member) {
      if(node->isLocal()) {
        e << kv("raw", format("%s %s;", widthUType(node->width).c_str(), node->name.c_str()));
      }
    }
    for(const auto & inst: super->insts) {
      switch(inst.infoType) {
        case SUPER_INFO_IF:
        case SUPER_INFO_ELSE:
        case SUPER_INFO_DEDENT:
        case SUPER_INFO_STR:
          e << kv("raw", inst.inst);
          break;
        case SUPER_INFO_ASSIGN_BEG: {
          if(inst.node->isLocal()) break;
          owned.insert(emitSave(inst.node));
          break;
        }
        case SUPER_INFO_ASSIGN_END: {
          if (inst.node->isLocal()) break;
          emitAct(inst.node);
          break;
        }
      }
    }
    if(super->superType == SUPER_EXTMOD) {
      for(size_t i = 1; i < super->member.size(); i++) {
        emitAct(super->member[i]);
      }
    }
    e << end;
    e << inlined << named("own");
    for(auto owned: owned) {
      e << owned;
    }
    e << end << pretty;
    e << end;
  }
  void emitReset(SuperNode * super, size_t id) {
    e << list << kv("id", id);
    std::string resetName = super->resetNode->type == NODE_REG_SRC 
      ? RESET_NAME(super->resetNode).c_str()
      : super->resetNode->name.c_str();
    e << kv("reset", resetName);
    std::set<int> nexts;
    for(auto node: super->member) {
      if(node->type == NODE_REG_RESET) {
        node = node->getResetSrc();
      }
      for(auto * next: node->next) {
        if(next->super->cppId >= 0) {
          nexts.insert(next->super->cppId);
        }
      }
    }
    e << inlined << named("acts");
    for(auto next: nexts) {
      e << next;
    }
    e << end << pretty << named("insts");
    for(auto inst: super->insts) {
      switch(inst.infoType) {
        case SUPER_INFO_IF:
        case SUPER_INFO_ELSE:
        case SUPER_INFO_DEDENT:
        case SUPER_INFO_STR:
          e << kv("raw", inst.inst);
          break;
        case SUPER_INFO_ASSIGN_BEG:
        case SUPER_INFO_ASSIGN_END:
          break;
      }
    }
    e << end << end;
  }
  void emitResetNode(Node * node) {
    assert(node->isReset() && node->width <= BASIC_WIDTH && !node->isArray());
  }
  void exportSchIR(graph * graph) {
    prepare(graph);
    e << list;
    e << kv("name", graph->name);
    e << kv("max-concat", maxConcatNum);
    e << named("states");
    for(auto [node, isReset]: allNodes) {
      emitNode(node, isReset);
    }
    e << end;
    e << named("logics");
    for(auto super: allSupers) {
      emitLogic(super);
    }
    e << end;
    e << named("resets");
    size_t reset_id = 0;
    for(auto super: graph->allReset) {
      if(super->resetNode->status == CONSTANT_NODE) {
        continue;
      }
      emitReset(super, reset_id++);
    }
    e << end;
    e << end;
  }
};

void emitSchIR(graph * graph, std::ostream & out) {
  SchIREmitter emitter(out);
  emitter.exportSchIR(graph);
}