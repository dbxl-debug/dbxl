# Hand-written equivalent of rich.c with XCOFF stabs (see test.s for conventions).
# Expected exit status: area = 10*10 = 100, tag = 'b' = 98 -> 198.
#
# Type numbers:
#   1 struct point / point_t      2 enum color         3 char[12]
#   4 point_t *                   5 struct shape       6 point_t[4]
#   7 char *                      8 int[5]             9 struct shape *
# Predefined: -1 int, -2 char, -3 short, -4 long, -5 unsigned char,
#             -7 unsigned short, -12 float, -13 double.
#
# struct shape layout (40 bytes): name 0, color 12, origin 16, corners 24,
#   ncorners 28, scale 32.

	.file	"rich.c"
	.toc
	.csect	.text[PR]
	.stabx	"point:T1=s8x:-1,0,32;y:-1,32,32;;",0,140,0
	.stabx	"point_t:t1",0,140,0
	.stabx	"color:T2=eRED:0,GREEN:1,BLUE:2,;",0,140,0
	.stabx	"shape:T5=s40name:3=ar-1;0;11;-2,0,96;color:2,96,32;origin:1,128,64;corners:4=*1,192,32;ncorners:-7,224,16;scale:-13,256,64;;",0,140,0

# ---------------------------------------------------------------- area
# frame 80: w 56, h 60, c 64; param s at 80+24 = 104
	.align	2
	.globl	area
	.globl	.area
	.csect	area[DS]
area:
	.long	.area, TOC[tc0], 0
	.csect	.text[PR]
.area:
	.function .area,.area,16,044,FE..area-.area
	.stabx	"area:F-1",.area,142,0
	.bf	27
	.stabx	"s:p9=*5",104,130,0
	.stabx	"w:-1",56,129,0
	.stabx	"h:-1",60,129,0
	.stabx	"c:4",64,129,0
	.line	1
	stwu	1,-80(1)
	stw	3,104(1)
	.line	6		# 32: c = s->corners;
	lwz	9,104(1)
	lwz	0,24(9)
	stw	0,64(1)
	.line	7		# 33: w = c[2].x - c[0].x;
	lwz	9,64(1)
	lwz	0,16(9)
	lwz	11,0(9)
	sf	0,11,0
	stw	0,56(1)
	.line	8		# 34: h = c[2].y - c[0].y;
	lwz	9,64(1)
	lwz	0,20(9)
	lwz	11,4(9)
	sf	0,11,0
	stw	0,60(1)
	.line	9		# 35: return w * h;
	lwz	0,56(1)
	lwz	11,60(1)
	mullw	3,0,11
	.line	10		# 36: }
	addi	1,1,80
	blr
LT..area:
	.long	0
	.byte	0,0,32,64,128,0,1,1
	.long	0
	.long	LT..area-.area
	.short	4
	.byte	"area"
	.align	2
	.ef	36
FE..area:

# ---------------------------------------------------------------- main
# frame 80: sp 56, a 60, tag 64
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
	.bf	39
	.stabx	"sp:9",56,129,0
	.stabx	"a:-1",60,129,0
	.stabx	"tag:-2",64,129,0
	.line	1
	mflr	0
	stw	0,8(1)
	stwu	1,-80(1)
	.line	6		# 44: sp = &box;
	lwz	9,LC..box(2)
	stw	9,56(1)
	.line	7		# 45: tag = sp->name[0];
	lwz	9,56(1)
	lbz	0,0(9)
	stb	0,64(1)
	.line	8		# 46: a = area(sp);
	lwz	3,56(1)
	bl	.area
	nop
	stw	3,60(1)
	.line	9		# 47: counts[0] = a;
	lwz	9,LC..counts(2)
	lwz	0,60(1)
	stw	0,0(9)
	.line	10		# 48: return a + tag;
	lwz	0,60(1)
	lbz	11,64(1)
	add	3,0,11
	.line	11		# 49: }
	addi	1,1,80
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
	.ef	49
FE..main:

# ---------------------------------------------------------------- data
# One csect per global, as xlc does. xldb mis-resolves globals that are
# plain labels inside a shared .data[RW] csect.
	.toc
LC..box:
	.tc	box[TC],box[RW]
LC..counts:
	.tc	counts[TC],counts[RW]

	.globl	square[RW]
	.csect	square[RW],3
	.long	0,0,10,0,10,10,0,10
	.stabx	"square:G6=ar-1;0;3;1",0,128,0

	.globl	box[RW]
	.csect	box[RW],3
	.byte	"box"
	.byte	0,0,0,0,0,0,0,0,0
	.long	1
	.long	5,5
	.long	square[RW]
	.short	4
	.short	0
	.long	0x3FF80000,0
	.stabx	"box:G5",0,128,0

	.csect	STR..hello[RO],2
	.byte	"hello, world"
	.byte	0

	.globl	greeting[RW]
	.csect	greeting[RW],2
	.long	STR..hello[RO]
	.stabx	"greeting:G7=*-2",0,128,0

	.globl	flags[RW]
	.csect	flags[RW],0
	.byte	0x81
	.stabx	"flags:G-5",0,128,0

	.globl	delta[RW]
	.csect	delta[RW],1
	.short	-3
	.stabx	"delta:G-3",0,128,0

	.globl	big[RW]
	.csect	big[RW],2
	.long	123456789
	.stabx	"big:G-4",0,128,0

	.globl	ratio[RW]
	.csect	ratio[RW],2
	.long	0x3E800000
	.stabx	"ratio:G-12",0,128,0

	.globl	counts[RW]
	.csect	counts[RW],2
	.long	1,2,3,4,5
	.stabx	"counts:G8=ar-1;0;4;-1",0,128,0
