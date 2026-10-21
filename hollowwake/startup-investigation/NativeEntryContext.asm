; Owned Microsoft x64 entry handoff. No guest instruction or OS metadata edits.
; RCX initially contains the preserved entry address, as a normal C argument.
; The observed native process entry has RAX=entry and RCX=the real process PEB.
PUBLIC RunPreservedEntry
.code
RunPreservedEntry PROC
    mov rax, rcx
    mov rcx, gs:[60h]
    jmp rax
RunPreservedEntry ENDP
END
