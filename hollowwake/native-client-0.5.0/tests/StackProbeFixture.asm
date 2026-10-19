; Original x64 ABI fixture. The supplied probe must actually grow the stack:
; writing 64 KiB below a fresh thread's guard without probing faults immediately.
PUBLIC HWOwnedStackProbeFixture
.code
HWOwnedStackProbeFixture PROC FRAME
 push rbp
 .pushreg rbp
 sub rsp,20h
 .allocstack 20h
 lea rbp,[rsp+20h]
 .setframe rbp,20h
 .endprolog
 mov r11,rcx
 mov rax,10000h
 call r11
 cmp rax,10000h
 jne badProbe
 sub rsp,rax
 mov r8,rsp
 xor edx,edx
writePages:
 mov qword ptr [r8],rdx
 add r8,1000h
 inc edx
 cmp edx,16
 jb writePages
 mov r8,rsp
 xor edx,edx
readPages:
 cmp qword ptr [r8],rdx
 jne badProbe
 add r8,1000h
 inc edx
 cmp edx,16
 jb readPages
 mov eax,621
 jmp finish
badProbe:
 mov eax,-1
finish:
 lea rsp,[rbp-20h]
 add rsp,20h
 pop rbp
 ret
HWOwnedStackProbeFixture ENDP
END
