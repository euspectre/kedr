module.name = <$module.name$>

module.author = <$module.author$>

module.license = <$module.license$>

<$if concat(header)$><$headerSection: join(\n)$>

<$endif$><$if concat(function.name)$><$block: join(\n)$>

<$endif$>