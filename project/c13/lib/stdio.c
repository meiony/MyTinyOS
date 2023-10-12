#include "stdio.h"
#include "../kernel/interrupt.h"
#include "../kernel/global.h"
#include "string.h"
#include "user/syscall.h"
#include "kernel/print.h"

#define va_start(ap, v) ap = (va_list) &v   // 把 ap 指向第一个固定参数 v
                                            // 这里把第一个char*地址赋给 ap 强制转换一下
#define va_arg(ap, t) *((t*) (ap += 4))     // ap 指向下一个参数并返回其值(强制类型转换 得到栈中参数)
#define va_end(ap) ap = NULL                // 清除 ap


/* 将整型转换成字符(integer to ascii), value:待转换的整数, buf_ptr_addr: 缓冲区指针的地址, base: 进制 */
static void itoa(uint32_t value, char** buf_ptr_addr, uint8_t base) {
    uint32_t m = value % base;      // 求模, 最先掉下来的是最低位(求出每一位)
    uint32_t i = value / base;      // 取整
    if(i) {
        // 如果倍数不为 0 则递归调用, 直到没有数位
        // 后面才修改指针是因为高位要写入低地址, 通过递归先在低地址写入高位
        // 在递归中会修改指针指向高地址, 因此在递归返回时低位会对应高地址
        itoa(i, buf_ptr_addr, base);
    }
    if(m < 10) {
        // 如果余数是 0~9, 转换后写入缓冲区并更新缓冲区指针(一级指针)
        // (*buf_ptr_addr) 得到一级指针, (*buf_ptr_addr)++: 一级指针指向下一个可写入位置
        // *((*buf_ptr_addr)++): (*buf_ptr_addr, 为一级指针) 指向的位置的值
        *((*buf_ptr_addr)++) = m + '0';         // 将数字 0~9 转换为字符'0'~'9'
    } else {
        // 否则余数是 A~F, 转换后写入缓冲区并更新缓冲区指针(一级指针)
        *((*buf_ptr_addr)++) = m - 10 + 'A';    // 将数字 A~F 转换为字符'A'~'F'
    }
}


/* 将参数 ap 按照格式 format 输出到字符串 str, 并返回替换后 str 长度 */
uint32_t vsprintf(char* str, const char* format, va_list ap) {
    char* buf_ptr = str;
    const char* index_ptr = format;
    char index_char = *index_ptr;
    int32_t arg_int;
    char* arg_str;

    // 逐个字符操作
    while(index_char) {
        if(index_char != '%') {
            // 如果当前字符不是 %, 在当前缓冲区写入字符, 缓冲区指针指向下一个地址
            *(buf_ptr++) = index_char;
            // index_char 指向下一个 format 的字符
            index_char = *(++index_ptr);
            continue;
        }

        // 如果当前字符是 %
        index_char = *(++index_ptr);    // 得到 % 后面的字符
        switch(index_char) {
            case 's':                            // %s: 字符串   
                arg_str = va_arg(ap, char*);     // ap 指向下一个参数并返回其值
                strcpy(buf_ptr, arg_str);        // 将 ap 指向的字符串拷贝到 buf_ptr 
                buf_ptr += strlen(arg_str);      // 更新 buf_ptr 跨过拷贝的字符串
                index_char = *(++index_ptr);     // 跳过格式字符并更新 index_char
                break;

            case 'c':                            // %c: 字符 char
                *(buf_ptr++) = va_arg(ap, char);
                index_char = *(++index_ptr);
                break;

            case 'd':                            // %d: 10进制数
                arg_int = va_arg(ap, int);       // ap 指向下一个参数并返回其值
                // 若是负数, 将其转为正数后, 在正数前面输出个负号 '-'
                if(arg_int < 0) {
                    arg_int = 0 - arg_int;
                    *buf_ptr++ = '-';
                }
                itoa(arg_int, &buf_ptr, 10);
                index_char = *(++index_ptr);     // 跳过格式字符并更新 index_char
                break;
            
            case 'x':                            // %x: 16进制数
                arg_int = va_arg(ap, int);       // ap 指向下一个参数并返回其值
                itoa(arg_int, &buf_ptr, 16);
                index_char = *(++index_ptr);     // 跳过格式字符并更新 index_char
                break;
        }
    }
    return strlen(str);
}


/* 同 printf 不同的地方就是字符串不是写到终端, 而是写到 buf 中 */
uint32_t sprintf(char* buf, const char* format, ...) {
    va_list args;
    uint32_t retval;        // buf 中字符串的长度
    va_start(args, format);     
    retval = vsprintf(buf, format, args);
    va_end(args);
    return retval;
}


/* 格式化输出字符串 format */
uint32_t printf(const char* format, ...) {
    va_list args;
    va_start(args, format);     // 使 args 指向 format
    char buf[1024] = {0};       // 用于存储拼接后的字符串
    vsprintf(buf, format, args);
    va_end(args);
    return write(buf);			// 在终端输出 buf 中的字符串
}
