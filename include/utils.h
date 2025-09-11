#pragma once

#define NR_THREAD 10
#define ORDERED_TOPO_SORT
// #define PERF

#define LENGTH(a) (sizeof(a) / sizeof(a[0]))

#define _STR(a) # a
#define STR(a)  _STR(a)
#define _CONCAT(a, b) a ## b
#define CONCAT(a, b)  _CONCAT(a, b)

#define MAX(a, b) ((a >= b) ? a : b)
#define MIN(a, b) ((a >= b) ? b : a)
#define ABS(a) (a >= 0 ? a : -a)
#define ROUNDUP(a, sz) ((((uintptr_t)a) + (sz) - 1) & ~((sz) - 1))
#define BITMASK(bits) ((1ull << (bits)) - 1)
#define BITS(x, hi, lo) (((x) >> (lo)) & BITMASK((hi) - (lo) + 1)) // similar to x[hi:lo] in verilog

#define nodeType(node) widthUType(node->width)

#define Cast(width, sign)                     \
  ("(" + (sign ? widthSType(width) : widthUType(width)) + ")")

#define widthType(width, sign)                     \
  (sign ? widthSType(width) : widthUType(width))

#define widthUType(width) \
  std::string(width <= 8 ? "uint8_t" : \
            (width <= 16 ? "uint16_t" : \
            (width <= 32 ? "uint32_t" : \
            (width <= 64 ? "uint64_t" : format("unsigned _BitInt(%d)", ROUNDUP(width, 64))))))

#define widthSType(width) \
  std::string(width <= 8 ? "int8_t" : \
            (width <= 16 ? "int16_t" : \
            (width <= 32 ? "int32_t" : \
            (width <= 64 ? "int64_t" : format("_BitInt(%d)", ROUNDUP(width, 64))))))

#define widthBits(width) \
        (width <= 8 ? 8 : \
        (width <= 16 ? 16 : \
        (width <= 32 ? 32 : \
        (width <= 64 ? 64 : ROUNDUP(width, 64)))))

#define BASIC_WIDTH 256
#define MAX_UINT_WIDTH 2048
#define BASIC_TYPE __uint128_t
#define uint128_t __uint128_t
#define MAX_U8 0xff
#define MAX_U16 0xffff
#define MAX_U32 0xffffffff
#define MAX_U64 0xffffffffffffffff

#define UCast(width) (std::string("(") + widthUType(width) + ")")

#define MAP(c, f) c(f)

// #define TIME_COUNT

#ifdef TIME_COUNT
#define MUX_COUNT(...) __VA_ARGS__
#else
#define MUX_COUNT(...)
#endif

#define MUX_DEF(macro, ...) \
  do { \
    if (macro) { \
      __VA_ARGS__ \
    } \
  } while(0)

#define MUX_NDEF(macro, ...) \
  do { \
    if (!macro) { \
      __VA_ARGS__ \
    } \
  } while(0)



#ifndef CLOCK_GATE_NAME
#define CLOCK_GATE_NAME "ClockGate"
#endif

enum ResetType { UNCERTAIN, ASYRESET, UINTRESET, ZERO_RESET };
enum OPType {
  OP_EMPTY,
  OP_MUX,
/* 2expr */
  OP_ADD,
  OP_SUB,
  OP_MUL,
  OP_DIV,
  OP_REM,
  OP_LT,
  OP_LEQ,
  OP_GT,
  OP_GEQ,
  OP_EQ,
  OP_NEQ,
  OP_DSHL,
  OP_DSHR,
  OP_AND,
  OP_OR,
  OP_XOR,
  OP_CAT,
/* 1expr */
  OP_ASUINT,
  OP_ASSINT,
  OP_ASCLOCK,
  OP_ASASYNCRESET,
  OP_CVT,
  OP_NEG,
  OP_NOT,
  OP_ANDR,
  OP_ORR,
  OP_XORR,
/* 1expr1int */
  OP_PAD,
  OP_SHL,
  OP_SHR,
  OP_HEAD,
  OP_TAIL,
/* 1expr2int */
  OP_BITS,
  OP_BITS_NOSHIFT, // used for bit operations
/* index */
  OP_INDEX_INT,
  OP_INDEX,
/* when, may be replaced by mux */
  OP_WHEN,
/* special */
  OP_PRINTF,
  OP_ASSERT,
  OP_EXIT,
/* leaf non-node enode */
  OP_INT,
/* for arrays */
  OP_GROUP,
/* special nodes for memory */
  OP_READ_MEM,
  OP_WRITE_MEM,
  OP_INFER_MEM,
/* special nodes for invalid node */
  OP_INVALID,
  OP_RESET,
/* width processing */
  OP_SEXT,
/* extmodule / dipc */
  OP_EXT_FUNC,
/* aggregate when node */
  OP_STMT_SEQ,
  OP_STMT_WHEN,
  OP_STMT_NODE
};

class TypeInfo;
class PNode;
class Node;
class AggrParentNode;
class ENode;
class ExpTree;
class SuperNode;
class valInfo;
class clockVal;

#define newBasic(node) (node->name + "$new")
#define newName(node) newBasic(node)
#define oldName(node) (node->name + "$old$" + std::to_string(node->id))
