#include <ostream>
#include <unordered_map>
#include "common.h"

using namespace sexpr;

std::string_view stringifyNodeType(NodeType ty) {
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

std::string_view stringifySuperType(SuperType ty) {
    switch(ty) {
        case SUPER_VALID: return "valid";
        case SUPER_EXTMOD: return "extmod";
        case SUPER_ASYNC_RESET: return "async-reset";
        case SUPER_UINT_RESET: return "unit-reset";
        case SUPER_UPDATE_REG: return "update-reg";
    }
}

std::string_view stringifySuperInfo(SuperInfo ty) {
    switch(ty) {
      case SUPER_INFO_IF: return "if";
      case SUPER_INFO_ELSE: return "else";
      case SUPER_INFO_DEDENT: return "dedent";
      case SUPER_INFO_STR: return "str";
      case SUPER_INFO_ASSIGN_BEG: return "assign-beg";
      case SUPER_INFO_ASSIGN_END: return "assign-end";
    }
}

void emitNode(Emitter & e, Node * node) {
    e << named(stringifyNodeType(node->type));
    e << kv("name", node->name);
    e << kv("width", node->width);
    e << kv("signed", node->sign);
    e << kv("is-local", node->isLocal());
    if(!node->dimension.empty()) {
        e << named("dim");
        for(auto dim: node->dimension) {
            e << dim;
        }
        e << end;
    }
    e << end;
}

void emitNodes(Emitter & e, std::string_view name, const std::vector<Node*> & nodes) {
    e << named(name);
    for(auto item: nodes) {
        e << inlined;
        emitNode(e, item);
        e << pretty;
    }
    e << end;
}

using SuperMap = std::unordered_map<SuperNode*, size_t>;

void emitSuperVec(Emitter & e, std::string_view name, std::set<SuperNode*> & vec, SuperMap & super2idx) {
    e << inlined << named(name);
    for(auto prev: vec) {
        e << super2idx[prev];
    }
    e << end << pretty;
}

void emitSuperNodes(Emitter & e, std::string_view name, const std::vector<SuperNode*> & nodes, SuperMap & super2idx) {
    e << named(name);
    for(auto node: nodes) {
        e << named("super");
        e << kv("id", super2idx[node]);
        e << kv ("type", kw(stringifySuperType(node->superType)));
        emitSuperVec(e, "prev", node->prev, super2idx);
        emitSuperVec(e, "next", node->next, super2idx);
        emitSuperVec(e, "dep-prev", node->depPrev, super2idx);
        emitSuperVec(e, "dep-next", node->depNext, super2idx);
        if(node->resetNode) {
            e << named("reset-node");
            emitNode(e, node->resetNode);
            e << end;
        }
        if(!node->instsEmpty()) {
            e.named("insts");
            for(auto inst: node->insts) {
                e << inlined << named(stringifySuperInfo(inst.infoType));
                switch(inst.infoType) {
                  case SUPER_INFO_IF:
                  case SUPER_INFO_ELSE:
                  case SUPER_INFO_DEDENT:
                  case SUPER_INFO_STR: e << kv("inst", inst.inst); break;
                  case SUPER_INFO_ASSIGN_BEG: emitNode(e, inst.node); break;
                  case SUPER_INFO_ASSIGN_END: emitNode(e, inst.node); break;
                }
                e << end << pretty;
            }
            e << end;
        }
        if(!node->member.empty()) {
            emitNodes(e, "member", node->member);
        }
        e << end;
    }
    e << end;
}

void exportGraph(graph* g, std::ostream & out) {
    Emitter e(out);
    e << named("graph");
    emitNodes(e, "input", g->input);
    emitNodes(e, "output", g->output);
    emitNodes(e, "regsrc", g->regsrc);
    emitNodes(e, "memory", g->memory);
    SuperMap super2idx;
    for(auto super: g->sortedSuper) {
        size_t idx = super2idx.size();
        super2idx[super] = idx;
    }
    for(auto super: g->allReset) {
        size_t idx = super2idx.size();
        super2idx[super] = idx;
    }
    emitSuperNodes(e, "sorted-super", g->sortedSuper, super2idx);
    emitSuperNodes(e, "all-resets", g->allReset, super2idx);
    e << end;
}