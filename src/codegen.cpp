#include "core.h"

static int depth;

static int count(void) {
  static int i = 1;
  return i++;
}

static void push(void) {
  printf("  push %%rax\n");
  depth++;
}

static void pop(char *arg) {
  printf("  pop %s\n", arg);
  depth--;
}

// Round up `n` to the nearest multiple of `align`. For instance,
// align_to(5, 8) returns 8 and align_to(11, 8) returns 16.
static int align_to(int n, int align) {
  return (n + align - 1) / align * align;
}

// Compute the absolute address of a given node.
// It's an error if a given node does not reside in memory.
static void gen_addr(Node *node) {
  if (node->kind == Node::NodeKind::ND_VAR) {
    printf("  lea %d(%%rbp), %%rax\n", node->var->offset);
    return;
  }

  error("not an lvalue");
}

// Generate code for a given node.
static void gen_expr(Node *node) {
  switch (node->kind) {
    case Node::NodeKind::ND_NUM:
      printf("  mov $%d, %%rax\n", node->val);
      return;
    case Node::NodeKind::ND_NEG:
      gen_expr(node->lhs);
      printf("  neg %%rax\n");
      return;
    case Node::NodeKind::ND_VAR:
      gen_addr(node);
      printf("  mov (%%rax), %%rax\n");
      return;
    case Node::NodeKind::ND_ASSIGN:
      gen_addr(node->lhs);
      push();
      gen_expr(node->rhs);
      pop("%rdi");
      printf("  mov %%rax, (%%rdi)\n");
      return;
  }

  gen_expr(node->rhs);
  push();
  gen_expr(node->lhs);
  pop("%rdi");

  switch (node->kind) {
    case Node::NodeKind::ND_ADD:
      printf("  add %%rdi, %%rax\n");
      return;
    case Node::NodeKind::ND_SUB:
      printf("  sub %%rdi, %%rax\n");
      return;
    case Node::NodeKind::ND_MUL:
      printf("  imul %%rdi, %%rax\n");
      return;
    case Node::NodeKind::ND_DIV:
      printf("  cqo\n");
      printf("  idiv %%rdi\n");
      return;
    case Node::NodeKind::ND_EQ:
    case Node::NodeKind::ND_NE:
    case Node::NodeKind::ND_LT:
    case Node::NodeKind::ND_LE:
      printf("  cmp %%rdi, %%rax\n");

      if (node->kind == Node::NodeKind::ND_EQ)
        printf("  sete %%al\n");
      else if (node->kind == Node::NodeKind::ND_NE)
        printf("  setne %%al\n");
      else if (node->kind == Node::NodeKind::ND_LT)
        printf("  setl %%al\n");
      else if (node->kind == Node::NodeKind::ND_LE)
        printf("  setle %%al\n");

      printf("  movzb %%al, %%rax\n");
      return;
  }

  error("invalid expression");
}

static void gen_stmt(Node *node) {
  switch (node->kind) {
    case Node::NodeKind::ND_IF: {
      int c = count();
      gen_expr(node->cond);
      printf("  cmp $0, %%rax\n");
      printf("  je  .L.else.%d\n", c);
      gen_stmt(node->then);
      printf("  jmp .L.end.%d\n", c);
      printf(".L.else.%d:\n", c);
      if (node->els) gen_stmt(node->els);
      printf(".L.end.%d:\n", c);
      return;
    }
    case Node::NodeKind::ND_FOR: {
      int c = count();
      if (node->init) gen_stmt(node->init);
      printf(".L.begin.%d:\n", c);
      if (node->cond) {
        gen_expr(node->cond);
        printf("  cmp $0, %%rax\n");
        printf("  je  .L.end.%d\n", c);
      }
      gen_stmt(node->then);
      if (node->inc) gen_expr(node->inc);
      printf("  jmp .L.begin.%d\n", c);
      printf(".L.end.%d:\n", c);
      return;
    }
    case Node::NodeKind::ND_BLOCK:
      for (Node *n = node->body; n; n = n->next) gen_stmt(n);
      return;
    case Node::NodeKind::ND_RETURN:
      gen_expr(node->lhs);
      printf("  jmp .L.return\n");
      return;
    case Node::NodeKind::ND_EXPR_STMT:
      gen_expr(node->lhs);
      return;
  }

  error("invalid statement");
}

// Assign offsets to local variables.
static void assign_lvar_offsets(Function *prog) {
  int offset = 0;
  for (Obj *var = prog->locals; var; var = var->next) {
    offset += 8;
    var->offset = -offset;
  }
  prog->stack_size = align_to(offset, 16);
}

void codegen(Function *prog) {
  assign_lvar_offsets(prog);

  printf("  .globl main\n");
  printf("main:\n");

  // Prologue
  printf("  push %%rbp\n");
  printf("  mov %%rsp, %%rbp\n");
  printf("  sub $%d, %%rsp\n", prog->stack_size);

  gen_stmt(prog->body);

  printf(".L.return:\n");
  printf("  mov %%rbp, %%rsp\n");
  printf("  pop %%rbp\n");
  printf("  ret\n");
}
