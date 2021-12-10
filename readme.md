# Lab 2: Dynamic Analysis
This is a template for a simple fault tolerance and isolation project based
on LLVM. The project involves instrumenting an LLVM module in order to
produce a new program with varying policies and strategies for fault
tolerance.

After completing this project, a student should have a better understanding
of the workflow involved with dynamic analysis as well as common techniques
including instrumentation, runtime support libraries, and shadow memory.



## Introduction

This exercise will help you practice and demonstrate your ability to  implement dynamic analyses by using [LLVM](https://llvm.org/) for static program instrumentation. （给一个程序插桩，然后在程序运行时进行动态分析）You will implement several transformations on a program in order to make its behavior tolerate faults, albeit with uncertain or approximate quality of service.(为了使其行为能够容忍故障，尽管服务质量不确定或近似。)

Your analysis will roughly be a whole program analysis. That is, you may assume that you have access to (almost) the entire program. Any program state that needs to be inspected, tracked, or otherwise manipulated will be made available to you via a single compiled `.ll` file containing the IR of the program. All functions whose instructions you need to observe will also be present in this IR. External function calls to libraries like `libc` may occur. You should be able to safely interact with these libraries, but you need not modify them or consider their behavior in any way.（会提供一个.ll文件，不需要考虑libc的安全）

You will protect programs from different types of common bugs by checking a few simple policies at runtime. These are all standard bugs that will cause a program to crash, which can cause problems with availability.

1. Heap, global, and stack pointer accesses.  Any (1) load from or (2) store to a pointer that occurs in the given IR  must access an address that is a valid global, local variable, or heap  allocated memory that occurs within the provided IR.  Both spatial and temporal safety should be enforced.(应加强空间和时间安全)  Once memory is no longer temporally valid, accesses should be invalid.  You may assume that all heap memory will be allocated via a call to  `malloc()`. Allocated heap memory will only be freed via a call to `free()`.
我们要记录当前valid的内存。包括
    1、Heap。由malloc分配，free释放。
    2、stack。判断read和write的参数是不是local variable。
    3、global area。判断read和write的参数是不是global area。

2. Invalid frees.  Any call to `free()` must pass a pointer that is either valid or NULL.
判断free的指针是不是指向valid内存或者是不是为0。free只能是heap。

3. Divide by zero bugs.  Any integer division must have a nonzero denominator.
判断除数是否为零即可。

Your implementation *does not* need to be concerned about performance. No performance benchmarking of your solution will be done unless it is slow enough to impact the grading process. Penalties may apply in those extreme cases.(不必考虑性能)

You will implement a few different policies for handling these types of faults, varying in difficulty. The different policies will allow you to control the quality of service (QOS) of the instrumented program in order to make a trade off between preservation of the original program semantics and availability.

A template for the project is available.

To build this project, you should use LLVM 11.X, available on the [LLVM download page](http://llvm.org/releases/download.html#11.0.1). Downloading the pre-built binaries should provide you everything you need. The `bin/` subdirectory also contains appropriate versions of `clang` and other LLVM related tools that you may use for the project.

### Building using `cmake`

First create a build directory separate from your source directory in order to perform an out-of-source build. From within the build directory for the project, run:

```shell
cmake -DLLVM_DIR=<installation, build, or extraction path of LLVM>/lib/cmake/llvm <path to project source>
```

Then you can simply run `make` to build the project within the build directory. The `tolerator` program will reside at `out/bin/tolerator` in the build directory.

## Task 1 (Terminating with warnings)

The simplest way to handle incorrect behaviors is simply to terminate the running program and print (log) an error message describing what form of error has occurred. For this task, you will handle each of the three classes of bugs by simply printing out an error message to `stderr` and ending the program,（打印错误信息，并且终止程序，返回-1） making the program return -1 to the system to indicate an error.（我可以加个色彩？）

The specific error messages should be:

- "`FOUND: Invalid read from memory\n`"
- "`FOUND: Invalid write to memory\n`"
- "`FOUND: Invalid free of memory\n`"
- "`FOUND: Division by zero\n`"

Make sure your error messages are correct so that any automated grading does take points that you would otherwise earn. You have been provided with some test cases, but there are more that you can write on your own.

Your instrumentor should be able to run as:

```shell
tolerator <toinstrument.ll> -o instrumented
```

This should produce a program that can run as:

```shell
./instrumented <normal> <command> <line> <arguments>
```

## Task 2 (Task 1 + Ignoring invalid side effects)

Consider the impact of behaviors that don't produce a value but rather produce a *side effect*. In our case, this includes (1) stores to memory and (2) frees of memory. These instructions need not necessarily do anything in order for the program to be well formed. Storing to an invalid location will not even matter if we only read from valid locations. Similarly, no valid memory can be freed when we call free with an invalid address. Thus, we might simply ignore these instructions.

When your instrumentor is called with the `-ignore` argument, the resulting program shall log the bad behavior, as in part 1, but it will ignore the effects of invalid operations without values and continue running.

## Task 3 (Task 2 + Default values)

We might take this a step further. Note that we have not been able to avoid reads from invalid memory or division by zero. In these case *some* value must be produced; we cannot simply ignore the operation. However, we might consider whether the produced value matters a great deal…. If the value does not matter, we could simply return a default value instead.

When your instrumentor is called with the `-defaults` argument, the resulting program shall log the bad behavior and discard valueless faults, as in part 2, but it will provide erroneous(错误的) loads from memory and divisions by zero with the value `0` and keep running.

Note, now none of the original fault kinds will result in a program that terminates!

## Task 4 (Task 1 + local bypass)

Even more aggressively, we might say that all computation within a function that performs an invalid memory operation or division is suspect,(可疑的;不可信的;靠不住的; ) so we really ought to exit the function if we want to avoid the bad behavior while continuing to run. For any function returning `void`, this is easy, we can simply return directly from the function. For a function that returns a value, we can try returning a zero initialized version of that value type and keep executing.

When your instrumentor is called with the `-returns` argument, the resulting program shall log the bad behavior and return to the caller of the current function. Similar to task 3, any returned from function that produces a value should produce the zero initalized form of that value.

## Submitting

Create an archive of your solution by changing into the directory *containing* your project template and running:

```shell
tar zcvf lab2.tar.gz se-fault-tolerant-template/
```

Put the source archive and your  report into a .zip file submit it.

**Dead line:** 2021年12月22日23:59

## Related Work

There is a substantial amount of work on fault tolerant and failure oblivious computing. More recently, there has been interest in work that can scale the quality of service with other requirements of the software (whether fault tolerance or performance).

- [Rx: treating bugs as allergies—a safe method to survive software failures](https://dl.acm.org/citation.cfm?id=1095833)
- [Exterminator: Automatically Correcting Memory Errors with High Probability](https://people.cs.umass.edu/~emery/pubs/pldi028-novark.pdf)
- [Enhancing Server Availability and Security through Failure-Oblivious Computing](https://www.usenix.org/legacy/event/osdi04/tech/full_papers/rinard/rinard.pdf)
- [DoubleTake: Fast and Precise Error Detection via Evidence-Based Dynamic Analysis](http://www.cs.utsa.edu/~tongpingliu/pubs/doubletake-icse16.pdf)
- [Obtaining  and  Reasoning  About  Good  Enough  Software](https://people.csail.mit.edu/rinard/paper/dac12.pdf)
- [Detecting and Escaping Infinite Loops with Jolt](https://groups.csail.mit.edu/pac/jolt/paper.pdf)
- [Bolt:  On-Demand  Infinite  Loop  Escapein  Unmodified  Binaries](https://people.csail.mit.edu/rinard/paper/oopsla12.pdf)
- [Automatic recovery from runtime failures](https://software.imdea.org/~alessandra.gorla/papers/Carzaniga-ARMOR-ICSE13.pdf)
- [AI: A Lightweight System for Tolerating Concurrency Bugs](https://james0zan.github.io/resource/fse14_zhang.pdf)
- [An Analysis of Patch Plausibility and Correctness for Generate-and-Validate Patch Generation Systems](https://people.csail.mit.edu/fanl/papers/kali-issta2015.pdf)
- [Exhaustive Exploration of the Failure-oblivious Computing Search Space](https://arxiv.org/pdf/1710.09722.pdf)
- [Compiling Sandboxes: Formally Verified Software Fault Isolation](http://people.rennes.inria.fr/Frederic.Besson/compcertSFI.pdf)
- [Principles and Implementation Techniques of Software-Based Fault Isolation](http://www.cse.psu.edu/~gxt29/papers/sfi-final.pdf)
- [Efficient software-based fault isolation](http://web.stanford.edu/class/archive/cs/cs295/cs295.1086/papers/wahbe93efficient.pdf)
- [Automatic Failure Recovery for End-User Programs on Service Mobile Robots](https://arxiv.org/abs/1909.02778)