KEDR is a framework for dynamic (runtime and post factum) analysis of Linux kernel modules, including device drivers, file system modules, etc. The components of KEDR operate on a kernel module chosen by the user. They can intercept the function calls made by the module and, based on that, detect memory leaks, simulate resource shortage in the system as well as other uncommon situations, etc.

For the present, KEDR is provided for 32- and 64-bit x86 systems.

KEDR can be used in the development of kernel modules (as a component of QA system) as well as when analyzing the kernel failures on a user's system (technical support). Certification systems and other automated verification systems for kernel-mode software can also benefit from it.
