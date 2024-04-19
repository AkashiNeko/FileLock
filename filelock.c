#include <linux/dcache.h>
#include <linux/delay.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/syscalls.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/version.h>
#include <linux/time.h>
#include <linux/jiffies.h>

#define CR0_WP 0x00010000
#define MAX_LINES 256
typedef unsigned long UL;

// 参数
static char* files = "";
static char* procs = "";
module_param(files, charp, S_IRUGO);
module_param(procs, charp, S_IRUGO);

// 全局变量
void** syscall_table;
asmlinkage long (*orig_sys_open)(const char __user*, int, umode_t);
const char* FILE_LIST[MAX_LINES] = {};
const char* PROC_LIST[MAX_LINES] = {};
size_t FILE_LIST_SIZE = 0;
size_t PROC_LIST_SIZE = 0;

// 工具函数
int split_path(char* paths, const char* list[MAX_LINES]) {
    if (*paths == '\0') return 0;
    const char* start = paths;
    size_t cur = 0;
    for (char *p = paths; *p; ++p) {
        if (*p == ':') {
            if (start != p) list[cur++] = start;
            start = p + 1;
            *p = '\0';
        }
    }
    if (*start) list[cur++] = start;
    return cur;
}

int in_file_list(const char* file_path) {
    for (size_t i = 0; i < FILE_LIST_SIZE; ++i) {
        int found = 1;
        for (size_t j = 0; FILE_LIST[i][j]; ++j) {
            if (file_path[j] != FILE_LIST[i][j]) {
                found = 0;
                break;
            }
        }
        if (found) return 1;
    }
    return 0;
}

int in_process_list(const char* proc_path) {
    const size_t Length = sizeof(PROC_LIST) / sizeof(char*);
    for (size_t i = 0; i < PROC_LIST_SIZE; ++i)
        if (!strcmp(PROC_LIST[i], proc_path)) return 1;
    return 0;
}

void** find_sys_call_table(void) {
    for (UL ptr = (UL)sys_close; ptr < (UL)&loops_per_jiffy; ptr += sizeof(void*))
        if (((UL*)ptr)[__NR_close] == (UL)sys_close)
            return (void**)ptr;
    return NULL;
}

char* __add_vm0(char *buf, char *end, unsigned int num, int width) {
    char str[11];
    int len;

    snprintf(str, sizeof(str), "%0*u", width, num);
    len = min_t(int, strlen(str), end - buf - 1); // -1 为了空字符
    memcpy(buf, str, len);
    buf += len;

    return buf;
}


char* get_time(char *buf, size_t buf_len) {
    static int month_days[] = {
        31, 28, 31, 30, 31, 30,
        31, 31, 30, 31, 30, 31, 28
    };
    struct timespec now;
    time64_t t;
    unsigned long days, seconds;
    int years, months, hours, minutes, seconds_part;
    int leap_years;

    // 获取当前时间的秒数
    t = get_seconds();

    // 计算自1970年1月1日以来的天数和剩余秒数
    days = t / (24 * 60 * 60);
    seconds = t % (24 * 60 * 60);

    // 将天数转换为年月日
    // 注意：这个计算假设格里高利历，并且不考虑闰秒
    years = days / 365; // 忽略闰年
    leap_years = years / 4; // 完整的闰年数
    days -= (years * 365) + (leap_years);

    // 计算月份和月内天数
    for (months = 0; month_days[months] < days; months++);

    month_days[months - 1] = 0; // 重置为当月第一天
    days -= month_days[months - 1]; // 计算当月的天数

    // 将剩余的秒数转换为小时、分钟和秒
    hours = seconds / (60 * 60);
    seconds %= 60 * 60;
    minutes = seconds / 60;
    seconds_part = seconds % 60;

    // 格式化时间为 "年-月-日 时：分：秒"
    snprintf(buf, buf_len, "%04d-%02d-%02d %02d:%02d:%02d",
             years + 1970, months + 1, days + 1, hours, minutes, seconds_part);

    return buf;
}

void print_log(const char* process, char* file, int access) {
    char* time[32] = {};
    printk(KERN_INFO "FileLock: [%s] %s -> %s [%s]\n", get_time(time, 31),
        process, file, access ? "argeed" : "rejected");
}

// 劫持 open 的自定义函数
asmlinkage long hijacked_open(const char __user* filename, int flags, umode_t mode) {
    // 打开文件，获取 fd
    int fd = orig_sys_open(filename, flags, mode);
    if (fd < 0) return fd;

    // 获取 file struct 和文件的绝对路径
    struct file* file = fget(fd);
    char filepath[256] = {0};
    char* apath = d_path(&file->f_path, filepath, sizeof filepath);
    fput(file);

    // 目录是否受保护？
    if (in_file_list(apath)) {
        const char* arg0 = (const char*)current->mm->arg_start;
        if (0 == in_process_list(arg0)) {
            print_log(arg0, apath, 0);
            sys_close(fd);
            return -EACCES; // 返回权限不够错误码
        } else {
            print_log(arg0, apath, 1);
        }
    }
    return fd;
}

// init
int init_module(void) {
    // 解析文件列表
    FILE_LIST_SIZE = split_path(files, FILE_LIST);

    // 解析进程列表
    PROC_LIST_SIZE = split_path(procs, PROC_LIST);

    // 获取系统调用函数表
    syscall_table = find_sys_call_table();
    if (!syscall_table) {
        printk(KERN_ERR "FileLock: cannot find the sys_call_table! load failed..\n");
        return -1;
    }

    // 修改 CR0 寄存器的 WP 位，解锁 syscall_table
    UL cr0 = read_cr0();
    write_cr0(cr0 & ~CR0_WP);

    // 保存 open 系统调用的函数指针，并让它指向自定义的函数
    orig_sys_open = syscall_table[__NR_open];
    syscall_table[__NR_open] = hijacked_open;

    // 重新锁定 syscall_table
    write_cr0(cr0);

    printk(KERN_NOTICE "[FileLock]: Module load done!\n");
    return 0;
}

// exit
void cleanup_module(void) {
    UL cr0 = read_cr0();
    write_cr0(cr0 & ~CR0_WP);
    syscall_table[__NR_open] = orig_sys_open;
    write_cr0(cr0);
    printk(KERN_NOTICE "[FileLock]: Module exited.\n");
}

MODULE_LICENSE("MIT");
MODULE_AUTHOR("AkashiNeko");
