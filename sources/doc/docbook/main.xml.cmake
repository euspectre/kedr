<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE article PUBLIC "-//OASIS//DTD DocBook XML V4.5//EN"
    "@DOCBOOK_DTD_FILE@" [ 
        <!ENTITY % kedr-entities SYSTEM "entities.xml"> %kedr-entities; 
        <!ENTITY kedr_intro SYSTEM "intro.xml">
        <!ENTITY kedr_getting_started SYSTEM "getting_started.xml">

        <!ENTITY kedr_overview SYSTEM "overview.xml">

        <!ENTITY kedr_service SYSTEM "control.xml">
        <!ENTITY kedr_trace SYSTEM "capture_trace.xml">
        <!ENTITY kedr_work SYSTEM "how_kedr_works.xml">
        <!ENTITY kedr_call_mon SYSTEM "call_mon.xml">
        <!ENTITY kedr_fault_sim SYSTEM "fault_sim.xml">
        <!ENTITY kedr_analyze_trace SYSTEM "analyze_trace.xml">
        <!ENTITY kedr_leak_check SYSTEM "leak_check.xml">
        <!ENTITY kedr_using SYSTEM "using_kedr.xml">
        
        <!ENTITY kedr_payload_api SYSTEM "payload_api.xml">
        <!ENTITY kedr_functions_support SYSTEM "functions_support.xml">
        <!ENTITY kedr_standard_callm_payloads SYSTEM "standard_callm_payloads.xml">
        <!ENTITY kedr_standard_fsim_payloads SYSTEM "standard_fsim_payloads.xml">
        <!ENTITY kedr_standard_fsim_indicators SYSTEM "standard_fsim_indicators.xml">
        <!ENTITY kedr_fault_simulation_api SYSTEM "fault_simulation_api.xml">

        <!ENTITY kedr_using_gen SYSTEM "using_gen.xml">        
        <!ENTITY kedr_custom_callm_payloads SYSTEM "custom_callm_payloads.xml">
        <!ENTITY kedr_custom_fsim_payloads SYSTEM "custom_fsim_payloads.xml">
		<!ENTITY kedr_happens_before_parameter SYSTEM "happens_before_parameter.xml">
        <!ENTITY kedr_custom_fsim_scenarios SYSTEM "custom_fsim_scenarios.xml">
        <!ENTITY kedr_custom_analysis SYSTEM "custom_analysis.xml">
        <!ENTITY kedr_extend SYSTEM "extend.xml">
        <!ENTITY kedr_reference SYSTEM "reference.xml">
        
        <!ENTITY kedr_glossary SYSTEM "glossary.xml">
    ]
>

<article lang="en">
<title>KEDR &rel-version; Reference Manual</title>
<articleinfo>
    <releaseinfo>KEDR Framework version &rel-version; (&rel-date;)</releaseinfo>
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

<!-- Using KEDR -->
&kedr_using;

<!-- Extending KEDR -->
&kedr_extend;

<!-- Reference -->
&kedr_reference;

<!-- Glossary -->
&kedr_glossary;

</article>
