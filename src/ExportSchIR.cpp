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

struct SchIREmitter {
  Emitter e;
  SchIREmitter(std::ostream & out): e(out) {}
  std::vector<SuperNode*> allSupers;
  NodeMap node2idx;
  std::vector<Node*> allNodes;
  void addNode(Node * node) {
    if (node->type == NODE_SPECIAL || node->type == NODE_REG_RESET || (node->status != VALID_NODE)) return;
    if (node->type == NODE_REG_DST && !node->regSplit) return;
    if (node->type == NODE_WRITER) return;
    if (node->isLocal()) return;
    if (node2idx.count(node)) return;
    node2idx[node] = allNodes.size();
    allNodes.push_back(node);
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
    for (auto* mem : graph->memory) addNode(mem);
  }
  template<typename T>
  void emitActIds(const T & ids) {
    for(const auto & id: ids) {
      if(id < 0) continue;
      e << id;
    }
  }
  void emitNode(Node * node) {
    e << inlined << tup 
      << kw(stringifyNodeType(node->type))
      << node->name
      << node->width;
    e << tup;
    if(node->type == NODE_MEMORY) {
      e << node->depth;
    }
    for(auto dim: node->dimension) {
      e << dim;
    }
    e << end;
    e << tup;
    emitActIds(node->nextActiveId);
    e << end;
    e << end << pretty;
  }
  void emitLogic(SuperNode * super) {
    e << list;
    e << kv("id", super->cppId);
    e << kv("always", super->superType == SUPER_EXTMOD);
    e << inlined
      << named("next");
    for(auto next: super->depNext) {
      if(next->cppId < 0) continue;
      e << next->cppId;
    }
    e << end 
      << pretty;
    e << inlined
      << named("owned");
    for(auto node: super->member) {
      if(node2idx.count(node)) {
        e << node2idx[node];
      }
    }
    e << end
      << pretty;
    e << named("insts");
    if(super->superType == SUPER_EXTMOD) {
      for(size_t i = 1; i < super->member.size(); i++) {
        auto node = super->member[i];
        e << inlined << named("save");
        e << node->name;
        e << node->width;
        e << end << pretty;
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
        case SUPER_INFO_ASSIGN_BEG:
          if (inst.node->isLocal() || inst.node->isArray() || inst.node->type == NODE_WRITER) break;
          e << inlined << named("save");
          e << inst.node->name;
          e << inst.node->width;
          e << end << pretty;
          break;
        case SUPER_INFO_ASSIGN_END:
          if (inst.node->isLocal() || !inst.node->needActivate()) break;
          if (inst.node->isArray() || inst.node->type == NODE_WRITER) {
            e << inlined << named("act") << list << end;
            e << tup;
            emitActIds(inst.node->nextActiveId);
            e << end << end << pretty;
          } else {
            e << inlined << named("act") << list << inst.node->name << end;
            e << tup;
            emitActIds(inst.node->nextActiveId);
            e << end << end << pretty;
          }
          break;
      }
    }
    if(super->superType == SUPER_EXTMOD) {
      for(size_t i = 1; i < super->member.size(); i++) {
        auto node = super->member[i];
        assert(node->type != NODE_EXT_IN);
        e << inlined << named("act") << list << node->name << end;
        e << tup;
        emitActIds(node->nextActiveId);
        e << end << end << pretty;
      }
    }
    e << end
      << end;
  }
  void emitReset(SuperNode * super, size_t id) {
    e << list;
    e << kv("id", id);
#define RESET_NAME(node) (node->name + "$RESET")
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
    e << inlined
      << named("acts");
    for(auto next: nexts) {
      e << next;
    }
    e << end
      << pretty;
    e << named("insts");
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
    e << end;
    e << end;
  }
  void exportSchIR(graph * graph) {
    prepare(graph);
    e << list;
    e << kv("name", graph->name);
    e << kv("max-concat", maxConcatNum);
    e << named("states");
    for(auto * node: allNodes) {
      emitNode(node);
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