<$if isIndicatorState$>// Indicator variables
struct indicator_real_state
{
<$if concat(indicator.state.name)$>    <$stateVariableDeclaration: join(\n    )$>
<$endif$><$if concat(indicator.file.name)$>    <$controlFileDeclaration: join(\n    )$>
<$endif$>};
<$endif$>