.set noreorder
.set noat
.section .text.bda_start,"ax",@progbits
.globl bda_start
.ent bda_start
bda_start:
    addiu $sp,$sp,-32
    sw $ra,28($sp)
    lui $t0,%hi(__bss_start)
    addiu $t0,$t0,%lo(__bss_start)
    lui $t1,%hi(__bss_end)
    addiu $t1,$t1,%lo(__bss_end)
1:
    beq $t0,$t1,2f
    nop
    sw $zero,0($t0)
    b 1b
    addiu $t0,$t0,4
2:
    jal scp_runtime_init
    nop
    jal bda_main
    nop
    sw $v0,24($sp)
    jal scp_runtime_fini
    nop
    lw $v0,24($sp)
    lw $ra,28($sp)
    jr $ra
    addiu $sp,$sp,32
.end bda_start
.text
.globl scp_on_stack
.ent scp_on_stack
scp_on_stack:
    addiu $sp,$sp,-24
    sw $ra,20($sp)
    sw $s0,16($sp)
    move $s0,$sp
    move $sp,$a0
    addiu $sp,$sp,-32
    jalr $a1
    nop
    move $sp,$s0
    lw $s0,16($sp)
    lw $ra,20($sp)
    jr $ra
    addiu $sp,$sp,24
.end scp_on_stack
.globl scp_setjmp
.ent scp_setjmp
scp_setjmp:
    sw $s0,0($a0)
    sw $s1,4($a0)
    sw $s2,8($a0)
    sw $s3,12($a0)
    sw $s4,16($a0)
    sw $s5,20($a0)
    sw $s6,24($a0)
    sw $s7,28($a0)
    sw $sp,32($a0)
    sw $fp,36($a0)
    sw $ra,40($a0)
    sw $gp,44($a0)
    jr $ra
    move $v0,$zero
.end scp_setjmp
.globl scp_longjmp
.ent scp_longjmp
scp_longjmp:
    lw $s0,0($a0)
    lw $s1,4($a0)
    lw $s2,8($a0)
    lw $s3,12($a0)
    lw $s4,16($a0)
    lw $s5,20($a0)
    lw $s6,24($a0)
    lw $s7,28($a0)
    lw $sp,32($a0)
    lw $fp,36($a0)
    lw $ra,40($a0)
    lw $gp,44($a0)
    sltiu $v0,$a1,1
    jr $ra
    addu $v0,$a1,$v0
.end scp_longjmp
