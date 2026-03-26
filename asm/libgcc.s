.include "asm/macros.inc"
.syntax unified
.text

@ libgcc runtime (thunks, division, modulo)
	thumb_func_start FUN_08051868
FUN_08051868: @ 08051868
	bx r0
	nop
	thumb_func_start FUN_0805186c
FUN_0805186c: @ 0805186C
	bx r1
	nop
	thumb_func_start FUN_08051870
FUN_08051870: @ 08051870
	bx r2
	nop
	bx r3
	nop
	bx r4
	nop
	bx r5
	nop
	bx r6
	nop
	bx r7
	nop
	bx r8
	nop
	bx r9
	nop
	bx r10
	nop
	bx r11
	nop
	bx r12
	nop
	bx sp
	nop
	bx lr
	nop
	thumb_func_start FUN_080518a4
FUN_080518a4: @ 080518A4
	cmp r1, #0x00
	beq _0805192C
	push {r4}
	adds r4, r0, #0x0
	eors r4, r1
	mov r12, r4
	movs r3, #0x01
	movs r2, #0x00
	cmp r1, #0x00
	bpl _080518BA
	negs r1, r1
_080518BA:
	cmp r0, #0x00
	bpl _080518C0
	negs r0, r0
_080518C0:
	cmp r0, r1
	bcc _0805191E
	movs r4, #0x01
	lsls r4, r4, #0x1C
_080518C8:
	cmp r1, r4
	bcs _080518D6
	cmp r1, r0
	bcs _080518D6
	lsls r1, r1, #0x04
	lsls r3, r3, #0x04
	b _080518C8
_080518D6:
	lsls r4, r4, #0x03
_080518D8:
	cmp r1, r4
	bcs _080518E6
	cmp r1, r0
	bcs _080518E6
	lsls r1, r1, #0x01
	lsls r3, r3, #0x01
	b _080518D8
_080518E6:
	cmp r0, r1
	bcc _080518EE
	subs r0, r0, r1
	orrs r2, r3
_080518EE:
	lsrs r4, r1, #0x01
	cmp r0, r4
	bcc _080518FA
	subs r0, r0, r4
	lsrs r4, r3, #0x01
	orrs r2, r4
_080518FA:
	lsrs r4, r1, #0x02
	cmp r0, r4
	bcc _08051906
	subs r0, r0, r4
	lsrs r4, r3, #0x02
	orrs r2, r4
_08051906:
	lsrs r4, r1, #0x03
	cmp r0, r4
	bcc _08051912
	subs r0, r0, r4
	lsrs r4, r3, #0x03
	orrs r2, r4
_08051912:
	cmp r0, #0x00
	beq _0805191E
	lsrs r3, r3, #0x04
	beq _0805191E
	lsrs r1, r1, #0x04
	b _080518E6
_0805191E:
	adds r0, r2, #0x0
	mov r4, r12
	cmp r4, #0x00
	bpl _08051928
	negs r0, r0
_08051928:
	pop {r4}
	mov pc, lr
_0805192C:
	push {lr}
	bl FUN_08051938
	movs r0, #0x00
	pop {pc}
	lsls r0, r0, #0x00
	thumb_func_start FUN_08051938
FUN_08051938: @ 08051938
	mov pc, lr
	lsls r0, r0, #0x00
	thumb_func_start FUN_0805193c
FUN_0805193c: @ 0805193C
	movs r3, #0x01
	cmp r1, #0x00
	beq _08051A00
	bpl _08051946
	negs r1, r1
_08051946:
	push {r4}
	push {r0}
	cmp r0, #0x00
	bpl _08051950
	negs r0, r0
_08051950:
	cmp r0, r1
	bcc _080519F4
	movs r4, #0x01
	lsls r4, r4, #0x1C
_08051958:
	cmp r1, r4
	bcs _08051966
	cmp r1, r0
	bcs _08051966
	lsls r1, r1, #0x04
	lsls r3, r3, #0x04
	b _08051958
_08051966:
	lsls r4, r4, #0x03
_08051968:
	cmp r1, r4
	bcs _08051976
	cmp r1, r0
	bcs _08051976
	lsls r1, r1, #0x01
	lsls r3, r3, #0x01
	b _08051968
_08051976:
	movs r2, #0x00
	cmp r0, r1
	bcc _0805197E
	subs r0, r0, r1
_0805197E:
	lsrs r4, r1, #0x01
	cmp r0, r4
	bcc _08051990
	subs r0, r0, r4
	mov r12, r3
	movs r4, #0x01
	rors r3, r4
	orrs r2, r3
	mov r3, r12
_08051990:
	lsrs r4, r1, #0x02
	cmp r0, r4
	bcc _080519A2
	subs r0, r0, r4
	mov r12, r3
	movs r4, #0x02
	rors r3, r4
	orrs r2, r3
	mov r3, r12
_080519A2:
	lsrs r4, r1, #0x03
	cmp r0, r4
	bcc _080519B4
	subs r0, r0, r4
	mov r12, r3
	movs r4, #0x03
	rors r3, r4
	orrs r2, r3
	mov r3, r12
_080519B4:
	mov r12, r3
	cmp r0, #0x00
	beq _080519C2
	lsrs r3, r3, #0x04
	beq _080519C2
	lsrs r1, r1, #0x04
	b _08051976
_080519C2:
	movs r4, #0x0E
	lsls r4, r4, #0x1C
	ands r2, r4
	beq _080519F4
	mov r3, r12
	movs r4, #0x03
	rors r3, r4
	tst r2, r3
	beq _080519D8
	lsrs r4, r1, #0x03
	adds r0, r0, r4
_080519D8:
	mov r3, r12
	movs r4, #0x02
	rors r3, r4
	tst r2, r3
	beq _080519E6
	lsrs r4, r1, #0x02
	adds r0, r0, r4
_080519E6:
	mov r3, r12
	movs r4, #0x01
	rors r3, r4
	tst r2, r3
	beq _080519F4
	lsrs r4, r1, #0x01
	adds r0, r0, r4
_080519F4:
	pop {r4}
	cmp r4, #0x00
	bpl _080519FC
	negs r0, r0
_080519FC:
	pop {r4}
	mov pc, lr
_08051A00:
	push {lr}
	bl FUN_08051938
	movs r0, #0x00
	pop {pc}
	lsls r0, r0, #0x00
	thumb_func_start FUN_08051a0c
FUN_08051a0c: @ 08051A0C
	cmp r1, #0x00
	beq _08051A7A
	movs r3, #0x01
	movs r2, #0x00
	push {r4}
	cmp r0, r1
	bcc _08051A74
	movs r4, #0x01
	lsls r4, r4, #0x1C
_08051A1E:
	cmp r1, r4
	bcs _08051A2C
	cmp r1, r0
	bcs _08051A2C
	lsls r1, r1, #0x04
	lsls r3, r3, #0x04
	b _08051A1E
_08051A2C:
	lsls r4, r4, #0x03
_08051A2E:
	cmp r1, r4
	bcs _08051A3C
	cmp r1, r0
	bcs _08051A3C
	lsls r1, r1, #0x01
	lsls r3, r3, #0x01
	b _08051A2E
_08051A3C:
	cmp r0, r1
	bcc _08051A44
	subs r0, r0, r1
	orrs r2, r3
_08051A44:
	lsrs r4, r1, #0x01
	cmp r0, r4
	bcc _08051A50
	subs r0, r0, r4
	lsrs r4, r3, #0x01
	orrs r2, r4
_08051A50:
	lsrs r4, r1, #0x02
	cmp r0, r4
	bcc _08051A5C
	subs r0, r0, r4
	lsrs r4, r3, #0x02
	orrs r2, r4
_08051A5C:
	lsrs r4, r1, #0x03
	cmp r0, r4
	bcc _08051A68
	subs r0, r0, r4
	lsrs r4, r3, #0x03
	orrs r2, r4
_08051A68:
	cmp r0, #0x00
	beq _08051A74
	lsrs r3, r3, #0x04
	beq _08051A74
	lsrs r1, r1, #0x04
	b _08051A3C
_08051A74:
	adds r0, r2, #0x0
	pop {r4}
	mov pc, lr
_08051A7A:
	push {lr}
	bl FUN_08051938
	movs r0, #0x00
	pop {pc}
	thumb_func_start FUN_08051a84
FUN_08051a84: @ 08051A84
	cmp r1, #0x00
	beq _08051B3A
	movs r3, #0x01
	cmp r0, r1
	bcs _08051A90
	mov pc, lr
_08051A90:
	push {r4}
	movs r4, #0x01
	lsls r4, r4, #0x1C
_08051A96:
	cmp r1, r4
	bcs _08051AA4
	cmp r1, r0
	bcs _08051AA4
	lsls r1, r1, #0x04
	lsls r3, r3, #0x04
	b _08051A96
_08051AA4:
	lsls r4, r4, #0x03
_08051AA6:
	cmp r1, r4
	bcs _08051AB4
	cmp r1, r0
	bcs _08051AB4
	lsls r1, r1, #0x01
	lsls r3, r3, #0x01
	b _08051AA6
_08051AB4:
	movs r2, #0x00
	cmp r0, r1
	bcc _08051ABC
	subs r0, r0, r1
_08051ABC:
	lsrs r4, r1, #0x01
	cmp r0, r4
	bcc _08051ACE
	subs r0, r0, r4
	mov r12, r3
	movs r4, #0x01
	rors r3, r4
	orrs r2, r3
	mov r3, r12
_08051ACE:
	lsrs r4, r1, #0x02
	cmp r0, r4
	bcc _08051AE0
	subs r0, r0, r4
	mov r12, r3
	movs r4, #0x02
	rors r3, r4
	orrs r2, r3
	mov r3, r12
_08051AE0:
	lsrs r4, r1, #0x03
	cmp r0, r4
	bcc _08051AF2
	subs r0, r0, r4
	mov r12, r3
	movs r4, #0x03
	rors r3, r4
	orrs r2, r3
	mov r3, r12
_08051AF2:
	mov r12, r3
	cmp r0, #0x00
	beq _08051B00
	lsrs r3, r3, #0x04
	beq _08051B00
	lsrs r1, r1, #0x04
	b _08051AB4
_08051B00:
	movs r4, #0x0E
	lsls r4, r4, #0x1C
	ands r2, r4
	bne _08051B0C
	pop {r4}
	mov pc, lr
_08051B0C:
	mov r3, r12
	movs r4, #0x03
	rors r3, r4
	tst r2, r3
	beq _08051B1A
	lsrs r4, r1, #0x03
	adds r0, r0, r4
_08051B1A:
	mov r3, r12
	movs r4, #0x02
	rors r3, r4
	tst r2, r3
	beq _08051B28
	lsrs r4, r1, #0x02
	adds r0, r0, r4
_08051B28:
	mov r3, r12
	movs r4, #0x01
	rors r3, r4
	tst r2, r3
	beq _08051B36
	lsrs r4, r1, #0x01
	adds r0, r0, r4
_08051B36:
	pop {r4}
	mov pc, lr
_08051B3A:
	push {lr}
	bl FUN_08051938
	movs r0, #0x00
	pop {pc}
	thumb_func_start MemCopy
MemCopy: @ 08051B44
	push {r4, r5, lr}
	adds r5, r0, #0x0
	adds r4, r5, #0x0
	adds r3, r1, #0x0
	cmp r2, #0x0F
	bls _08051B84
	adds r0, r3, #0x0
	orrs r0, r5
	movs r1, #0x03
	ands r0, r1
	cmp r0, #0x00
	bne _08051B84
	adds r1, r5, #0x0
_08051B5E:
	ldm r3!, {r0}
	stm r1!, {r0}
	ldm r3!, {r0}
	stm r1!, {r0}
	ldm r3!, {r0}
	stm r1!, {r0}
	ldm r3!, {r0}
	stm r1!, {r0}
	subs r2, #0x10
	cmp r2, #0x0F
	bhi _08051B5E
	cmp r2, #0x03
	bls _08051B82
_08051B78:
	ldm r3!, {r0}
	stm r1!, {r0}
	subs r2, #0x04
	cmp r2, #0x03
	bhi _08051B78
_08051B82:
	adds r4, r1, #0x0
_08051B84:
	subs r2, #0x01
	movs r0, #0x01
	negs r0, r0
	cmp r2, r0
	beq _08051B9E
	adds r1, r0, #0x0
_08051B90:
	ldrb r0, [r3, #0x00]
	strb r0, [r4, #0x00]
	adds r3, #0x01
	adds r4, #0x01
	subs r2, #0x01
	cmp r2, r1
	bne _08051B90
_08051B9E:
	adds r0, r5, #0x0
	pop {r4, r5, pc}
	lsls r0, r0, #0x00
	thumb_func_start thunk_FUN_080001d0
thunk_FUN_080001d0: @ 08051BA4
	bx pc
	nop
	.2byte 0xB988
	.2byte 0xEAFE
	thumb_func_start thunk_FUN_080001e0
thunk_FUN_080001e0: @ 08051BAC
	bx pc
	nop
	.2byte 0xB98A
	.2byte 0xEAFE
	thumb_func_start thunk_FUN_08000278
thunk_FUN_08000278: @ 08051BB4
	bx pc
	nop
	.2byte 0xB9AE
	.2byte 0xEAFE
	thumb_func_start thunk_FUN_0800020c
thunk_FUN_0800020c: @ 08051BBC
	bx pc
	nop
	.2byte 0xB991
	.2byte 0xEAFE
	thumb_func_start thunk_FUN_080002a0
thunk_FUN_080002a0: @ 08051BC4
	bx pc
	nop
	.2byte 0xB9B4
	.2byte 0xEAFE
	thumb_func_start thunk_FUN_080002d0
thunk_FUN_080002d0: @ 08051BCC
	bx pc
	nop
	.2byte 0xB9BE
	.2byte 0xEAFE
	lsls r0, r0, #0x00
	lsls r0, r0, #0x02
	strh r0, [r0, #0x00]
	lsls r0, r0, #0x00
	lsls r0, r0, #0x02
	strh r0, [r0, #0x00]
	lsls r0, r0, #0x00
	lsls r0, r0, #0x02
	strh r0, [r0, #0x00]
	lsls r0, r0, #0x00
	lsls r0, r0, #0x02
	strh r0, [r0, #0x00]
	lsls r0, r0, #0x00
	lsls r0, r0, #0x02
	strh r0, [r0, #0x00]
	lsls r0, r0, #0x00
	lsls r0, r0, #0x02
	strh r0, [r0, #0x00]
	lsls r0, r0, #0x00
	lsls r0, r0, #0x02
	strh r0, [r0, #0x00]
	lsls r0, r0, #0x00
	lsls r0, r0, #0x02
	strh r0, [r0, #0x00]
	lsls r0, r0, #0x00
	lsls r0, r0, #0x02
	strh r0, [r0, #0x00]
	lsls r0, r0, #0x00
	lsls r0, r0, #0x02
	strh r0, [r0, #0x00]
	lsls r0, r0, #0x00
	lsls r0, r0, #0x02
	strh r0, [r0, #0x00]
	lsls r0, r0, #0x00
	lsls r0, r0, #0x02
	strh r0, [r0, #0x00]
	lsls r0, r0, #0x00
	lsls r0, r0, #0x02
	strh r0, [r0, #0x00]
	lsls r0, r0, #0x00
	lsls r0, r0, #0x02
	strh r0, [r0, #0x00]
	lsls r0, r0, #0x00
	lsls r0, r0, #0x02
	strh r0, [r0, #0x00]
	lsls r0, r0, #0x00
	lsls r0, r0, #0x02
	strh r0, [r0, #0x00]
	lsls r0, r0, #0x00
	lsls r0, r0, #0x02
	strh r0, [r0, #0x00]
	lsls r0, r0, #0x00
	lsls r0, r0, #0x02
	strh r0, [r0, #0x00]
	lsls r0, r0, #0x00
	lsls r0, r0, #0x02
	strh r0, [r0, #0x00]
	lsls r0, r0, #0x00
	lsls r0, r0, #0x02
	strh r0, [r0, #0x00]
	lsls r0, r0, #0x00
	lsls r0, r0, #0x02
	strh r0, [r0, #0x00]
	lsls r0, r0, #0x00
	lsls r0, r0, #0x02
	strh r0, [r0, #0x00]
	lsls r0, r0, #0x00
	lsls r0, r0, #0x02
	strh r0, [r0, #0x00]
	lsls r0, r0, #0x00
	lsls r0, r0, #0x02
	strh r0, [r0, #0x00]
	lsls r0, r0, #0x00
	lsls r0, r0, #0x02
	strh r0, [r0, #0x00]
	lsls r0, r0, #0x00
	lsls r0, r0, #0x02
	strh r0, [r0, #0x00]
	lsls r0, r0, #0x00
	lsls r0, r0, #0x02
	strh r0, [r0, #0x00]
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r4, r4, #0x06
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r4, r5, #0x04
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r4, r5, #0x04
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r4, r5, #0x04
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r6, r1, #0x04
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r0, r6, #0x03
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r2, r2, #0x03
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r4, r7, #0x00
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r4, r3, #0x08
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r4, r5, #0x04
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r2, r0, #0x07
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r6, r6, #0x04
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r0, r2, #0x06
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r0, r6, #0x03
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r2, r0, #0x07
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r4, r0, #0x01
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r4, r5, #0x04
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r6, r1, #0x04
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r0, r7, #0x01
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r6, r0, #0x06
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r3, r4, #0x05
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r0, r6, #0x03
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r2, r2, #0x03
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r4, r0, #0x01
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r4, r5, #0x04
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r4, r5, #0x04
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r0, r1, #0x03
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r6, r0, #0x06
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r4, r5, #0x04
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r0, r6, #0x03
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r4, r5, #0x04
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r4, r0, #0x01
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r2, r6, #0x04
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r4, r2, #0x04
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r5, r4, #0x02
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r6, r0, #0x06
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r6, r3, #0x06
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r0, r6, #0x03
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r2, r6, #0x03
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r0, r1, #0x01
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r4, r6, #0x02
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r2, r0, #0x07
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r0, r2, #0x06
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r0, r2, #0x06
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r0, r2, #0x06
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r0, r2, #0x06
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r0, r2, #0x06
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r0, r1, #0x01
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r4, r7, #0x00
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r4, r7, #0x00
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r4, r7, #0x00
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r4, r6, #0x02
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r0, r7, #0x01
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r4, r6, #0x02
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r4, r4, #0x01
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r0, r5, #0x00
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r4, r4, #0x01
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r0, r4, #0x02
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r2, r2, #0x03
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r4, r7, #0x05
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r4, r5, #0x04
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r4, r6, #0x02
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r0, r1, #0x03
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r0, r6, #0x00
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r0, r2, #0x01
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r4, r7, #0x00
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r4, r1, #0x02
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r4, r6, #0x02
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r0, r2, #0x01
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r4, r6, #0x02
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r4, r4, #0x01
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r0, r6, #0x00
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r4, r5, #0x04
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r0, r1, #0x03
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r0, r1, #0x03
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r4, r6, #0x02
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r0, r1, #0x03
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r4, r6, #0x02
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r0, r1, #0x03
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r0, r6, #0x00
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r4, r7, #0x00
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r0, r2, #0x01
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r6, r3, #0x02
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r4, r6, #0x02
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r4, r0, #0x01
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r4, r6, #0x02
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r0, r7, #0x01
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r0, r5, #0x00
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r4, r6, #0x02
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r6, r0, #0x01
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r0, r2, #0x06
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r0, r2, #0x06
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r0, r2, #0x06
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r0, r2, #0x06
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r0, r2, #0x06
	lsls r0, r4, #0x00
	lsls r0, r4, #0x00
	lsls r0, r5, #0x00
	lsls r2, r2, #0x02
	lsls r1, r4, #0x01
	lsls r7, r7, #0x03
	lsls r0, r2, #0x06
	lsls r2, r4, #0x07
	lsls r3, r0, #0x03
	lsls r0, r2, #0x06
	lsls r2, r4, #0x06
	lsls r3, r0, #0x03
	lsls r6, r5, #0x06
	lsls r5, r0, #0x02
	lsls r3, r0, #0x03
	lsls r0, r2, #0x06
	lsls r5, r0, #0x02
	lsls r7, r6, #0x03
	lsls r2, r2, #0x06
	lsls r5, r4, #0x04
	lsls r2, r3, #0x03
	lsls r0, r2, #0x06
	lsls r2, r4, #0x06
	lsls r5, r2, #0x03
	lsls r2, r2, #0x06
	lsls r2, r3, #0x05
	lsls r2, r3, #0x03
	lsls r5, r3, #0x03
	lsls r1, r7, #0x07
	lsls r4, r6, #0x01
	lsls r2, r2, #0x02
	lsls r7, r5, #0x05
	lsls r7, r5, #0x03
	lsls r0, r4, #0x06
	lsls r7, r4, #0x03
	lsls r7, r3, #0x03
	lsls r0, r1, #0x06
	lsls r3, r1, #0x02
	lsls r1, r7, #0x03
	lsls r0, r1, #0x06
	lsls r1, r3, #0x04
	lsls r1, r7, #0x03
	lsls r0, r1, #0x06
	lsls r1, r3, #0x04
	lsls r6, r2, #0x03
	lsls r0, r1, #0x06
	lsls r7, r2, #0x03
	lsls r1, r7, #0x03
	lsls r0, r1, #0x06
	lsls r7, r2, #0x03
	lsls r3, r0, #0x03
	lsls r4, r0, #0x06
	lsls r0, r3, #0x03
	lsls r7, r3, #0x03
	lsls r0, r4, #0x04
	lsls r1, r0, #0x00
	lsls r1, r2, #0x03
	lsls r2, r2, #0x02
	lsls r7, r4, #0x01
	lsls r0, r4, #0x01
	lsls r1, r4, #0x07
	lsls r3, r1, #0x06
	lsls r0, r2, #0x03
	lsls r1, r4, #0x07
	lsls r1, r0, #0x06
	lsls r1, r2, #0x03
	lsls r4, r6, #0x05
	lsls r6, r6, #0x03
	lsls r1, r1, #0x03
	lsls r1, r4, #0x07
	lsls r3, r1, #0x06
	lsls r7, r6, #0x03
	lsls r4, r6, #0x05
	lsls r6, r4, #0x03
	lsls r1, r1, #0x03
	lsls r1, r4, #0x07
	lsls r1, r0, #0x06
	lsls r6, r1, #0x03
	lsls r4, r3, #0x07
	lsls r1, r0, #0x06
	lsls r0, r2, #0x03
	lsls r4, r3, #0x03
	lsls r6, r2, #0x03
	lsls r3, r4, #0x03
	lsls r2, r2, #0x02
	lsls r6, r7, #0x01
	lsls r2, r1, #0x02
	lsls r0, r0, #0x08
	lsls r5, r3, #0x03
	lsls r3, r6, #0x03
	lsls r0, r0, #0x08
	lsls r5, r3, #0x03
	lsls r3, r6, #0x03
	lsls r0, r0, #0x08
	lsls r2, r6, #0x04
	lsls r4, r4, #0x03
	lsls r5, r3, #0x07
	lsls r7, r4, #0x04
	lsls r7, r6, #0x03
	lsls r0, r0, #0x08
	lsls r5, r5, #0x06
	lsls r4, r4, #0x03
	lsls r5, r3, #0x07
	lsls r3, r7, #0x01
	lsls r6, r1, #0x03
	lsls r5, r3, #0x07
	lsls r7, r4, #0x04
	lsls r4, r4, #0x03
	lsls r5, r6, #0x03
	lsls r1, r0, #0x03
	lsls r0, r6, #0x03
	lsls r2, r2, #0x02
	lsls r7, r2, #0x04
	lsls r4, r0, #0x03
	lsls r4, r3, #0x06
	lsls r0, r0, #0x05
	lsls r2, r6, #0x03
	lsls r4, r3, #0x06
	lsls r5, r3, #0x01
	lsls r2, r6, #0x03
	lsls r7, r1, #0x06
	lsls r6, r7, #0x03
	lsls r2, r5, #0x03
	lsls r4, r2, #0x06
	lsls r6, r7, #0x03
	lsls r1, r5, #0x03
	lsls r7, r1, #0x06
	lsls r2, r1, #0x04
	lsls r7, r1, #0x03
	lsls r4, r2, #0x06
	lsls r3, r0, #0x05
	lsls r6, r3, #0x02
