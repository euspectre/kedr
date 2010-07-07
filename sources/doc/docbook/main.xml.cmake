<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE article PUBLIC "-//OASIS//DTD DocBook XML V4.5//EN"
    "@DOCBOOK_DTD_FILE@" [ 
        <!ENTITY % kedr-entities SYSTEM "entities.xml"> %kedr-entities; 
        <!ENTITY kedr_service SYSTEM "service.xml">
        <!ENTITY kedr_intro SYSTEM "intro.xml">
        <!ENTITY kedr_call_mon SYSTEM "call_mon.xml">
        <!ENTITY kedr_extend SYSTEM "extend.xml">
        <!ENTITY kedr_fault_sim SYSTEM "fault_sim.xml">
        <!ENTITY kedr_getting_started SYSTEM "getting_started.xml">
        <!ENTITY kedr_install SYSTEM "install.xml">
        <!ENTITY kedr_overview SYSTEM "overview.xml">
        <!ENTITY kedr_tips SYSTEM "tips.xml">
    ]
>

<article lang="en">
<title>KEDR &rel-version; Reference Manual</title>
<articleinfo>
    <releaseinfo>KEDR &rel-version; Reference Manual (&rel-date;)</releaseinfo>
    <authorgroup>
	    <author>
	        <firstname>Eugene</firstname>
	        <surname>Shatokhin</surname>
	        <email>spectre@ispras.ru</email>
	    </author>
	    <author>
	        <firstname>Andrey</firstname>
	        <surname>Tsyvarev</surname>
	        <email>tsyvarev@ispras.ru</email>
	    </author>
    </authorgroup>
    <copyright>
        <year>&kedr-lifespan;</year>
        <holder>&ispras-name;</holder>
    </copyright>
    <legalnotice>
        <para><ulink url="&ispras-url;">&ispras-url;</ulink></para>
    </legalnotice>
</articleinfo>

<!-- ============ Top-level sections ============ -->
<!-- Introduction -->
&kedr_intro;

<!-- Overview -->
&kedr_overview; 

<!-- Getting Started -->
&kedr_getting_started;

<!-- Call Monitoring -->
&kedr_call_mon;

<!-- Fault Simulation -->
&kedr_fault_sim;

<!-- Extending KEDR -->
&kedr_extend;

<!-- Tips and Tricks -->
&kedr_tips;
<!-- Controlling KEDR -->
&kedr_service;
	
</article>
