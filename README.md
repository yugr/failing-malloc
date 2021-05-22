A simple 5-minute checker which simulates OOM failures
by returning `NULL` from `malloc`.

This project is developed for [Lalambda '21 school](https://lalambda.school/en)
and is meant for educational purposes only.

BRANCHES IN THIS REPO MAY BE FORCED CHANGED AT ANY MOMENT.

# Usage

To instrument programs need to either `export LD_PRELOAD=path/to/libfailingmalloc.so`
or `echo path/to/libfailingmalloc.so >> /etc/ld.so.preload`.
