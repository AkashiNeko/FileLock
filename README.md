# 文件访问控制

1. 分别有一份包含路径或文件列表的黑名单配置文件，和包含进程绝对路径白名单的配置文件。
2. 加载该模块后，只有白名单列表中的进程可以访问黑名单列表中的文件，其他进程都无法访问黑名单中的文件（包括root权限的进程）。
3. 模块日志需要使用 `dmesg` 命令查看。
