# Load the fault simulation subsystem
module <$fault_simulation_module$>
<$if concat(payload.name)$>
# Load the standard fault simulation plugins
<$payload_elem : join(\n)$>
<$endif$>
<$if concat(indicator.path)$>
# Load the standard indicators
<$indicator_elem : join(\n)$>
<$endif$>
