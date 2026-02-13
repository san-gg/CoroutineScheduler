# Coroutine Scheduler
A Coroutine Scheduler is a user-space scheduling system for managing coroutines—lightweight subroutines that can be paused and resumed cooperatively without relying on OS-level threads.</br>
So far, it includes:<br>
+ Sleep system call.
+ Channel with Buffered data.

`* There is No dynamic stack size (cannot grow or shrink at runtime)`

👉 Read the full blog here: [Building a Go-style Coroutine Scheduler in C++](https://medium.com/@sanketputhane/building-a-go-style-coroutine-scheduler-in-c-e382e02e494c)
