<$if concat(indicator.parameters.type)$>struct point_data
{
    <$pointDataField : join(\n    )$>
};<$endif$>