	.text
.globl main
main:
	nop
	push %rbp
	movq %rsp, %rbp
	sub $24, %rsp
	movslq -24(%rbp), %rax
	push %rax
	movslq -16(%rbp), %rax
	pop %rcx
	cmp %eax, %ecx
	setl %al
	movzb %al, %eax
	test %rax, %rax
	je .L0
	movq $4, %rax
	jmp .L1
	.L0:
	movq $5, %rax
	.L1:
	mov %eax, -8(%rbp)
	push %rdi
	sub $8, %rsp
	movslq -8(%rbp), %rax
	movq %rax, %rdi
	mov $0, %eax
	call print_int
	add $8, %rsp
	pop %rdi
	leave
	ret
