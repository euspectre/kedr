<$if concat(payload.name)$>#Load kedr tracing module
module <$kedr_trace_module$>
#Load call monitor payloads
<$payload_elem : join(\n)$>
<$endif$>
