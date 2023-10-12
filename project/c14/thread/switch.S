[bits 32]
section .text

; 相当于函数 void switch_to(struct task_struct* cur, struct task_struct* next);
global switch_to
switch_to:
    ; 备份当前线程的环境, 栈中还包含返回地址
    push esi
    push edi
    push ebx
    push ebp

    mov eax, [esp + 20]     ; 得到栈中的参数cur, cur = [esp + 20], 4 * 4 + 4(返回地址) = 20
    mov [eax], esp          ; 保存栈顶指针 esp 到 task_struct 的 self_kstack 字段
                            ; self_kstack 在 task_struct 中的偏移为 0
                            ; 所以直接往 thread 开头处存 4 字节即可


    ;------------------  以上是备份当前线程的环, 下面是恢复下一个线程的环境  ----------------
    mov eax, [esp + 24]     ; 获取栈中参数 next
    mov esp, [eax]          ; pcb 的第一个成员是 self_kstack 成员
                            ; 它用来记录 0 级栈顶指针, 被换上 cpu 时用来恢复 0 级栈
                            ; 0 级栈中保存了进程或线程所有信息, 包括 3 级栈指针

    pop ebp
    pop ebx
    pop edi
    pop esi
    ret                     ; 返回到栈中的返回地址, 即利用 ret 跳转执行流(thread_stack 中的 eip)
                            ; 未由中断进入, 第一次执行时会返回到 kernel_thread
