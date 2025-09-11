/**
 * @file common.h
 * @brief common headers and macros
 */

#ifndef COMMON_H
#define COMMON_H

#include <sstream>
#include <string>
#include <iostream>
#include <vector>
#include <fstream>
#include <stdlib.h>
#include <cstring>
#include <cstdint>
#include <string.h>
#include <algorithm>
#include <set>
#include <map>
#include <gmp.h>
#include <cstdarg>

class TypeInfo;
class PNode;
class Node;
class AggrParentNode;
class ENode;
class ExpTree;
class SuperNode;
class valInfo;
class clockVal;

#include "opFuncs.h"
#include "debug.h"
#include "Node.h"
#include "PNode.h"
#include "ExpTree.h"
#include "StmtTree.h"
#include "graph.h"
#include "util.h"
#include "valInfo.h"
#include "perf.h"
#include "config.h"

#define TIMER_START(name) struct timeval CONCAT(__timer_, name) = getTime();
#define TIMER_END(name) do { \
  struct timeval t = getTime(); \
  char buf[256]; \
  snprintf(buf, sizeof(buf) - 1, "Timer.%s()." STR(name), __func__); \
  showTime(buf, CONCAT(__timer_, name), t); \
} while (0)

struct ordercmp {
  bool operator()(Node* n1, Node* n2) {
    return n1->order > n2->order;
  }
};

void getENodeRelyNodes(ENode* enode, std::set<Node*>& allNodes);

#endif
