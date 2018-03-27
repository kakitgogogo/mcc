// all defs and punctuators

#ifdef def

// keywords

// type-specifier
def(KW_VOID, "void")
def(KW_CHAR, "char")
def(KW_SHORT, "short")
def(KW_INT, "int")
def(KW_LONG, "long")
def(KW_FLOAT, "float")
def(KW_DOUBLE, "double")
def(KW_SIGNED, "signed")
def(KW_UNSIGNED, "unsigned")
def(KW_BOOL, "_Bool")

def(KW_UNION, "union")
def(KW_STRUCT, "struct")
def(KW_ENUM, "enum")

// type-qualifier
def(KW_CONST, "const")
def(KW_RESTRICT, "restrict")
def(KW_VOLATILE, "volatile")
def(KW_ATOMIC, "_Atomic")
def(KW_COMPLEX, "_Complex")
def(KW_IMAGINARY, "_Imaginary")

// storage-class-specifier
def(KW_TYPEDEF, "typedef")
def(KW_TYPEOF, "typeof")
def(KW_EXTERN, "extern")
def(KW_STATIC, "static")
def(KW_THREAD_LOCAL, "_Thread_local")
def(KW_AUTO, "auto")
def(KW_REGISTER, "register")

// function-speccifier
def(KW_INLINE, "inline")
def(KW_NORETURN, "_Noreturn")

// alignment-specifier
def(KW_ALIGNAS, "_Alignas")

// The rest is neither specifier nor qualifier
def(KW_ALIGNOF, "_Alignof")
def(KW_BREAK, "break")
def(KW_CASE, "case")
def(KW_CONTINUE, "continue")
def(KW_DEFAULT, "default")
def(KW_DO, "do")
def(KW_ELSE, "else")
def(KW_FOR, "for")
def(KW_GENERIC, "_Generic")
def(KW_GOTO, "goto")
def(KW_IF, "if")
def(KW_RETURN, "return")
def(KW_SIZEOF, "sizeof")
def(KW_STATIC_ASSERT, "_Static_assert")
def(KW_SWITCH, "switch")
def(KW_WHILE, "while")

// punctuators
def(P_ARROW, "->")
def(P_ASSIGN_ADD, "+=")
def(P_ASSIGN_AND, "&=")
def(P_ASSIGN_DIV, "/=")
def(P_ASSIGN_MOD, "%=")
def(P_ASSIGN_MUL, "*=")
def(P_ASSIGN_OR, "|=")
def(P_ASSIGN_SAL, "<<=")
def(P_ASSIGN_SAR, ">>=")
def(P_ASSIGN_SUB, "-=")
def(P_ASSIGN_XOR, "^=")
def(P_DEC, "--")
def(P_EQ, "==")
def(P_GE, ">=")
def(P_INC, "++")
def(P_LE, "<=")
def(P_LOGAND, "&&")
def(P_LOGOR, "||")
def(P_NE, "!=")
def(P_SAL, "<<")
def(P_SAR, ">>") 
def(P_ELLIPSIS, "...")
def(P_HASHHASH, "##")

#endif
