	lw	0	1	five	r1 = 5
	lw	0	2	neg1	r2 = -1
start	beq	0	1	end	if r1 == 0, end. comps look like: 5-0, 4-0, 3-0, 2-0, 1-0, 0-0
	add	1	2	1	r1 -= 1 (this should happen 5x)
	beq	0	0	start	go back to start
end	halt
five	.fill	5
neg1	.fill	-1
