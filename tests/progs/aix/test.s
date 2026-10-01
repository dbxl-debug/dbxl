# Hand-written equivalent of test.c with XCOFF stabs debug info,
# in the style of gcc's xcoffout / xlc -g output.
#
# Frame layout (both functions, 64-byte frame, r1-based, no r31):
#   0(1) back chain, 8(1) saved LR (caller's frame), 24..55 param save area,
#   56(1) first local, 60(1) second local.
#   Incoming params are stored in the caller's param save area:
#   a at 64+24 = 88(1), b at 64+28 = 92(1).
#
# Line numbers: .bf N is absolute; .line K is relative, K = line - (N - 1).

	.file	"test.c"
	.toc
	.csect	.text[PR]

# ---------------------------------------------------------------- add
	.align	2
	.globl	add
	.globl	.add
	.csect	add[DS]
add:
	.long	.add, TOC[tc0], 0
	.csect	.text[PR]
.add:
	.function .add,.add,16,044,FE..add-.add
	.stabx	"add:F-1",.add,142,0
	.bf	4
	.stabx	"a:p-1",88,130,0
	.stabx	"b:p-1",92,130,0
	.stabx	"sum:-1",56,129,0
	.line	1
	stwu	1,-64(1)
	stw	3,88(1)
	stw	4,92(1)
	.line	4		# line 7: sum = a + b;
	lwz	9,88(1)
	lwz	0,92(1)
	add	0,9,0
	stw	0,56(1)
	.line	5		# line 8: return sum;
	lwz	3,56(1)
	.line	6		# line 9: }
	addi	1,1,64
	blr
LT..add:
	.long	0
	.byte	0,0,32,64,128,0,2,1
	.long	0
	.long	LT..add-.add
	.short	3
	.byte	"add"
	.align	2
	.ef	9
FE..add:

# ---------------------------------------------------------------- main
	.align	2
	.globl	main
	.globl	.main
	.csect	main[DS]
main:
	.long	.main, TOC[tc0], 0
	.csect	.text[PR]
.main:
	.function .main,.main,16,044,FE..main-.main
	.stabx	"main:F-1",.main,142,0
	.bf	12
	.stabx	"i:-1",56,129,0
	.stabx	"total:-1",60,129,0
	.line	1
	mflr	0
	stw	0,8(1)
	stwu	1,-64(1)
	.line	4		# line 14: int total = 0;
	li	0,0
	stw	0,60(1)
	.line	6		# line 16: for (i = 0; ...
	li	0,0
	stw	0,56(1)
	b	L..cond
L..body:
	.line	7		# line 17: total = add(total, i);
	lwz	3,60(1)
	lwz	4,56(1)
	bl	.add
	nop
	stw	3,60(1)
	.line	8		# line 18: counter++;
	lwz	9,LC..counter(2)
	lwz	11,0(9)
	addi	11,11,1
	stw	11,0(9)
	.line	6		# line 16: i++
	lwz	11,56(1)
	addi	11,11,1
	stw	11,56(1)
L..cond:
	.line	6		# line 16: i < 5
	lwz	0,56(1)
	cmpwi	0,0,4
	ble	0,L..body
	.line	10		# line 20: return total;
	lwz	3,60(1)
	.line	11		# line 21: }
	addi	1,1,64
	lwz	0,8(1)
	mtlr	0
	blr
LT..main:
	.long	0
	.byte	0,0,32,65,128,0,0,1
	.long	LT..main-.main
	.short	4
	.byte	"main"
	.align	2
	.ef	21
FE..main:

# ---------------------------------------------------------------- data
	.toc
LC..counter:
	.tc	counter[TC],counter

	.globl	counter
	.csect	.data[RW]
	.align	2
counter:
	.long	0
	.stabx	"counter:G-1",0,128,0
