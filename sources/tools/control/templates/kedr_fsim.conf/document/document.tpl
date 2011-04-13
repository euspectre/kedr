<$if concat(payload.name)$>#Load kedr fault simulation module
module <$fault_simulation_module$>
#Load fault simulation payloads
<$payload_elem : join(\n)$>
<$endif$>
