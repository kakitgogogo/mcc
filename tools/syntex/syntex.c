%% 
#_____________________________ Expressions _____________________________
prim_expr : 'ident' | constant | 'string' | '(' expr ')' | generic

constant : 'int_const' | 'float_const' | 'enum_const'

generic : '_Generic' '(' assign_expr ',' generic_assoc_list ')'
generic_assoc_list : generic_assoc generic_assoc_list_tail
generic_assoc_list_tail : ',' generic_assoc generic_assoc_list_tail | 'empty'
generic_assoc : type_name ':' assign_expr | 'default' ':' assign_expr

post_expr	:	prim_expr post_expr_tail | '(' type_name ')' '{' initializer_list 'opt:,' '}' post_expr_tail
post_expr_tail :	'[' expr ']' post_expr_tail 
				|	'(' args_expr ')' post_expr_tail
				|	'.' ident post_expr_tail
				|	'->' ident post_expr_tail
				|	'++' post_expr_tail
				|	'--' post_expr_tail
				|	'empty'

# Read of parameter list can be simplified in concrete implementation
args_expr :	args_list_expr | 'empty'
args_list_expr :	assign_expr args_list_expr_tail
args_list_expr_tail :	',' assign_expr args_list_expr_tail | 'empty'

unary_expr :	post_expr
			|	'++' unary_expr
			|	'--' unary_expr
			|	unary_oper cast_expr
			|	'sizeof' sizeof_operand
			|	'_Alignof' '(' type_name ')'

# Just relying on '(' cannot predict the brance
sizeof_operand : unary_expr | '(' type_name ')'

unary_oper :	'&' | '*' | '+' | '-' | '~' | '!'

# Just relying on '(' cannot predict the brance
cast_expr :	unary_expr | '(' type_name ')' cast_expr

mul_expr :	cast_expr mul_expr_tail 
mul_expr_tail :	'*' cast_expr mul_expr_tail
				|	'/' cast_expr mul_expr_tail
				|	'%' cast_expr mul_expr_tail
				|	'empty'
 
add_expr :	mul_expr add_expr_tail 
add_expr_tail :	'+' mul_expr add_expr_tail
				|	'-' mul_expr add_expr_tail
				|	'empty'

shift_expr :	add_expr shift_expr_tail 
shift_expr_tail :	'<<' add_expr shift_expr_tail
				|	'>>' add_expr shift_expr_tail
				|	'empty'

rela_expr :	shift_expr rela_expr_tail 
rela_expr_tail :	'<' shift_expr rela_expr_tail
				|	'>' shift_expr rela_expr_tail
				|	'<=' shift_expr rela_expr_tail
				|	'>=' shift_expr rela_expr_tail
				|	'empty'

equal_expr :	rela_expr equal_expr_tail 
equal_expr_tail :	'==' rela_expr equal_expr_tail
					|	'!=' rela_expr equal_expr_tail
					|	'empty'

and_expr :	equal_expr and_expr_tail 
and_expr_tail :	'&' equal_expr and_expr_tail
				|	'empty'

xor_expr :	and_expr xor_expr_tail 
xor_expr_tail :	'^' and_expr xor_expr_tail
				|	'empty'

or_expr :	xor_expr or_expr_tail 
or_expr_tail :	'|' xor_expr or_expr_tail
				|	'empty'

land_expr :	or_expr land_expr_tail 
land_expr_tail :	'&&' or_expr land_expr_tail
				|	'empty'

lor_expr :	land_expr lor_expr_tail 
lor_expr_tail :	'||' land_expr lor_expr_tail
				|	'empty'

cond_expr :	lor_expr cond_expr_tail
cond_expr_tail :	'?' expr ':' cond_expr | 'empty'

# according to C11: assign_expr : cond_expr | unary_expr assign_oper assign_expr
# here, all are prefixed with cond_expr
# because the concrete implementation can judge whether it can be assigned. 
assign_expr :	cond_expr | unary_expr assign_oper assign_expr
assign_oper :	'='| '*='| '/='| '%='| '+='| '-='| '<<='| '>>='| '&='| '|='| '^='

expr :	assign_expr expr_tail
expr_tail :	',' assign_expr expr_tail
			|	'empty'

const_expr :	cond_expr

#_____________________________ Declarations _____________________________
decl :	decl_spec init_declarator_list ';' | static_assert_decl

decl_spec :	storage_spec decl_spec_tail
			|	type_spec decl_spec_tail
			|	type_qual decl_spec_tail
			|	func_spec decl_spec_tail
			|	alignment_spec decl_spec_tail
decl_spec_tail :	decl_spec | 'empty'

init_declarator_list :	init_declarator init_declarator_list_tail | 'empty'
init_declarator_list_tail :	',' init_declarator init_declarator_list_tail | 'empty'

init_declarator :	declarator init_declarator_tail
init_declarator_tail :	'=' initializer | 'empty'

storage_spec :	'typedef' | 'extern' | 'static' | '_Thread_local' | 'auto' | 'register'

type_spec :	'int' | 'short' | 'long' | 'float' | 'double' | 'char' | 'void' 
			| 'signed' | 'unsigned' | '_Bool' | '_Complex' | '_Imaginary' 
			| struct_or_union_spec | enum_spec | 'typedef_name'

# struct or union
struct_or_union_spec :	'struct|union_{' struct_decl_list '}' 
					 | 'struct|union_ident_{' struct_decl_list '}'
					 | 'struct|union_ident'

# struct_decl_list may be empty
struct_decl_list :	struct_decl struct_decl_list | 'empty'
struct_decl :	spec_qual_list struct_declarator_list ';' | static_assert_decl

spec_qual_list :	type_spec spec_qual_list_tail
				|	type_qual spec_qual_list_tail
spec_qual_list_tail : 	spec_qual_list | 'empty'

struct_declarator_list :	struct_declarator struct_declarator_list_tail | 'empty'
struct_declarator_list_tail :	',' struct_declarator struct_declarator_list_tail | 'empty'

struct_declarator :	declarator struct_declarator_tail | ':' const_expr
struct_declarator_tail :	':' const_expr | 'empty'
#

# enum
enum_spec :	'enum_{' enum_spec_tail | 'enum_ident_{' enum_spec_tail | 'enum_ident'
enum_spec_tail :	enum_list 'opt:,' '}'

enum_list :	enumerator enum_list_tail
enum_list_tail :	',' enumerator enum_list_tail | 'empty'

enumerator :	'ident' enumerator_tail
enumerator_tail :	'=' const_expr | 'empty'
#

type_qual	:	'const' | 'restrict' | 'volatile' | 'atomic'

func_spec : 'inline' | 'noreturn'

alignment_spec	:	'_Alignas' '(' alignment_spec_tail
alignment_spec_tail : type_name ')' | const_expr ')'

declarator :	pointer direct_declarator | direct_declarator

direct_declarator :	'ident' direct_declarator_tail
					|	'(' declarator ')' direct_declarator_tail

direct_declarator_tail :	'[' array_size ']' direct_declarator_tail
						|	'(' param_list ')' direct_declarator_tail
						|	'empty'

pointer :	'*' pointer_tail
pointer_tail :	type_qual_list pointer | 'empty'

type_qual_list :	type_qual type_qual_list | 'empty'

# not support variable length array
# C11 6.7.6.2p4: Variable length arrays are a conditional feature that implementations need not support
array_size :	const_expr | 'empty'

param_list :	param_type_list 'opt:,...' | ident_list

param_type_list :	param_decl param_type_list | 'empty'

param_decl :	decl_spec param_decl_tail
param_decl_tail :	declarator | 'empty'

ident_list :	ident ',' ident_list | 'empty'

type_name :	spec_qual_list type_name_tail
type_name_tail : abstract_declarator | 'empty'

abstract_declarator : pointer abstract_declarator_tail | direct_abstract_declarator
abstract_declarator_tail : direct_abstract_declarator | 'empty'

direct_abstract_declarator : '(' inside_the_paren ')' direct_abstract_declarator_tail
						   | '[' array_size ']' direct_abstract_declarator_tail
direct_abstract_declarator_tail : '(' param_type_list  'opt:,...' ')' direct_abstract_declarator_tail
								| '[' array_size ']' direct_abstract_declarator_tail
								| 'empty'
inside_the_paren : abstract_declarator | param_type_list 'opt:,...' | 'empty'

initializer :	assign_expr | '{' initializer_list 'opt:,' '}'

initializer_list :	initializer initializer_list_tail 
				 | designation initializer initializer_list_tail | 'empty'
initializer_list_tail :	',' initializer_list

designation : designator_list '='
designator_list : designator designator_list_tail
designator_list_tail : designator designator_list_tail | 'empty'
designator : '[' const_expr ']' | '.' 'ident'

static_assert_decl : '_Static_assert' '(' const_expr ',' 'string' ')' ';'

#_____________________________ Statements _____________________________
stmt 	:	label_stmt
		|	compound_stmt
		|	expr_stmt
		|	if_stmt
		|	switch_stmt
		|	for_stmt
		|	while_stmt
		|	jump_stmt

label_stmt : 'ident' ':' stmt
		   | 'case' const_expr ':' stmt
		   | 'default' ':' stmt

compound_stmt :	'{' block_item_list '}'

block_item_list : block_item block_item_list | 'empty'

block_item : decl | stmt

expr_stmt :	';' | expr ';'

if_stmt :	'if' '(' expr ')' '{' stmt '}' else_stmt
else_stmt :	'else' '{' stmt '}' | 'empty'

switch_stmt : 'switch' '(' expr ')' stmt

for_stmt :	'for' '(' for_head ')' '{' stmt '}'
for_head : expr_stmt expr_stmt expr | decl  expr_stmt expr

while_stmt :	'while' '(' expr ')' stmt | 'do' stmt 'while' '(' expr ')' ';'

jump_stmt :	'goto' ident ';' | 'continue' ';' | 'break' ';' | 'return' expr_stmt

#_____________________________ External Definitions _____________________________
tran_unit :	extern_decl tran_unit | 'empty'

extern_decl :	func_def | decl

func_def : decl_spec declarator func_def_tail
func_def_tail : decl_list compound_stmt | compound_stmt

decl_list : decl decl_list_tail
decl_list_tail : decl decl_list_tail | 'empty'

# parsing argument list is simplified in actual implementation