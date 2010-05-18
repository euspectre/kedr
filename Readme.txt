The goal of the project is to create a set of tools for dynamic (runtime) 
analysis and verification of Linux kernel-mode drivers. The tools are 
intended to be used by the driver developers and may be useful for the 
driver certification systems as well.

The tools developed in this project should allow analysing a driver or a 
group of drivers chosen by the user. This would complement many existing 
tools for error detection, fault simulation, memory checking, etc., that 
operate on the Linux kernel as a whole.

The analysis should be carried out with as little influence on the rest of 
the operating system as reasonably achievable without sacrificing other 
goals mentioned here. 

The idea of the typical usage of the tools is as follows. The user works 
with the "target" driver as usual or may be runs some specific tests on it. 
At the same time, our tools are monitoring the operation of the driver, 
checking if it works correctly, doing fault simulation if requested, 
dumping the data about the actions made by the driver to some kind of a 
trace for future analysis, etc.

The driver analysis engine is intended to be extendable to be able to serve 
as a platform for the development of various custom tools for Linux driver 
verification and probably for other kinds of analysis.
