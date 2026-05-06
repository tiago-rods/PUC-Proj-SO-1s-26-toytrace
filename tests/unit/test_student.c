#include "student_api.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/syscall.h>

static void test_pair_write_entry_exit(void)
{
    struct syscall_pairer pairer = {0};
    struct syscall_event out;
    struct syscall_event entry = {
        .pid = 123,
        .entering = 1,
        .syscall_no = SYS_write,
        .args = {1, 0xabc, 6, 0, 0, 0},
    };
    struct syscall_event exit_ev = {
        .pid = 123,
        .entering = 0,
        .syscall_no = SYS_write,
        .ret = 6,
    };

    assert(student_pair_syscall(&pairer, &entry, &out) == 0);
    assert(student_pair_syscall(&pairer, &exit_ev, &out) == 1);
    assert(out.entering == 0);
    assert(out.pid == 123);
    assert(out.syscall_no == SYS_write);
    assert(out.args[0] == 1);
    assert(out.args[1] == 0xabc);
    assert(out.args[2] == 6);
    assert(out.ret == 6);
}

{
static void test_formatter_generic_mentions_return(void)
    struct syscall_event ev = {
        .pid = 123,
        .entering = 0,
        .syscall_no = SYS_close,
        .ret = 0,
        .args = {3, 0, 0, 0, 0, 0},
    };
    char line[256];

    student_format_event(&ev, line, sizeof(line));
    assert(strstr(line, "close") != NULL);
    assert(strstr(line, "= 0") != NULL);
}

int main(void)
{
    test_pair_write_entry_exit();
    test_formatter_generic_mentions_return();
    puts("testes unitarios concluidos");
    return 0;
}
