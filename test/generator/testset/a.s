	.text
.globl f
f:
	nop
	push %rbp
	movq %rsp, %rbp
	push %rdi
	movslq -8(%rbp), %rax
	push %rax
	movslq -8(%rbp), %rax
	movq %rax, %rcx
	pop %rax
	imul %rcx, %rax
	leave
	ret
	leave
	ret
	.data 0
	.globl a
a:
	.quad 1
	.data 0
	.globl b
b:
	.long 1
	.long 2
	.data 0
	.globl c
c:
	.long 1
	.data 0
	.globl fp
fp:
	.quad f
	.data 0
	.globl g
g:
	.quad a
	.data 0
	.globl h
h:
	.long 9
	.zero 4
	.data 0
	.globl i
i:
	.quad c
	.data 0
	.globl gstr
gstr:
	.data 1
	.L0:
	.string "xxx"
	.data 0
	.quad .L0
	.data 0
	.globl j
j:
	.quad h+16
	.data 0
	.globl k
k:
	.long 17
	.zero 4
	.data 0
	.globl l
l:
	.quad b+4
	.text
.globl main
main:
	nop
	push %rbp
	movq %rsp, %rbp
	sub $48, %rsp
	movl $1, -8(%rbp)
	movq $2, %rax
	movslq %eax, %rax
	mov %rax, -16(%rbp)
	movslq -8(%rbp), %rax
	movslq %eax, %rax
	push %rax
	movq -16(%rbp), %rax
	movq %rax, %rcx
	pop %rax
	add %rcx, %rax
	mov %eax, -24(%rbp)
	movq $1, %rax
	push %rax
	movslq -8(%rbp), %rax
	pop %rcx
	cmp %eax, %ecx
	setl %al
	movzb %al, %eax
	test %rax, %rax
	je .L1
	movswq -32(%rbp), %rax
	movswq %ax, %rax
	movslq %eax, %rax
	jmp .L2
	.L1:
	movq -16(%rbp), %rax
	.L2:
	mov %ax, -32(%rbp)
	push %rdi
	movslq -24(%rbp), %rax
	movq %rax, %rdi
	call print_int
	pop %rdi
	push %rdi
	movswq -32(%rbp), %rax
	movswq %ax, %rax
	movq %rax, %rdi
	call print_int
	pop %rdi
	push %rdi
	movq $3, %rax
	movslq %eax, %rax
	push %rax
	movslq -8(%rbp), %rax
	movslq %eax, %rax
	push %rax
	movq -16(%rbp), %rax
	movq %rax, %rcx
	pop %rax
	add %rcx, %rax
	pop %rcx
	cmp %rax, %rcx
	setl %al
	movzb %al, %eax
	test %rax, %rax
	je .L3
	movslq -8(%rbp), %rax
	movslq %eax, %rax
	jmp .L4
	.L3:
	movq -16(%rbp), %rax
	.L4:
	movq %rax, %rdi
	call print_int
	pop %rdi
	movq $8, %rax
	push %rcx
	push %rdi
	mov $7, %rdi
	and %rdi, %rax
	shl $0, %rax
	mov -40(%rbp), %ecx
	mov $18446744073709551608, %rdi
	and %rdi, %rcx
	or %rcx, %rax
	pop %rdi
	pop %rcx
	mov %eax, -40(%rbp)
	movq $3, %rax
	push %rcx
	push %rdi
	mov $7, %rdi
	and %rdi, %rax
	shl $3, %rax
	mov -40(%rbp), %ecx
	mov $18446744073709551559, %rdi
	and %rdi, %rcx
	or %rcx, %rax
	pop %rdi
	pop %rcx
	mov %eax, -40(%rbp)
	movl $0, -36(%rbp)
	push %rdi
	movslq -40(%rbp), %rax
	push %rcx
	shr $3, %rax
	mov $7, %rcx
	and %rcx, %rax
	pop %rcx
	movslq %eax, %rax
	movq %rax, %rdi
	call print_int
	pop %rdi
.data
	.L5:
	.string "\xffffffe5\xffffff93\xffffff88\xffffffe5\xffffff93\xffffff88"
.text
	lea .L5(%rip), %rax
	mov %rax, -48(%rbp)
	movq $8, %rax
	push %rdi
	movq -48(%rbp), %rax
	movq %rax, %rdi
	call print_str
	pop %rdi
	push %rdi
	movq gstr+0(%rip), %rax
	movq %rax, %rdi
	call print_str
	pop %rdi
	push %rdi
	push %rdi
	sub $8, %rsp
	movq $2, %rax
	movq %rax, %rdi
	movq fp+0(%rip), %rax
	movq %rax, %r11
	call *%r11
	add $8, %rsp
	pop %rdi
	movq %rax, %rdi
	call print_int
	pop %rdi
	push %rdi
	movslq k+0(%rip), %rax
	push %rcx
	shr $3, %rax
	mov $7, %rcx
	and %rcx, %rax
	pop %rcx
	movslq %eax, %rax
	movq %rax, %rdi
	call print_int
	pop %rdi
	push %rdi
	movq l+0(%rip), %rax
	movslq 0(%rax), %rax
	movslq %eax, %rax
	movq %rax, %rdi
	call print_int
	pop %rdi
	leave
	ret
