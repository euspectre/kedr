<$if concat(payload.name)$>
#Load kedr tracing module
module <$kedr_trace_module$>
<$if concat(payload_is_callm)$>
#Load call monitor payloads
<$payload_elem_callm : join()$><$endif$><$if concat(payload.is_fsim)$>
#Load module for fault simulation support
module <$fault_simulation_module$>

#Load fault simulation payloads
<$payload_elem_fsim : join()$><$endif$>
<$endif$>
