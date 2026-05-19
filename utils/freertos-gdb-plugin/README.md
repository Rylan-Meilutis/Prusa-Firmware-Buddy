# FreeRTOS plugin for gdb


## What is this
This is a plugin for debugging FreeRTOS data structures in gdb debugger.
It supports listing FreeRTOS tasks and switching gdb context to a chosen
task to inspect its stack — including reconstructing the preempted task
when the dump was taken from inside an ISR.


## How do I use this
Make sure your gdb supports Python.

Start the debug session as usual and load the plugin:
```
source utils/freertos-gdb-plugin/freertos-gdb-plugin.py
```

After the plugin is loaded, gdb starts providing new command `freertos`.
For details on how to use this command, read the help:
```
help freertos
```

Happy hacking!


## Crash dumps taken from inside an ISR (handler mode)

When the crash dump was taken from inside an ISR (handler mode), the running
task's preempted state is reconstructed from the hardware exception frame on
PSP. Run `freertos thread <id-of-running-task>` to view it.

Caveats:
- R4-R11 keep the ISR's values (they're not recoverable from a dump), so
  deep unwinding past the topmost frame may be inaccurate.
- Pass `--fpu` if the preempted task had active FP state at preemption
  (e.g. a task that just ran an FP instruction). We can't autodetect this
  from the dump. Symptom of wrong choice: incorrect SP/LR after the topmost
  frame, garbage backtrace below it. Position-wise the flag goes before the
  subcommand argument, e.g. `freertos thread --fpu 10` or
  `freertos thread apply all --fpu bt`.


## References
https://sourceware.org/gdb/current/onlinedocs/gdb.html/Python.html
https://github.com/espressif/freertos-gdb
