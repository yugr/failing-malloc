A simple 5-minute checker which simulates OOM failures
by returning `NULL` from `malloc`.

This project is developed for [Lalambda '21 school](https://lalambda.school/en)
and is meant for educational purposes only.

BRANCHES IN THIS REPO MAY BE FORCED CHANGED AT ANY MOMENT.

# Usage

To instrument programs need to either `export LD_PRELOAD=path/to/libfailingmalloc.so`
or `echo path/to/libfailingmalloc.so >> /etc/ld.so.preload`.

Options are supplied through environment variables:
* `FAILING_MALLOC_FAIL_AFTER` - do not try to return `NULL`
  for first `N` calls

To fuzz an app run it in a loop:
```
$ while true; do
  export FAILING_MALLOC_FAIL_AFTER=$((`od -vAn -N1 -tu1 < /dev/urandom` >> 5))
  LD_PRELOAD=path/to/libfailingmalloc.so myapp
  sleep 0.5
done
```
